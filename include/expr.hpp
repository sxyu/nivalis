#pragma once
#ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
#define _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
#include <ostream>
#include <vector>

#include "env.hpp"
namespace nivalis {

namespace OpCode {
// nivalis bytecode
enum _OpCode {
    null = 0, // returns NaN
    val,      // stores value in 8 bytes after
    ref,      // stores address of var in env in 4 bytes after
    bsel = 8, // evaluate first and ignore; evaluate and return second

    // control and special forms
    bnz = 16, // if first is not zero, second, else third (short-circuiting)
    sums,
    prods,

    // binary arithmetic operators
    add = 32, sub,
    mul = 48, div, mod,
    power = 64, logbase,
    max = 80, min,
    land, lor, lxor,
    choose, // n choose k
    fafact, // falling factorial

    // binary comparison operators
    lt = 96, le, eq, ne, ge, gt,

    // unary operators
    nop = 32768, // idenitity
    uminusb, notb,
    absb, sqrtb, sgnb, floorb, ceilb, roundb,
    expb, exp2b, logb, log10b, log2b,
    sinb, cosb, tanb, asinb, acosb, atanb, sinhb, coshb, tanhb,
    gammab, factb,

    // diagnostics
    dead = 57005,
};
}  // namespace OpCode

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
    Expr operator^(const Expr& other) const;
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

    // Get opcode of binary operator from char e.g. '+' -> 32
    // If not a valid operator, returns bsel
    static uint32_t opcode_from_opchar(char c);

    // Optimize expression
    void optimize();

    // Abstract syntax tree
    std::vector<uint32_t> ast;
};
// Display as string
std::ostream& operator<<(std::ostream& os, const Expr& expr);

}  // namespace nivalis
#endif // ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
