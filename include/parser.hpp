#pragma once
#ifndef _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB
#define _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB

#include<string>
#include<vector>

#include "env.hpp"
#include "expr.hpp"
namespace nivalis {

// Nivalis parser
struct Parser {
    Parser();

    // Parse 'expr' in environment 'env'
    // If mode_explicit is true, errors when undefined variable used (normal)
    // If mode_explicit is false, defined the variable implicitly
    Expr operator()(const std::string& expr, Environment& env,
                    bool mode_explicit = true) const;
private:
    struct ParserImpl;
};

}  // namespace nivalis
#endif // ifndef _EVAL_H_124AFB48_06EE_4E9B_A309_BF189C5976DB
