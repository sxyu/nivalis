#pragma once
#ifndef _DIFF_EXPR_H_294536FA_7441_4074_BFD3_C3DD1E5806E5
#define _DIFF_EXPR_H_294536FA_7441_4074_BFD3_C3DD1E5806E5
#include <cstdint>
#include <vector>

#include "env.hpp"

namespace nivalis {
namespace detail {

//  Take derivative
std::vector<uint32_t> diff_ast(const std::vector<uint32_t>& ast, uint32_t var_addr, Environment& env);

}  // namespace detail
}  // namespace nivalis
#endif // ifndef _DIFF_EXPR_H_294536FA_7441_4074_BFD3_C3DD1E5806E5
