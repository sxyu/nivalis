#include "diff_expr.hpp"

#include <cmath>
#include <iostream>
#include "opcodes.hpp"
#include "util.hpp"
#include "eval_expr.hpp"

namespace nivalis {
namespace detail {

namespace {

#define EV_NEXT eval_ast(env, ast)
#define DIFF_NEXT diff_ast_recursive(ast, env, var_addr, out)
#define PUSH_OP(op) out.push_back(op)
#define PUSH_CONST(dbl) util::push_dbl(out, dbl)
#define CHAIN_RULE(derivop1, derivop2) { \
                               const uint32_t* tmp = *ast; \
                               out.push_back(mul); DIFF_NEXT; \
                               derivop1; \
                               copy_to_derivative(tmp, out); \
                               derivop2; \
                            }
#define let_ab_i64 int64_t a = static_cast<int64_t>(EV_NEXT), \
                           b = static_cast<int64_t>(EV_NEXT)
#define let_ab_u32 unsigned a = static_cast<unsigned>(std::max(EV_NEXT,0.)), \
                            b = static_cast<unsigned>(std::max(EV_NEXT,0.))

const uint32_t* copy_to_derivative(const uint32_t* ast, std::vector<uint32_t>& out) {
    const auto* init_pos = ast;
    skip_ast(&ast);
    std::copy(init_pos, ast, std::back_inserter(out));
    return ast;
}

// Implementation
bool diff_ast_recursive(const uint32_t** ast, Environment& env, uint32_t var_addr, std::vector<uint32_t>& out) {
    using namespace OpCode;
    uint32_t opcode = **ast;
    ++*ast;
    switch(opcode) {
        case null: return false;
        case val: PUSH_CONST(0.); *ast += 2; break;
        case ref: PUSH_CONST(**ast == var_addr ? 1. : 0.); ++*ast; break;
        case bnz:
            {
                PUSH_OP(bnz);
                *ast = copy_to_derivative(*ast, out);
                DIFF_NEXT; DIFF_NEXT;
            }
            break;
        case sums:
            {
                uint32_t var_id = **ast; ++*ast;
                if (var_id == var_addr) return false; // Can't diff wrt index
                if (**ast != val) return false; // Index must be constant for derivative
                ++*ast;
                int64_t a = static_cast<int64_t>(util::as_double(*ast)); *ast += 2;
                if (**ast != val) {
                    return false; // Index must be constant for derivative
                }
                ++*ast;
                int64_t b = static_cast<int64_t>(util::as_double(*ast)); *ast += 2;
                int64_t step = (a <= b) ? 1 : -1; b += step;
                for (int64_t i = a; i != b; i += step) {
                    const uint32_t* tmp = *ast;
                    if (i + step != b) PUSH_OP(OpCode::add);
                    env.vars[var_id] = static_cast<double>(i);
                    diff_ast_recursive(&tmp, env, var_addr, out);
                }
                skip_ast(ast);
            }
            break;
        case prods:
            {
                uint32_t var_id = **ast; ++*ast;
                if (var_id == var_addr) return false; // Can't diff wrt index
                if (**ast != val) return false; // Index must be constant for derivative
                ++*ast;
                int64_t a = static_cast<int64_t>(util::as_double(*ast)); *ast += 2;
                if (**ast != val) return false; // Index must be constant for derivative
                ++*ast;
                int64_t b = static_cast<int64_t>(util::as_double(*ast));
                *ast += 2;
                int64_t step = (a <= b) ? 1 : -1; b += step;
                std::vector<uint32_t> diff_tmp;
                for (int64_t i = a; i != b; i += step) {
                    if (i + step != b) PUSH_OP(OpCode::add);
                    for (int64_t j = a; j != b; j += step) {
                        if (j + step != b) PUSH_OP(OpCode::mul);
                        if (j == i) {
                            env.vars[var_id] = static_cast<double>(i);
                            diff_tmp.clear();
                            const uint32_t* tmp = *ast;
                            eval_ast_sub_var(&tmp, var_id, static_cast<double>(i), diff_tmp);
                            tmp = &diff_tmp[0];
                            diff_ast_recursive(&tmp, env, var_addr, out);
                        } else {
                            const uint32_t* tmp2 = *ast;
                            eval_ast_sub_var(&tmp2, var_id, static_cast<double>(j), out);
                        }
                    }
                }
                skip_ast(ast);
            }
            break;

        case bsel: skip_ast(ast); DIFF_NEXT; break;
        case add: case sub: PUSH_OP(opcode); DIFF_NEXT; DIFF_NEXT; break;
        case mul: {
                      // Product rule
                      PUSH_OP(add);
                      const uint32_t* tmp1 = *ast;
                      PUSH_OP(mul); DIFF_NEXT; // df *
                      copy_to_derivative(*ast, out); // g
                      PUSH_OP(mul); DIFF_NEXT; copy_to_derivative(tmp1, out); // + dg * f
                  }
                  break;
        case div: {
                      // Quotient rule
                      PUSH_OP(div); PUSH_OP(sub);
                      const uint32_t* tmp1 = *ast, *tmp1b = *ast;
                      PUSH_OP(mul); DIFF_NEXT;  // df *
                      const uint32_t* tmp2 = *ast;
                      copy_to_derivative(tmp2, out);  // g
                      PUSH_OP(mul); DIFF_NEXT; copy_to_derivative(tmp1, out); // - dg * f
                      tmp1 = tmp1b;
                      PUSH_OP(sqrb); copy_to_derivative(tmp2, out); // / g^2
                  }
                  break;
        case mod:
                  DIFF_NEXT;
                  // Cannot take derivative wrt modulus
                  if (eval_ast_find_var(ast, var_addr)) return false;
                  break;

        case power: 
                  {
                      const uint32_t* tmp1 = *ast;
                      skip_ast(&tmp1);
                      const uint32_t* expon_pos = tmp1;
                      bool expo_nonconst = eval_ast_find_var(&tmp1, var_addr);
                      if (expo_nonconst) {
                           const uint32_t* base_pos = *ast;
                           std::vector<uint32_t> elnb;
                           elnb.push_back(mul); copy_to_derivative(expon_pos, elnb); 
                           elnb.push_back(logb); copy_to_derivative(base_pos, elnb); 
                           skip_ast(ast); skip_ast(ast);
                           const uint32_t* elnbptr = &elnb[0];
                           PUSH_OP(mul); PUSH_OP(expb); copy_to_derivative(elnbptr, out); 
                           diff_ast_recursive(&elnbptr, env, var_addr, out);
                      } else {
                          CHAIN_RULE(PUSH_OP(mul);
                                  copy_to_derivative(*ast, out);
                                  PUSH_OP(power), // g(x) goes here
                                  PUSH_OP(sub); copy_to_derivative(*ast, out); PUSH_CONST(1));
                          skip_ast(ast);
                      }
                  }
                  break;
        case logbase: 
                  {
                      const uint32_t* tmp = *ast;
                      skip_ast(&tmp);
                      bool base_nonconst = eval_ast_find_var(&tmp, var_addr);
                      // Cannot take derivative wrt base
                      if (base_nonconst) return false;
                      CHAIN_RULE(PUSH_OP(div);
                              PUSH_CONST(1.0);PUSH_OP(mul);
                              PUSH_OP(logb);
                              copy_to_derivative(*ast, out),);
                      skip_ast(ast);
                  }
                  break;
        case max: case min:
        {
            PUSH_OP(bnz); PUSH_OP(opcode == max ? ge : le); 
            const uint32_t* tmp = *ast;
            copy_to_derivative(tmp, out); skip_ast(&tmp);
            copy_to_derivative(tmp, out);
            DIFF_NEXT; DIFF_NEXT;
            break;
        }
        case land: case lor: case lxor: case gcd: case lcm: case choose: case fafact: case rifact:
        case lt: case le: case eq: case ne: case ge: case gt:
                  PUSH_CONST(0.0); skip_ast(ast); skip_ast(ast); // Integer functions: 0 derivative
        case betab:
                  {
                      const uint32_t* tmp = *ast, *x_pos = *ast;
                      skip_ast(&tmp);
                      const uint32_t* y_pos = tmp;
                      PUSH_OP(add);
                          PUSH_OP(mul);
                              PUSH_OP(mul);
                                  PUSH_OP(betab);
                                      copy_to_derivative(x_pos,out); copy_to_derivative(y_pos,out);
                                  PUSH_OP(sub);
                                      PUSH_OP(digammab); copy_to_derivative(x_pos,out);
                                      PUSH_OP(digammab); PUSH_OP(add); copy_to_derivative(x_pos,out); copy_to_derivative(y_pos,out);
                          DIFF_NEXT;
                          PUSH_OP(mul);
                              PUSH_OP(mul);
                                  PUSH_OP(betab);
                                      copy_to_derivative(x_pos,out); copy_to_derivative(y_pos,out);
                                  PUSH_OP(sub);
                                      PUSH_OP(digammab); copy_to_derivative(y_pos,out);
                                      PUSH_OP(digammab); PUSH_OP(add); copy_to_derivative(x_pos,out); copy_to_derivative(y_pos,out);
                          DIFF_NEXT;
                  }
                  break;
        case polygammab:
                  {
                      const uint32_t* tmp1 = *ast;
                      bool idx_nonconst = eval_ast_find_var(&tmp1, var_addr);
                      if (idx_nonconst) return false; // Can't differentiate wrt polygamma index
                      tmp1 = *ast;
                      skip_ast(ast);
                      CHAIN_RULE(PUSH_OP(polygammab); 
                              PUSH_OP(add);
                              copy_to_derivative(tmp1, out);
                              PUSH_CONST(1),);
                  }
                break;
        case unaryminus: PUSH_OP(unaryminus); DIFF_NEXT; break;
        case absb: {
                       const uint32_t* tmp = *ast;
                       double value = eval_ast(env, &tmp);
                       if (value >= 0.) { DIFF_NEXT; }
                       else { PUSH_OP(unaryminus); DIFF_NEXT; }
                   }
        case sqrtb: CHAIN_RULE(
                            PUSH_OP(mul);
                            PUSH_CONST(0.5);
                            PUSH_OP(power), // g(x) goes here
                            PUSH_CONST(-0.5)); break;
        case sqrb: CHAIN_RULE(PUSH_OP(mul); PUSH_CONST(2.),); break;
        case lnot: case sgn: case floorb: case ceilb: case roundb: case factb:
                   PUSH_CONST(0.0); skip_ast(ast); break; // Integer functions; 0 derivative
        case expb: CHAIN_RULE(PUSH_OP(expb),); break;
        case exp2b: CHAIN_RULE( PUSH_OP(mul); PUSH_CONST(log(2)); PUSH_OP(exp2b),); break;
        case logb:  CHAIN_RULE(PUSH_OP(div);PUSH_CONST(1.0),); break;
        case log2b:  CHAIN_RULE(PUSH_OP(div);PUSH_CONST(1.0);PUSH_OP(mul);PUSH_CONST(log(2));,); break;
        case log10b: CHAIN_RULE(PUSH_OP(div);PUSH_CONST(1.0);PUSH_OP(mul);PUSH_CONST(log(10));,); break;
        case sinb:  CHAIN_RULE(PUSH_OP(cosb),); break;
        case cosb:  CHAIN_RULE(PUSH_OP(unaryminus); PUSH_OP(sinb),); break;
        case tanb:  CHAIN_RULE(PUSH_OP(div); PUSH_CONST(1); PUSH_OP(sqrb); PUSH_OP(cosb),); break;
        case asinb: case acosb: 
            CHAIN_RULE(if (opcode == acosb) PUSH_OP(unaryminus);
                    PUSH_OP(div); PUSH_CONST(1);
                     PUSH_OP(sqrtb); PUSH_OP(sub); PUSH_CONST(1); PUSH_OP(sqrb),); break;
        case atanb:  CHAIN_RULE(PUSH_OP(div); PUSH_CONST(1); PUSH_OP(add); PUSH_CONST(1); PUSH_OP(sqrb),); break;
        case sinhb: CHAIN_RULE(PUSH_OP(coshb),); break;
        case coshb: CHAIN_RULE(PUSH_OP(sinhb),); break;
        case tanhb: CHAIN_RULE(PUSH_OP(sub); PUSH_CONST(1); PUSH_OP(sqrb); PUSH_OP(tanhb),);
        case tgammab:  CHAIN_RULE(
                              PUSH_OP(mul); PUSH_OP(tgammab);
                              copy_to_derivative(tmp, out);
                              PUSH_OP(digammab),);
                      break;
        case digammab: CHAIN_RULE(PUSH_OP(trigammab),); break;
        case trigammab: CHAIN_RULE(PUSH_OP(polygammab); PUSH_CONST(2),); break;
        case lgammab: CHAIN_RULE(PUSH_OP(digammab),); break;
        case erfb:   CHAIN_RULE(
                              PUSH_OP(mul); PUSH_CONST(2.0 / sqrt(M_PI));
                              PUSH_OP(expb); PUSH_OP(unaryminus); PUSH_OP(sqrb),);
                     break;

        case zetab:  return false; // Derivative not available
        default: return false;
    }
    return true;
}

}  // namespace


std::vector<uint32_t> diff_ast(const std::vector<uint32_t>& ast, uint32_t var_addr, Environment& env) {
    std::vector<uint32_t> dast;
    const uint32_t* astptr = &ast[0];
    if (!diff_ast_recursive(&astptr, env, var_addr, dast)) {
        dast.resize(1);
        dast[0] = OpCode::null;
    }
    return dast;
}

}  // namespace detail
}  // namespace nivalis
