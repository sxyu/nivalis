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

namespace {
    // Constants, lookup tables
    char lb[] = "([{", rb[] = ")]}";
    std::map<std::string, uint32_t> func_opcodes;

#define RETURN_IF_FALSE(todo) if(!todo) return false;
#define PARSE_ERR(toprint) do { \
     error_msg_stream.str("");\
     error_msg_stream << toprint;\
     error_msg = error_msg_stream.str(); \
     if(!quiet) std::cout << error_msg; \
     return false; \
   } while(false)
}  // namespace

struct ParseSession {
    enum Priority {
        PRI_COMPARISON,
        PRI_ADD_SUB,
        PRI_MUL_DIV,
        PRI_UNARY_PLUS_MINUS,
        PRI_POW,
        PRI_BRACKETS,
        _PRI_COUNT,
        _PRI_LOWEST = 0
    };

    // expr: expression to parse
    // env: parsing environment (to check/define variables)
    // mode_explicit: if true, errors when encounters undefined variable;
    //                else defines it
    ParseSession(const std::string& expr, Environment& env, std::string& error_msg,
                 bool mode_explicit, bool quiet)
        : env(env), expr(expr), error_msg(error_msg), mode_explicit(mode_explicit), quiet(quiet) { }

    Expr parse() {
        tok_link.resize(expr.size(), -1);
        result.ast.clear();
        if (!_mk_tok_link() ||
            !_parse(0, static_cast<int64_t>(expr.size()), _PRI_LOWEST)) {
            result.ast.clear();
            result.ast.resize(1, OpCode::dead);
        }
        return result;
    }

private:
    // Make token link table
    bool _mk_tok_link() {
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
        while (std::isspace(expr[right-1])) --right;
        while (std::isspace(expr[left])) ++left;
        switch(pri) {
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
                    else if (~tok_link[i]) {
                        i = tok_link[i];
                    }
                }

            case PRI_ADD_SUB:
                for (int64_t i = right - 1; i >= left; --i) {
                    const char c = expr[i];
                    if ((c == '+' || c == '-') && 
                            i > left && !util::is_operator(expr[i-1])) {
                        result.ast.push_back(c == '+' ? OpCode::add :
                                OpCode::sub);
                        return _parse(left, i, pri) && _parse(i + 1, right, pri + 1);
                    }
                    else if (~tok_link[i]) {
                        i = tok_link[i];
                    }
                }
                break;
            case PRI_MUL_DIV:
                for (int64_t i = right - 1; i >= left; --i) {
                    const char c = expr[i];
                    if (c == '*' || c == '/' || c == '%') {
                        result.ast.push_back(c == '*' ? OpCode::mul : (
                                    c == '/' ? OpCode::div : OpCode::mod));
                        return _parse(left, i, pri) && _parse(i + 1, right, pri + 1);
                    }
                    else if (~tok_link[i]) {
                        i = tok_link[i];
                    }
                }
                break;
            case PRI_UNARY_PLUS_MINUS:
                for (int64_t i = left; i < right; ++i) {
                    const char c = expr[i];
                    if (c == '+' || c == '-') {
                        result.ast.push_back(c == '+' ? OpCode::nop : OpCode::uminusb);
                        return _parse(i + 1, right, pri);
                    }
                    else if (~tok_link[i]) {
                        i = tok_link[i];
                    }
                }
                break;
            case PRI_POW:
                for (int64_t i = left; i < right; ++i) {
                    const char c = expr[i];
                    if (c == '^') {
                        result.ast.push_back(OpCode::power);
                        return _parse(left, i, pri + 1) && _parse(i + 1, right, pri);
                    }
                    else if (~tok_link[i]) {
                        i = tok_link[i];
                    }
                }
                break;
            case PRI_BRACKETS:
                const char c = expr[left], cr = expr[right-1];
                if (left >= right) {
                    PARSE_ERR("Syntax error\n");
                }
                if ((c == '(' && cr == ')') ||
                    (c == '[' && cr == ']')) {
                    // Parentheses
                    return _parse(left + 1, right - 1, _PRI_LOWEST);
                }
                if (c == '{' && cr == '}') {
                    // Conditional clause
                    int64_t stkh = 0, last_begin = left+1;
                    bool last_colon = false;
                    for (int64_t i = left + 1; i < right - 1; ++i) {
                        const char cc = expr[i];
                        if (util::is_open_bracket(cc)) {
                            ++stkh;
                        }
                        else if (util::is_close_bracket(cc)) {
                            --stkh;
                        }
                        else if (stkh == 0) {
                            if (cc == ':') {
                                if (last_colon) {
                                    PARSE_ERR("Syntax error: consecutive : "
                                              "in conditional clause\n");
                                }
                                result.ast.push_back(OpCode::bnz);
                                RETURN_IF_FALSE(_parse(last_begin, i, _PRI_LOWEST));
                                last_begin = i+1;
                                while(std::isspace(expr[last_begin])) ++last_begin;
                                last_colon = true;
                            }
                            else if (cc == ',') {
                                if (!last_colon && i > left+1) {
                                    PARSE_ERR("Syntax error: consecutive , "
                                              "in conditional clause\n");
                                }
                                RETURN_IF_FALSE(_parse(last_begin, i, _PRI_LOWEST));
                                last_begin = i+1;
                                while(std::isspace(expr[last_begin])) ++last_begin;
                                last_colon = false;
                            }
                        }
                    }
                    RETURN_IF_FALSE(_parse(last_begin, right - 1, _PRI_LOWEST));
                    if (last_colon) {
                        result.ast.push_back(OpCode::null);
                    }
                    return true;
                }
                else if (cr == ']' && ~tok_link[left] &&
                        tok_link[left]+1 < expr.size() &&
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
                    for (size_t k = funname_end+1; k < arg_end - 1; ++k) {
                        if (expr[k] == ':') var_end = k;
                        else if (~var_end && expr[k] == ',') comma_pos = k;
                    }
                    if (func_name == "sum" || func_name == "prod") {
                        if (!~var_end || !~comma_pos) {
                            PARSE_ERR(func_name << " expected argument syntax "
                                       "(<var>:<begin>,<end>)\n");
                        }
                        result.ast.push_back(func_name[0] == 's' ? 
                                             OpCode::sums : OpCode::prods);
                        std::string varname =
                            expr.substr(funname_end + 1, var_end -
                                   funname_end-1);
                        result.ast.push_back(env.addr_of(varname, false));
                        RETURN_IF_FALSE(
                                _parse(var_end+1, comma_pos, _PRI_LOWEST));
                        RETURN_IF_FALSE(
                                _parse(comma_pos+1, arg_end-1, _PRI_LOWEST));
                        return _parse(arg_end + 1, right - 1, _PRI_LOWEST);
                    } else {
                        PARSE_ERR("Unrecognized special form '" << func_name << "'\n");
                    }
                }
                else if (cr == ')' && ~tok_link[left]) {
                    // Function
                    int64_t funname_end = tok_link[left] + 1;
                    if (expr[funname_end] != '(') {
                        PARSE_ERR("Invalid function call syntax\n");
                    }
                    const std::string func_name = expr.substr(left, funname_end - left);
                    auto it = func_opcodes.find(func_name);
                    if (it != func_opcodes.end()) {
                        result.ast.push_back(it->second);
                        int64_t stkh = 0, last_begin = funname_end + 1;
                        for (int64_t i = funname_end + 1; i < right - 1; ++i) {
                            const char cc = expr[i];
                            if (cc == '(') {
                                ++stkh;
                            } else if (cc == ')') {
                                --stkh;
                            } else if (cc == ',') {
                                if (stkh == 0) {
                                    RETURN_IF_FALSE(_parse(last_begin, i, _PRI_LOWEST));
                                    last_begin = i+1;
                                }
                            }
                        }
                        return _parse(last_begin, right-1, _PRI_LOWEST);
                    } else {
                        PARSE_ERR("Unrecognized function '" << func_name << "'\n");
                    }
                }
                else if ((c >= '0' && c <= '9') || c == '.') {
                    // Number
                    std::string tmp = expr.substr(left, right - left);
                    char* endptr;
                    util::push_dbl(result.ast, std::strtod(
                                tmp.c_str(), &endptr));
                    if (endptr != tmp.c_str() + (right-left)) {
                        PARSE_ERR("Numeric parsing failed on '"
                                  << tmp << "'\n");
                    }
                    return true;
                } else if (util::is_varname_first(c) && 
                           util::is_literal(cr)) {
                    // Variable name
                    result.ast.resize(result.ast.size() + 2, OpCode::ref);
                    const std::string varname =
                        expr.substr(left, right - left);
                    if ((result.ast.back()
                                = env.addr_of(varname, mode_explicit)) == -1) {
                        PARSE_ERR("Undefined variable \"" << varname + "\"\n");
                    }
                    return true;
                } else if (c == '#' &&
                           util::is_varname_first(expr[left + 1])) {
                    // Embed variable value as constant
                    const std::string varname =
                        expr.substr(left + 1, right - left - 1);
                    auto idx = env.addr_of(varname, true);
                    if (~idx) {
                        util::push_dbl(result.ast, env.vars[idx]);
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
        return true;
    }
    Environment& env;
    const std::string& expr;
    std::string& error_msg;
    std::stringstream error_msg_stream;
    Expr result;
    bool mode_explicit, quiet;

    // 'Token links':
    // Pos. of last char in token if at first char
    // Pos. of first char in token if at last char
    // -1 else
    std::vector<int64_t> tok_link;
};

Parser::Parser(){
    if (func_opcodes.empty()){
        // Set lookup tables
        func_opcodes["pow"] = OpCode::power;
        func_opcodes["log"] = OpCode::logbase;
        func_opcodes["max"] = OpCode::max;
        func_opcodes["min"] = OpCode::min;
        func_opcodes["and"] = OpCode::land;
        func_opcodes["or"] = OpCode::lor;
        func_opcodes["xor"] = OpCode::lxor;

        func_opcodes["abs"] = OpCode::absb;
        func_opcodes["sqrt"] = OpCode::sqrtb;
        func_opcodes["sgn"] = OpCode::sgnb;
        func_opcodes["floor"] = OpCode::floorb;
        func_opcodes["ceil"] = OpCode::ceilb;
        func_opcodes["round"] = OpCode::roundb;

        func_opcodes["exp"] = OpCode::expb;
        func_opcodes["exp2"] = OpCode::exp2b;
        func_opcodes["ln"] = OpCode::logb;
        func_opcodes["log10"] = OpCode::log10b;
        func_opcodes["log2"] = OpCode::log2b;

        func_opcodes["sin"] = OpCode::sinb;
        func_opcodes["cos"] = OpCode::cosb;
        func_opcodes["tan"] = OpCode::tanb;
        func_opcodes["asin"] = OpCode::asinb;
        func_opcodes["acos"] = OpCode::acosb;
        func_opcodes["atan"] = OpCode::atanb;

        func_opcodes["sinh"] = OpCode::sinhb;
        func_opcodes["cosh"] = OpCode::coshb;
        func_opcodes["tanh"] = OpCode::tanhb;

        func_opcodes["gamma"] = OpCode::gammab;
        func_opcodes["fact"] = OpCode::factb;
        func_opcodes["choose"] = OpCode::choose;
        func_opcodes["fafact"] = OpCode::fafact;

        func_opcodes["print"] = OpCode::print;
        func_opcodes["printchar"] = OpCode::printc;
    }
}

Expr Parser::operator()(const std::string& expr, Environment& env,
                        bool mode_explicit, bool quiet) const {
    error_msg.clear();
    if (expr.empty()) return Expr();
    ParseSession sess(expr, env, error_msg, mode_explicit, quiet);
    return sess.parse();
}

}  // namespace nivalis
