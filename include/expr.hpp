#pragma once
#ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
#define _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
#include <ostream>
#include <vector>

#include "env.hpp"
namespace nivalis {

namespace OpCode {
enum _OpCode {
    null = 0, val, ref,
    bnz = 16,

    add = 32, sub,
    mul = 48, div, mod,
    power = 64, logbase,
    max = 80, min,
    land, lor, lxor,
    lt = 96, le, eq, ne, ge, gt,

    nop = 32768, uminusb, notb,
    absb, sqrtb, sgnb, floorb, ceilb, roundb,
    expb, logb, log10b, log2b,
    sinb, cosb, tanb, asinb, acosb, atanb, sinhb, coshb, tanhb,
    gammab, factb,
    
    printc = 65534, print,
    dead = 57005
};
}  // namespace OpCode

// Nivalis expression
struct Expr {
    Expr();

    // Evaluate expression in environment
    double operator()(Environment& env) const;

    // Optimize expression
    void optimize();

    // Abstract syntax tree
    std::vector<uint32_t> ast;
};
// Display as string
std::ostream& operator<<(std::ostream& os, const Expr& expr);

}  // namespace nivalis
#endif // ifndef _EXPR_H_331EE476_6D57_4B33_8148_D5EA882BC818
