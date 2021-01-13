#include "expr.hpp"

#include <string_view>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "env.hpp"
#include "util.hpp"
#include "opcodes.hpp"

namespace nivalis {
namespace {
using ::nivalis::OpCode::repr;

// Hash combine
void hash_combine(uint64_t& seed, uint64_t v) {
    seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// ** AST node form processing + optimization **
// AST Link Node representation
// instead of list-form AST, we explicitly put pointers to children
// to allow easy modification/deletion during optimization
struct ASTLinkNode {
    explicit ASTLinkNode(uint32_t opcode) : opcode(opcode) {
        ref = -1;
        nonconst_flag = false;
        null_flag = false;
        hash = 0;
        val = 0.;
    }
    static ASTLinkNode value(complex val) {
        ASTLinkNode tmp(OpCode::val);
        tmp.val = val;
        return tmp;
    }
    uint64_t rehash(const std::vector<ASTLinkNode>& nodes) {
        hash = (uint64_t)opcode;
        hash_combine(hash, ref);
        if (opcode == OpCode::val) {
            hash_combine(hash, *reinterpret_cast<uint64_t*>(&val));
        } else
            hash_combine(hash, 0);
        for (size_t i = 0; i < std::min<size_t>(c.size(), 3); ++i) {
            hash_combine(hash, nodes[c[i]].hash);
        }
        for (size_t i = std::min<size_t>(c.size(), 3); i < 3; ++i) {
            hash_combine(hash, 0);
        }
        if (opcode == OpCode::call) {
            hash_combine(hash, (size_t)call_info[0]);
        } else {
            hash_combine(hash, 0);
        }
        return hash;
    }

    bool equals(const ASTLinkNode& other,
                const std::vector<ASTLinkNode>& nodes) const {
        if (other.hash != hash || other.opcode != opcode || other.ref != ref ||
            other.val != val || other.c.size() != c.size())
            return false;
        for (size_t i = 0; i < c.size(); ++i) {
            if (!nodes[other.c[i]].equals(nodes[c[i]], nodes)) {
                return false;
            }
        }
        return true;
    }

    uint64_t hash;
    uint32_t opcode;
    uint64_t ref;
    uint32_t call_info[2];
    std::vector<size_t> c;
    complex val;
    bool nonconst_flag, null_flag;
};

// Convert ast nodes -> link form
uint32_t _ast_to_link_nodes(const Expr::ASTNode** ast,
                            std::vector<ASTLinkNode>& store) {
    uint32_t node_idx = static_cast<uint32_t>(store.size());
    uint32_t opcode = (*ast)->opcode;
    store.emplace_back(opcode);
    size_t n_args = OpCode::n_args(opcode);
    if (opcode == OpCode::call) {
        // User function, special way to get number of arguments
        // (which is not fixed)
        n_args = (size_t)(*ast)->call_info[1];
        memcpy(store[node_idx].call_info, (*ast)->call_info,
               sizeof((*ast)->call_info));
    }
    if (opcode == OpCode::val)
        store[node_idx].val = (*ast)->val;
    else if (OpCode::has_ref(opcode))
        store[node_idx].ref = (*ast)->ref;
    ++*ast;
    store[node_idx].c.resize(n_args);
    for (size_t i = 0; i < n_args; ++i) {
        uint32_t ret = _ast_to_link_nodes(ast, store);
        store[node_idx].c[i] = ret;
    }
    auto& node = store[node_idx];
    using namespace OpCode;
    switch (node.opcode) {
        case sub:
            // Convert - to +- for convenience later
            {
                node.opcode = add;
                auto ri = node.c[1];
                auto& r = store[ri];
                if (r.opcode == OpCode::val) {
                    r.val = -r.val;
                } else {
                    node.c[1] = static_cast<uint32_t>(store.size());
                    store.emplace_back(OpCode::unaryminus);
                    store.back().c.push_back(ri);
                }
            }
            break;
        case divi:
            // Convert / to * pow -1 for convenience later
            // (not truly equivalent but will optimize anyway)
            {
                node.opcode = mul;
                auto ri = node.c[1];
                auto& r = store[ri];
                if (r.opcode == OpCode::val) {
                    r.val = 1. / r.val;
                } else {
                    node.c[1] = static_cast<uint32_t>(store.size());
                    store.emplace_back(OpCode::power);
                    {
                        auto& pownd = store.back();
                        pownd.c.push_back(ri);
                        pownd.c.push_back(static_cast<uint32_t>(store.size()));
                    }
                    store.emplace_back(OpCode::val);
                    store.back().val = -1.;
                }
            }
            break;
        // Expand shorthand functions
        case expb: {
            double base = node.opcode == expb ? M_E : 2.;
            node.opcode = power;
            node.c.insert(node.c.begin(), store.size());
            store.emplace_back(OpCode::val);
            store.back().val = base;
        } break;
        case log10b:
        case logb: {
            double base = node.opcode == (node.opcode == log10b ? 10. : M_E);
            node.opcode = logbase;
            node.c.push_back(store.size());
            store.emplace_back(OpCode::val);
            store.back().val = base;
        } break;
        case sqrtb:
        case sqrb: {
            double expo = node.opcode == sqrtb ? 0.5 : 2.;
            node.opcode = power;
            node.c.push_back(store.size());
            store.emplace_back(OpCode::val);
            store.back().val = expo;
        } break;
    }
    return node_idx;
}
// Convert ast nodes -> link formn(wrapper)
size_t ast_to_link_nodes(const Expr::AST& ast,
                         std::vector<ASTLinkNode>& store) {
    const Expr::ASTNode* astptr = &ast[0];
    return _ast_to_link_nodes(&astptr, store);
}

// Convert link form AST nodes -> usual AST nodes
void ast_from_link_nodes(const std::vector<ASTLinkNode>& nodes, Expr::AST& out,
                         size_t index = 0) {
    const auto& node = nodes[index];
    if (index >= nodes.size()) {
        std::cerr << "ast_from_link_nodes ERROR: invalid AST. Node index "
                  << index << " out of bounds.\n";
        return;
    }
    size_t out_idx = out.size();
    out.emplace_back(node.opcode);
    auto& out_node = out.back();
    if (node.opcode == OpCode::val) {
        out_node.val = node.val;
    } else if (OpCode::has_ref(node.opcode)) {
        out_node.ref = node.ref;
    }
    size_t n_args = OpCode::n_args(node.opcode);
    if (node.opcode == OpCode::call) {
        // Copy back the user function data
        n_args = (size_t)node.call_info[1];
        memcpy(out_node.call_info, node.call_info, sizeof(node.call_info));
    }
    for (size_t i = 0; i < std::min(node.c.size(), n_args); ++i) {
        ast_from_link_nodes(nodes, out, node.c[i]);
        if (node.opcode == OpCode::thunk_ret &&
            nodes[node.c[i]].opcode == OpCode::thunk_jmp) {
            // Set thunk ref
            out.back().ref = out.size() - out_idx - 1;
        }
    }
}

// Print link nodes for debugging purposes
void print_link_nodes(const std::vector<ASTLinkNode>& nodes) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        std::cout << i << ": ";
        const char* r = OpCode::repr(nodes[i].opcode);
        for (size_t j = 0; j < strlen(r); ++j) {
            if (r[j] == '\v')
                std::cout << "v";
            else if (r[j] == '\r')
                std::cout << "r";
            else
                std::cout << r[j];
        }
        if (nodes[i].opcode == OpCode::val) {
            std::cout << nodes[i].val;
        }
        if (OpCode::has_ref(nodes[i].opcode)) {
            std::cout << nodes[i].ref;
        }
        std::cout << " -> ";
        for (size_t j = 0; j < nodes[i].c.size(); ++j) {
            std::cout << nodes[i].c[j] << " ";
        }
        std::cout << "\n";
    }
}

// Main optimization rules
void optim_link_nodes_main(Environment& env, std::vector<ASTLinkNode>& nodes,
                           size_t vi = 0) {
    if ((OpCode::has_ref(nodes[vi].opcode) && ~nodes[vi].ref) ||
        nodes[vi].opcode == OpCode::call)
        nodes[vi].nonconst_flag = true;
    size_t i = 0;
    for (; i < nodes[vi].c.size(); ++i) {
        auto ui = nodes[vi].c[i];
        if (ui == -1) break;
        nodes[ui].nonconst_flag = false;
        nodes[ui].null_flag = false;
        optim_link_nodes_main(env, nodes, ui);
        // Detect nonconst
        nodes[vi].nonconst_flag |= nodes[ui].nonconst_flag;
        // Detect nan
        if (nodes[ui].opcode == OpCode::val &&
            nodes[vi].opcode != OpCode::bnz  // bnz can short-circuit
        ) {
            if (std::isnan(nodes[ui].val.real())) nodes[vi].null_flag = true;
        }
    }
    if (i == 0) {
        // nothing
    } else if (nodes[vi].null_flag) {
        // Set null
        nodes[vi].opcode = OpCode::null;
        nodes[vi].c.clear();
    } else if (!nodes[vi].nonconst_flag &&
               nodes[vi].opcode != OpCode::thunk_ret &&
               nodes[vi].opcode != OpCode::thunk_jmp) {
        // Evaluate constants
        std::vector<Expr::ASTNode> ast;
        ast_from_link_nodes(nodes, ast, vi);
        // Debug
        // print_link_nodes(nodes); std::cout << ast <<" T\n";
        nodes[vi].val = detail::eval_ast(env, ast);
        nodes[vi].opcode = OpCode::val;
        nodes[vi].c.clear();
    } else {
        static auto is_positive = [](std::vector<ASTLinkNode>& nodes,
                                     size_t idx) -> bool {
            return nodes[idx].opcode == OpCode::absb;
        };
        // Apply rules
        using namespace OpCode;
        size_t li = vi, ri = vi;
        if (nodes[vi].c.size() > 0) {
            li = nodes[vi].c[0];
            if (nodes[vi].c.size() > 1) ri = nodes[vi].c[1];
        }
        static const size_t NEW_NODE = std::numeric_limits<size_t>::max();
        switch (nodes[vi].opcode) {
            case absb:
                // Involution
                if (is_positive(nodes, li)) nodes[vi] = nodes[li];
                break;
            case floorb:
            case ceilb:
            case roundb:
                if (nodes[li].opcode == floorb || nodes[li].opcode == ceilb ||
                    nodes[li].opcode == roundb) {
                    nodes[vi] = nodes[li];
                }
                break;
            case unaryminus:
                // Combine -
                if (nodes[li].opcode == val) {
                    nodes[vi] = nodes[li];
                    nodes[vi].val = -nodes[vi].val;
                } else if (nodes[li].opcode == unaryminus) {
                    nodes[vi] = nodes[nodes[li].c[0]];
                    break;
                }
                if (nodes[li].opcode == mul) {
                    auto lli = nodes[li].c[0];
                    auto* ll = &nodes[lli];
                    if (ll->opcode == val) {
                        ll->val = -ll->val;
                        nodes[vi] = nodes[li];
                    }
                    break;
                }
                break;
            case add:
                // Normalize
                if ((nodes[ri].opcode == val && nodes[li].opcode != val) ||
                    nodes[li].opcode == add) {
                    if (nodes[vi].opcode == add) {
                        std::swap(nodes[li], nodes[ri]);
                    }
                }
                if (nodes[ri].opcode == add && nodes[li].opcode == val) {
                    auto rli = nodes[ri].c[0];
                    auto* rl = &nodes[rli];
                    if (rl->opcode == val) {
                        nodes[li].val += rl->val;
                        nodes[ri] = nodes[nodes[ri].c[1]];
                    }
                }
                if (nodes[ri].opcode == unaryminus &&
                    nodes[nodes[ri].c[0]].opcode == add &&
                    nodes[li].opcode == val) {
                    auto rli = nodes[nodes[ri].c[0]].c[0];
                    auto* rl = &nodes[rli];
                    if (rl->opcode == val) {
                        nodes[li].val -= rl->val;
                        nodes[nodes[ri].c[0]] =
                            nodes[nodes[nodes[ri].c[0]].c[1]];
                    }
                }

                if (nodes[vi].opcode == add) {
                    // Try swapping, if also add
                    if (nodes[li].opcode == add) {
                        std::swap(nodes[nodes[li].c[0]], nodes[ri]);
                        optim_link_nodes_main(env, nodes, li);
                        if (nodes[li].opcode == add)
                            std::swap(nodes[nodes[li].c[0]], nodes[ri]);
                    }
                    if (nodes[li].opcode == add) {
                        std::swap(nodes[nodes[li].c[1]], nodes[ri]);
                        optim_link_nodes_main(env, nodes, li);
                        if (nodes[li].opcode == add)
                            std::swap(nodes[nodes[li].c[0]], nodes[ri]);
                    }
                    if (nodes[ri].opcode == add) {
                        std::swap(nodes[li], nodes[nodes[ri].c[0]]);
                        optim_link_nodes_main(env, nodes, ri);
                        if (nodes[ri].opcode == add)
                            std::swap(nodes[li], nodes[nodes[ri].c[0]]);
                    }
                    if (nodes[ri].opcode == add) {
                        std::swap(nodes[li], nodes[nodes[ri].c[1]]);
                        optim_link_nodes_main(env, nodes, ri);
                        if (nodes[ri].opcode == add)
                            std::swap(nodes[li], nodes[nodes[ri].c[0]]);
                    }
                }

                // Factoring
                {
                    bool factored = false;
                    for (int l_side = 0; l_side < 2; ++l_side) {
                        for (int r_side = 0; r_side < 2; ++r_side) {
                            size_t lcoeffi, rcoeffi, rtermi, ltermi;
                            bool l_neg = false, r_neg = false;
                            lcoeffi = NEW_NODE;
                            if (nodes[li].opcode == unaryminus) {
                                ltermi = nodes[li].c[0];
                                l_neg = true;
                            } else {
                                ltermi = li;
                            }
                            if (nodes[ltermi].opcode == mul) {
                                lcoeffi = nodes[ltermi].c[l_side];
                                ltermi = nodes[ltermi].c[1 - l_side];
                            } else if (l_side == 1)
                                continue;
                            rcoeffi = NEW_NODE;
                            if (nodes[ri].opcode == unaryminus) {
                                rtermi = nodes[ri].c[0];
                                r_neg = true;
                            } else {
                                rtermi = ri;
                            }
                            if (nodes[rtermi].opcode == mul) {
                                rcoeffi = nodes[rtermi].c[r_side];
                                rtermi = nodes[rtermi].c[1 - r_side];
                            } else if (r_side == 1)
                                continue;

                            bool lcoeff_new = (lcoeffi == NEW_NODE);
                            if (lcoeff_new) {
                                lcoeffi = nodes.size();
                                nodes.push_back(ASTLinkNode::value(1.0));
                            }
                            if (rcoeffi == NEW_NODE) {
                                rcoeffi = nodes.size();
                                nodes.push_back(ASTLinkNode::value(1.0));
                            }

                            if (nodes[ltermi].equals(nodes[rtermi], nodes)) {
                                // print_link_nodes(nodes);
                                if (l_neg) {
                                    size_t neg_i = lcoeff_new
                                                       ? nodes[li].c[0]
                                                       : nodes[nodes[li].c[0]]
                                                             .c[1 - r_side];
                                    nodes[neg_i].opcode = unaryminus;
                                    nodes[neg_i].c.resize(1);
                                    nodes[neg_i].c[0] = lcoeffi;
                                    lcoeffi = neg_i;
                                }
                                if (r_neg) {
                                    nodes[ri].c[0] = rcoeffi;
                                    rcoeffi = ri;
                                }

                                nodes[li].opcode = add;
                                nodes[vi].opcode = mul;
                                nodes[li].c.resize(2);
                                nodes[li].ref = -1;
                                nodes[li].c[0] = lcoeffi;
                                nodes[li].c[1] = rcoeffi;
                                nodes[vi].c[1] = rtermi;
                                nodes[vi].nonconst_flag = false;
                                optim_link_nodes_main(env, nodes, vi);
                                factored = true;
                                break;
                            }
                        }
                        if (factored) break;
                    }
                    if (factored) break;
                }

                // Trig
                if (nodes[vi].opcode == add && nodes[li].opcode == power &&
                    nodes[ri].opcode == power) {
                    size_t lli = nodes[li].c[0], lri = nodes[li].c[1];
                    size_t rli = nodes[ri].c[0], rri = nodes[ri].c[1];
                    if (nodes[lri].opcode == val && nodes[rri].opcode == val &&
                        nodes[lri].val == 2. && nodes[rri].val == 2.) {
                        if (((nodes[lli].opcode == sinb &&
                              nodes[rli].opcode == cosb) ||
                             (nodes[lli].opcode == cosb &&
                              nodes[rli].opcode == sinb)) &&
                            nodes[nodes[lli].c[0]].equals(
                                nodes[nodes[rli].c[0]], nodes)) {
                            nodes[vi].opcode = val;
                            nodes[vi].val = 1.;
                            break;
                        }
                    }
                }

                if (nodes[li].opcode == val && nodes[li].val == 0.) {
                    nodes[vi] = nodes[ri];
                } else if (nodes[ri].opcode == val && nodes[ri].val == 0.)
                    nodes[vi] = nodes[li];
                break;
            case mul:
                // Take out - sign
                if (nodes[li].opcode == unaryminus &&
                    nodes[ri].opcode == unaryminus) {
                    nodes[li] = nodes[nodes[li].c[0]];
                    nodes[ri] = nodes[nodes[ri].c[0]];  // Cancel
                } else if (nodes[li].opcode == unaryminus) {
                    size_t tmp = nodes[li].c[0];
                    nodes[li] = nodes[vi];
                    nodes[vi].opcode = unaryminus;
                    nodes[vi].c.resize(1);
                    nodes[li].c[0] = tmp;
                    nodes[vi].nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    return;
                } else if (nodes[ri].opcode == unaryminus) {
                    size_t tmp = nodes[ri].c[0];
                    nodes[ri] = nodes[vi];
                    nodes[vi].opcode = unaryminus;
                    nodes[vi].c[0] = nodes[vi].c[1];
                    nodes[vi].c.resize(1);
                    nodes[ri].c[1] = tmp;
                    nodes[vi].nonconst_flag = false;
                    nodes[ri].nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    return;
                }
                // Try swapping, if also mul
                if (nodes[li].opcode == mul) {
                    std::swap(nodes[nodes[li].c[0]], nodes[ri]);
                    optim_link_nodes_main(env, nodes, li);
                }
                if (nodes[li].opcode == mul) {
                    std::swap(nodes[nodes[li].c[1]], nodes[ri]);
                    optim_link_nodes_main(env, nodes, li);
                }
                if (nodes[ri].opcode == mul) {
                    std::swap(nodes[li], nodes[nodes[ri].c[0]]);
                    optim_link_nodes_main(env, nodes, ri);
                }
                if (nodes[ri].opcode == mul) {
                    std::swap(nodes[li], nodes[nodes[ri].c[1]]);
                    optim_link_nodes_main(env, nodes, ri);
                }
                // Normalize left/right
                if ((nodes[ri].opcode == val && nodes[li].opcode != val) ||
                    (nodes[ri].opcode != mul && nodes[li].opcode == mul)) {
                    std::swap(nodes[li], nodes[ri]);
                }
                if ((nodes[li].opcode == val && nodes[li].val == 0.) |
                    (nodes[ri].opcode == val && nodes[ri].val == 0.)) {
                    nodes[vi].opcode = OpCode::val;
                    nodes[vi].val = 0.;
                } else if (nodes[li].opcode == val && nodes[li].val == 1.)
                    nodes[vi] = nodes[ri];
                else if (nodes[ri].opcode == val && nodes[ri].val == 1.)
                    nodes[vi] = nodes[li];
                else if (nodes[li].opcode == val && nodes[li].val == -1.) {
                    nodes[vi].opcode = unaryminus;
                    nodes[li] = nodes[ri];
                    nodes[vi].c.pop_back();
                    break;
                } else if (nodes[ri].opcode == val && nodes[ri].val == -1.) {
                    nodes[vi].opcode = unaryminus;
                    nodes[vi].c.pop_back();
                } else {
                    // Combining powers
                    size_t lf, rf, rmi, lmi;
                    double lf_new_val, rf_new_val;
                    if (nodes[li].opcode == power) {
                        lmi = nodes[li].c[0];
                        lf = nodes[li].c[1];
                    } else if (nodes[li].opcode == unaryminus) {
                        lmi = nodes[ri].c[0];
                        lf = NEW_NODE;
                        lf_new_val = -1.;
                    } else {
                        lmi = li;
                        lf = NEW_NODE;
                        lf_new_val = 1.;
                    }
                    if (nodes[ri].opcode == power) {
                        rf = nodes[ri].c[1];
                        rmi = nodes[ri].c[0];
                    } else if (nodes[ri].opcode == unaryminus) {
                        rmi = nodes[ri].c[0];
                        rf = NEW_NODE;
                        rf_new_val = -1.;
                    } else {
                        rf = NEW_NODE;
                        rmi = ri;
                        rf_new_val = 1.;
                    }
                    if (lf == NEW_NODE) {
                        lf = nodes.size();
                        nodes.push_back(ASTLinkNode::value(lf_new_val));
                    }
                    if (rf == NEW_NODE) {
                        rf = nodes.size();
                        nodes.push_back(ASTLinkNode::value(rf_new_val));
                    }
                    if (nodes[lmi].equals(nodes[rmi], nodes)) {
                        nodes[vi].opcode = OpCode::power;
                        nodes[li].opcode = OpCode::add;
                        nodes[li].c.resize(2);
                        nodes[li].ref = -1;
                        nodes[li].c[0] = lf;
                        nodes[li].c[1] = rf;
                        nodes[vi].c[0] = rmi;
                        nodes[vi].c[1] = li;
                        nodes[vi].nonconst_flag = false;
                        optim_link_nodes_main(env, nodes, vi);
                    }
                }
                break;
            case power:
                if (nodes[li].opcode == val) {
                    if (nodes[li].val == complex(1.0, 0.0) ||
                        nodes[li].val == complex(0.0, 0.0)) {
                        nodes[vi].opcode = OpCode::val;
                        nodes[vi].val = nodes[li].val;
                        break;
                    }
                }
                if (nodes[ri].opcode == val) {
                    if (nodes[ri].val == 1.) {
                        nodes[vi] = nodes[li];
                        break;
                    } else if (nodes[ri].val == 0.) {
                        nodes[vi].opcode = OpCode::val;
                        nodes[vi].val = 1.;
                        nodes[vi].c.clear();
                        nodes[vi].nonconst_flag = false;
                        break;
                    }
                }
                if (nodes[li].opcode == power) {
                    nodes[li].opcode = mul;
                    nodes[vi].opcode = power;
                    nodes[vi].c[0] = nodes[li].c[0];
                    nodes[li].c[0] = ri;
                    nodes[vi].c[1] = li;
                    nodes[vi].nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    break;
                }
                if (nodes[ri].opcode == logbase &&
                    nodes[li].equals(nodes[nodes[ri].c[1]], nodes) &&
                    is_positive(nodes, nodes[ri].c[0])) {
                    // Inverse
                    nodes[vi] = nodes[nodes[ri].c[0]];
                    nodes[vi].nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    break;
                }
                break;
            case logbase:
                if (nodes[li].opcode == power &&
                    nodes[ri].equals(nodes[nodes[li].c[0]], nodes)) {
                    // Inverse
                    nodes[vi] = nodes[nodes[li].c[1]];
                    nodes[vi].nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    break;
                } else if (nodes[li].opcode == power &&
                           is_positive(nodes, nodes[li].c[0])) {
                    // Take the exponent out
                    nodes[vi].opcode = mul;
                    nodes[li].opcode = logbase;
                    nodes[vi].c[0] = nodes[li].c[1];
                    nodes[vi].c[1] = li;
                    nodes[li].c[1] = ri;
                    break;
                }
                break;
            case bnz:
                if (nodes[li].opcode == val) {
                    if (std::abs(nodes[li].val) != 0) {
                        nodes[vi] = nodes[ri];  // Left
                    } else {
                        nodes[vi] = nodes[nodes[vi].c[2]];  // Right
                    }
                    if (nodes[vi].opcode == OpCode::thunk_ret) {
                        nodes[vi] = nodes[nodes[vi].c[0]];
                    }
                    break;
                } else {
                    // Remove branch if if/else exprs equal
                    size_t r2i = nodes[vi].c[2];
                    if (nodes[ri].opcode == OpCode::thunk_ret &&
                        nodes[r2i].opcode == OpCode::thunk_ret) {
                        size_t rli = nodes[ri].c[0];
                        size_t r2li = nodes[r2i].c[0];
                        if (nodes[rli].equals(nodes[r2li], nodes)) {
                            nodes[vi] = nodes[rli];
                            break;
                        }
                    }
                }
                break;
            case bsel:
                // Super useless opcode
                nodes[vi] = nodes[ri];
                break;
            case max:
            case min:
                // If both sides are same,
                // keep only one of them
                if (nodes[li].equals(nodes[ri], nodes)) {
                    nodes[vi] = nodes[li];
                }
                break;
            case eq:
            case ne:
            case le:
            case ge:
            case lt:
            case gt:
                if (nodes[li].equals(nodes[ri], nodes)) {
                    nodes[vi].c.clear();
                    nodes[vi].val = (double)(nodes[vi].opcode == eq ||
                                             nodes[vi].opcode == le ||
                                             nodes[vi].opcode == ge);
                    nodes[vi].opcode = val;
                }
                break;
        }
    }
    for (size_t j = 0; j < nodes[vi].c.size(); ++j) {
        nodes[nodes[vi].c[j]].rehash(nodes);
    }
    nodes[vi].rehash(nodes);
}
// Final pass optimization rules, replace x^-1 with 1/x,
// apply shorthand functions like exp(x), log2(x) etc.
void optim_link_nodes_final_pass(Environment& env,
                                 std::vector<ASTLinkNode>& nodes,
                                 size_t vi = 0) {
    size_t i = 0;
    for (; i < nodes[vi].c.size(); ++i) {
        size_t ui = nodes[vi].c[i];
        if (ui == -1) break;
        optim_link_nodes_final_pass(env, nodes, ui);
    }
    if (i == 0 || !nodes[vi].nonconst_flag) {
        // nothing
    } else {
        // Apply rules
        using namespace OpCode;
        size_t li = vi, ri = vi;
        if (nodes[vi].c.size() > 0) {
            li = nodes[vi].c[0];
            if (nodes[vi].c.size() > 1) ri = nodes[vi].c[1];
        }
        switch (nodes[vi].opcode) {
            case add:
                if (nodes[ri].opcode == unaryminus) {
                    // Convert a+(-b) [form preferred by optimizer]
                    // back to a-b
                    nodes[vi].c[1] = nodes[ri].c[0];
                    nodes[vi].opcode = sub;
                }
                break;
            case mul:
                if (nodes[ri].opcode == divi) {
                    size_t rli = nodes[ri].c[0];
                    if (nodes[rli].opcode == val && nodes[rli].val == 1.) {
                        nodes[vi].opcode = divi;
                        nodes[ri] = nodes[nodes[ri].c[1]];
                    }
                }
                break;
            case power:
                if (nodes[ri].opcode == val) {
                    if (nodes[ri].val == 2.) {
                        nodes[vi].opcode = OpCode::sqrb;
                        nodes[vi].c.resize(1);
                        break;
                    } else if (nodes[ri].val == 0.5) {
                        nodes[vi].opcode = OpCode::sqrtb;
                        nodes[vi].c.resize(1);
                        break;
                    } else if (nodes[ri].val == complex(-1, 0.0)) {
                        // Convert ()^-1 back to divide
                        nodes[vi].opcode = OpCode::divi;
                        nodes[ri] = nodes[li];
                        nodes[vi].c[0] = nodes.size();
                        nodes.push_back(ASTLinkNode::value(1.0));
                        break;
                    }
                }
                if (nodes[li].opcode == val) {
                    if (nodes[li].val == M_E) {
                        nodes[vi].opcode = OpCode::expb;
                        nodes[vi].c[0] = nodes[vi].c[1];
                        nodes[vi].c.resize(1);
                        break;
                    }
                }
                break;
            case logbase:
                if (nodes[ri].opcode == val) {
                    if (nodes[ri].val == M_E) {
                        nodes[vi].opcode = OpCode::logb;
                        nodes[vi].c.resize(1);
                        break;
                    } else if (nodes[ri].val == 10.) {
                        nodes[vi].opcode = OpCode::log10b;
                        nodes[vi].c.resize(1);
                        break;
                    }
                }
                break;
        }
    }
}
}  // namespace

// Implementation of optimize in Expr class
void Expr::optimize(int num_passes) {
    // std::cout << *this << std::endl;
    std::vector<ASTLinkNode> nodes;
    ast_to_link_nodes(ast, nodes);

    Environment dummy_env;
    for (int i = 0; i < num_passes; ++i)
        optim_link_nodes_main(dummy_env, nodes);
    optim_link_nodes_final_pass(dummy_env, nodes);

    ast.clear();
    ast_from_link_nodes(nodes, ast);
    // std::cout << "FIN" << std::endl;
}
}  // namespace nivalis
