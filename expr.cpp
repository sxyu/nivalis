#include "expr.hpp"
#include <numeric>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "util.hpp"
namespace nivalis {

namespace {
#define EV_NEXT eval_ast(env, ast)
#define EV_NEXT_SKIP eval_ast_skip(ast)
#define EV_NEXT_FIND_VAR if (eval_ast_find_var(ast, var_id)) { return true; }
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
        case bsel: case add: case sub: case mul: case div: case mod:
        case power: case logbase: case max: case min:
        case land: case lor: case lxor:
        case lt: case le: case eq: case ne: case ge: case gt:
            EV_NEXT_SKIP; EV_NEXT_SKIP; break;
        case null: break;
        // Unary
        default: EV_NEXT_SKIP;
    }
}

// Walk through AST to check if variable is present
bool eval_ast_find_var(const uint32_t** ast, uint32_t var_id) {
    using namespace nivalis::OpCode;
    uint32_t opcode = **ast;
    ++*ast;
    switch(opcode) {
        case val: *ast += 2; break;
        case ref:
          if (**ast == var_id) return true;
          ++*ast;
          break;
        case bnz: EV_NEXT_FIND_VAR; EV_NEXT_FIND_VAR; EV_NEXT_FIND_VAR; break;
        // Binary
        case bsel: case add: case sub: case mul: case div: case mod:
        case power: case logbase: case max: case min:
        case land: case lor: case lxor:
        case lt: case le: case eq: case ne: case ge: case gt:
            EV_NEXT_FIND_VAR; EV_NEXT_FIND_VAR; break;
        case null: break;
        // Unary
        default: EV_NEXT_FIND_VAR;
    }
    return false;
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

        case bsel: EV_NEXT; ret = EV_NEXT; break;
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

// Walk through AST and detect if each subtree has constants only
bool eval_ast_detect_constexpr(const uint32_t** ast,
        const uint32_t* begin,
        std::vector<bool>& is_constexpr) {
    using namespace nivalis::OpCode;
    uint32_t opcode = **ast;
    auto nodepos = *ast - begin;
    ++*ast;
    bool nontrivial = false;
    switch(opcode) {
        case val: *ast += 2; break;
        case ref: ++*ast; nontrivial = true; break;
        case bnz: EV_NEXT_DC; EV_NEXT_DC; EV_NEXT_DC; break;
        // Binary
        case bsel: case add: case sub: case mul: case div: case mod:
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
// Replace constant subtrees with one precomputed value
void eval_ast_optim_constexpr(
        Environment & env,
        const uint32_t** ast,
        const uint32_t* begin,
        const std::vector<bool>& is_constexpr,
        std::vector<uint32_t>& ast_out) {
    using namespace nivalis::OpCode;
    uint32_t opcode = **ast;
    auto nodepos = *ast - begin;
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
        case bsel: case add: case sub: case mul: case div: case mod:
        case power: case logbase: case max: case min:
        case land: case lor: case lxor:
        case lt: case le: case eq: case ne: case ge: case gt:
            EV_NEXT_OPTIM; EV_NEXT_OPTIM; break;
        case null: break;
        // Unary
        default: EV_NEXT_OPTIM;
    }
}

Expr combine_expr(uint32_t opcode, const Expr& a, const Expr& b) {
    Expr new_expr;
    new_expr.ast.clear();
    new_expr.ast.reserve(a.ast.size() + b.ast.size() + 1);
    new_expr.ast.push_back(opcode);
    std::copy(a.ast.begin(), a.ast.end(), std::back_inserter(new_expr.ast));
    std::copy(b.ast.begin(), b.ast.end(), std::back_inserter(new_expr.ast));
    return new_expr;
}
Expr wrap_expr(uint32_t opcode, const Expr& a) {
    Expr new_expr;
    new_expr.ast.clear();
    new_expr.ast.reserve(a.ast.size() + 1);
    new_expr.ast.push_back(opcode);
    std::copy(a.ast.begin(), a.ast.end(), std::back_inserter(new_expr.ast));
    return new_expr;
}

}  // namespace

Expr::Expr() {
    ast.resize(8);
}
double Expr::operator()(Environment& env) const {
    const uint32_t* astptr = &ast[0];
    return eval_ast(env, &astptr);
}

Expr Expr::operator+(const Expr& other) const {
    return combine_expr(OpCode::add, *this, other);
}
Expr Expr::operator-(const Expr& other) const {
    return combine_expr(OpCode::sub, *this, other);
}
Expr Expr::operator*(const Expr& other) const {
    return combine_expr(OpCode::mul, *this, other);
}
Expr Expr::operator/(const Expr& other) const {
    return combine_expr(OpCode::div, *this, other);
}
Expr Expr::operator^(const Expr& other) const {
    return combine_expr(OpCode::power, *this, other);
}
Expr Expr::combine(uint32_t opcode, const Expr& other) const {
    return combine_expr(opcode, *this, other);
}
Expr Expr::operator-() const {
    return wrap_expr(OpCode::uminusb, *this);
}
Expr Expr::wrap(uint32_t opcode) const {
    return wrap_expr(opcode, *this);
}

bool Expr::has_var(uint32_t addr) const {
    const uint32_t* astptr = &ast[0];
    return eval_ast_find_var(&astptr, addr);
}

Expr Expr::null() {
    return Expr();
}
Expr Expr::zero() {
    return constant(0);
}
Expr Expr::constant(double val) {
    Expr z;
    z.ast.clear();
    util::push_dbl(z.ast, val);
    return z;
}
uint32_t Expr::opcode_from_opchar(char c) {
    switch (c) {
        case '+': return OpCode::add;
        case '-': return OpCode::sub;
        case '*': return OpCode::mul;
        case '/': return OpCode::div;
        case '%': return OpCode::mod;
        case '^': return OpCode::power;
        case '<': return OpCode::lt;
        case '>': return OpCode::gt;
        case '=': return OpCode::eq;
        default: return OpCode::bsel;
    };
}

void Expr::optimize() {
    if (ast.empty()) return;
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
    os << "Expr[";
    for (uint32_t i : expr.ast) {
        os << i << " ";
    }
    os << "]";
    return os;
}

}  // namespace nivalis
