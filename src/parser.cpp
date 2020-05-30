#include "parser.hpp"

#include<iostream>
#include<vector>
#include<map>
#include<string>
#include<sstream>
#include<cmath>
#include<cctype>
#include "util.hpp"
namespace nivalis {

    // Set error message and return false
#define PARSE_ERR(errmsg) do { \
     std::stringstream error_msg_stream; \
     error_msg_stream << errmsg;\
     if (error_msg) error_msg->append(error_msg_stream.str()); \
     if(!quiet) std::cout << errmsg; \
     return false; \
   } while(false)

struct ParseSession {
    enum Priority {
        PRI_OR,
        PRI_AND,
        PRI_COMPARISON,
        PRI_ADD_SUB,
        PRI_MUL_DIV,
        PRI_POW,
        PRI_BRACKETS,
        _PRI_COUNT,
        _PRI_LOWEST = 0
    };

    // expr: expression to parse
    // env: parsing environment (to check/define variables)
    // mode_explicit: if true, errors when encounters undefined variable;
    //                else defines it
    ParseSession(const std::string& expr, Environment& env, std::string* error_msg,
                 bool mode_explicit, bool quiet, size_t max_args)
        : env(env), expr(expr), error_msg(error_msg),
            mode_explicit(mode_explicit), quiet(quiet), max_args(max_args) {
    }

    Expr parse() {
        tok_link.resize(expr.size(), -1);
        result.ast.clear();
        if (!_mk_tok_link() ||
            !_parse(0, static_cast<int64_t>(expr.size()), _PRI_LOWEST)) {
            result.ast.clear();
            result.ast.resize(1);
        }
        return result;
    }

private:
    // Make token link table
    bool _mk_tok_link() {
        // Parenthesis matching
        static const char lb[] = "([{", rb[] = ")]}";
        std::vector<int64_t> starts[3];
        int64_t lit_start = -1;
        for (int64_t pos = 0; pos < static_cast<int64_t>(expr.size()); ++pos) {
            const char c = expr[pos];
            if (util::is_literal(c)) {
                if (lit_start == -1) lit_start = pos;
            } else {
                if (lit_start != -1) {
                    tok_link[lit_start] = pos-1;
                    tok_link[pos-1] = lit_start;
                    lit_start = -1;
                }
            }
            for (int i = 0; i < 3; ++i) {
                if (c == lb[i]) {
                    starts[i].push_back(pos);
                } else if (c == rb[i]) {
                    if (starts[i].empty()) {
                        PARSE_ERR("Unmatched '" << rb[i] << "'\n");
                    }
                    tok_link[starts[i].back()] = pos;
                    tok_link[pos] = starts[i].back();
                    starts[i].pop_back();
                }
            }
        }
        for (int i = 0; i < 3; ++i) {
            if (!starts[i].empty()) {
                PARSE_ERR("Unmatched '" << lb[i] << "'\n");
            }
        }
        if (lit_start != -1) {
            tok_link[lit_start] = static_cast<int64_t>(expr.size()) - 1;
            tok_link[expr.size() - 1] = lit_start;
            lit_start = -1;
        }
        lit_start = -1;
        return true;
    }

    // Recursive parse procedure
    bool _parse(int64_t left, int64_t right, int pri) {
        // Remove spaces
        while (std::isspace(expr[right-1])) --right;
        while (std::isspace(expr[left])) ++left;
        switch(pri) {
            case PRI_AND: case PRI_OR: case PRI_POW:
                {
                    if (pri == PRI_POW) {
                        // Deal with unary +-
                        while (left < right &&
                                (expr[left] == '+' || expr[left] == '-')) {
                            if (expr[left] == '-')
                                result.ast.push_back(OpCode::unaryminus);
                            ++left;
                        }
                    }
                    const uint32_t opc =
                        pri == PRI_AND ? OpCode::land :
                        pri == PRI_OR ? OpCode::lor :
                        OpCode::power;
                    const char pat = OpCode::to_char(opc);
                    for (int64_t i = left; i < right; ++i) {
                        const char c = expr[i];
                        if (c == pat) {
                            result.ast.push_back(opc);
                            return _parse(left, i, pri + 1) &&
                                   _parse(i + 1, right, pri);
                        }
                        else if (tok_link[i] > i) {
                            i = tok_link[i];
                        }
                    }
                }
                break;
            case PRI_COMPARISON:
                for (int64_t i = right - 1; i >= left; --i) {
                    const char c = expr[i];
                    if ((c == '<' || c == '>' || c == '=')) {
                        uint32_t opcode;
                        int64_t off = 0;
                        if (c == '='){
                            if (i > left) {
                                const char pre_c = expr[i-1];
                                off = 1;
                                switch(pre_c) {
                                    case '!': opcode = OpCode::ne; break;
                                    case '=': opcode = OpCode::eq; break;
                                    case '<': opcode = OpCode::le; break;
                                    case '>': opcode = OpCode::ge; break;
                                    default:
                                     off = 0;
                                     opcode = OpCode::eq;
                                }
                            } else opcode = OpCode::eq;
                        } else {
                            opcode = c == '<' ? OpCode::lt : OpCode::gt;
                        }
                        result.ast.push_back(opcode);
                        return _parse(left, i - off, pri) &&
                               _parse(i + 1, right, pri + 1);
                    }
                    else if (~tok_link[i] && tok_link[i] < i) {
                        i = tok_link[i];
                    }
                }
                break;
            case PRI_ADD_SUB:
                for (int64_t i = right - 1; i >= left; --i) {
                    const char c = expr[i];
                    if ((c == '+' || c == '-') &&
                            i > left && !util::is_operator(expr[i-1])) {
                        result.ast.push_back(c == '+' ? OpCode::add :
                                OpCode::sub);
                        return _parse(left, i, pri) && _parse(i + 1, right, pri + 1);
                    }
                    else if (~tok_link[i] && tok_link[i] < i) {
                        i = tok_link[i];
                    }
                }
                break;
            case PRI_MUL_DIV:
                for (int64_t i = right - 1; i >= left; --i) {
                    const char c = expr[i];
                    if (c == '*' || c == '/' || c == '%') {
                        result.ast.push_back(c == '*' ? OpCode::mul : (
                                    c == '/' ? OpCode::divi : OpCode::mod));
                        return _parse(left, i, pri) && _parse(i + 1, right, pri + 1);
                    }
                    else if (~tok_link[i] && tok_link[i] < i) {
                        i = tok_link[i];
                    }
                }
                break;
            case PRI_BRACKETS:
                while (left < right &&
                        (expr[left] == '+' || expr[left] == '-')) {
                    if (expr[left] == '-')
                        result.ast.push_back(OpCode::unaryminus);
                    ++left;
                }
                const char c = expr[left], cr = expr[right-1];
                if (left >= right) {
                    PARSE_ERR("Syntax error\n");
                }
                if ((c == '(' && cr == ')') ||
                    (c == '[' && cr == ']')) {
                    // Parentheses
                    if (tok_link[left] != right - 1) {
                        PARSE_ERR("Syntax error '" <<
                                expr.substr(left, right - left) <<
                                "'\n");;
                    }
                    return _parse(left + 1, right - 1, _PRI_LOWEST);
                }
                if (c == '{' && cr == '}') {
                    // Conditional clause
                    int64_t stkh = 0, last_begin = left+1;
                    bool last_colon = false;
                    size_t thunk_beg;
                    size_t nest_depth = 0;
                    for (int64_t i = left + 1; i < right - 1; ++i) {
                        const char cc = expr[i];
                        if (util::is_open_bracket(cc)) {
                            ++stkh;
                        } else if (util::is_close_bracket(cc)) {
                            --stkh;
                        } else if (stkh == 0) {
                            if (cc == ':') {
                                if (last_colon) {
                                    PARSE_ERR("Syntax error: consecutive : "
                                              "in conditional clause\n");
                                }
                                result.ast.push_back(OpCode::bnz);
                                if (!_parse(last_begin, i, _PRI_LOWEST)) return false;
                                last_begin = i+1;
                                while(std::isspace(expr[last_begin]))
                                    ++last_begin;
                                last_colon = true;
                                begin_thunk();
                            }
                            else if (cc == ',') {
                                if (!last_colon && i > left+1) {
                                    PARSE_ERR("Syntax error: consecutive , "
                                              "in conditional clause\n");
                                }
                                if (!_parse(last_begin, i, _PRI_LOWEST)) return false;
                                end_thunk();
                                begin_thunk();
                                ++nest_depth;
                                last_begin = i+1;
                                while(std::isspace(expr[last_begin])) ++last_begin;
                                last_colon = false;
                            }
                        }
                    }
                    if (!_parse(last_begin, right - 1, _PRI_LOWEST)) return false;
                    if (last_colon) {
                        thunk_beg = result.ast.size();
                        end_thunk();
                        begin_thunk();
                        result.ast.push_back(OpCode::null);
                        end_thunk();
                    }
                    for (size_t t = 0; t < nest_depth; ++t) {
                        end_thunk();
                    }
                    return true;
                }
                else if (cr == ']' && ~tok_link[left] &&
                        tok_link[left]+1 < static_cast<int64_t>(expr.size()) &&
                         (expr[tok_link[left]+1] == '(' ||
                          expr[tok_link[left]+1] == '[')) {
                    // Special form
                    int64_t funname_end = tok_link[left] + 1;
                    int64_t arg_end =
                        expr[tok_link[left]+1] == '(' ?
                        tok_link[tok_link[left]+1] + 1 :
                        tok_link[left] + 1;
                    if (expr[arg_end] != '[') {
                        PARSE_ERR("Expected '[' after special form argument\n");
                    }
                    const std::string func_name = expr.substr(left, funname_end - left);

                    size_t var_end = -1, comma_pos = -1;
                    for (size_t k = funname_end+1; k < static_cast<size_t>(arg_end - 1); ++k) {
                        if (expr[k] == ':') var_end = k;
                        else if (~var_end && expr[k] == ',') comma_pos = k;
                    }
                    if (func_name == "sum" || func_name == "prod") {
                        // Sum/prod special forms
                        if (!~var_end || !~comma_pos) {
                            PARSE_ERR(func_name << " expected argument syntax "
                                       "(<var>:<begin>,<end>)\n");
                        }
                        result.ast.push_back(func_name[0] == 's' ?
                                             OpCode::sums : OpCode::prods);
                        std::string varname =
                            expr.substr(funname_end + 1, var_end -
                                   funname_end-1);
                        result.ast.back().ref = env.addr_of(varname, false);
                        if (!_parse(var_end+1, comma_pos, _PRI_LOWEST)) return false;

                        if (!_parse(comma_pos+1, arg_end-1, _PRI_LOWEST)) return false;
                        begin_thunk();
                        if (!_parse(arg_end + 1, right - 1, _PRI_LOWEST)) return false;
                        end_thunk();
                        return true;
                    } else if (func_name.size() >= 4 &&
                               func_name.substr(0, 4) == "diff") {
                        // Derivative special form
                        std::string ord_str = func_name.substr(4);
                        int ord = 1;
                        if (ord_str.size()) {
                            if (!util::is_whole_number(ord_str)) {
                                ord = -1;
                            } else {
                                ord = std::atoi(ord_str.c_str());
                            }
                            if (ord > 5) {
                                PARSE_ERR("Derivative order is too high\n");
                            }
                        }
                        if (ord >= 0) {
                            std::string varname;
                            if (~var_end) varname = expr.substr(funname_end + 1, var_end - funname_end - 1);
                            else varname = expr.substr(funname_end + 1, arg_end - funname_end-2);
                            if (varname.empty() || !util::is_varname(varname)) {
                                PARSE_ERR(func_name << " expected argument syntax "
                                        "(<var>)\n");
                            }
                            auto addr = env.addr_of(varname, false);
                            decltype(result.ast) tmp;
                            tmp.swap(result.ast);
                            if (!_parse(arg_end + 1, right - 1, _PRI_LOWEST)) return false;
                            Expr diff = result;
                            for (int i = 0; i < ord; ++i) {
                                diff = diff.diff(addr, env);
                                diff.optimize();
                            }
                            tmp.swap(result.ast);
                            size_t sz = result.ast.size();
                            result.ast.resize(sz + diff.ast.size());
                            std::copy(diff.ast.begin(), diff.ast.end(), result.ast.begin() + sz);
                            return true;
                        }
                    }
                    PARSE_ERR("Unrecognized special form '" << func_name << "'\n");
                }
                else if (cr == ')' && ~tok_link[left]) {
                    // Function
                    int64_t funname_end = tok_link[left] + 1;
                    if (expr[funname_end] != '(') {
                        PARSE_ERR("Invalid function call syntax\n");
                    }
                    const std::string func_name =
                        expr.substr(left, funname_end - left);

                    uint32_t func_opcode = -1;
                    size_t expected_argcount = -1;

                    // First look at user-defined functions
                    auto func_addr = env.addr_of_func(func_name);
                    if (func_addr != -1) {
                        expected_argcount = env.funcs[func_addr].n_args;
                        // Add call
                        result.ast.push_back(Expr::ASTNode::call((uint32_t) func_addr,
                                    (uint32_t)expected_argcount));
                    } else {
                        // Else, look at built-in functions
                        const auto& func_opcodes = OpCode::funcname_to_opcode_map();
                        auto it = func_opcodes.find(func_name);
                        if (it != func_opcodes.end()) {
                            func_opcode = it->second;
                            if (func_opcode == -1) {
                                // Special handling (pseudo instruction)
                                if (func_name[0] == 'f') {
                                    result.ast.push_back(OpCode::tgammab);
                                    result.ast.push_back(OpCode::add);
                                    result.ast.push_back(1.0);
                                } else if (func_name[0] == 'N') {
                                    result.ast.push_back(OpCode::mul);
                                    result.ast.push_back(1. / sqrt(2* M_PI));
                                    result.ast.push_back(OpCode::expb);
                                    result.ast.push_back(OpCode::mul);
                                    result.ast.push_back(-0.5);
                                    result.ast.push_back(OpCode::sqrb);
                                }
                            } else {
                                result.ast.push_back(func_opcode);
                            }
                            expected_argcount = OpCode::n_args(func_opcode);
                        }
                    }
                    if (~expected_argcount) {
                        // Valid function, process args
                        int64_t stkh = 0, last_begin = funname_end + 1;
                        size_t argcount = 0, non_space_count = 0;
                        for (int64_t i = funname_end + 1; i < right - 1; ++i) {
                            const char cc = expr[i];
                            if (cc == '(') {
                                ++stkh;
                            } else if (cc == ')') {
                                --stkh;
                            } else if (cc == ',') {
                                if (stkh == 0) {
                                    if (!_parse(last_begin, i, _PRI_LOWEST)) return false;
                                    last_begin = i+1;
                                    ++argcount;
                                }
                            } else if (!std::isspace(cc)) {
                                ++non_space_count;
                            }
                        }
                        if (non_space_count) {
                            ++argcount;
                            if (!_parse(last_begin, right-1, _PRI_LOWEST)) return false;
                        } // else: function call with no args
                        if (argcount != expected_argcount) {
                            if (func_opcode == OpCode::logbase &&
                                    argcount == 1) {
                                // log: use ln if only 1 arg (HACK)
                                result.ast.push_back(M_E);
                            } else {
                                PARSE_ERR(func_name << ": wrong number of "
                                        "arguments (expecting " <<
                                        expected_argcount << ")\n");
                                return false;
                            }
                        }
                        return true;
                    } else {
                        PARSE_ERR("Unknown function '" << func_name << "'\n");
                    }
                }
                else if ((c >= '0' && c <= '9') || c == '.') {
                    // Number
                    std::string tmp = expr.substr(left, right - left);
                    char* endptr;
                    result.ast.push_back( // val
                            std::strtod(tmp.c_str(), &endptr));
                    if (endptr != tmp.c_str() + (right-left)) {
                        PARSE_ERR("Numeric parsing failed on '"
                                  << tmp << "'\n");
                    }
                    return true;
                } else if ((util::is_varname_first(c) ||
                            (c=='@' && left < right - 1)) &&
                           util::is_literal(cr)) {
                    // Variable name
                    const std::string varname =
                        expr.substr(left, right - left);
                    uint64_t addr = env.addr_of(varname, true);
                    if (addr == -1) {
                        // If variable not found, try finding function with 0 args
                        addr = env.addr_of_func(varname);
                        if (addr == -1 || env.funcs[addr].n_args != 0) {
                            // If such function not found, look for parse-time constant like pi
                            const auto& constant_values =
                                OpCode::constant_value_map();
                            auto it = constant_values.find(varname);
                            if (it != constant_values.end()) {
                                if (std::isnan(it->second)) {
                                    result.ast.push_back(OpCode::null);
                                } else {
                                    result.ast.push_back(// val
                                            it->second);
                                }
                                return true;
                            }
                            // If no constant not found either,
                            // (1) mode explicit: return error
                            // (2) else: create the variable in env
                            if (!mode_explicit) {
                                result.ast.push_back(Expr::ASTNode(OpCode::ref,
                                            env.addr_of(varname, false)));
                            } else {
                                PARSE_ERR("Undefined variable or 0-argument function \"" << varname + "\"\n");
                            }
                        } else {
                            // Function call
                            result.ast.push_back(Expr::ASTNode::call((uint32_t) addr, uint32_t(0)));
                        }
                    } else {
                        // Variable reference
                        result.ast.push_back(Expr::ASTNode(OpCode::ref, addr));
                    }
                    if (result.ast.back().opcode == OpCode::ref &&
                        result.ast.back().ref >= env.vars.size()) {
                        PARSE_ERR("Internal error: variable address out of bounds \"" <<
                                result.ast.back().ref <<
                                "\"\n");
                        result.ast.back().opcode = OpCode::null;
                    }
                    return true;
                } else if (c == '$' && cr >= '0' && cr <= '9' &&
                           util::is_whole_number(
                               expr.substr(left + 1, right - left - 1))) {
                    // Explicit function argument
                    int64_t idx = std::atoll(expr.substr(left + 1, right - left - 1).c_str());
                    if (idx < 0 || (size_t)idx >= max_args) {
                        PARSE_ERR("Invalid explicit function argument $" << idx << "\n");
                    }
                    result.ast.emplace_back(OpCode::arg, idx);
                    return true;
                } else if (c == '`' &&
                           util::is_varname_first(expr[left + 1])) {
                    // Embed variable value as constant
                    const std::string varname =
                        expr.substr(left + 1, right - left - 1);
                    auto idx = env.addr_of(varname, true);
                    if (~idx) {
                        result.ast.push_back(env.vars[idx]);
                        return true;
                    } else {
                        PARSE_ERR("Undefined variable \"" << varname <<
                                  "\", cannot use as constant\n");
                    }
                } else {
                    PARSE_ERR("Unrecognized literal '" <<
                        expr.substr(left, right - left) << "'\n");
                }
        }
        return _parse(left, right, pri+1);
    }
    Environment& env;
    const std::string& expr;
    std::string* error_msg;
    Expr result;
    bool mode_explicit, quiet;
    size_t max_args;

    // Thunk management helpers
    void begin_thunk() {
        thunks.push_back(result.ast.size());
        result.ast.emplace_back(OpCode::thunk_ret);
    }
    void end_thunk() {
        result.ast.emplace_back(OpCode::thunk_jmp,
                    result.ast.size() - thunks.back());
        thunks.pop_back();
    }

    // Beginnings of thunks
    std::vector<size_t> thunks;

    // 'Token links':
    // Pos. of last char in token if at first char
    // Pos. of first char in token if at last char
    // -1 else
    std::vector<int64_t> tok_link;
};

// Parse an expression
Expr parse(const std::string& expr, Environment& env,
        bool mode_explicit, bool quiet, size_t max_args,
        std::string* error_msg) {
    if (expr.empty()) return Expr();
    // If already error, add newline
    if (error_msg && error_msg->size() && error_msg->back() != '\n') error_msg->push_back('\n');
    if (expr.size() && expr[0] == '#') return Expr::AST(1); // Comment
    ParseSession sess(
            expr, env, error_msg, mode_explicit, quiet, max_args);
    return sess.parse();
}

}  // namespace nivalis
