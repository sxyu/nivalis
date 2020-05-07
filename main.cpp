#include "parser.hpp"
#include "plotgui.hpp"
#include "util.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cctype>

namespace {
const char * WIND_NAME = "Nivalis";

std::string get_word(std::string& line) {
    size_t i = 0;
    for (; i < line.size(); ++i) if (!std::isspace(line[i])) break;
    for (; i < line.size(); ++i) if (std::isspace(line[i])) break;
    std::string word = line.substr(0, i);
    line = line.substr(i);
    return word;
}

std::vector<std::string> get_args(std::string& line) {
    std::vector<std::string> args;
    while (line.size()) {
        args.push_back(get_word(line));
    }
    return args;
}

}  // namespace

int main(int argc, char ** argv) {
    using namespace nivalis;
    std::cout << "Nivalis 0.0.1 (c) Alex Yu 2020\n";

    std::string orig_line, line, var;
    int assn_opcode = OpCode::bsel;
    Environment env;
    Parser parse;

    std::cout << std::setprecision(16);
    while(true) {
        std::cout << ">>> " << std::flush;
        std::getline(std::cin, orig_line);
        if (!std::cin) break;
        line.clear();
        for (char c : orig_line) {
            if (!std::isspace(c)) line.push_back(c);
        }

        std::string cmd = get_word(orig_line);
        if (cmd == "exit") break;
        else if (cmd == "plot") {
            // Plot function
            util::trim(orig_line);
            PlotGUI gui(env, orig_line);
        }
        else if (cmd == "del") {
            // Delete variable
            util::trim(orig_line);
            if (env.del(orig_line)) {
                std::cout << "del " << orig_line << std::endl;
            } else {
                std::cout << "Undefined variable " << orig_line << "\n";
            }
        } else {
            // Evaluate
            var.clear();
            assn_opcode = OpCode::bsel;
            if (util::is_varname_first(line[0])) {
                size_t pos = util::find_equality(line);
                if (~pos) {
                    var = line.substr(0, pos);
                    if (util::is_arith_operator(var.back())) {
                        assn_opcode = Expr::opcode_from_opchar(var.back());
                        var.pop_back();
                    }
                    if (util::is_varname(var)) {
                        // Assignment
                        line = line.substr(pos + 1);
                    }
                }
            }

            auto expr = parse(line, env);
            double output;
            if (expr.ast[0] != OpCode::dead &&
                !std::isnan(output = expr(env))) {
                expr.optimize();
                if (var.size()) {
                    double var_val;
                    if (assn_opcode != OpCode::bsel) {
                        auto addr = env.addr_of(var, true);
                        if (addr == -1) {
                            std::cout << "Undefined variable \"" << var
                                << "\" (operator assignment)\n";
                            continue;
                        }
                        var_val = Expr::constant(env.vars[addr])
                            .combine(assn_opcode,
                                    Expr::constant(output))(env);
                    } else {
                        var_val = output;
                    }
                    env.set(var, var_val);
                    std::cout << var << " = " << var_val << std::endl;
                } else {
                    std::cout << output << std::endl;
                }
            }
        }
    }
    return 0;
}
