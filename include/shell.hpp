#pragma once
#ifndef _SHELL_H_4B53399C_23B9_41F1_B31C_11746367D104
#define _SHELL_H_4B53399C_23B9_41F1_B31C_11746367D104
#include <string>
#include <ostream>
#include "env.hpp"

namespace nivalis {

class Shell {
public:
    explicit Shell(Environment& env, std::ostream& os);
    // Evaluate a line; returns true iff no error
    bool eval_line(std::string line); // string copy intentional
    // Whether shell is 'closed' (must be handled by frontend)
    bool closed = false;
private:
    std::ostream& os;
    Environment& env;
};

}  // namespace nivalis
#endif // ifndef _SHELL_H_4B53399C_23B9_41F1_B31C_11746367D104
