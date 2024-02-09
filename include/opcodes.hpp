#pragma once
#ifndef _OPCODES_H_2A5399F9_0F2D_4BE8_91AD_126C7B428834
#define _OPCODES_H_2A5399F9_0F2D_4BE8_91AD_126C7B428834
#include <cstdint>
#include <map>
#include <string>
namespace nivalis {

// Contains definitions of OpCodes and constants
// Modify this file/opcodes.cpp to add new operators or constants
// implement in eval_expr.cpp eval_ast
// add optimization rules in ast_nodes.cpp
namespace OpCode {
// nivalis bytecode operators
enum _OpCode {
    null = 0, // returns NaN
    val,      // stores value in val
    ref,      // stores address of var in ref
    arg,      // function argument (index in ref)

    // thunk system
    thunk_ret = 8,     // beginning of thunk (unevaluated segment)
    thunk_jmp,         // end of thunk

    // function system
    call = 12,         // call function in environment

    // control and special forms
    bnz = 16, // if first is not zero, second, else third (short-circuiting)
    sums,
    prods,

    bsel = 24, // evaluate first and ignore; evaluate and return second

    // binary arithmetic operators
    add = 32, sub,
    mul = 48, divi, mod,
    power = 64, logbase,
    max = 80, min,
    land, lor, lxor,

    // binary comparison operators
    lt = 96, le, eq, ne, ge, gt,

    // binary math operators
    // integer
    gcd = 16384, lcm,
    choose, // n choose k
    fafact, // falling factorial
    rifact, // rising factorial

    // float
    betab, // beta function
    polygammab, // polygamma function

    // unary operators
    unaryminus = 32768,
    lnot,
    absb, sqrtb, sqrb, sgn, floorb, ceilb, roundb,
    expb, exp2b, logb, log10b, log2b, factb,
    sinb, cosb, tanb, asinb, acosb, atanb, sinhb, coshb, tanhb,
    tgammab, lgammab, digammab, trigammab,
    erfb, zetab,

    sigmoidb,
    softplusb,
    gausspdfb,
};

// Get # args the operator takes
size_t n_args(uint32_t opcode);

// Check if the operator has a reference (node.ref)
constexpr bool has_ref(uint32_t opcode) {
    return opcode == ref || opcode == sums ||
        opcode == prods || opcode == arg;
}

// Representations of expressions for printing purposes
// @: replace with subexpression
// \r: replace with ref in next 4 bytes
// \v: replace with value in next 8 bytes (double)
const char* repr(uint32_t opcode);

// Same as repr(), but prints in LaTeX format
const char* latex_repr(uint32_t opcode);

// Get opcode of binary operator from char e.g. '+' -> 32
// If not a valid binary operator, returns bsel
uint32_t from_char(char opchar);

// Binary operator character from opcode e.g. 32 -> '+'
// If not a valid binary operator, returns 0
char to_char(uint32_t opcode);

// Get map from function name to opcode
const std::map<std::string, uint32_t, std::less<> >& funcname_to_opcode_map();

// Get map from constant name to constant value
const std::map<std::string, double>& constant_value_map();

}  // namespace OpCode
}  // namespace nivalis
#endif // ifndef _OPCODES_H_2A5399F9_0F2D_4BE8_91AD_126C7B428834
