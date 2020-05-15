#pragma once
#ifndef _SHELL_H_4B53399C_23B9_41F1_B31C_11746367D104
#define _SHELL_H_4B53399C_23B9_41F1_B31C_11746367D104
#include <string>
#include <ostream>
#include "env.hpp"
#include "parser.hpp"

namespace nivalis {

class Shell {
public:
    explicit Shell(Environment& env, std::ostream& os);
    // Evaluate a line
    void eval_line(std::string line); // string copy intentional
    // Whether shell is 'closed'
    bool closed = false;
private:
    std::ostream& os;
    Environment& env;
    Parser parse;
};

}  // namespace nivalis
#endif // ifndef _SHELL_H_4B53399C_23B9_41F1_B31C_11746367D104
