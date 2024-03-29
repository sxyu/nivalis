#include "expr.hpp"

#include <cmath>
#include <unordered_set>
// #include <iostream>
#include "opcodes.hpp"
#include "util.hpp"
#include "env.hpp"
#include "expr.hpp"

namespace nivalis {
namespace {

// Uncomment to enable debug prints in newton's method
// #define NEWTON_DEBUG

#ifdef NEWTON_DEBUG
#define NEWTON_PRINT(x) std::cout << (x)
#else
#define NEWTON_PRINT(x)
#endif

#define DIFF_NEXT if (!diff(ast, diff_arg_id)) return false
#define PUSH(v) out.push_back(v)
#define CHAIN_RULE(derivop1, derivop2) { \
                               const Expr::ASTNode* tmp = *ast; \
                               out.push_back(mul); DIFF_NEXT; \
                               derivop1; \
                               copy_ast(tmp, out); \
                               derivop2; \
                            }

void skip_ast(const Expr::ASTNode** ast) {
    const auto* init_pos = ast;
    auto opc = (*ast)->opcode;
    size_t n_args = OpCode::n_args(opc);
    if (opc == OpCode::call) {
        n_args = (*ast)->call_info[1];
    }
    ++*ast;
    for (size_t i = 0; i < n_args; ++i) skip_ast(ast);
}

// Versions of has_var/sub_var that only applies to subtree
bool ast_has_var(const Expr::ASTNode** ast, uint64_t var_id) {
    const auto* init_pos = ast;
    auto opcode = (*ast)->opcode;
    auto n_args = OpCode::n_args(opcode);
    if (OpCode::has_ref(opcode) &&
        opcode != OpCode::arg &&
        (*ast)->ref == var_id) {
        return true;
    }

    ++*ast;
    for (size_t i = 0; i < n_args; ++i) {
        if (ast_has_var(ast, var_id)) return true;
    }
    return false;
}

void ast_sub_var(const Expr::ASTNode** ast, uint64_t var_id, double value, Expr::AST& out) {
    const auto* init_pos = ast;
    auto opcode = (*ast)->opcode;
    out.push_back(**ast);
    if (opcode == OpCode::ref &&
        (*ast)->ref == var_id) {
        out.back().opcode = OpCode::val;
        out.back().val = value;
    }

    ++*ast;
    for (size_t i = 0; i < OpCode::n_args(opcode); ++i) {
        ast_sub_var(ast, var_id, value, out);
    }
}

// Implementation
struct Differentiator {
    Differentiator(const Expr::AST& ast, uint64_t var_addr,
            Environment& env, std::vector<Expr::ASTNode>& out)
        : nodes(ast), ast_root(&ast[0]), var_addr(var_addr), env(env), out(out) {
        vis_asts.insert(ast_root);
    }

    const Expr::ASTNode* copy_ast(const Expr::ASTNode* ast,
            std::vector<Expr::ASTNode>& out) {
        const auto* init_pos = ast;
        skip_ast(&ast);
        for (const auto* n = init_pos; n != ast; ++n) {
            if (n->opcode == OpCode::arg) {
                // Substitute function argument in terms of the input
                copy_ast(&argv.back()[n->ref][0], out);
            } else {
                out.push_back(*n);
            }
        }
        return ast;
    }

    bool diff(const Expr::ASTNode** ast = nullptr, uint32_t diff_arg_id = -1) {
        if (ast == nullptr) ast = &ast_root;
        using namespace OpCode;
        uint32_t opcode = (*ast)->opcode;
        // std::cout << opcode << " " << diff_arg_id << " " << out.size() << " :: ";
        // for (auto& i : out) { std::cout << i.opcode << " "; }
        // std::cout << "\n";
        ++*ast;
        switch(opcode) {
            case null: PUSH(null); break;
            case val: PUSH(0.); break;
            case ref: PUSH(((*ast)-1)->ref == var_addr ? 1. : 0.); break;
            case arg: PUSH(((*ast)-1)->ref == diff_arg_id ? 1. : 0.); break;
            case call:
                      {
                          uint32_t fid = ((*ast)-1)->call_info[0];
                          size_t n_args = env.funcs[fid].n_args;

                          const auto& fexpr = env.funcs[fid].expr;
                          if (vis_asts.count(&fexpr.ast[0])) {
                              // Prevent recursion/cycles
                              return false;
                          }
                          {
                              std::vector<Expr::AST> call_args;
                              call_args.resize(n_args);
                              const Expr::ASTNode* tmp = *ast;
                              for (size_t i = 0; i < n_args; ++i) {
                                  tmp = copy_ast(tmp, call_args[i]);
                              }
                              argv.push_back(std::move(call_args));
                          }
                          {
                              if (n_args > 0) out.push_back(add);
                              const Expr::ASTNode* f_astptr = &fexpr.ast[0];
                              vis_asts.insert(&fexpr.ast[0]);
                              if (!diff(&f_astptr)) {
                                  return false;
                              }
                              vis_asts.erase(&fexpr.ast[0]);
                          }
                          for (size_t i = 0; i < n_args; ++i) {
                              if (i < n_args - 1) out.push_back(add);
                              out.push_back(mul);
                              DIFF_NEXT;
                              const Expr::ASTNode* f_astptr = &fexpr.ast[0];
                              vis_asts.insert(&fexpr.ast[0]);
                              if (!diff(&f_astptr, (uint32_t)i)) return false;
                              vis_asts.erase(&fexpr.ast[0]);
                          }
                          argv.pop_back();
                      }
                      break;
            case bnz:
                      {
                          PUSH(bnz);
                          *ast = copy_ast(*ast, out);
                          if ((*ast)->opcode != thunk_ret) return false; ++*ast;
                          begin_thunk(); DIFF_NEXT; end_thunk();
                          if ((*ast)->opcode != thunk_jmp) return false; ++*ast;
                          if ((*ast)->opcode != thunk_ret) return false; ++*ast;
                          begin_thunk(); DIFF_NEXT; end_thunk();
                          if ((*ast)->opcode != thunk_jmp) return false; ++*ast;
                      }
                      break;
            case sums:
                      {
                          uint64_t var_id = ((*ast)-1)->ref;
                          // Can't diff wrt index
                          if (var_id == var_addr) return false;
                          // Index must be constant for derivative
                          if ((*ast)->opcode != val) return false;
                          int64_t a = static_cast<int64_t>((*ast)->val); ++*ast;
                          // Index must be constant for derivative
                          if ((*ast)->opcode != val) return false;
                          int64_t b = static_cast<int64_t>((*ast)->val); ++*ast;
                          int64_t step = (a <= b) ? 1 : -1; b += step;
                          if ((*ast)->opcode != thunk_ret) return false; ++*ast;
                          Expr::AST diff_tmp;
                          for (int64_t i = a; i != b; i += step) {
                              const Expr::ASTNode* tmp = *ast;
                              if (i + step != b) PUSH(OpCode::add);
                              env.vars[var_id] = static_cast<double>(i);
                              diff_tmp.clear();
                              ast_sub_var(&tmp, var_id, static_cast<double>(i), diff_tmp);
                              tmp = &diff_tmp[0];
                              if (!diff(&tmp, diff_arg_id)) return false;
                          }
                          skip_ast(ast);
                          if ((*ast)->opcode != thunk_jmp) return false; ++*ast;
                      }
                      break;
            case prods:
                      {
                          uint64_t var_id = ((*ast)-1)->ref;
                          // Can't diff wrt index
                          if (var_id == var_addr) return false;
                          // Index must be constant for derivative
                          if ((*ast)->opcode != val) return false;
                          int64_t a = static_cast<int64_t>((*ast)->val); ++*ast;
                          // Index must be constant for derivative
                          if ((*ast)->opcode != val) return false;
                          int64_t b = static_cast<int64_t>((*ast)->val); ++*ast;
                          int64_t step = (a <= b) ? 1 : -1; b += step;
                          Expr::AST diff_tmp;
                          if ((*ast)->opcode != thunk_ret) return false; ++*ast;
                          for (int64_t i = a; i != b; i += step) {
                              if (i + step != b) PUSH(OpCode::add);
                              for (int64_t j = a; j != b; j += step) {
                                  if (j + step != b) PUSH(OpCode::mul);
                                  if (j == i) {
                                      env.vars[var_id] = static_cast<double>(i);
                                      diff_tmp.clear();
                                      const Expr::ASTNode* tmp = *ast;
                                      ast_sub_var(&tmp, var_id, static_cast<double>(j), diff_tmp);
                                      tmp = &diff_tmp[0];
                                      if (!diff(&tmp, diff_arg_id)) return false;
                                  } else {
                                      const Expr::ASTNode* tmp2 = *ast;
                                      ast_sub_var(&tmp2, var_id, static_cast<double>(j), out);
                                  }
                              }
                          }
                          skip_ast(ast);
                          if ((*ast)->opcode != thunk_jmp) return false; ++*ast;
                      }
                      break;

            case bsel: skip_ast(ast); DIFF_NEXT; break;
            case add: case sub: PUSH(opcode); DIFF_NEXT; DIFF_NEXT; break;
            case mul: {
                          // Product rule
                          PUSH(add);
                          const Expr::ASTNode* tmp1 = *ast;
                          PUSH(mul); DIFF_NEXT; // df *
                          copy_ast(*ast, out); // g
                          PUSH(mul);
                          DIFF_NEXT;
                          copy_ast(tmp1, out); // + dg * f
                      }
                break;
            case divi: {
                          // Quotient rule
                          PUSH(divi); PUSH(sub);
                          const Expr::ASTNode* tmp1 = *ast, *tmp1b = *ast;
                          PUSH(mul); DIFF_NEXT;  // df *
                          const Expr::ASTNode* tmp2 = *ast;
                          copy_ast(tmp2, out);  // g
                          PUSH(mul); DIFF_NEXT; copy_ast(tmp1, out); // - dg * f
                          tmp1 = tmp1b;
                          PUSH(sqrb); copy_ast(tmp2, out); // / g^2
                      }
                      break;
            case mod:
                      DIFF_NEXT;
                      // Cannot take derivative wrt modulus
                      if (ast_has_var(ast, var_addr)) return false;
                      break;

            case power:
                      {
                          const Expr::ASTNode* tmp1 = *ast;
                          skip_ast(&tmp1);
                          const Expr::ASTNode* expon_pos = tmp1;
                          bool expo_nonconst = ast_has_var(&tmp1, var_addr);
                          if (expo_nonconst) {
                              const Expr::ASTNode* base_pos = *ast;
                              Expr::AST elnb;
                              elnb.push_back(mul);
                              copy_ast(expon_pos, elnb);
                              elnb.push_back(logb);
                              copy_ast(base_pos, elnb);
                              skip_ast(ast); skip_ast(ast);
                              const Expr::ASTNode* elnbptr = &elnb[0];
                              PUSH(mul); PUSH(expb);
                              copy_ast(elnbptr, out);
                              if (!diff(&elnbptr, diff_arg_id)) return false;
                          } else {
                              CHAIN_RULE(PUSH(mul);
                                      copy_ast(*ast, out);
                                      PUSH(power), // g(x) goes here
                                      PUSH(sub); copy_ast(*ast, out); PUSH(1.));
                              skip_ast(ast);
                          }
                      }
                      break;
            case logbase:
                      {
                          const Expr::ASTNode* tmp = *ast;
                          skip_ast(&tmp);
                          bool base_nonconst = ast_has_var(&tmp, var_addr);
                          // Cannot take derivative wrt base
                          if (base_nonconst) return false;
                          CHAIN_RULE(PUSH(divi);
                                  PUSH(1.0);PUSH(mul);
                                  PUSH(logb);
                                  copy_ast(*ast, out),);
                          skip_ast(ast);
                      }
                      break;
            case max: case min:
                      {
                          PUSH(bnz); PUSH(opcode == max ? ge : le);
                          const Expr::ASTNode* tmp = *ast;
                          copy_ast(tmp, out);
                          skip_ast(&tmp); copy_ast(tmp, out);
                          begin_thunk(); DIFF_NEXT; end_thunk();
                          begin_thunk(); DIFF_NEXT; end_thunk();
                          break;
                      }
            case land: case lor: case lxor: case gcd: case lcm: case choose: case fafact: case rifact:
            case lt: case le: case eq: case ne: case ge: case gt:
                      PUSH(0.0); skip_ast(ast); skip_ast(ast); // Integer functions: 0 derivative
                      break;
            case betab:
                      {
                          const Expr::ASTNode* tmp = *ast, *x_pos = *ast;
                          skip_ast(&tmp);
                          const Expr::ASTNode* y_pos = tmp;
                          PUSH(add);
                          PUSH(mul);
                          PUSH(mul);
                          PUSH(betab);
                          copy_ast(x_pos,out);
                          copy_ast(y_pos,out);
                          PUSH(sub);
                          PUSH(digammab); copy_ast(x_pos,out);
                          PUSH(digammab); PUSH(add);
                          copy_ast(x_pos,out); copy_ast(y_pos,out);
                          DIFF_NEXT;
                          PUSH(mul);
                          PUSH(mul);
                          PUSH(betab);
                          copy_ast(x_pos,out);
                          copy_ast(y_pos,out);
                          PUSH(sub);
                          PUSH(digammab); copy_ast(y_pos,out);
                          PUSH(digammab); PUSH(add); copy_ast(x_pos,out); copy_ast(y_pos,out);
                          DIFF_NEXT;
                      }
                      break;
            case polygammab:
                      {
                          const Expr::ASTNode* tmp1 = *ast;
                          bool idx_nonconst = ast_has_var(&tmp1, var_addr);
                          if (idx_nonconst) return false; // Can't differentiate wrt polygamma index
                          tmp1 = *ast;
                          skip_ast(ast);
                          CHAIN_RULE(PUSH(polygammab);
                                  PUSH(add);
                                  copy_ast(tmp1, out);
                                  PUSH(1.),);
                      }
                      break;
            case unaryminus: PUSH(unaryminus); DIFF_NEXT; break;
            case absb: {
                           PUSH(mul); PUSH(sgn);
                           copy_ast(*ast, out);
                           DIFF_NEXT;
                       }
                             break;
            case sqrtb: CHAIN_RULE(
                                PUSH(mul);
                                PUSH(0.5);
                                PUSH(power), // g(x) goes here
                                PUSH(-0.5)); break;
            case sqrb: CHAIN_RULE(PUSH(mul); PUSH(2.),); break;
            case lnot: case sgn: case floorb: case ceilb: case roundb: case factb:
                       PUSH(0.0); skip_ast(ast); break; // Integer functions; 0 derivative
            case expb: CHAIN_RULE(PUSH(expb),); break;
            case exp2b: CHAIN_RULE( PUSH(mul); PUSH(log(2.)); PUSH(exp2b),); break;
            case logb:  CHAIN_RULE(PUSH(divi);PUSH(1.0),); break;
            case log2b:  CHAIN_RULE(PUSH(divi);PUSH(1.0);PUSH(mul);PUSH(log(2));,); break;
            case log10b: CHAIN_RULE(PUSH(divi);PUSH(1.0);PUSH(mul);PUSH(log(10));,); break;
            case sinb:  CHAIN_RULE(PUSH(cosb),); break;
            case cosb:  CHAIN_RULE(PUSH(unaryminus); PUSH(sinb),); break;
            case tanb:  CHAIN_RULE(PUSH(power); PUSH(cosb);
                                ,PUSH(-2.)); break;
            case asinb: case acosb:
                        CHAIN_RULE(if (opcode == acosb) PUSH(unaryminus);
                                PUSH(divi); PUSH(1.);
                                PUSH(sqrtb); PUSH(sub); PUSH(1.); PUSH(sqrb),); break;
            case atanb:  CHAIN_RULE(PUSH(divi); PUSH(1.); PUSH(add); PUSH(1.); PUSH(sqrb),); break;
            case sinhb: CHAIN_RULE(PUSH(coshb),); break;
            case coshb: CHAIN_RULE(PUSH(sinhb),); break;
            case tanhb: CHAIN_RULE(PUSH(sub); PUSH(1.); PUSH(sqrb); PUSH(tanhb),); break;
            case tgammab:
                        {
                            const Expr::ASTNode* tmp = *ast;
                            out.push_back(mul); DIFF_NEXT;
                            PUSH(mul); PUSH(tgammab); copy_ast(tmp, out);
                            PUSH(digammab); copy_ast(tmp, out);
                        }
                        break;
            case digammab: CHAIN_RULE(PUSH(trigammab),); break;
            case trigammab: CHAIN_RULE(PUSH(polygammab); PUSH(2.),); break;
            case lgammab: CHAIN_RULE(PUSH(digammab),); break;
            case erfb:   CHAIN_RULE(
                                 PUSH(mul); PUSH(2.0 / sqrt(M_PI));
                                 PUSH(expb); PUSH(unaryminus); PUSH(sqrb),);
                         break;
            case sigmoidb: 
                         {
                            PUSH(mul);
                            PUSH(mul);
                            PUSH(sigmoidb);
                            copy_ast(*ast, out);
                            PUSH(sub);
                            PUSH(1.);
                            PUSH(sigmoidb);
                            copy_ast(*ast, out);
                            DIFF_NEXT;
                         }
                        ; 
                        break;
            case softplusb: 
                         {
                            PUSH(mul);
                            PUSH(sigmoidb);
                            copy_ast(*ast, out);
                            DIFF_NEXT;
                         }
                         break;

            case gausspdfb: 
                         {
                            PUSH(mul);
                            PUSH(mul);
                            PUSH(unaryminus);
                            copy_ast(*ast, out);
                            PUSH(gausspdfb);
                            copy_ast(*ast, out);
                            DIFF_NEXT;
                         }
                         break;

            case zetab:  return false; // Derivative not available
        }
        return true;
    }
private:
    const Expr::AST& nodes;
    const Expr::ASTNode* ast_root;
    size_t var_addr;
    std::vector<std::vector<Expr::AST> > argv;
    Environment env;
    std::vector<Expr::ASTNode>& out;
    std::unordered_set<const Expr::ASTNode*> vis_asts;

    // Thunk management helpers
    void begin_thunk() {
        thunks.push_back(out.size());
        out.emplace_back(OpCode::thunk_ret);
    }
    void end_thunk() {
        out.emplace_back(OpCode::thunk_jmp,
                    out.size() - thunks.back());
        thunks.pop_back();
    }

    // Beginnings of thunks
    std::vector<size_t> thunks;
};

}  // namespace


std::vector<Expr::ASTNode> diff_ast(const Expr::AST& ast,
        uint64_t var_addr, Environment& env) {
    std::vector<Expr::ASTNode> dast;
    Differentiator diff(ast, var_addr, env, dast);
    if (!diff.diff()) {
        dast.resize(1);
        dast[0] = OpCode::null;
    }
    return dast;
}

// Interface for differentiating expr
Expr Expr::diff(uint64_t var_addr, Environment& env) const {
    Expr dexpr;
    dexpr.ast.clear();

    Differentiator diff(ast, var_addr, env, dexpr.ast);
    if (!diff.diff()) {
        dexpr.ast.resize(1);
        dexpr.ast[0].opcode = OpCode::null;
    } else {
        dexpr.optimize();
    }
    return dexpr;
}

// Newton's method implementation
double Expr::newton(uint64_t var_addr, double x0, Environment& env,
        double eps_step, double eps_abs, int max_iter,
        double xmin, double xmax, const Expr* deriv,
        double fx0, double dfx0) const {
    if (deriv == nullptr) {
        Expr deriv_expr = diff(var_addr, env);
        return newton(var_addr, x0, env, eps_step,
                eps_abs, max_iter, xmin, xmax, &deriv_expr, fx0, dfx0);
    }
    NEWTON_PRINT("\n init@"); NEWTON_PRINT(x0);
    for (int i = 0; i < max_iter; ++i) {
        if (i || dfx0 == std::numeric_limits<double>::max()) {
            env.vars[var_addr] = x0;
            if (i || fx0 == std::numeric_limits<double>::max()) {
                fx0 = (*this)(env);
                if(std::isnan(fx0)) {
                    NEWTON_PRINT(" FAIL f=NAN");
                    return std::numeric_limits<double>::quiet_NaN(); // Fail
                }
            }
            dfx0 = (*deriv)(env);
            if(std::isnan(dfx0) || dfx0 == 0.) {
                NEWTON_PRINT(" FAIL d=NAN");
                return std::numeric_limits<double>::quiet_NaN(); // Fail
            }
        }
        double delta = fx0 / dfx0;
        x0 -= delta;
        NEWTON_PRINT(" "); NEWTON_PRINT(x0);
        if (std::fabs(delta) < eps_step && std::fabs(fx0) < eps_abs) {
            // Found root
            return x0;
        }
        if (x0 < xmin || x0 > xmax) {
            NEWTON_PRINT(" FAIL bounds");
            return std::numeric_limits<double>::quiet_NaN(); // Fail
        }
    }
    NEWTON_PRINT(" FAIL iter");
    return std::numeric_limits<double>::quiet_NaN(); // Fail
}
}  // namespace nivalis
