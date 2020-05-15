#include "parser.hpp"
#include "version.hpp"
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
    std::cout << "Nivalis " NIVALIS_VERSION " " NIVALIS_COPYRIGHT << std::endl;

    std::string orig_line, line, var;
    std::vector<std::string> def_fn_args;
    bool def_fn;
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
            def_fn_args.clear();
            def_fn = false;
            assn_opcode = OpCode::bsel;
            if (util::is_varname_first(line[0])) {
                size_t pos = util::find_equality(line);
                if (~pos) {
                    var = line.substr(0, pos);
                    util::trim(var);
                    if (util::is_arith_operator(var.back())) {
                        assn_opcode = OpCode::from_char(var.back());
                        var.pop_back();
                    }
                    if (var.back() == ')') {
                        auto brpos = var.find('(');
                        if (brpos != std::string::npos) {
                            def_fn = true;
                            size_t prev_comma = brpos + 1;
                            for (size_t i = brpos + 1; i < var.size(); ++i) {
                                if (var[i] == ',' || var[i] == ')') {
                                    def_fn_args.push_back(var.substr(
                                                prev_comma, i - prev_comma));
                                    util::trim(def_fn_args.back());
                                    prev_comma = i + 1;
                                }
                            }
                            var = var.substr(0, brpos);
                            util::rtrim(var);
                            // Function def
                            line = line.substr(pos + 1);
                        }
                    } else if (util::is_varname(var)) {
                        // Assignment
                        line = line.substr(pos + 1);
                    }
                }
            }

            for (size_t i = 0; i < def_fn_args.size(); ++i) {
                // Pre-register variables
                env.addr_of(def_fn_args[i], false);
            }
            auto expr = parse(line, env, !(do_diff || do_optim));
            if (do_optim) {
                expr.optimize();
                std::cout << expr.repr(env) << "\n";
            }
            if (do_diff) {
                Expr diff = expr.diff(diff_var_addr, env);
                std::cout << diff.repr(env) << "\n";
            } else {
                double output;
                // Assignment statement
                if (var.size()) {
                    if (def_fn) {
                        // Define function
                        std::vector<uint64_t> bindings;
                        for (size_t i = 0; i < def_fn_args.size(); ++i) {
                            bindings.push_back(env.addr_of(def_fn_args[i]));
                        }
                        auto addr = env.def_func(var, expr, bindings);
                        std::cout << var << "(";
                        for (size_t i = 0; i < def_fn_args.size(); ++i) {
                            if (i) std::cout << ", ";
                            std::cout << "$" << i;
                        }
                        std::cout << ") = "  <<
                            env.funcs[addr].expr.repr(env) << std::endl;

                    } else {
                        // Define variable
                        double var_val;
                        if (assn_opcode != OpCode::bsel) {
                            // Operator assignment
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
                            // Usual assignment
                            var_val = output;
                        }
                        env.set(var, var_val);
                        std::cout << var << " = " << var_val << std::endl;
                    }
                } else if (!std::isnan(output = expr(env))) {
                    std::cout << output << std::endl;
                }
            }
        }
    }
    return 0;
}
