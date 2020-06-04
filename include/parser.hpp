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

// Convert LaTeX math to Nivalis expression
std::string latex_to_nivalis(const std::string& expr_in);

// Convert Nivalis expression to LaTeX math (may not be concise)
// by parsing the expression within in environment and then exporting the
// AST as LaTeX using Expr::latex_repr. Only works for single expressions
// (e.g. not polyline expression).
// The env may be modified.
std::string nivalis_to_latex(const std::string& expr_in, Environment& env);

// Convert Nivalis expression to LaTeX math partially by applying fixed string
// manipulations. This should work on anything expression, but the
// resulting expression may not be standard LaTeX math.
// Nevertheless, it should be possible to apply latex_to_nivalis() on the
// resulting expression and then use parse() to get a valid AST
std::string nivalis_to_latex_safe(const std::string& expr_in);

}  // namespace nivalis
#endif // ifndef _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB
