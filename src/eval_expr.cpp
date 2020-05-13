#include "version.hpp"
#include "eval_expr.hpp"

#include <string_view>
#include <iostream>
#include <cmath>
#include <numeric>
#ifdef ENABLE_NIVALIS_BOOST_MATH
#include <boost/math/special_functions/beta.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/digamma.hpp>
#include <boost/math/special_functions/trigamma.hpp>
#include <boost/math/special_functions/polygamma.hpp>
#include <boost/math/special_functions/zeta.hpp>
#include <boost/math/special_functions/binomial.hpp>
#include <boost/math/special_functions/factorials.hpp>
#endif
#include "opcodes.hpp"
#include "util.hpp"

#define EV_NEXT eval_ast(env, ast)
#define EV_UNARY(func) ret = func(eval_ast(env, ast)); break;
#define let_ab_i64 int64_t a = static_cast<int64_t>(EV_NEXT), \
                           b = static_cast<int64_t>(EV_NEXT)
#define let_ab_u32 unsigned a = static_cast<unsigned>(std::max(EV_NEXT,0.)), \
                            b = static_cast<unsigned>(std::max(EV_NEXT,0.))

namespace nivalis {
namespace detail {

namespace {
const double NONE = std::numeric_limits<double>::quiet_NaN();
using ::nivalis::OpCode::subexpr_repr;
using ::nivalis::OpCode::repr;
#ifndef ENABLE_NIVALIS_BOOST_MATH
void print_boost_warning(uint32_t opcode) {
    std::cerr << "Function " << OpCode::repr(opcode)
        << " requires Nivalis to be compiled with Boost" << std::endl;
}

// Falling factorial
double fa_fact (size_t x, size_t to_exc = 1) {
    if (x >= to_exc + 175) // too expensive
        return std::numeric_limits<double>::quiet_NaN();
    double result = 1.;
    while (x > to_exc) result *= x--;
    return result;
}
#endif
}  // namespace

void skip_ast(const uint32_t** ast) {
    std::string_view repr = subexpr_repr(**ast); ++*ast;
    for (char c : repr) {
        switch(c) {
            case '@': skip_ast(ast); break; // subexpr
            case '#': *ast += 2; break; // value
            case '&': ++*ast; break; // ref
        }
    }
}

bool eval_ast_find_var(const uint32_t** ast, uint32_t var_id) {
    std::string_view repr = subexpr_repr(**ast); ++*ast;
    for (char c : repr) {
        switch(c) {
            case '@': if (eval_ast_find_var(ast, var_id)) { return true; }
                      break; // subexpr
            case '#': util::as_double(*ast); *ast += 2; break; // value
            case '&': if (**ast == var_id) return true; ++*ast; break; // ref
        }
    }
    return false;
}

void eval_ast_sub_var(const uint32_t** ast, uint32_t var_id, double value, std::vector<uint32_t>& out) {
    std::string_view repr = subexpr_repr(**ast);
    out.push_back(**ast);
    ++*ast;
    for (char c : repr) {
        switch(c) {
            case '@': eval_ast_sub_var(ast, var_id, value, out); break; // subexpr
            case '#':
                      out.push_back(**ast); ++ *ast;
                      out.push_back(**ast); ++ *ast;
                      break; // value
            case '&':
                      if (**ast == var_id) {
                          out.pop_back();
                          util::push_dbl(out, value);
                      } else {
                          out.push_back(**ast);
                      }
                      ++*ast;
                      break; // ref
        }
    }
}

double eval_ast(Environment& env, const uint32_t** ast) {
#ifdef ENABLE_NIVALIS_BOOST_MATH
    using namespace boost;
    using namespace boost::math;
#endif
    using namespace nivalis::OpCode;
    uint32_t opcode = **ast;
    ++*ast;
    double ret;
    switch(opcode) {
        case null: ret = std::numeric_limits<double>::quiet_NaN(); break;
        case val: ret = util::as_double(*ast); *ast += 2; break;
        case ref: ret = env.vars[**ast]; ++*ast; break;
        case bnz:
            {
                double pred = EV_NEXT;
                if (pred != 0.0) {
                    ret = EV_NEXT; skip_ast(ast);
                } else {
                    skip_ast(ast); ret = EV_NEXT;
                }
            }
            break;
        case sums:
            {
                uint32_t var_id = **ast; ++*ast;
                let_ab_i64;
                int64_t step = (a <= b) ? 1 : -1; b += step;
                ret = 0.0;
                for (int64_t i = a; i != b; i += step) {
                    const uint32_t* tmp = *ast;
                    env.vars[var_id] = static_cast<double>(i);
                    ret += eval_ast(env, &tmp);
                }
                skip_ast(ast);
            }
            break;
        case prods:
            {
                uint32_t var_id = **ast; ++*ast;
                let_ab_i64;
                int64_t step = (a <= b) ? 1 : -1; b += step;
                ret = 1.0;
                for (int64_t i = a; i != b; i += step) {
                    const uint32_t* tmp = *ast;
                    env.vars[var_id] = static_cast<double>(i);
                    ret *= eval_ast(env, &tmp);
                }
                skip_ast(ast);
            }
            break;

        case bsel: EV_NEXT; ret = EV_NEXT; break;
        case add: ret = EV_NEXT + EV_NEXT; break;
        case sub: ret = EV_NEXT; ret -= EV_NEXT; break;
        case mul: ret = EV_NEXT * EV_NEXT; break;
        case div: ret = EV_NEXT; ret /= EV_NEXT; break;
        case mod: ret = EV_NEXT; ret = std::fmod(ret, EV_NEXT); break;
        case power: ret = EV_NEXT; ret = pow(ret, EV_NEXT); break;
        case logbase: ret = EV_NEXT; ret = log(ret) / log(EV_NEXT); break;

        case max: ret = std::max(EV_NEXT, EV_NEXT); break;
        case min: ret = std::min(EV_NEXT, EV_NEXT); break;
        case land: ret = static_cast<double>(EV_NEXT && EV_NEXT); break;
        case lor: ret = static_cast<double>(EV_NEXT || EV_NEXT); break;
        case lxor: ret = static_cast<bool>(EV_NEXT) ^ static_cast<bool>(EV_NEXT); break;
        case gcd: { let_ab_i64; ret = static_cast<double>(std::gcd(a, b)); } break;
        case lcm: { let_ab_i64; ret = a * b / static_cast<double>(std::gcd(a, b)); } break;
#ifdef ENABLE_NIVALIS_BOOST_MATH
        case choose: { let_ab_u32; ret = binomial_coefficient<double>(a, b); } break;
        case fafact: { let_ab_u32; ret = falling_factorial<double>(a, b); } break;
        case rifact: { let_ab_u32; ret = rising_factorial<double>(a, b); } break;
        case betab: ret = EV_NEXT; ret = beta<double>(ret, EV_NEXT); break;
        case polygammab: ret = EV_NEXT; ret = polygamma<double>(static_cast<int>(ret), EV_NEXT);
#else
        case choose: {
                         double ad = std::round(EV_NEXT);
                         double bd = std::round(EV_NEXT);
                         if (ad < 0 || bd < 0) {
                             ret = std::numeric_limits<double>::quiet_NaN(); break;
                         }
                         size_t a = static_cast<size_t>(ad), b = static_cast<size_t>(bd);
                         b = std::min(b, a-b);
                         ret = fa_fact(a, a-b) / fa_fact(b);
                     }
                     break;
        case fafact: {
                         double ad = std::round(EV_NEXT);
                         double bd = std::round(EV_NEXT);
                         if (ad < 0 || bd < 0) {
                             ret = std::numeric_limits<double>::quiet_NaN(); break;
                         }
                         size_t a = static_cast<size_t>(ad), b = static_cast<size_t>(bd);
                         ret = fa_fact(a, a-b);
                     }
                   break;
        case rifact: {
                         double ad = std::round(EV_NEXT);
                         double bd = std::round(EV_NEXT);
                         if (ad < 0 || bd < 0) {
                             ret = std::numeric_limits<double>::quiet_NaN(); break;
                         }
                         size_t a = static_cast<size_t>(ad), b = static_cast<size_t>(bd);
                         ret = fa_fact(a+b-1, a-1);
                     }
                     break;
        case betab: case polygammab: skip_ast(ast); skip_ast(ast); ret = NONE; print_boost_warning(opcode);
#endif
                break;
        case lt: ret = EV_NEXT; ret = static_cast<double>(ret < EV_NEXT); break;
        case le: ret = EV_NEXT; ret = static_cast<double>(ret <= EV_NEXT); break;
        case eq: ret = static_cast<double>(EV_NEXT == EV_NEXT); break;
        case ne: ret = static_cast<double>(EV_NEXT != EV_NEXT); break;
        case ge: ret = EV_NEXT; ret = static_cast<double>(ret >= EV_NEXT); break;
        case gt: ret = EV_NEXT; ret = static_cast<double>(ret > EV_NEXT); break;

        case unaryminus: ret = -EV_NEXT; break;
        case lnot: ret = static_cast<double>(!EV_NEXT); break;
        case absb: EV_UNARY(fabs);
        case sqrtb: EV_UNARY(sqrt);
        case sqrb: ret = eval_ast(env, ast); ret *= ret; break;
        case sgn: ret = EV_NEXT;
                   ret = ret > 0 ? 1 : (ret == 0 ? 0 : -1); break;
        case floorb: EV_UNARY(floor); case ceilb: EV_UNARY(ceil);
        case roundb: EV_UNARY(round);

        case expb:  EV_UNARY(exp);   case exp2b:  EV_UNARY(exp2);
        case logb:  EV_UNARY(log);
        case factb:
                    {
                        unsigned n = static_cast<unsigned>(std::max(eval_ast(env, ast), 0.));
#ifdef ENABLE_NIVALIS_BOOST_MATH
                        ret = factorial<double>(n);
#else
                        ret = fa_fact(n, 1);
#endif
                    }
                    break;
        case log2b: EV_UNARY(log2);  case log10b: EV_UNARY(log10);
        case sinb:  EV_UNARY(sin);   case cosb:   EV_UNARY(cos);
        case tanb:  EV_UNARY(tan);   case asinb:  EV_UNARY(asin);
        case acosb: EV_UNARY(acos);  case atanb:  EV_UNARY(atan);
        case sinhb: EV_UNARY(sinh);  case coshb:  EV_UNARY(cosh);
        case tanhb: EV_UNARY(tanh);
#ifdef ENABLE_NIVALIS_BOOST_MATH
        case tgammab:  EV_UNARY(tgamma<double>);
        case digammab:  EV_UNARY(digamma<double>);
        case trigammab:  EV_UNARY(trigamma<double>);
        case lgammab:  EV_UNARY(lgamma<double>);
        case zetab:  EV_UNARY(zeta<double>);
#else
        case tgammab:  EV_UNARY(std::tgamma);
        case lgammab:  EV_UNARY(std::lgamma);
        // The following functions are unavailable without Boost
        case digammab: case trigammab: case zetab:
           skip_ast(ast); ret = NONE; print_boost_warning(opcode); break;
#endif
        case erfb:  EV_UNARY(erf);
        default: ret = NONE; break;
    }
    return ret;
}

bool to_padded_ast(const uint32_t** ast, std::vector<uint32_t>& out) {
    uint32_t opcode = **ast;
    std::string_view repr = subexpr_repr(opcode); ++*ast;
    if (opcode == OpCode::sums || opcode == OpCode::prods ||
            opcode == OpCode::bnz) return false;
    out.push_back(0); out.push_back(0);
    out.push_back(opcode);
    for (char c : repr) {
        switch(c) {
            case '@': if(!to_padded_ast(ast, out)) return false;
                          break; // subexpr
            case '#':
                      out[out.size() - 3] = **ast; ++*ast;
                      out[out.size() - 2] = **ast; ++*ast;
                      break; // value
            case '&':
                      out[out.size() - 2] = **ast; ++*ast; break; // ref
        }
    }
    return true;
}

double eval_padded_ast(Environment& env, const std::vector<uint32_t>& ast) {
    thread_local std::vector<double> stk;
    stk.resize(ast.size());
    size_t top = -1;
    using namespace boost;
    using namespace boost::math;
    using namespace nivalis::OpCode;

#define ARG3 stk[top-2]
#define ARG2 stk[top-1]
#define ARG1 stk[top]
#define COMBINE --top; break
#define EV_UNARYP(func) ARG1 = func(ARG1); break

    for (size_t cidx = ast.size() - 1; ~cidx; cidx -= 3) {
        auto opcode = ast[cidx];
        switch(opcode) {
            case null: stk[++top] = NONE; break;
            case val:
                stk[++top] = util::as_double(&ast[cidx-2]);  break;
            case ref:
                stk[++top] = env.vars[ast[cidx-1]]; break;
            case bnz:
                if (ARG1 != 0.) ARG3 = ARG2; top -= 2; break;

            case bsel: COMBINE;
            case add: ARG2 += ARG1; COMBINE;
            case sub: ARG2 = ARG1 - ARG2; COMBINE;
            case mul: ARG2 *= ARG1;  COMBINE;
            case div: ARG2 = ARG1 / ARG2; COMBINE;
            case mod: ARG2 = std::fmod(ARG1, ARG2); COMBINE;
            case power: ARG2 = std::pow(ARG1, ARG2); COMBINE;
            case logbase: ARG2 = log(ARG1) / log(ARG2); COMBINE;
            case max: ARG2 = std::max(ARG1, ARG2); COMBINE;
            case min: ARG2 = std::min(ARG1, ARG2); COMBINE;
            case land: ARG2 = static_cast<double>(ARG1 && ARG2); COMBINE;
            case lor: ARG2 = static_cast<double>(ARG1 || ARG2); COMBINE;
            case lxor: ARG2 = static_cast<double>(
                               (ARG1 != 0.) ^ (ARG2 != 0.)); COMBINE;
            case gcd: ARG2 = static_cast<double>(
                              std::gcd((int64_t) ARG1,
                                       (int64_t) ARG2)); COMBINE;
            case lcm: ARG2 = ARG1 * ARG2 /
                      static_cast<double>(std::gcd(
                          (int64_t) ARG1, (int64_t) ARG2)); COMBINE;
#ifdef ENABLE_NIVALIS_BOOST_MATH
            case choose: ARG2 = binomial_coefficient<double>(
                            (uint32_t)ARG1, (uint32_t)ARG2); COMBINE;
            case fafact: ARG2 = falling_factorial<double>(
                            (uint32_t)ARG1, (uint32_t)ARG2); COMBINE;
            case rifact: ARG2 = rising_factorial<double>(
                            (uint32_t)ARG1, (uint32_t)ARG2); COMBINE;
            case betab: ARG2 = beta<double>(ARG1, ARG2); COMBINE;
            case polygammab: ARG2 = beta<double>((int)ARG1, ARG2); COMBINE;
#else
            case choose: {
                             double ad = std::round(ARG1);
                             double bd = std::round(ARG2);
                             if (ad < 0 || bd < 0) {
                                 ret = std::numeric_limits<double>::quiet_NaN(); break;
                             }
                             size_t a = static_cast<size_t>(ad), b = static_cast<size_t>(bd);
                             b = std::min(b, a-b);
                             ARG2 = fa_fact(a, a-b) / fa_fact(b);
                             COMBINE;
                         }
            case fafact: {
                             double ad = std::round(ARG1);
                             double bd = std::round(ARG2);
                             if (ad < 0 || bd < 0) {
                                 ret = std::numeric_limits<double>::quiet_NaN(); break;
                             }
                             size_t a = static_cast<size_t>(ad), b = static_cast<size_t>(bd);
                             ARG2 = fa_fact(a, a-b);
                             COMBINE;
                         }
            case rifact: {
                             double ad = std::round(ARG1);
                             double bd = std::round(ARG2);
                             if (ad < 0 || bd < 0) {
                                 ret = std::numeric_limits<double>::quiet_NaN(); break;
                             }
                             size_t a = static_cast<size_t>(ad), b = static_cast<size_t>(bd);
                             ARG2 = fa_fact(a+b-1, a-1);
                             COMBINE;
                         }
            case betab: case polygammab:
                ARG1 = NONE; print_boost_warning(opcode); break;
#endif
            case lt: ARG2 = static_cast<double>(ARG1 < ARG2); COMBINE;
            case le: ARG2 = static_cast<double>(ARG1 <= ARG2); COMBINE;
            case eq: ARG2 = static_cast<double>(ARG1 == ARG2); COMBINE;
            case ne: ARG2 = static_cast<double>(ARG1 != ARG2); COMBINE;
            case ge: ARG2 = static_cast<double>(ARG1 >= ARG2); COMBINE;
            case gt: ARG2 = static_cast<double>(ARG1 > ARG2); COMBINE;

            case unaryminus: ARG1 = -ARG1; break;
            case lnot: ARG1 = static_cast<double>(!(ARG1)); break;
            case absb: EV_UNARYP(std::fabs);
            case sqrtb: EV_UNARYP(std::sqrt);
            case sqrb: ARG1 *= ARG1; break;
            case sgn: ARG1 = ARG1 > 0 ? 1 : (ARG1 == 0 ? 0 : -1); break;
            case floorb: EV_UNARYP(floor);
            case ceilb:  EV_UNARYP(ceil); case roundb: EV_UNARYP(round);

            case expb:   EV_UNARYP(exp); case exp2b:  EV_UNARYP(exp2);
            case logb:   EV_UNARYP(log);
            case factb:
                        {
                            unsigned n = static_cast<unsigned>(std::max(
                                        ARG1, 0.));
#ifdef ENABLE_NIVALIS_BOOST_MATH
                            ARG1 = factorial<double>(n);
#else
                            ARG1 = fa_fact(n, 1);
#endif
                        }
                        break;
            case log2b: EV_UNARYP(log2);  case log10b: EV_UNARYP(log10);
            case sinb:  EV_UNARYP(sin);   case cosb:   EV_UNARYP(cos);
            case tanb:  EV_UNARYP(tan);   case asinb:  EV_UNARYP(asin);
            case acosb: EV_UNARYP(acos);  case atanb:  EV_UNARYP(atan);
            case sinhb: EV_UNARYP(sinh);  case coshb:  EV_UNARYP(cosh);
            case tanhb: EV_UNARYP(tanh);
            case tgammab:  EV_UNARYP(std::tgamma);
            case lgammab:  EV_UNARYP(std::lgamma);
#ifdef ENABLE_NIVALIS_BOOST_MATH
            case digammab:  EV_UNARYP(digamma<double>);
            case trigammab:  EV_UNARYP(trigamma<double>);
            case zetab:  EV_UNARYP(zeta<double>);
#else
           // The following functions are unavailable without Boost
            case digammab: case trigammab: case zetab:
                ARG1 = NONE; print_boost_warning(opcode); break;
#endif
            case erfb:  EV_UNARYP(erf); break;
            default:  break;
        }
    }
    return stk[0];
}

void print_ast(std::ostream& os, const uint32_t** ast, const Environment* env) {
    std::string_view reprs = repr(**ast); ++*ast;
    for (char c : reprs) {
        switch(c) {
            case '@': print_ast(os, ast, env); break; // subexpr
            case '#': os << util::as_double(*ast); *ast += 2; break; // value
            case '&':
                      if (env != nullptr) os << env->varname.at(**ast);
                      else os << "&" << **ast;
                      ++*ast; break; // ref
            default:
                      os << c;
        }
    }
}

}  // namespace detail
}  // namespace nivalis
