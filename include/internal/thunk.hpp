#pragma once
#ifndef _THUNK_H_657D6CF2_26ED_4F84_B20C_FDF2A8CF83CF
#define _THUNK_H_657D6CF2_26ED_4F84_B20C_FDF2A8CF83CF

#include <vector>
#include "expr.hpp"

namespace nivalis {

// Nested thunk helper
struct ThunkManager {
    ThunkManager(Expr::AST& ast);
    // Call when starting a thunk
    void begin();
    // Call when finishing a thunk
    void end();

    // Beginnings of thunks
    std::vector<size_t> thunks;
    Expr::AST& ast;
};

}  // namespace nivalis

#endif // ifndef _THUNK_H_657D6CF2_26ED_4F84_B20C_FDF2A8CF83CF
