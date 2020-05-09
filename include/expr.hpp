#pragma once
#ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
#define _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
#include <ostream>
#include <vector>

#include "env.hpp"
#include "opcodes.hpp"
namespace nivalis {

// Nivalis expression
struct Expr {
    Expr();

    // Evaluate expression in environment
    double operator()(Environment& env) const;

    // Combine expressions with basic operator
    Expr operator+(const Expr& other) const; Expr operator-(const Expr& other) const;
    Expr operator*(const Expr& other) const; Expr operator%(const Expr& other) const;
    Expr operator/(const Expr& other) const; Expr operator^(const Expr& other) const;
    // Combine with other using binary operator
    Expr combine(uint32_t opcode, const Expr& other) const;
    // Unary minus wrapping
    Expr operator-() const;
    // Apply unary operator with given opcode
    Expr wrap(uint32_t opcode) const;

    // Checks if expression contains given variable
    bool has_var(uint32_t addr) const;

    // Null expr
    static Expr null();
    // Zero expr
    static Expr zero();
    // Const expr
    static Expr constant(double val);

    // Optimize expression in-place
    void optimize();

    // String representation of expression (can be evaluated again)
    std::string repr(const Environment& env) const;

    // Take the derivative wrt var with address 'var_addr' in the given environment
    Expr diff(uint32_t var_addr, Environment& env) const;

    // Use Newton(-Raphson) method to compute root for variable with addr var_addr
    // optionally, supply computed derivative
    // eps_step: stopping condition, |f(x)/df(x)|
    // eps_abs: stopping condition, |f(x)|
    // max_iter: stopping condition, steps
    double newton(uint32_t var_addr, double x0, Environment& env,
                  double eps_step, double eps_abs, int max_iter = 20,
                  double xmin = -std::numeric_limits<double>::max(),
                  double xmax = std::numeric_limits<double>::max(),
                  const Expr* deriv = nullptr) const;

    // Abstract syntax tree
    std::vector<uint32_t> ast;
};

// Display as string
std::ostream& operator<<(std::ostream& os, const Expr& expr);

}  // namespace nivalis
#endif // ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
