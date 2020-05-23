#pragma once
#ifndef _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB
#define _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB

#include<string>

#include "env.hpp"
#include "expr.hpp"
namespace nivalis {

// Parse 'expr' in environment 'env'
// If mode_explicit is true, errors when undefined variable used (normal)
// If mode_explicit is false, defined the variable implicitly
// If quiet is true, does not print anything on error
//   > if error_msg is not null, writes the error to it
// max_args specifies max number of explicit function arguments (like $1) to allow
Expr parse(const std::string& expr, Environment& env,
        bool mode_explicit = true, bool quiet = false,
        size_t max_args = 0,
        std::string* error_msg = nullptr);

}  // namespace nivalis
#endif // ifndef _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB
