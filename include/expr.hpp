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
    Expr operator+(const Expr& other) const;
    Expr operator-(const Expr& other) const;
    Expr operator*(const Expr& other) const;
    Expr operator/(const Expr& other) const;
    // Combine with other using binary operator
    Expr combine(uint32_t opcode, const Expr& other) const;
    // Unary minus wrapping
    Expr operator-() const;
    // Apply unary operator with given opcode
    Expr wrap(uint32_t opcode) const;

    // Checks if expression contains given variable
    bool has_var(uint32_t addr) const;
   
    // Substitute variable
    void sub_var(uint32_t addr, double value);

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
    Expr diff(uint64_t var_addr, Environment& env) const;

    // Checks if this is a null expression
    bool is_null() const;

    // Use Newton(-Raphson) method to compute root for variable with addr var_addr
    // eps_step: stopping condition, |f(x)/df(x)|
    // eps_abs: stopping condition, |f(x)|
    // max_iter: stopping condition, steps
    // deriv: optionally, supply computed derivative
    // fx0, dfx0: optionally, supply computed funciton/derivative values at x0
    double newton(uint64_t var_addr, double x0, Environment& env,
                  double eps_step, double eps_abs, int max_iter = 20,
                  double xmin = -std::numeric_limits<double>::max(),
                  double xmax = std::numeric_limits<double>::max(),
                  const Expr* deriv = nullptr,
                  double fx0 = std::numeric_limits<double>::max(),
                  double dfx0 = std::numeric_limits<double>::max()) const;

    // Represents abstract syntax tree node
    // no child pointers: first child immediately follows in list
    // second child follows after subtree of 1st child, etc.
    struct ASTNode {
        // Opcode of the node (val for value, ref for reference)
        uint32_t opcode;
        ASTNode();
        // Not explicit on purpose
        ASTNode(uint32_t opcode, uint64_t ref = -1);
        ASTNode(OpCode::_OpCode opcode);
        ASTNode(double val);
        bool operator==(const ASTNode& other) const;
        bool operator!=(const ASTNode& other) const;
        union {
            // Variable reference id
            uint64_t ref;
            // Stored value
            double   val;
        };
    };

    typedef std::vector<Expr::ASTNode> AST;
    // Padded AST, for performance enhancement
    AST ast;
};

namespace detail {
// Evaluate an AST directly (advanced)
double eval_ast(Environment& env, const Expr::AST& ast);

// Substitute variables in AST, in-place (advanced)
void sub_var_ast(Expr::AST& ast, int64_t addr, double value);

// Checks if AST contains given variable
bool has_var_ast(const Expr::AST& ast, uint32_t addr);
}  // namespace

// Display as string
std::ostream& operator<<(std::ostream& os, const Expr& expr);
std::ostream& operator<<(std::ostream& os, const Expr::ASTNode& node);

}  // namespace nivalis
#endif // ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
