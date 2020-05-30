#pragma once
#ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
#define _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
#include <ostream>
#include <istream>
#include <vector>

#include "opcodes.hpp"
namespace nivalis {

struct Environment; // in env.hpp

// Nivalis expression
struct Expr {
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
        static ASTNode varref(uint64_t id);
        static ASTNode call(uint32_t id, uint32_t n_arg);

        bool operator==(const ASTNode& other) const;
        bool operator!=(const ASTNode& other) const;
        union {
            // Variable reference id
            uint64_t ref;
            // Stored value
            double   val;
            // Used for func calls (func_id, n_args)
            uint32_t call_info[2];
        };
    };
    typedef std::vector<Expr::ASTNode> AST;

    Expr();
    Expr(const AST& ast);

    // Evaluate expression in environment
    double operator()(Environment& env) const;
    // Evaluate expression in environment, setting one argument
    double operator()(double arg, Environment& env) const;
    // Evaluate expression in environment, setting arguments
    double operator()(const std::vector<double>& args, Environment& env) const;

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

    // Substitute variable with value in-place
    void sub_var(uint32_t addr, double value);

    // Substitute variable with another expression
    // will copy internally
    // Only substitutes OpCode ref's
    void sub_var(uint32_t addr, const Expr& expr);

    // Null expr
    static Expr null();
    // Zero expr
    static Expr zero();
    // Const expr
    static Expr constant(double val);

    // String representation of expression (can be evaluated again)
    std::ostream& repr(std::ostream& os, const Environment& env) const;

    // Binary serialization
    std::ostream& to_bin(std::ostream& os) const;
    std::istream& from_bin(std::istream& is);

    // Checks if this is a null expression OR a value which is nan
    bool is_null() const;

    // Checks if this is a value
    bool is_val() const;

    // Checks if this is a ref
    bool is_ref() const;

    // Shorthand for ast[]
    const ASTNode& operator[](int idx) const;
    // Shorthand for ast[]
    ASTNode& operator[](int idx);

    // Next section implemented optimize_expr.cpp
    // Optimize expression in-place
    void optimize();

    // Next section implemented diff_expr.cpp
    // Take the derivative wrt var with address 'var_addr' in the given environment
    Expr diff(uint64_t var_addr, Environment& env) const;

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

    // DATA: Abstract syntax tree
    AST ast;
};

namespace detail {
// Evaluate an AST directly (advanced)
double eval_ast(Environment& env, const Expr::AST& ast,
                const std::vector<double>& arg_vals = {});

// Print AST
size_t print_ast(std::ostream& os, const Expr::AST& ast,
               const Environment* env = nullptr,
               size_t idx = 0);

// Substitute variables in AST, in-place (advanced)
void sub_var_ast(Expr::AST& ast, int64_t addr, double value);

// Substitute variables in AST, in-place (advanced)
void sub_var_ast(Expr::AST& ast, int64_t addr, const Expr& expr);

// Checks if AST contains given variable
bool has_var_ast(const Expr::AST& ast, uint32_t addr);
}  // namespace

// Display as string
std::ostream& operator<<(std::ostream& os, const Expr& expr);
std::ostream& operator<<(std::ostream& os, const Expr::ASTNode& node);

}  // namespace nivalis
#endif // ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
