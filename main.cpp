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
    std::cout << "Nivalis 0.0.2 alpha (c) Alex Yu 2020\n";

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
        util::trim(orig_line);
        if (cmd == "exit") break;
        else if (cmd == "plot") {
            // Plot function
            PlotGUI gui(env, orig_line);
        }
        else if (cmd == "del") {
            // Delete variable
            if (env.del(orig_line)) {
                std::cout << "del " << orig_line << std::endl;
            } else {
                std::cout << "Undefined variable " << orig_line << "\n";
            }
        } else {
            bool do_optim = cmd == "opt";
            bool do_diff = cmd == "diff";
            if (do_optim) line = orig_line;
            uint32_t diff_var_addr;
            if (do_diff) {
                std::string diff_var = get_word(orig_line);
                util::trim(diff_var); util::trim(orig_line);
                if (!util::is_varname(diff_var)) {
                    std::cout << diff_var << " is not a valid variable name\n";
                    continue;
                }
                diff_var_addr = env.addr_of(diff_var, false);
                line = orig_line;
            }
            // Evaluate
            var.clear();
            assn_opcode = OpCode::bsel;
            if (util::is_varname_first(line[0])) {
                size_t pos = util::find_equality(line);
                if (~pos) {
                    var = line.substr(0, pos);
                    if (util::is_arith_operator(var.back())) {
                        assn_opcode = OpCode::from_char(var.back());
                        var.pop_back();
                    }
                    if (util::is_varname(var)) {
                        // Assignment
                        line = line.substr(pos + 1);
                    }
                }
            }

            auto expr = parse(line, env, !do_diff);
            if (do_diff) {
                Expr diff = expr.diff(diff_var_addr, env);
                std::cout << diff.repr(env) << "\n";
            } else {
                double output;
                if (!std::isnan(output = expr(env))) {
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
    }
    return 0;
}
