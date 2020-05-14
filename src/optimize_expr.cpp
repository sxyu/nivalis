#include "expr.hpp"

#include <string_view>
#include <iostream>
#include <cstring>
#include <cmath>
#include <boost/math/constants/constants.hpp>
#include "util.hpp"
#include "opcodes.hpp"

namespace nivalis {
namespace {
using ::nivalis::OpCode::repr;

// ** AST node form processing + optimization **
// AST Link Node representation
// instead of list-form AST, we explicitly put pointers to children
// to allow easy modification/deletion during optimization
struct ASTLinkNode {
    explicit ASTLinkNode(uint32_t opcode) : opcode(opcode) {
        ref = -1;
        nonconst_flag = false;
        null_flag = false;
    }
    uint32_t opcode, ref;
    std::vector<size_t> c;
    double val;
    bool nonconst_flag, null_flag;
};


// Convert ast nodes -> link form
uint32_t ast_to_link_nodes(
        Expr::ASTNode** ast,
        std::vector<ASTLinkNode>& store) {
    uint32_t node_idx = static_cast<uint32_t>(store.size());
    uint32_t opcode = (*ast)->opcode;
    store.emplace_back(opcode);
    auto n_args = OpCode::n_args(opcode);
    if (opcode == OpCode::val) store[node_idx].val = (*ast)->val;
    else if (OpCode::has_ref(opcode)) store[node_idx].ref = (*ast)->ref;
    ++*ast;
    store[node_idx].c.resize(n_args);
    for (size_t i = 0; i < n_args; ++i) {
        uint32_t ret = ast_to_link_nodes(ast, store);
        store[node_idx].c[i] = ret;
    }
    auto& node = store[node_idx];
    if (node.opcode == OpCode::sub) {
        // Convert - to +- for convenience later
        node.opcode = OpCode::add;
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
    return node_idx;
}

// Convert link form AST nodes -> usual AST nodes
void ast_from_link_nodes(const std::vector<ASTLinkNode>& nodes,
        Expr::AST& out, uint32_t index = 0) {
    const auto& node = nodes[index];
    if (index >= nodes.size()) {
        std::cerr << "ast_from_link_nodes ERROR: invalid AST. Node index " <<
            index << " out of bounds.\n";
        return;
    }
    size_t out_idx = out.size();
    out.emplace_back(node.opcode);
    if (node.opcode == OpCode::val) {
        out.back().val = node.val;
    } else if (OpCode::has_ref(node.opcode)) {
        out.back().ref = node.ref;
    }
    for (size_t i = 0; i < std::min(node.c.size(),
                OpCode::n_args(node.opcode)); ++i) {
        ast_from_link_nodes(nodes, out, node.c[i]);
        if (node.opcode == OpCode::thunk_ret &&
            nodes[node.c[i]].opcode == OpCode::thunk_jmp) {
            out.back().ref = out_idx;
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
        for (int j = 0; j < nodes[i].c.size(); ++j) {
            std::cout << nodes[i].c[j] << " ";
        }
        std::cout << "\n";
    }
}

// Main optimization rules
void optim_link_nodes(Environment& env,
        std::vector<ASTLinkNode>& nodes, uint32_t vi = 0) {
    auto& v = nodes[vi];
    if (~v.ref) v.nonconst_flag = true;
    size_t i = 0;
    for (; i < v.c.size(); ++i) {
        auto ui = v.c[i];
        if (ui == -1) break;
        optim_link_nodes(env, nodes, ui);
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
    if (i == 0) return;
    if (v.null_flag) {
        // Set null
        v.opcode = OpCode::null;
        v.c.clear();
    } else if (!v.nonconst_flag &&
            v.opcode != OpCode::thunk_ret &&
            v.opcode != OpCode::thunk_jmp) {
        // Evaluate constants
        std::vector<Expr::ASTNode> ast;
        ast_from_link_nodes(nodes, ast, vi);
        // print_link_nodes(nodes);
        // Expr tmp; tmp.ast = ast;
        // std::cout << tmp <<" T\n";
        v.val = detail::eval_ast(env, ast);
        v.opcode = OpCode::val;
        v.c.clear();
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
            case absb:
                // Involution
                if (l->opcode == absb ||
                        l->opcode == sqrb) v = *l;
                break;
            case floorb: case ceilb: case roundb:
                if (l->opcode == floorb ||
                    l->opcode == ceilb ||
                    l->opcode == roundb) {
                    v = *l;
                }
                break;
            case unaryminus:
                if (l->opcode == val) {
                    v = *l;
                    v.val = -v.val;
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
            case add: case sub:
                while (r->opcode == unaryminus) {
                    ri = v.c[1] = r->c[0];
                    r = &nodes[ri];
                    v.opcode ^= 1;
                }
                // Normalize
                if ((r->opcode == val && l->opcode != val) ||
                     l->opcode == add || l->opcode == sub) {
                    if (v.opcode == add) {
                        std::swap(v.c[0], v.c[1]);
                        std::swap(l, r); std::swap(li, ri);
                    }
                }
                if ((r->opcode == add || r->opcode == sub) &&
                        l->opcode == val) {
                    auto rli = r->c[0];
                    auto* rl = &nodes[rli];
                    if (rl->opcode == val) {
                        l->val += rl->val * (v.opcode == sub ? -1 : 1);
                        if (r->opcode == sub) v.opcode ^= 1;
                        *r = nodes[r->c[1]];
                    }
                }
                // Crazy factoring rules
                if (r->opcode == ref && l->opcode == ref) {
                    if (r->ref == l->ref) {
                        if (v.opcode == sub) {
                            v.opcode = val; v.val = 0.;
                            v.nonconst_flag = false;
                            v.c.clear();
                        } else {
                            v.opcode = mul;
                            l->opcode = val; l->val = 2.;
                        }
                        break;
                    }
                }
                if (r->opcode == mul && l->opcode == ref) {
                    auto rli = r->c[0], rri = r->c[1];
                    auto* rl = &nodes[rli], * rr = &nodes[rri];
                    if (rr->opcode == ref && rl->opcode == val &&
                            rr->ref == l->ref) {
                        l->opcode = val;
                        l->val = 1. + rl->val * (v.opcode == sub ? -1 : 1);
                        if (l->val == 0.) {
                            v.opcode = val; v.val = 0.; v.nonconst_flag = false;
                            v.c.clear();
                        } else {
                            v.opcode = mul; *r = *rr;
                        }
                        break;
                    }
                }
                if (r->opcode == ref && l->opcode == mul) {
                    auto lli = l->c[0], lri = l->c[1];
                    auto* ll = &nodes[lli], * lr = &nodes[lri];
                    if (lr->opcode == ref && ll->opcode == val &&
                            lr->ref == r->ref) {
                        r->opcode = val;
                        r->val = (v.opcode == sub ? -1 : 1) + ll->val;
                        if (r->val == 0.) {
                            v.opcode = val; v.val = 0.; v.nonconst_flag = false;
                            v.c.clear();
                        } else {
                            v.opcode = mul; *l = *lr;
                            std::swap(*l, *r);
                        }
                        break;
                    }
                }
                if (r->opcode == mul && l->opcode == mul) {
                    auto lli = l->c[0], lri = l->c[1];
                    auto rli = r->c[0], rri = r->c[1];
                    auto* ll = &nodes[lli], * lr = &nodes[lri];
                    auto* rl = &nodes[rli], * rr = &nodes[rri];
                    if (lr->opcode == ref && ll->opcode == val &&
                        rr->opcode == ref && rl->opcode == val) {
                        if (lr->ref == rr->ref) {
                            l->opcode = val;
                            l->val = ll->val +
                                (v.opcode == sub ? -1 : 1) * rl->val;
                            if (l->val == 0.) {
                                v.opcode = val; v.val = 0.; v.nonconst_flag = false;
                                v.c.clear();
                            } else {
                                v.opcode = mul; *r = *rr;
                            }
                            break;
                        } else if (lr->ref > rr->ref) {
                            if (v.opcode == sub) {
                                ll->val = -ll->val;
                                rl->val = -rl->val;
                            }
                            std::swap(l, r);
                        }
                    }
                }

                if (l->opcode == val && l->val == 0.) {
                    if (v.opcode == sub) {
                        v.opcode = unaryminus; v.c[0] = v.c[1]; v.c.pop_back();
                    } else v = *r;
                } else if (r->opcode == val && r->val == 0.) v = *l;
                break;
            case mul:
                // Normalize
                if ((r->opcode == val && l->opcode != val) ||
                    (r->opcode != mul && l->opcode == mul)) {
                    std::swap(v.c[0], v.c[1]);
                    std::swap(l, r); std::swap(li, ri);
                }
                if (r->opcode == mul) {
                    auto rli = r->c[0];
                    auto* rl = &nodes[rli];
                    if (rl->opcode == val) {
                        if (l->opcode == val) {
                            l->val *= rl->val;
                            *r = nodes[r->c[1]];
                        } else {
                            std::swap(*l, *rl);
                            optim_link_nodes(env, nodes, ri);
                        }
                    }
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
                } else if (l->opcode == expb && r->opcode == expb) {
                    v.opcode = OpCode::expb; l->opcode = OpCode::add;
                    l->c[1] = r->c[0]; v.c.pop_back();
                    v.nonconst_flag = false;
                    optim_link_nodes(env, nodes, vi);
                    break;
                } else if (l->opcode == exp2b && r->opcode == exp2b) {
                    v.opcode = OpCode::exp2b; l->opcode = OpCode::add;
                    l->c[1] = r->c[0]; v.c.pop_back();
                    v.nonconst_flag = false;
                    optim_link_nodes(env, nodes, vi);
                    break;
                } else if (l->opcode == power && r->opcode == power
                           && nodes[l->c[0]].opcode == val && nodes[r->c[0]].opcode == val &&
                           nodes[l->c[0]].val == nodes[r->c[0]].val) {
                    v.opcode = OpCode::power; l->opcode = OpCode::add;
                    l->c[0] = l->c[1]; l->c[1] = r->c[1];
                    v.c[0] = r->c[0]; v.c[1] = li;
                    v.nonconst_flag = false;
                    optim_link_nodes(env, nodes, vi);
                    break;
                }
                break;
            case divi:
                if (l->opcode == val && l->val == 0.) {
                    v.opcode = OpCode::val; v.val = 0.;
                } else if (r->opcode == val) {
                    if (r->val == 0.) v.opcode = OpCode::null;
                    else {
                        v.opcode = OpCode::mul;
                        r->val = 1./r->val;
                        std::swap(v.c[0], v.c[1]);
                        std::swap(l,r); std::swap(li,ri);
                        if (r->opcode == mul && l->opcode == val) {
                            auto rli = r->c[0];
                            auto* rl = &nodes[rli];
                            if (rl->opcode == val) {
                                l->val *= rl->val;
                                *r = nodes[r->c[1]];
                            }
                        }
                        break;
                    }
                } else if (r->opcode == val && r->val == 1.) {
                    v = *l;
                } else if (r->opcode == val && r->val == -1.) {
                    v.opcode = unaryminus;
                    v.c.pop_back();
                } else if (l->opcode == expb && r->opcode == expb) {
                    v.opcode = OpCode::expb; l->opcode = OpCode::sub;
                    l->c[1] = r->c[0]; v.c.pop_back();
                    v.nonconst_flag = false;
                    optim_link_nodes(env, nodes, vi);
                    break;
                } else if (l->opcode == exp2b && r->opcode == exp2b) {
                    v.opcode = OpCode::exp2b; l->opcode = OpCode::sub;
                    l->c[1] = r->c[0]; v.c.pop_back();
                    v.nonconst_flag = false;
                    optim_link_nodes(env, nodes, vi);
                    break;
                } else if (l->opcode == power && r->opcode == power
                           && nodes[l->c[0]].opcode == val && nodes[r->c[0]].opcode == val &&
                           nodes[l->c[0]].val == nodes[r->c[0]].val) {
                    v.opcode = OpCode::power; l->opcode = OpCode::sub;
                    l->c[0] = l->c[1]; l->c[1] = r->c[1];
                    v.c[0] = r->c[0]; v.c[1] = li;
                    v.nonconst_flag = false;
                    optim_link_nodes(env, nodes, vi);
                    break;
                }
                if (l->opcode == mul) {
                    auto lli = l->c[0], lri = l->c[1];
                    auto* ll = &nodes[lli], * lr = &nodes[lri];
                    if (ll->opcode == val) {
                        // Pivot
                        std::swap(*ll, *lr);
                        std::swap(*lr, *r);
                        l->opcode = divi; v.opcode = mul;
                        optim_link_nodes(env, nodes, li);
                        if (r->opcode == val) {
                            std::swap(*l, *r);
                        }
                        break;
                    }
                }
                break;
            case power:
                if (l->opcode == val) {
                    if (l->val == 1. || l->val == 0) {
                        v.opcode = OpCode::val;
                        v.val = l->val;
                    } else if (l->val == boost::math::double_constants::e) {
                        v.opcode = OpCode::expb;
                        v.c[0] = v.c[1]; v.c.pop_back();
                        break;
                    } else if (l->val == 2.) {
                        v.opcode = OpCode::exp2b;
                        v.c[0] = v.c[1]; v.c.pop_back();
                        break;
                    }
                }
                if (v.opcode == power &&
                    r->opcode == val) {
                    if (r->val == 1.) {
                        v = *l; break;
                    } else if (r->val == 0.) {
                        v.opcode = OpCode::val;
                        v.val = 1.;
                        v.c.clear();
                        v.nonconst_flag = false;
                        break;
                    } else if (r->val == 0.5) {
                        v.opcode = OpCode::sqrtb;
                        v.c.pop_back(); break;
                    } else if (r->val == 2) {
                        v.opcode = OpCode::sqrb;
                        v.c.pop_back(); break;
                    }
                }
                break;
            case logbase:
                if (r->opcode == val) {
                    if (r->val == 2) {
                        v.opcode = OpCode::log2b;
                        v.c.pop_back(); break;
                    } else if (r->val == 10.) {
                        v.opcode = OpCode::log10b;
                        v.c.pop_back(); break;
                    } else if (r->val == boost::math::double_constants::e) {
                        v.opcode = OpCode::logb;
                        v.c.pop_back(); break;
                    }
                }
                break;
        }
    }
}
}  // namespace

// Implementation of optimize in Expr class
void Expr::optimize() {
    std::vector<ASTLinkNode> nodes;
    ASTNode* astptr = &ast[0];
    ast_to_link_nodes(&astptr, nodes);

    Environment dummy_env;
    optim_link_nodes(dummy_env, nodes);

    ast.clear();
    ast_from_link_nodes(nodes, ast);
}
}  // namespace nivalis
