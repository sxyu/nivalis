#pragma once
#ifndef _AST_NODES_H_72E24F9F_E6EA_4F25_8412_9ABFB1C5B2FF
#define _AST_NODES_H_72E24F9F_E6EA_4F25_8412_9ABFB1C5B2FF
#include <cstdint>
#include <vector>
#include "env.hpp"

namespace nivalis {
namespace detail {
// ** AST node form processing + optimization **
// AST Node representation
struct ASTNode {
    explicit ASTNode(uint32_t opcode); 
    uint32_t opcode, c[3], ref;
    double val;
    bool nonconst_flag, null_flag;
};

// Convert AST in uint32_t vector form to nodes (returns root)
uint32_t ast_to_nodes(const uint32_t** ast, std::vector<ASTNode>& store);

// Convert AST in nodes form to byte vector
void ast_from_nodes(const std::vector<ASTNode>& nodes, std::vector<uint32_t>& out, uint32_t index = 0); 

// Eval AST in nodes form
// (currently, converts to AST in vector form and calls eval_ast)
double eval_ast_nodes(Environment& env, const std::vector<ASTNode>& nodes, uint32_t idx = 0); 

// Optimize AST in node form (implementation in ast_nodes.cpp)
void optim_nodes(Environment& env, std::vector<ASTNode>& nodes, uint32_t vi = 0);
}  // namespace detail
}  // namespace nivalis
#endif // ifndef 72E24F9F_E6EA_4F25_8412_9ABFB1C5B2FF
