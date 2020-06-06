#include "shell.hpp"

#include "version.hpp"

#include <vector>
#include <cctype>
#include <cmath>
#include "opcodes.hpp"
#include "env.hpp"
#include "parser.hpp"
#include "util.hpp"

namespace {
std::string get_word(std::string& str_to_parse) {
    size_t i = 0;
    for (; i < str_to_parse.size(); ++i) if (!std::isspace(str_to_parse[i])) break;
    for (; i < str_to_parse.size(); ++i) if (std::isspace(str_to_parse[i])) break;
    std::string word = str_to_parse.substr(0, i);
    str_to_parse = str_to_parse.substr(i);
    return word;
}

std::vector<std::string> get_args(std::string& str_to_parse) {
    std::vector<std::string> args;
    while (str_to_parse.size()) {
        args.push_back(get_word(str_to_parse));
    }
    return args;
}

}  // namespace

namespace nivalis {

Shell::Shell(Environment& env, std::ostream& os) : env(env), os(os) {
    os << "Nivalis " NIVALIS_VERSION " " NIVALIS_COPYRIGHT << std::endl;
}
bool Shell::eval_line(std::string line) {
    int assn_opcode = OpCode::bsel;
    std::string str_to_parse, var;
    std::vector<std::string> def_fn_args;
    bool def_fn;
    util::trim(line);
    str_to_parse = line;
    if (line == "exit") {
        // Exit shell, if applicable
        closed = true;
        return true;
    }

    std::string cmd;
    if (line.size() && line[0] == '%') {
        cmd = get_word(line);
    }
    util::trim(line);
    util::trim(cmd);
    if (cmd == "%del") {
        // Delete variable
        if (env.del(line)) {
            os << "del " << line << std::endl;
        } else {
            os << "Undefined variable " << line << "\n";
            return false;
        }
    } else if (cmd == "%delf") {
        // Delete function
        if (env.del_func(line)) {
            os << "delf " << line << std::endl;
        } else {
            os << "Undefined function " << line << "\n";
            return false;
        }
    } else {
        bool do_optim = cmd == "%" || cmd == "%tex" || cmd == "%niv";
        bool will_use_latex = (use_latex || cmd == "%tex") && cmd != "%niv";
        bool do_diff = cmd == "%diff";
        if (do_optim) str_to_parse = line;
        uint64_t diff_var_addr;
        if (do_diff) {
            std::string diff_var = get_word(line);
            util::trim(diff_var); util::trim(line);
            if (!util::is_varname(diff_var)) {
                os << diff_var << " is not a valid variable name\n";
                return false;
            }
            diff_var_addr = env.addr_of(diff_var, false);
            str_to_parse = line;
        }
        // Evaluate
        def_fn = false;
        assn_opcode = OpCode::bsel;
        if (util::is_varname_first(str_to_parse[0])) {
            size_t pos = util::find_equality(str_to_parse);
            if (~pos) {
                var = str_to_parse.substr(0, pos);
                util::trim(var);
                if (util::is_arith_operator(var.back())) {
                    assn_opcode = OpCode::from_char(var.back());
                    var.pop_back();
                    util::trim(var);
                }
                if (var.back() == ')') {
                    auto brpos = var.find('(');
                    if (brpos != std::string::npos) {
                        // Function def
                        def_fn = true;
                        size_t prev_comma = brpos + 1;
                        for (size_t i = brpos + 1; i < var.size(); ++i) {
                            if (var[i] == ',' || var[i] == ')') {
                                def_fn_args.push_back(var.substr(
                                            prev_comma, i - prev_comma));
                                util::trim(def_fn_args.back());
                                if (var[i] == ')' && def_fn_args.size() == 1 &&
                                    def_fn_args.back().empty()) {
                                    // Function with no arguments
                                    def_fn_args.clear();
                                    break;
                                }
                                if (def_fn_args.back().empty() ||
                                    (def_fn_args.back()[0] != '$' &&
                                    !util::is_varname(def_fn_args.back()))) {
                                    os << "'" << def_fn_args.back() <<
                                        "': invalid argument variable name\n";
                                    def_fn = false;
                                    break;
                                }
                                prev_comma = i + 1;
                            }
                        }
                        var = var.substr(0, brpos);
                        util::rtrim(var);
                        if (def_fn) str_to_parse = str_to_parse.substr(pos + 1);
                    }
                } else if (util::is_varname(var)) {
                    // Assignment
                    str_to_parse = str_to_parse.substr(pos + 1);
                }
            }
        }

        for (size_t i = 0; i < def_fn_args.size(); ++i) {
            // Pre-register variables
            env.addr_of(def_fn_args[i], false);
        }
        std::string parse_err;
        Expr expr = parse(will_use_latex ?  latex_to_nivalis(str_to_parse)
                : str_to_parse, env, !(do_diff || do_optim), // expicit
                true, // quiet
                def_fn_args.size(), // max args
                &parse_err);
        if (parse_err.size()) {
            os << parse_err;
            return false;
        }
        if (do_optim) {
            expr.optimize();
            expr.repr(os, env) << "\n";
        } else if (do_diff) {
            Expr diff = expr.diff(diff_var_addr, env);
            diff.repr(os, env) << "\n";
        } else {
            double output;
            if (def_fn || !std::isnan(output = expr(env))) {
                // Assignment statement
                if (var.size() && parse_err.empty()) {
                    if (def_fn) {
                        // Define function
                        std::vector<uint64_t> bindings;
                        for (size_t i = 0; i < def_fn_args.size(); ++i) {
                            bindings.push_back(
                                    def_fn_args[i][0] == '$' ? -1 :
                                    env.addr_of(def_fn_args[i]));
                        }
                        auto addr = env.def_func(var, expr, bindings);
                        if (~addr) {
                            os << var << "(";
                            for (size_t i = 0; i < def_fn_args.size(); ++i) {
                                if (i) os << ", ";
                                os << "$" << i;
                            }
                            os << ") = ";
                            env.funcs[addr].expr.repr(os, env) << std::endl;
                        }

                    } else {
                        // Define variable
                        double var_val;
                        if (assn_opcode != OpCode::bsel) {
                            // Operator assignment
                            auto addr = env.addr_of(var, true);
                            if (addr == -1) {
                                os << "Undefined variable \"" << var
                                    << "\" (operator assignment)\n";
                                return false;
                            }
                            var_val = Expr::constant(env.vars[addr])
                                .combine(assn_opcode,
                                        Expr::constant(output))(env);
                        } else {
                            // Usual assignment
                            var_val = output;
                        }
                        env.set(var, var_val);
                        os << var << " = " << var_val << std::endl;
                    }
                } else {
                    os << output << std::endl;
                }
            }
        }
    }
    return true;
}
}  // namespace nivalis
