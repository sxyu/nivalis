#include "ast_nodes.hpp"

#include <string_view>
#include <cstring>
#include <cmath>
#include <boost/math/constants/constants.hpp>
#include "util.hpp"
#include "opcodes.hpp"
#include "eval_expr.hpp"

namespace nivalis {
namespace detail {

namespace {
using ::nivalis::OpCode::subexpr_repr;
using ::nivalis::OpCode::repr;
}  // namespace

ASTNode::ASTNode(uint32_t opcode) : opcode(opcode) {
    memset(c, -1, sizeof c);
    ref = -1;
    nonconst_flag = false;
    null_flag = false;
}

uint32_t ast_to_nodes(const uint32_t** ast, std::vector<ASTNode>& store) {
    uint32_t node_idx = store.size();
    store.emplace_back(**ast);
    std::string_view repr = subexpr_repr(store.back().opcode);
    ++*ast;
    uint32_t cct = 0;
    for (char c : repr) {
        switch(c) {
            case '@': store[node_idx].c[cct++] = ast_to_nodes(ast, store);
                      break; // subexpr
            case '#': store[node_idx].val = util::as_double(*ast); *ast += 2; break; // value
            case '&': store[node_idx].ref = **ast; ++*ast; break; // ref
        }
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
            store.back().c[0] = ri;
        }
    }
    return node_idx;
}

void ast_from_nodes(const std::vector<ASTNode>& nodes, std::vector<uint32_t>& out, uint32_t index) {
    const auto& node = nodes[index];
    std::string_view repr = subexpr_repr(node.opcode);
    out.push_back(node.opcode);
    uint32_t cct = 0;
    for (char c : repr) {
        switch(c) {
            case '@': ast_from_nodes(nodes, out, node.c[cct++]); break; // subexpr
            case '#': out.pop_back(); util::push_dbl(out, node.val); break; // value
            case '&': out.push_back(node.ref); break; // ref
        }
    }
}

double eval_ast_nodes(Environment& env, const std::vector<ASTNode>& nodes, uint32_t idx) {
    std::vector<uint32_t> ast;
    ast_from_nodes(nodes, ast, idx);
    const uint32_t* astptr = &ast[0];
    return eval_ast(env, &astptr);
}

void optim_nodes(Environment& env, std::vector<ASTNode>& nodes, uint32_t vi) {
    auto& v = nodes[vi];
    if (~v.ref) v.nonconst_flag = true;
    size_t i = 0;
    for (; i < 3; ++i) {
        auto ui = v.c[i];
        if (ui == -1) break;
        optim_nodes(env, nodes, ui);
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
        v.c[0] = v.c[1] = -1;
    } else if (!v.nonconst_flag) {
        // Evaluate constants
        v.val = eval_ast_nodes(env, nodes, vi);
        v.opcode = OpCode::val;
        v.c[0] = v.c[1] = -1;
    } else {
        // Apply rules
        using namespace OpCode;
        auto li = v.c[0], ri = v.c[1];
        if (li == -1) li = vi;
        if (ri == -1) ri = vi;
        auto* l = &nodes[li], *r = &nodes[ri];
        switch(v.opcode) {
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
                    }
                    v = *l;
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
                        v.opcode = unaryminus; v.c[0] = v.c[1]; v.c[1] = -1;
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
                            optim_nodes(env, nodes, ri);
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
                    v.c[1] = -1;
                    break;
                } else if (r->opcode == val && r->val == -1.) {
                    v.opcode = unaryminus;
                    v.c[1] = -1;
                } else if (l->opcode == expb && r->opcode == expb) {
                    v.opcode = OpCode::expb; l->opcode = OpCode::add;
                    l->c[1] = r->c[0]; v.c[1] = -1;
                    v.nonconst_flag = false;
                    optim_nodes(env, nodes, vi);
                    break;
                } else if (l->opcode == exp2b && r->opcode == exp2b) {
                    v.opcode = OpCode::exp2b; l->opcode = OpCode::add;
                    l->c[1] = r->c[0]; v.c[1] = -1;
                    v.nonconst_flag = false;
                    optim_nodes(env, nodes, vi);
                    break;
                } else if (l->opcode == power && r->opcode == power
                           && nodes[l->c[0]].opcode == val && nodes[r->c[0]].opcode == val &&
                           nodes[l->c[0]].val == nodes[r->c[0]].val) {
                    v.opcode = OpCode::power; l->opcode = OpCode::add;
                    l->c[0] = l->c[1]; l->c[1] = r->c[1];
                    v.c[0] = r->c[0]; v.c[1] = li;
                    v.nonconst_flag = false;
                    optim_nodes(env, nodes, vi);
                    break;
                }
                break;
            case div:
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
                    v.c[1] = -1;
                } else if (l->opcode == expb && r->opcode == expb) {
                    v.opcode = OpCode::expb; l->opcode = OpCode::sub;
                    l->c[1] = r->c[0]; v.c[1] = -1;
                    v.nonconst_flag = false;
                    optim_nodes(env, nodes, vi);
                    break;
                } else if (l->opcode == exp2b && r->opcode == exp2b) {
                    v.opcode = OpCode::exp2b; l->opcode = OpCode::sub;
                    l->c[1] = r->c[0]; v.c[1] = -1;
                    v.nonconst_flag = false;
                    optim_nodes(env, nodes, vi);
                    break;
                } else if (l->opcode == power && r->opcode == power
                           && nodes[l->c[0]].opcode == val && nodes[r->c[0]].opcode == val &&
                           nodes[l->c[0]].val == nodes[r->c[0]].val) {
                    v.opcode = OpCode::power; l->opcode = OpCode::sub;
                    l->c[0] = l->c[1]; l->c[1] = r->c[1];
                    v.c[0] = r->c[0]; v.c[1] = li;
                    v.nonconst_flag = false;
                    optim_nodes(env, nodes, vi);
                    break;
                }
                if (l->opcode == mul) {
                    auto lli = l->c[0], lri = l->c[1];
                    auto* ll = &nodes[lli], * lr = &nodes[lri];
                    if (ll->opcode == val) {
                        // Pivot
                        std::swap(*ll, *lr);
                        std::swap(*lr, *r);
                        l->opcode = div; v.opcode = mul;
                        optim_nodes(env, nodes, li);
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
                        v.c[0] = v.c[1]; v.c[1] = -1;
                        break;
                    } else if (l->val == 2.) {
                        v.opcode = OpCode::exp2b;
                        v.c[0] = v.c[1]; v.c[1] = -1;
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
                        v.nonconst_flag = false;
                        break;
                    } else if (r->val == 0.5) {
                        v.opcode = OpCode::sqrtb;
                        v.c[1] = -1; break;
                    } else if (r->val == 2) {
                        v.opcode = OpCode::sqrb;
                        v.c[1] = -1; break;
                    }
                }
                break;
            case logbase:
                if (r->opcode == val) {
                    if (r->val == 2) {
                        v.opcode = OpCode::log2b;
                        v.c[1] = -1; break;
                    } else if (r->val == 10.) {
                        v.opcode = OpCode::log10b;
                        v.c[1] = -1; break;
                    } else if (r->val == boost::math::double_constants::e) {
                        v.opcode = OpCode::logb;
                        v.c[1] = -1; break;
                    }
                }
                break;
        }
    }
}
}  // namespace detail
}  // namespace nivalis
