#include "internal/thunk.hpp"

namespace nivalis {
ThunkManager::ThunkManager(Expr::AST& ast) : ast(ast) { }
void ThunkManager::begin() {
    thunks.push_back(ast.size());
    ast.emplace_back(OpCode::thunk_ret);
}
void ThunkManager::end() {
    ast.emplace_back(OpCode::thunk_jmp,
            ast.size() - thunks.back());
    thunks.pop_back();
}
}  // namespace nivalis
