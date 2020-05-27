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
    seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
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
    uint64_t rehash(const std::vector<ASTLinkNode>& nodes) {
        hash = (uint64_t) opcode;
        hash_combine(hash, ref);
        if (opcode == OpCode::val) {
            hash_combine(hash, *reinterpret_cast<uint64_t*>(&val));
        } else hash_combine(hash, 0);
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
                other.val != val || other.c.size() != c.size()) return false;
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
    double val;
    bool nonconst_flag, null_flag;
};


// Convert ast nodes -> link form
uint32_t _ast_to_link_nodes(
        const Expr::ASTNode** ast,
        std::vector<ASTLinkNode>& store) {
    uint32_t node_idx = static_cast<uint32_t>(store.size());
    uint32_t opcode = (*ast)->opcode;
    store.emplace_back(opcode);
    size_t n_args = OpCode::n_args(opcode);
    if (opcode == OpCode::call) {
        // User function, special way to get number of arguments
        // (which is not fixed)
        n_args = (size_t)(*ast)->call_info[1];
        memcpy(store[node_idx].call_info,
                (*ast)->call_info,
                sizeof((*ast)->call_info));
    }
    if (opcode == OpCode::val) store[node_idx].val = (*ast)->val;
    else if (OpCode::has_ref(opcode)) store[node_idx].ref = (*ast)->ref;
    ++*ast;
    store[node_idx].c.resize(n_args);
    for (size_t i = 0; i < n_args; ++i) {
        uint32_t ret = _ast_to_link_nodes(ast, store);
        store[node_idx].c[i] = ret;
    }
    auto& node = store[node_idx];
    using namespace OpCode;
    switch(node.opcode) {
        case sub:
            // Convert - to +- for convenience later
            {
                node.opcode = add;
                auto ri = node.c[1];
                auto & r = store[ri];
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
                auto & r = store[ri];
                if (r.opcode == OpCode::val) {
                    r.val = 1./r.val;
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
        case expb: case exp2b:
            {
                double base = node.opcode == expb ? M_E : 2.;
                node.opcode = power;
                node.c.insert(node.c.begin(), store.size());
                store.emplace_back(OpCode::val);
                store.back().val = base;
            }
            break;
        case log2b: case log10b: case logb:
            {
                double base = node.opcode == log2b ? 2. :
                              (node.opcode == log10b ? 10. : M_E);
                node.opcode = logbase;
                node.c.push_back(store.size());
                store.emplace_back(OpCode::val);
                store.back().val = base;
            }
            break;
        case sqrtb: case sqrb:
            {
                double expo = node.opcode == sqrtb ? 0.5 : 2.;
                node.opcode = power;
                node.c.push_back(store.size());
                store.emplace_back(OpCode::val);
                store.back().val = expo;
            }
            break;
    }
    return node_idx;
}
// Convert ast nodes -> link form
size_t ast_to_link_nodes(
        const Expr::AST& ast,
        std::vector<ASTLinkNode>& store) {
    const Expr::ASTNode* astptr = &ast[0];
    size_t nidx = _ast_to_link_nodes(&astptr, store);
    // Add utility nodes
    store.emplace_back(OpCode::val); store.back().val = -1.0;
    store.emplace_back(OpCode::val); store.back().val = 1.0;
    store.emplace_back(OpCode::val); store.back().val = 0.0;
    return nidx;
}

// Convert link form AST nodes -> usual AST nodes
void ast_from_link_nodes(const std::vector<ASTLinkNode>& nodes,
        Expr::AST& out, size_t index = 0) {
    const auto& node = nodes[index];
    if (index >= nodes.size()) {
        std::cerr << "ast_from_link_nodes ERROR: invalid AST. Node index " <<
            index << " out of bounds.\n";
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
        memcpy(out_node.call_info,
                node.call_info,
                sizeof(node.call_info));
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
        std::cout << OpCode::repr(nodes[i].opcode);
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
void optim_link_nodes_main(Environment& env,
        std::vector<ASTLinkNode>& nodes, size_t vi = 0) {
    auto& v = nodes[vi];
    if (OpCode::has_ref(v.opcode) && ~v.ref) v.nonconst_flag = true;
    size_t i = 0;
    for (; i < v.c.size(); ++i) {
        auto ui = v.c[i];
        if (ui == -1) break;
        nodes[ui].nonconst_flag = false;
        nodes[ui].null_flag = false;
        optim_link_nodes_main(env, nodes, ui);
        auto& u = nodes[ui];
        // Detect nonconst
        v.nonconst_flag |= u.nonconst_flag;
        // Detect nan
        if (u.opcode == OpCode::val &&
             v.opcode != OpCode::bnz // bnz can short-circuit
             ) {
            if (std::isnan(u.val)) v.null_flag = true;
        }
    }
    if (i == 0) {
        // nothing
    } else if (v.null_flag) {
        // Set null
        v.opcode = OpCode::null;
        v.c.clear();
    } else if (!v.nonconst_flag &&
            v.opcode != OpCode::thunk_ret &&
            v.opcode != OpCode::thunk_jmp) {
        // Evaluate constants
        std::vector<Expr::ASTNode> ast;
        ast_from_link_nodes(nodes, ast, vi);
        // Debug
        // print_link_nodes(nodes); std::cout << ast <<" T\n";
        v.val = detail::eval_ast(env, ast);
        v.opcode = OpCode::val;
        v.c.clear();
    } else {
        static auto is_positive = [](std::vector<ASTLinkNode>& nodes,
                    size_t idx) -> bool{
            return nodes[idx].opcode == OpCode::absb ||
                     nodes[idx].opcode == OpCode::sqrb ||
                     (nodes[idx].opcode == OpCode::power &&
                        nodes[nodes[idx].c[1]].opcode == OpCode::val &&
                        nodes[nodes[idx].c[1]].val >= 0 &&
                        std::fmod(nodes[nodes[idx].c[1]].val, 2.) == 0) ||
                     (nodes[idx].opcode == OpCode::power &&
                        nodes[nodes[idx].c[0]].opcode == OpCode::val);
        };
        // Apply rules
        using namespace OpCode;
        size_t li = vi, ri = vi;
        if (v.c.size() > 0){
            li = v.c[0];
            if (v.c.size() > 1)
                ri = v.c[1];
        }
        auto* l = &nodes[li], *r = &nodes[ri];
        switch(v.opcode) {
            case absb:
                // Involution
                if (is_positive(nodes, li)) v = *l;
                break;
            case floorb: case ceilb: case roundb:
                if (l->opcode == floorb ||
                    l->opcode == ceilb ||
                    l->opcode == roundb) {
                    v = *l;
                }
                break;
            case unaryminus:
                // Combine -
                if (l->opcode == val) {
                    v = *l;
                    v.val = -v.val;
                } else if (l->opcode == unaryminus) {
                    v = nodes[l->c[0]];
                    break;
                }
                if (l->opcode == mul) {
                    auto lli = l->c[0];
                    auto* ll = &nodes[lli];
                    if (ll->opcode == val) {
                        ll->val = -ll->val;
                        v = *l;
                    }
                    break;
                }
                break;
            case add:
                // Normalize
                if ((r->opcode == val && l->opcode != val) ||
                     l->opcode == add) {
                    if (v.opcode == add) {
                        std::swap(*l, *r);
                    }
                }
                if (r->opcode == add && l->opcode == val) {
                    auto rli = r->c[0];
                    auto* rl = &nodes[rli];
                    if (rl->opcode == val) {
                        l->val += rl->val;
                        *r = nodes[r->c[1]];
                    }
                }
                if (r->opcode == unaryminus &&
                      nodes[r->c[0]].opcode == add &&
                      l->opcode == val) {
                    auto rli = nodes[r->c[0]].c[0];
                    auto* rl = &nodes[rli];
                    if (rl->opcode == val) {
                        l->val -= rl->val;
                        nodes[r->c[0]] =
                            nodes[nodes[r->c[0]].c[1]];
                    }
                }

                if (v.opcode == add) {
                    // Try swapping, if also add
                    if (l->opcode == add) {
                        std::swap(nodes[l->c[0]], *r);
                        optim_link_nodes_main(env, nodes, li);
                        if (l->opcode == add) std::swap(nodes[l->c[0]], *r);
                    }
                    if (l->opcode == add) {
                        std::swap(nodes[l->c[1]], *r);
                        optim_link_nodes_main(env, nodes, li);
                        if (l->opcode == add) std::swap(nodes[l->c[0]], *r);
                    }
                    if (r->opcode == add) {
                        std::swap(*l, nodes[r->c[0]]);
                        optim_link_nodes_main(env, nodes, ri);
                        if (r->opcode == add) std::swap(*l, nodes[r->c[0]]);
                    }
                    if (r->opcode == add) {
                        std::swap(*l, nodes[r->c[1]]);
                        optim_link_nodes_main(env, nodes, ri);
                        if (r->opcode == add) std::swap(*l, nodes[r->c[0]]);
                    }
                }

                // Factoring
                {
                    bool factored = false;
                    for (int l_side = 0; l_side < 2; ++l_side) {
                        for (int r_side = 0; r_side < 2; ++r_side) {
                            ASTLinkNode* lterm;
                            size_t lcoeffi, rcoeffi, rtermi;
                            bool l_neg = false, r_neg = false;
                            lcoeffi = nodes.size() - 2;
                            if (l->opcode == unaryminus) {
                                lterm = &nodes[l->c[0]];
                                l_neg = true;
                            } else {
                                lterm = l;
                            }
                            if (lterm->opcode == mul) {
                                lcoeffi = lterm->c[l_side];
                                lterm = &nodes[lterm->c[1 - l_side]];
                            } else if (l_side == 1) continue;
                            rcoeffi = nodes.size() - 2;
                            if (r->opcode == unaryminus) {
                                rtermi = r->c[0];
                                r_neg = true;
                            } else {
                                rtermi = ri;
                            }
                            if (nodes[rtermi].opcode == mul) {
                                rcoeffi = nodes[rtermi].c[r_side];
                                rtermi = nodes[rtermi].c[1 - r_side];
                            } else if (r_side == 1) continue;

                            if (lterm->equals(nodes[rtermi], nodes)) {
                                if (l_neg) {
                                    size_t neg_i =
                                        (lcoeffi == (nodes.size() - 2)) ?
                                        l->c[0] :
                                        nodes[l->c[0]].c[1-r_side];
                                    nodes[neg_i].opcode = unaryminus;
                                    nodes[neg_i].c.resize(1);
                                    nodes[neg_i].c[0] = lcoeffi;
                                    lcoeffi = neg_i;
                                }
                                if (r_neg) {
                                    r->c[0] = rcoeffi;
                                    rcoeffi = ri;
                                }

                                l->opcode = add; v.opcode = mul;
                                l->c.resize(2); l->ref = -1;
                                l->c[0] = lcoeffi; l->c[1] = rcoeffi;
                                v.c[1] = rtermi;
                                v.nonconst_flag = false;
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
                if (v.opcode == add && l->opcode == power &&
                        r->opcode == power) {
                    auto lli = l->c[0], lri = l->c[1];
                    auto rli = r->c[0], rri = r->c[1];
                    auto& lr = nodes[lri], & rr = nodes[rri];
                    auto& ll = nodes[lli], & rl = nodes[rli];
                    if (lr.opcode == val && rr.opcode == val &&
                        lr.val == 2. && rr.val == 2.) {
                        if (((ll.opcode == sinb && rl.opcode == cosb) ||
                            (ll.opcode == cosb && rl.opcode == sinb)) &&
                            nodes[ll.c[0]].equals(nodes[rl.c[0]], nodes)) {
                            v.opcode = val; v.val = 1.;
                            break;
                        }
                    }
                }

                if (l->opcode == val && l->val == 0.) {
                    v = *r;
                } else if (r->opcode == val && r->val == 0.) v = *l;
                break;
            case mul:
                // Take out - sign
                if (l->opcode == unaryminus &&
                        r->opcode == unaryminus) {
                    *l = nodes[l->c[0]]; *r = nodes[r->c[0]]; // Cancel
                } else if (l->opcode == unaryminus)  {
                    auto tmp = l->c[0]; *l = v;
                    v.opcode = unaryminus;
                    v.c.resize(1);
                    l->c[0] = tmp;
                    v.nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    return;
                } else if (r->opcode == unaryminus)  {
                    auto tmp = r->c[0]; *r = v;
                    v.opcode = unaryminus;
                    v.c[0] = v.c[1]; v.c.resize(1);
                    r->c[1] = tmp;
                    v.nonconst_flag = false;
                    r->nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    return;
                }
                // Try swapping, if also mul
                if (l->opcode == mul) {
                    std::swap(nodes[l->c[0]], *r);
                    optim_link_nodes_main(env, nodes, li);
                }
                if (l->opcode == mul) {
                    std::swap(nodes[l->c[1]], *r);
                    optim_link_nodes_main(env, nodes, li);
                }
                if (r->opcode == mul) {
                    std::swap(*l, nodes[r->c[0]]);
                    optim_link_nodes_main(env, nodes, ri);
                }
                if (r->opcode == mul) {
                    std::swap(*l, nodes[r->c[1]]);
                    optim_link_nodes_main(env, nodes, ri);
                }
                // Normalize left/right
                if ((r->opcode == val && l->opcode != val) ||
                    (r->opcode != mul && l->opcode == mul)) {
                    std::swap(*l, *r);
                }
                if ((l->opcode == val && l->val == 0.) |
                    (r->opcode == val && r->val == 0.)) {
                    v.opcode = OpCode::val; v.val = 0.;
                }
                else if (l->opcode == val && l->val == 1.) v = *r;
                else if (r->opcode == val && r->val == 1.) v = *l;
                else if (l->opcode == val && l->val == -1.) {
                    v.opcode = unaryminus;
                    *l = *r;
                    v.c.pop_back();
                    break;
                } else if (r->opcode == val && r->val == -1.) {
                    v.opcode = unaryminus;
                    v.c.pop_back();
                } else {
                    // Combining powers
                    ASTLinkNode* lm, * rm;
                    size_t lf, rf, rmi;
                    if (l->opcode == power) {
                        lm = &nodes[l->c[0]];
                        lf = l->c[1];
                    } else if (r->opcode == unaryminus) {
                        rm = &nodes[r->c[0]];
                        rf = nodes.size() - 3;
                    } else {
                        lm = l;
                        lf = nodes.size() - 2;
                    }
                    if (r->opcode == power) {
                        rm = &nodes[r->c[0]];
                        rf = r->c[1];
                        rmi = r->c[0];
                    } else {
                        rm = r;
                        rf = nodes.size() - 2;
                        rmi = ri;
                    }
                    if (lm->equals(*rm, nodes)) {
                        v.opcode = OpCode::power; l->opcode = OpCode::add;
                        l->c.resize(2);
                        l->ref = -1;
                        l->c[0] = lf; l->c[1] = rf;
                        v.c[0] = rmi; v.c[1] = li;
                        v.nonconst_flag = false;
                        optim_link_nodes_main(env, nodes, vi);
                    }
                }
                break;
            case power:
                if (l->opcode == val) {
                    if (l->val == 1. || l->val == 0) {
                        v.opcode = OpCode::val;
                        v.val = l->val;
                        break;
                    }
                }
                if (r->opcode == val) {
                    if (r->val == 1.) {
                        v = *l; break;
                    } else if (r->val == 0.) {
                        v.opcode = OpCode::val;
                        v.val = 1.;
                        v.c.clear();
                        v.nonconst_flag = false;
                        break;
                    }
                }
                if (l->opcode == power) {
                    l->opcode = mul;
                    v.opcode = power;
                    v.c[0] = l->c[0];
                    l->c[0] = ri;
                    v.c[1] = li;
                    v.nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    break;
                }
                if (r->opcode == logbase &&
                    l->equals(nodes[r->c[1]], nodes) &&
                    is_positive(nodes, r->c[0])) {
                    // Inverse
                    v = nodes[r->c[0]];
                    v.nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    break;
                }
                break;
            case logbase:
                if (l->opcode == power &&
                    r->equals(nodes[l->c[0]], nodes)) {
                    // Inverse
                    v = nodes[l->c[1]];
                    v.nonconst_flag = false;
                    optim_link_nodes_main(env, nodes, vi);
                    break;
                } else if (l->opcode == power &&
                        is_positive(nodes, l->c[0])) {
                    // Take the exponent out
                    v.opcode = mul;
                    l->opcode = logbase;
                    v.c[0] = l->c[1];
                    v.c[1] = li;
                    l->c[1] = ri;
                    break;
                }
                break;
            case bnz:
                if (l->opcode == val) {
                    if (l->val) {
                        v = *r; // Left
                    } else {
                        v = nodes[v.c[2]]; // Right
                    }
                    if (v.opcode == OpCode::thunk_ret) {
                        v = nodes[v.c[0]];
                    }
                    break;
                } else {
                    auto* r2 = &nodes[v.c[2]];
                    if (r->opcode == OpCode::thunk_ret &&
                        r2->opcode == OpCode::thunk_ret) {
                        auto* rl = &nodes[r->c[0]];
                        auto* r2l = &nodes[r2->c[0]];
                        if (rl->equals(*r2l, nodes)) {
                            v = *rl;
                            break;
                        }
                    }
                }
                break;
            case bsel:
                // Super useless opcode
                v = *r;
                break;
            case max: case min:
                // If both sides are same,
                // keep only one of them
                if (l->equals(*r, nodes)) {
                    v = *l;
                }
                break;
            case eq: case ne: case le: case ge: case lt: case gt:
                if (l->equals(*r, nodes)) {
                    v.c.clear();
                    v.val = (double)(
                            v.opcode == eq ||
                            v.opcode == le ||
                            v.opcode == ge);
                    v.opcode = val;
                }
                break;
        }
    }
    for (size_t j = 0; j < v.c.size(); ++j) {
        nodes[v.c[j]].rehash(nodes);
    }
    v.rehash(nodes);
}
// Second pass optimization rules, replace x^-1 with 1/x,
// apply shorthand functions like exp(x), log2(x)
void optim_link_nodes_second_pass(Environment& env,
        std::vector<ASTLinkNode>& nodes, size_t vi = 0) {
    auto& v = nodes[vi];
    size_t i = 0;
    for (; i < v.c.size(); ++i) {
        auto ui = v.c[i];
        if (ui == -1) break;
        optim_link_nodes_second_pass(env, nodes, ui);
    }
    if (i == 0 || !v.nonconst_flag) {
        // nothing
    } else {
        // Apply rules
        using namespace OpCode;
        size_t li = vi, ri = vi;
        if (v.c.size() > 0){
            li = v.c[0];
            if (v.c.size() > 1)
                ri = v.c[1];
        }
        auto* l = &nodes[li], *r = &nodes[ri];
        switch(v.opcode) {
            case add:
                if (r->opcode == unaryminus) {
                    // Convert a+(-b) [form preferred by optimizer]
                    // back to a-b
                    v.c[1] = r->c[0];
                    v.opcode = sub;
                }
                break;
            case mul:
                if (r->opcode == divi) {
                    auto rli = r->c[0];
                    auto* rl = &nodes[rli];
                    if (rl->opcode == val && rl->val == 1.) {
                        v.opcode = divi;
                        *r = nodes[r->c[1]];
                    }
                }
                break;
            case power:
                if (r->opcode == val) {
                    if (r->val == 2.) {
                        v.opcode = OpCode::sqrb;
                        v.c.resize(1); break;
                    } else if (r->val == 0.5) {
                        v.opcode = OpCode::sqrtb;
                        v.c.resize(1); break;
                    } else if (r->val == -1) {
                        // Convert ()^-1 back to divide
                        v.opcode = OpCode::divi;
                        *r = *l;
                        v.c[0] = nodes.size() - 2;
                        break;
                    }
                }
                if (l->opcode == val) {
                    if (l->val == 2.) {
                        v.opcode = OpCode::exp2b;
                        v.c[0] = v.c[1]; v.c.resize(1); break;
                    } else if (l->val == M_E) {
                        v.opcode = OpCode::expb;
                        v.c[0] = v.c[1]; v.c.resize(1); break;
                    }
                }
                break;
            case logbase:
                if (r->opcode == val) {
                    if (r->val == 2.) {
                        v.opcode = OpCode::log2b;
                        v.c.resize(1); break;
                    } else if (r->val == M_E) {
                        v.opcode = OpCode::logb;
                        v.c.resize(1); break;
                    } else if (r->val == 10.) {
                        v.opcode = OpCode::log10b;
                        v.c.resize(1); break;
                    }
                }
                break;
        }
    }
}
}  // namespace

// Implementation of optimize in Expr class
void Expr::optimize() {
     // diff x prod(i:1,3)[2*x]
    std::vector<ASTLinkNode> nodes;
    ast_to_link_nodes(ast, nodes);

    Environment dummy_env;
    for (int i = 0; i < 5; ++i)
        optim_link_nodes_main(dummy_env, nodes);
    optim_link_nodes_second_pass(dummy_env, nodes);

    ast.clear();
    ast_from_link_nodes(nodes, ast);
}
}  // namespace nivalis
