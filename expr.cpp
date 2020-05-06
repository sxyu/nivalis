#include "expr.hpp"
#include <numeric>
#include <iostream>
#include <iomanip>
#include <cmath>
#include "util.hpp"
namespace nivalis {

namespace {
#define EV_NEXT eval_ast(env, ast)
#define EV_NEXT_SKIP eval_ast_skip(ast)
#define EV_NEXT_DC nontrivial |= eval_ast_detect_constexpr(ast, begin, is_constexpr)
#define EV_NEXT_OPTIM do{\
    if(skip_node) eval_ast_skip(ast); \
    else eval_ast_optim_constexpr(env, ast, begin, is_constexpr, ast_out); \
} while(0)
#define EV_WRAP(func) ret = func(eval_ast(env, ast)); break;
#define RET_NONE ret=std::numeric_limits<double>::quiet_NaN()

// Walk through AST from node with minimum computations
void eval_ast_skip(const uint32_t** ast) {
    using namespace nivalis::OpCode;
    uint32_t opcode = **ast;
    ++*ast;
    switch(opcode) {
        case val: *ast += 2; break;
        case ref: ++*ast; break;
        case bnz: EV_NEXT_SKIP; EV_NEXT_SKIP; EV_NEXT_SKIP; break;
        // Binary
        case add: case sub: case mul: case div: case mod:
        case power: case logbase: case max: case min:
        case land: case lor: case lxor:
        case lt: case le: case eq: case ne: case ge: case gt:
            EV_NEXT_SKIP; EV_NEXT_SKIP; break;
        case null: break;
        // Unary
        default: EV_NEXT_SKIP;
    }
}

// Evaluate AST from node
double eval_ast(Environment& env, const uint32_t** ast) {
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
                    ret = EV_NEXT; EV_NEXT_SKIP;
                } else {
                    EV_NEXT_SKIP; ret = EV_NEXT;
                }
            }
            break;

        case add: ret = EV_NEXT + EV_NEXT; break;
        case sub: ret = EV_NEXT; ret -= EV_NEXT; break;
        case mul: ret = EV_NEXT * EV_NEXT; break;
        case div: ret = EV_NEXT; ret /= EV_NEXT; break;
        case mod: ret = EV_NEXT; ret *= EV_NEXT; break;
        case power: ret = EV_NEXT; ret = pow(ret, EV_NEXT); break;
        case logbase: ret = EV_NEXT; ret = log(ret) / log(EV_NEXT); break;

        case max: ret = std::max(EV_NEXT, EV_NEXT); break;
        case min: ret = std::min(EV_NEXT, EV_NEXT); break;
        case land: ret = EV_NEXT && EV_NEXT; break;
        case lor: ret = EV_NEXT || EV_NEXT; break;
        case lxor: ret = static_cast<bool>(EV_NEXT) ^ static_cast<bool>(EV_NEXT); break;

        case lt: ret = EV_NEXT; ret = ret < EV_NEXT; break;
        case le: ret = EV_NEXT; ret = ret <= EV_NEXT; break;
        case eq: ret = EV_NEXT == EV_NEXT; break;
        case ne: ret = EV_NEXT != EV_NEXT; break;
        case ge: ret = EV_NEXT; ret = ret >= EV_NEXT; break;
        case gt: ret = EV_NEXT; ret = ret > EV_NEXT; break;
            
        case nop: ret = EV_NEXT; break;
        case uminusb: ret = -EV_NEXT; break;
        case notb: ret = !EV_NEXT; break;
        case absb: EV_WRAP(fabs);
        case sqrtb: EV_WRAP(sqrt);
        case sgnb: ret = EV_NEXT;
                   ret = ret > 0 ? 1 : (ret == 0 ? 0 : -1); break;
        case floorb: EV_WRAP(floor); case ceilb: EV_WRAP(ceil);
        case roundb: EV_WRAP(round);

        case expb:  EV_WRAP(exp);   case logb:   EV_WRAP(log);
        case log2b: EV_WRAP(log2);  case log10b: EV_WRAP(log10);
        case sinb:  EV_WRAP(sin);   case cosb:   EV_WRAP(cos);
        case tanb:  EV_WRAP(tan);   case asinb:  EV_WRAP(asin);
        case acosb: EV_WRAP(acos);  case atanb:  EV_WRAP(atan);
        case sinhb: EV_WRAP(sinh);  case coshb:  EV_WRAP(cosh);
        case tanhb: EV_WRAP(tanh);
        case gammab:  EV_WRAP(tgamma);
        case factb: ret = tgamma(EV_NEXT + 1); break;
        case printc: std::cout << static_cast<char>(
                            static_cast<int>(EV_NEXT)); RET_NONE; break;
        case print: 
                    {
                        double val = EV_NEXT;
                        int digs = static_cast<int>(EV_NEXT);
                        std::streamsize ss = std::cout.precision();
                        std::cout << std::fixed << std::setprecision(digs) << val << "\n";
                        RET_NONE;
                        std::cout << std::defaultfloat << std::setprecision(ss);
                    }
                break;

        default: RET_NONE; break;
    }
    return ret;
}

// Walk through AST and detect if variables present,
// Detect if each subtee has constants only
bool eval_ast_detect_constexpr(const uint32_t** ast,
        const uint32_t* begin,
        std::vector<bool>& is_constexpr) {
    using namespace nivalis::OpCode;
    uint32_t opcode = **ast;
    uint32_t nodepos = *ast - begin;
    ++*ast;
    bool nontrivial = false;
    switch(opcode) {
        case val: *ast += 2; break;
        case ref: ++*ast; nontrivial = true; break;
        case bnz: EV_NEXT_DC; EV_NEXT_DC; EV_NEXT_DC; break;
        // Binary
        case add: case sub: case mul: case div: case mod:
        case power: case logbase: case max: case min:
        case land: case lor: case lxor:
        case lt: case le: case eq: case ne: case ge: case gt:
            EV_NEXT_DC; EV_NEXT_DC; break;
        case null: break;
        // Unary
        default: EV_NEXT_DC;
    }
    is_constexpr[nodepos] = !nontrivial;
    return nontrivial;
}
// Detect if each subtee has constants only
void eval_ast_optim_constexpr(
        Environment & env,
        const uint32_t** ast,
        const uint32_t* begin,
        const std::vector<bool>& is_constexpr,
        std::vector<uint32_t>& ast_out) {
    using namespace nivalis::OpCode;
    uint32_t opcode = **ast;
    uint32_t nodepos = *ast - begin;
    bool skip_node = false;
    if (is_constexpr[nodepos] && opcode != OpCode::val) {
        skip_node = true;
        const uint32_t* ast_ptr_copy = *ast;
        util::push_dbl(ast_out, eval_ast(env, &ast_ptr_copy));
    }
    else {
        ast_out.push_back(**ast);
    }
    ++*ast;
    switch(opcode) {
        case val: 
            ast_out.push_back(**ast); ++ *ast;
            ast_out.push_back(**ast); ++ *ast;
        break;
        case ref:
            ast_out.push_back(**ast); ++*ast;
            break;
        case bnz: EV_NEXT_OPTIM; EV_NEXT_OPTIM; EV_NEXT_OPTIM; break;
        // Binary
        case add: case sub: case mul: case div: case mod:
        case power: case logbase: case max: case min:
        case land: case lor: case lxor:
        case lt: case le: case eq: case ne: case ge: case gt:
            EV_NEXT_OPTIM; EV_NEXT_OPTIM; break;
        case null: break;
        // Unary
        default: EV_NEXT_OPTIM;
    }
}

}  // namespace

Expr::Expr() {
    ast.resize(8);
}
double Expr::operator()(Environment& env) const {
    const uint32_t* astptr = &ast[0];
    return eval_ast(env, &astptr);
}

void Expr::optimize() {
    std::vector<bool> is_constexpr(ast.size());
    const uint32_t* astptr = &ast[0];
    eval_ast_detect_constexpr(&astptr, astptr, is_constexpr);

    astptr = &ast[0];
    Environment dummy_env;
    std::vector<uint32_t> ast_out;
    eval_ast_optim_constexpr(dummy_env, &astptr, astptr, is_constexpr, ast_out);
    ast.swap(ast_out);
}

std::ostream& operator<<(std::ostream& os, const Expr& expr) {
    os << "Expr";
    return os;
}

}  // namespace nivalis
