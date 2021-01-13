#include "expr.hpp"

#include "version.hpp"
#include "env.hpp"

#include <string_view>
#include <iostream>
#include <cmath>
#include <numeric>
#include "opcodes.hpp"
#include "util.hpp"

namespace nivalis {

namespace {
const complex NONE = std::numeric_limits<double>::quiet_NaN();
using ::nivalis::OpCode::repr;
}  // namespace

namespace detail {
complex eval_ast(Environment& env, const Expr::AST& ast,
                 const std::vector<complex>& arg_vals) {
    // Main AST evaluation stack
    thread_local std::vector<complex> stk;
    // Stack of thunks available:
    // contains positions of thunk_jmp encountered
    thread_local std::vector<size_t> thunks;
    // Thunk call stack: contains positions to return to
    // after reaching thunk_ret
    thread_local std::vector<size_t> thunks_stk;

    // Max function call stack height
    static const size_t MAX_CALL_STK_HEIGHT = 256;
    // Curr function call stack height
    thread_local size_t call_stk_height = 0;

    thread_local size_t top = -1;
    size_t init_top = top;

    // Make sure there is enough space
    stk.resize(top + ast.size() + 1);

    using namespace nivalis::OpCode;

    bool _is_thunk_ret = false;

    // Shorthands for first, 2nd, 3rd arguments to operator
#define ARG3 stk[top - 2]
#define ARG2 stk[top - 1]
#define ARG1 stk[top]
// Return value from thunk or func call
#define RET_VAL stk[top + 1]
// Quit without messing up stack
#define FAIL_AND_QUIT   \
    do {                \
        top = init_top; \
        return NONE;    \
    } while (0)

    for (size_t cidx = ast.size() - 1; ~cidx; --cidx) {
        const auto& node = ast[cidx];
        switch (node.opcode) {
            case null:
                stk[++top] = NONE;
                break;
            case val:
                stk[++top] = node.val;
                break;
            case ref:
                stk[++top] = env.vars[node.ref];
                break;
            case arg:
                stk[++top] = arg_vals[node.ref];
                break;
            case thunk_jmp:
                thunks.push_back(cidx);
                cidx -= ast[cidx].ref;
                break;
            case thunk_ret:
                cidx = thunks_stk.back() + 1;
                thunks_stk.pop_back();
                _is_thunk_ret = true;
                break;
            case call: {
                size_t n_args = node.call_info[1];
                auto& func = env.funcs[node.call_info[0]];
                std::vector<complex> f_args(n_args);
                for (size_t i = 0; i < n_args; ++i) {
                    f_args[i] = stk[top--];
                }
                if (n_args != func.n_args ||         // Should not happen
                    &func.expr.ast[0] == &ast[0] ||  // Disallow recursion
                    call_stk_height >
                        MAX_CALL_STK_HEIGHT)  // Too many nested calls
                    FAIL_AND_QUIT;
                ++call_stk_height;
                eval_ast(env, func.expr.ast, f_args);
                --call_stk_height;
                ++top;
            } break;
            case bnz:
                if (_is_thunk_ret) {
                    --top;
                    _is_thunk_ret = false;
                    ARG1 = RET_VAL;
                } else {
                    thunks_stk.push_back(cidx);
                    cidx = thunks[thunks.size() - (ARG1 == 0.) - 1];
                    thunks.resize(thunks.size() - 2);
                }
                break;

            case sums:
            case prods: {
                if (_is_thunk_ret) {
                    --top;
                    _is_thunk_ret = false;
                    // update arg3 (the output)
                    if (node.opcode == prods)
                        ARG3 *= RET_VAL;
                    else
                        ARG3 += RET_VAL;  // arg3 is output
                } else {
                    // Move over the arguments and use arg3
                    // as output
                    ++top;
                    ARG1 = ARG2;
                    ARG2 = ARG3;
                    ARG3 = node.opcode == prods ? 1. : 0.;
                }
                uint64_t var_id = node.ref;
                int64_t a = static_cast<int64_t>(ARG1.real()),
                        b = static_cast<int64_t>(ARG2.real());
                int64_t step = (a <= b) ? 1 : -1;
                if (std::isnan(ARG1.real())) {
                    top -= 2;
                    thunks.pop_back();
                } else {
                    env.vars[var_id] = static_cast<double>(a);
                    a += step;
                    if (a == b + step)
                        ARG1 = NONE;
                    else
                        ARG1 = static_cast<complex>(a);
                    thunks_stk.push_back(cidx);
                    cidx = thunks.back();
                }
            } break;

            case bsel:
                --top;
                break;
            case add:
                ARG2 += ARG1;
                --top;
                break;
            case sub:
                ARG2 = ARG1 - ARG2;
                --top;
                break;
            case mul:
                ARG2 *= ARG1;
                --top;
                break;
            case divi:
                ARG2 = ARG1 / ARG2;
                --top;
                break;
            case power:
                ARG2 = std::pow(ARG1, ARG2);
                --top;
                break;
            case logbase:
                ARG2 = log(ARG1) / log(ARG2);
                --top;
                break;
            case land:
                ARG2 = static_cast<double>(std::abs(ARG1) && std::abs(ARG2));
                --top;
                break;
            case lor:
                ARG2 = static_cast<double>(std::abs(ARG1) || std::abs(ARG2));
                --top;
                break;
            case lxor:
                ARG2 = static_cast<double>((std::abs(ARG1) != 0.) ^
                                           (std::abs(ARG2) != 0.));
                --top;
                break;
            case eq:
                ARG2 = static_cast<double>(ARG1 == ARG2);
                --top;
                break;
            case ne:
                ARG2 = static_cast<double>(ARG1 != ARG2);
                --top;
                break;
            case ge:
                ARG2 = static_cast<double>(ARG1.real() >= ARG2.real());
                --top;
                break;
            case gt:
                ARG2 = static_cast<double>(ARG1.real() > ARG2.real());
                --top;
                break;
            case le:
                ARG2 = static_cast<double>(ARG1.real() <= ARG2.real());
                --top;
                break;
            case lt:
                ARG2 = static_cast<double>(ARG1.real() < ARG2.real());
                --top;
                break;

            case unaryminus:
                ARG1 = -ARG1;
                break;
            case lnot:
                ARG1 = static_cast<double>(!(std::abs(ARG1)));
                break;
            case absb:
                ARG1 = std::abs(ARG1);
                break;
            case sqrtb:
                ARG1 = std::sqrt(ARG1);
                break;
            case sqrb:
                ARG1 *= ARG1;
                break;
            case sgn: {
                double tmp = ARG1.real();
                ARG1 = tmp > 0 ? 1 : (tmp == 0 ? 0 : -1);
            } break;
            case floorb:
                ARG1 = std::floor(ARG1.real());
                break;
            case ceilb:
                ARG1 = std::ceil(ARG1.real());
                break;
            case roundb:
                ARG1 = std::round(ARG1.real());
                break;

            case expb:
                ARG1 = exp(ARG1);
                break;
            case logb:
                ARG1 = log(ARG1);
                break;
            case log10b:
                ARG1 = log10(ARG1);
                break;
            case sinb:
                ARG1 = sin(ARG1);
                break;
            case cosb:
                ARG1 = cos(ARG1);
                break;
            case tanb:
                ARG1 = tan(ARG1);
                break;
            case asinb:
                ARG1 = asin(ARG1);
                break;
            case acosb:
                ARG1 = acos(ARG1);
                break;
            case atanb:
                ARG1 = atan(ARG1);
                break;
            case sinhb:
                ARG1 = sinh(ARG1);
                break;
            case coshb:
                ARG1 = cosh(ARG1);
                break;
            case tanhb:
                ARG1 = tanh(ARG1);
                break;

            case conjb:
                ARG1 = std::conj(ARG1);
                break;
            case realb:
                ARG1 = ARG1.real();
                break;
            case imagb:
                ARG1 = ARG1.imag();
                break;
            case argb:
                ARG1 = std::arg(ARG1);
                break;
        }
    }
    return stk[top--];
}
}  // namespace detail

// Interface for evaluating expression
complex Expr::operator()(Environment& env) const {
    return detail::eval_ast(env, ast);
}
complex Expr::operator()(complex arg, Environment& env) const {
    return detail::eval_ast(env, ast, {arg});
}
complex Expr::operator()(const std::vector<complex>& args,
                         Environment& env) const {
    return detail::eval_ast(env, ast, args);
}

}  // namespace nivalis
