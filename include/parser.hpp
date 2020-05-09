#pragma once
#ifndef _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB
#define _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB

#include<string>

#include "env.hpp"
#include "expr.hpp"
namespace nivalis {

// Nivalis parser
struct Parser {
    // Parse 'expr' in environment 'env'
    // If mode_explicit is true, errors when undefined variable used (normal)
    // If mode_explicit is false, defined the variable implicitly
    // If quiet is true, does not print anything on error
    Expr operator()(const std::string& expr, Environment& env,
                    bool mode_explicit = true, bool quiet = false) const;

    // Error message; empty if no error
    mutable std::string error_msg;
private:
    struct ParserImpl;
};

}  // namespace nivalis
#endif // ifndef _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB
