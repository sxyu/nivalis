#pragma once
#ifndef _EVAL_EXPR_H_1CD819C2_4DB3_4032_84F0_E1336A80D90C
#define _EVAL_EXPR_H_1CD819C2_4DB3_4032_84F0_E1336A80D90C
#include <cstdint>
#include <ostream>
#include "env.hpp"

namespace nivalis {
namespace detail {

// Walk through AST from node with minimum computations
void skip_ast(const uint32_t** ast); 

// Walk through AST from node and print subtree to os
// optionally, include environment to look up variable names
void print_ast(std::ostream& os, const uint32_t** ast, const Environment* env = nullptr); 

// Walk through AST to check if variable is present
bool eval_ast_find_var(const uint32_t** ast, uint32_t var_id); 

// Walk through AST to sub in variable (outputs to out)
void eval_ast_sub_var(const uint32_t** ast, uint32_t var_id,
        double value, std::vector<uint32_t>& out); 

// Evaluate AST from node (main operator implementations)
double eval_ast(Environment& env, const uint32_t** ast);

}  // namespace detail
}  // namespace nivalis
#endif // ifndef _EVAL_EXPR_H_1CD819C2_4DB3_4032_84F0_E1336A80D90C
