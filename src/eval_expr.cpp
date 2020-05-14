#include "expr.hpp"

#include "version.hpp"
#include "env.hpp"

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

namespace nivalis {

namespace {
const double NONE = std::numeric_limits<double>::quiet_NaN();
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

namespace detail {
double eval_ast(Environment& env, const Expr::AST& ast) {
    thread_local std::vector<double> stk;
    thread_local std::vector<size_t> thunks;
    thread_local std::vector<size_t> thunks_stk;
    stk.resize(stk.size() + ast.size());
    size_t top = -1;
    using namespace boost;
    using namespace boost::math;
    using namespace nivalis::OpCode;

    bool _is_thunk_ret = false;

    // Shorthands for first, 2nd, 3rd arguments to operator
#define ARG3 stk[top-2]
#define ARG2 stk[top-1]
#define ARG1 stk[top]
    // Thunk system helpers
    // For each command that calls thunks, we need to use the
    // following block ON_THUNK_RET(do on ret, do else)
#define ON_THUNK_RET(retdo, elsedo) do{if (_is_thunk_ret) {\
    --top; \
    _is_thunk_ret = false; \
    retdo \
} else { \
    elsedo \
}}while(0)
    // Return value from thunk, usable in retdo part of ON_THUNK_RET
#define THUNK_RET_VAL stk[top+1]
    // "Call" a thunk immediately after the current operator
#define CALL_THUNK_AT(id) do{thunks_stk.push_back(cidx); \
                        cidx = thunks[thunks.size() - (id) - 1]; }while(0)

    for (size_t cidx = ast.size() - 1; ~cidx; --cidx) {
        const auto& node = ast[cidx];
        switch(node.opcode) {
            case null: stk[++top] = NONE; break;
            case thunk_jmp:
                {
                   thunks.push_back(cidx);
                   cidx = ast[cidx].ref;
                }
                break;
            case thunk_ret:
                cidx = thunks_stk.back() + 1;
                thunks_stk.pop_back();
                _is_thunk_ret = true;
                break;
            case val:
                stk[++top] = node.val;  break;
            case ref:
                stk[++top] = env.vars[node.ref]; break;
            case bnz:
                ON_THUNK_RET(
                    ARG1 = THUNK_RET_VAL;
                ,
                    CALL_THUNK_AT(ARG1 == 0.);
                    thunks.resize(thunks.size() - 2);
                );
                break;

            case sums: case prods:
                {
                    ON_THUNK_RET(
                        // update arg3 (the output)
                        if (node.opcode == prods)
                            ARG3 *= THUNK_RET_VAL;
                        else
                            ARG3 += THUNK_RET_VAL; // arg3 is output
                    ,
                        // Move over the arguments and use arg3
                        // as output
                        ++top; ARG1 = ARG2; ARG2 = ARG3;
                        ARG3 = node.opcode == prods ? 1. : 0.;
                    );
                    uint64_t var_id = node.ref;
                    int64_t a = static_cast<int64_t>(ARG1),
                            b = static_cast<int64_t>(ARG2);
                    int64_t step = (a <= b) ? 1 : -1;
                    if (std::isnan(ARG1)) {
                        top -= 2; thunks.pop_back();
                    } else {
                        env.vars[var_id] = static_cast<double>(a);
                        a += step;
                        if (a == b + step) ARG1 = NONE;
                        else ARG1 = static_cast<double>(a);
                        CALL_THUNK_AT(0);
                    }
                }
                break;

            case bsel: --top; break;
            case add: ARG2 += ARG1; --top; break;
            case sub: ARG2 = ARG1 - ARG2; --top; break;
            case mul: ARG2 *= ARG1;  --top; break;
            case divi: ARG2 = ARG1 / ARG2; --top; break;
            case mod: ARG2 = std::fmod(ARG1, ARG2); --top; break;
            case power: ARG2 = std::pow(ARG1, ARG2); --top; break;
            case logbase: ARG2 = log(ARG1) / log(ARG2); --top; break;
            case max: ARG2 = std::max(ARG1, ARG2); --top; break;
            case min: ARG2 = std::min(ARG1, ARG2); --top; break;
            case land: ARG2 = static_cast<double>(ARG1 && ARG2); --top; break;
            case lor: ARG2 = static_cast<double>(ARG1 || ARG2); --top; break;
            case lxor: ARG2 = static_cast<double>(
                               (ARG1 != 0.) ^ (ARG2 != 0.)); --top; break;
            case gcd: ARG2 = static_cast<double>(
                              std::gcd((int64_t) ARG1,
                                       (int64_t) ARG2)); --top; break;
            case lcm: ARG2 = ARG1 * ARG2 /
                      static_cast<double>(std::gcd(
                          (int64_t) ARG1, (int64_t) ARG2)); --top; break;
#ifdef ENABLE_NIVALIS_BOOST_MATH
            case choose: ARG2 = binomial_coefficient<double>(
                            (uint32_t)ARG1, (uint32_t)ARG2); --top; break;
            case fafact: ARG2 = falling_factorial<double>(
                            (uint32_t)ARG1, (uint32_t)ARG2); --top; break;
            case rifact: ARG2 = rising_factorial<double>(
                            (uint32_t)ARG1, (uint32_t)ARG2); --top; break;
            case betab: ARG2 = beta<double>(ARG1, ARG2); --top; break;
            case polygammab: ARG2 = beta<double>((int)ARG1, ARG2); --top; break;
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
                             --top; break;
                         }
            case fafact: {
                             double ad = std::round(ARG1);
                             double bd = std::round(ARG2);
                             if (ad < 0 || bd < 0) {
                                 ret = std::numeric_limits<double>::quiet_NaN(); break;
                             }
                             size_t a = static_cast<size_t>(ad), b = static_cast<size_t>(bd);
                             ARG2 = fa_fact(a, a-b);
                             --top; break;
                         }
            case rifact: {
                             double ad = std::round(ARG1);
                             double bd = std::round(ARG2);
                             if (ad < 0 || bd < 0) {
                                 ret = std::numeric_limits<double>::quiet_NaN(); break;
                             }
                             size_t a = static_cast<size_t>(ad), b = static_cast<size_t>(bd);
                             ARG2 = fa_fact(a+b-1, a-1);
                             --top; break;
                         }
            case betab: case polygammab:
                ARG1 = NONE; print_boost_warning(opcode); break;
#endif
            case lt: ARG2 = static_cast<double>(ARG1 < ARG2); --top; break;
            case le: ARG2 = static_cast<double>(ARG1 <= ARG2); --top; break;
            case eq: ARG2 = static_cast<double>(ARG1 == ARG2); --top; break;
            case ne: ARG2 = static_cast<double>(ARG1 != ARG2); --top; break;
            case ge: ARG2 = static_cast<double>(ARG1 >= ARG2); --top; break;
            case gt: ARG2 = static_cast<double>(ARG1 > ARG2); --top; break;

            case unaryminus: ARG1 = -ARG1; break;
            case lnot: ARG1 = static_cast<double>(!(ARG1)); break;
            case absb: ARG1 = std::fabs(ARG1); break;
            case sqrtb: ARG1 = std::sqrt(ARG1); break;
            case sqrb: ARG1 *= ARG1; break;
            case sgn: ARG1 = ARG1 > 0 ? 1 : (ARG1 == 0 ? 0 : -1); break;
            case floorb: ARG1 = floor(ARG1); break;
            case ceilb: ARG1 = ceil(ARG1); break; case roundb: ARG1 = round(ARG1); break;

            case expb:   ARG1 = exp(ARG1); break; case exp2b: ARG1 = exp2(ARG1); break;
            case logb:   ARG1 = log(ARG1); break;
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
            case log2b: ARG1 = log2(ARG1); break;  case log10b: ARG1 = log10(ARG1); break;
            case sinb: ARG1 = sin(ARG1); break;   case cosb:   ARG1 = cos(ARG1); break;
            case tanb: ARG1 = tan(ARG1); break;   case asinb: ARG1 = asin(ARG1); break;
            case acosb: ARG1 = acos(ARG1); break;  case atanb: ARG1 = atan(ARG1); break;
            case sinhb: ARG1 = sinh(ARG1); break;  case coshb: ARG1 = cosh(ARG1); break;
            case tanhb: ARG1 = tanh(ARG1); break;
            case tgammab: ARG1 = std::tgamma(ARG1); break;
            case lgammab: ARG1 = std::lgamma(ARG1); break;
#ifdef ENABLE_NIVALIS_BOOST_MATH
            case digammab: ARG1 = digamma<double>(ARG1); break;
            case trigammab: ARG1 = trigamma<double>(ARG1); break;
            case zetab: ARG1 = zeta<double>(ARG1); break;
#else
           // The following functions are unavailable without Boost
            case digammab: case trigammab: case zetab:
                ARG1 = NONE; print_boost_warning(opcode); break;
#endif
            case erfb: ARG1 = erf(ARG1); break; break;
        }
    }
    return stk[top--];
}
}  // namespace detail

// Interface for evaluating expression
double Expr::operator()(Environment& env) const {
    return detail::eval_ast(env, ast);
}

}  // namespace nivalis
