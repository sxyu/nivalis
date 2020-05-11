#include "expr.hpp"
#include <numeric>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include "util.hpp"
#include "eval_expr.hpp"
#include "diff_expr.hpp"
#include "ast_nodes.hpp"
namespace nivalis {

namespace {
// Combine/wrap utils
Expr combine_expr(uint32_t opcode, const Expr& a, const Expr& b) {
    Expr new_expr;
    new_expr.ast.clear();
    new_expr.ast.reserve(a.ast.size() + b.ast.size() + 1);
    new_expr.ast.push_back(opcode);
    std::copy(a.ast.begin(), a.ast.end(), std::back_inserter(new_expr.ast));
    std::copy(b.ast.begin(), b.ast.end(), std::back_inserter(new_expr.ast));
    return new_expr;
}
#define COMBINE_EXPR(op) return combine_expr(op, *this, other)
Expr wrap_expr(uint32_t opcode, const Expr& a) {
    Expr new_expr;
    new_expr.ast.clear();
    new_expr.ast.reserve(a.ast.size() + 1);
    new_expr.ast.push_back(opcode);
    std::copy(a.ast.begin(), a.ast.end(), std::back_inserter(new_expr.ast));
    return new_expr;
}
}  // namespace

Expr::Expr() { ast.resize(2); }
double Expr::operator()(Environment& env) const {
    const uint32_t* astptr = &ast[0];
    return detail::eval_ast(env, &astptr);
}

Expr Expr::operator+(const Expr& other) const { COMBINE_EXPR(OpCode::add); }
Expr Expr::operator-(const Expr& other) const { COMBINE_EXPR(OpCode::sub); }
Expr Expr::operator*(const Expr& other) const { COMBINE_EXPR(OpCode::mul); }
Expr Expr::operator/(const Expr& other) const { COMBINE_EXPR(OpCode::div); }
Expr Expr::operator%(const Expr& other) const { COMBINE_EXPR(OpCode::mod); }
Expr Expr::operator^(const Expr& other) const { COMBINE_EXPR(OpCode::power); }
Expr Expr::combine(uint32_t opcode, const Expr& other) const { COMBINE_EXPR(opcode); }
Expr Expr::operator-() const { return wrap_expr(OpCode::unaryminus, *this); }
Expr Expr::wrap(uint32_t opcode) const { return wrap_expr(opcode, *this); }

bool Expr::has_var(uint32_t addr) const {
    const uint32_t* astptr = &ast[0];
    return detail::eval_ast_find_var(&astptr, addr);
}

Expr Expr::null() { return Expr(); }
Expr Expr::zero() { return constant(0); }
Expr Expr::constant(double val) {
    Expr z; z.ast.clear();
    util::push_dbl(z.ast, val);
    return z;
}

void Expr::optimize() {
    const uint32_t* astptr = &ast[0];
    std::vector<detail::ASTNode> nodes;
    detail::ast_to_nodes(&astptr, nodes);

    Environment dummy_env;
    detail::optim_nodes(dummy_env, nodes);

    ast.clear();
    detail::ast_from_nodes(nodes, ast);
}

std::string Expr::repr(const Environment& env) const {
    std::stringstream ss;
    const uint32_t* astptr = &ast[0];
    detail::print_ast(ss, &astptr, &env);
    auto s = ss.str();
    if (s.size() >= 2 &&
        s[0] == '(' && s.back() == ')') s = s.substr(1, s.size()-2);
    return s;
}

Expr Expr::diff(uint32_t var_addr, Environment& env) const {
    Expr dexpr;
    dexpr.ast = detail::diff_ast(ast, var_addr, env);
    dexpr.optimize();
    return dexpr;
}

bool Expr::is_null() const {
    return ast.empty() || ast[0] == OpCode::null ||
           (ast[0] == OpCode::val && std::isnan(util::as_double(&ast[1])));
}

double Expr::newton(uint32_t var_addr, double x0, Environment& env,
        double eps_step, double eps_abs, int max_iter,
        double xmin, double xmax, const Expr* deriv) const {
    if (deriv == nullptr) {
        Expr deriv_expr = diff(var_addr, env);
        return newton(var_addr, x0, env, eps_step,
                eps_abs, max_iter, xmin, xmax, &deriv_expr);
    }
    for (int i = 0; i < max_iter; ++i) {
        env.vars[var_addr] = x0;
        double fx = (*this)(env);
        if(std::isnan(fx))
            return std::numeric_limits<double>::quiet_NaN(); // Fail
        double dfx = (*deriv)(env);
        if(std::isnan(dfx) || dfx == 0.)
            return std::numeric_limits<double>::quiet_NaN(); // Fail
        double delta = fx / dfx;
        x0 -= delta;
        if (std::fabs(delta) < eps_step && std::fabs(fx) < eps_abs) return x0;
        if (x0 < xmin || x0 > xmax) {
            return std::numeric_limits<double>::quiet_NaN(); // Fail
        }
    }
    return std::numeric_limits<double>::quiet_NaN(); // Fail
}

std::ostream& operator<<(std::ostream& os, const Expr& expr) {
    os << "nivalis::Expr[";
    const uint32_t* astptr = &expr.ast[0];
    detail::print_ast(os, &astptr);
    os << "]";
    return os;
}

}  // namespace nivalis
