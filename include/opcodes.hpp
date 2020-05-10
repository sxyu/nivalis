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
};

// Representations of expressions for printing purposes
// @: replace with subexpression
// &: replace with ref in next 4 bytes
// #: replace with value in next 8 bytes (double)
const char* repr(uint32_t opcode); 

// Concise representations of expressions for parsing
// @: replace with subexpression
// &: replace with ref in next 4 bytes
// #: replace with value in next 8 bytes (double)
const char* subexpr_repr(uint32_t opcode); 

// Get opcode of operator from char e.g. '+' -> 32
// If not a valid operator, returns bsel
uint32_t from_char(char opchar); 

// Get map from function name to opcode
const std::map<std::string, uint32_t>& funcname_to_opcode_map();

// Get map from constant name to constant value
const std::map<std::string, double>& constant_value_map();

}  // namespace OpCode
}  // namespace nivalis
#endif // ifndef _OPCODES_H_2A5399F9_0F2D_4BE8_91AD_126C7B428834
