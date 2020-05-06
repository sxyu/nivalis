#include "parser.hpp"

#include<iostream>
#include<vector>
#include<map>
#include<string>
#include<cmath>
#include "util.hpp"
namespace nivalis {

namespace {
    // Constants, lookup tables
    bool is_bracket[256], is_operator[256];
    char lb[] = "([{", rb[] = ")]}";
    std::map<std::string, uint32_t> func_opcodes;

#define RETURN_IF_FALSE(todo) if(!todo) return false;
}  // namespace

struct ParseSession {
    enum Priority {
        PRI_COMPARISON,
        PRI_ADD_SUB,
        PRI_MUL_DIV,
        PRI_POW,
        PRI_UNARY_PLUS_MINUS,
        PRI_BRACKETS,
        _PRI_COUNT,
        _PRI_LOWEST = 0
    };

    // expr: expression to parse
    // env: parsing environment (to check/define variables)
    // mode_explicit: if true, errors when encounters undefined variable;
    //                else defines it
    ParseSession(const std::string& expr, Environment& env)
        : env(env), expr(expr) { }

    Expr parse(bool mode_explicit = true) {
        this->mode_explicit = mode_explicit;
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
                        std::cout << "Unmatched '" << rb[i] << "'\n";
                        return false;
                    }
                    tok_link[starts[i].back()] = pos;
                    tok_link[pos] = starts[i].back();
                    starts[i].pop_back();
                }
            }
        }
        for (int i = 0; i < 3; ++i) {
            if (!starts[i].empty()) {
                std::cout << "Unmatched '" << lb[i] << "'\n";
                return false;
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

    bool _parse(int64_t left, int64_t right, int pri) {
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
                        RETURN_IF_FALSE(_parse(left, i - off, pri));
                        return _parse(i + 1, right, pri + 1);
                    }
                    else if (~tok_link[i]) {
                        i = tok_link[i];
                    }
                }

            case PRI_ADD_SUB:
                for (int64_t i = right - 1; i >= left; --i) {
                    const char c = expr[i];
                    if ((c == '+' || c == '-') && 
                            i > left &&
                            !is_operator[expr[i-1]]) {
                        result.ast.push_back(c == '+' ? OpCode::add :
                                OpCode::sub);
                        RETURN_IF_FALSE(_parse(left, i, pri));
                        return _parse(i + 1, right, pri + 1);
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
                        RETURN_IF_FALSE(_parse(left, i, pri));
                        return _parse(i + 1, right, pri + 1);
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
                        RETURN_IF_FALSE(_parse(left, i, pri + 1));
                        return _parse(i + 1, right, pri);
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
            case PRI_BRACKETS:
                const char c = expr[left], cr = expr[right-1];
                if (left >= right) {
                    std::cout << "Syntax error\n";
                    return false;
                }
                if ((c == '(' && cr == ')') ||
                    (c == '[' && cr == ']')) {
                    // Parentheses
                    return _parse(left + 1, right - 1, _PRI_LOWEST);
                }
                if (c == '{' && cr == '}') {
                    // Conditional clause
                    // Special handling
                    int64_t stkh = 0, last_begin = left+1;
                    bool last_colon = false;
                    for (int64_t i = left + 1; i < right - 1; ++i) {
                        const char cc = expr[i];
                        if (cc == '{' || cc == '(' || cc == ']') {
                            ++stkh;
                        }
                        else if (cc == '}' || cc == ')' || cc == ']') {
                            --stkh;
                        }
                        else if (stkh == 0) {
                            if (cc == ':') {
                                if (last_colon) {
                                    std::cout << "Syntax error: consecutive : "
                                        "in conditional clause\n";
                                    return false;
                                }
                                result.ast.push_back(OpCode::bnz);
                                RETURN_IF_FALSE(_parse(last_begin, i, _PRI_LOWEST));
                                last_begin = i+1;
                                while(std::isspace(expr[last_begin])) ++last_begin;
                                last_colon = true;
                            }
                            else if (cc == ',') {
                                if (!last_colon && i > left+1) {
                                    std::cout << "Syntax error: consecutive , "
                                        "in conditional clause\n";
                                    return false;
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
                else if (cr == ')' && ~tok_link[left]) {
                    // Function
                    int64_t funname_end = tok_link[left] + 1;
                    if (expr[funname_end] != '(') {
                        std::cout << "Invalid function call syntax\n";
                        return false;
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
                        std::cout << "Unrecognized function '" << func_name << "'\n";
                        return false;
                    }
                }
                else if ((c >= '0' && c <= '9') || c == '.') {
                    // Number
                    std::string tmp = expr.substr(left, right - left);
                    char* endptr;
                    util::push_dbl(result.ast, std::strtod(
                                tmp.c_str(), &endptr));
                    if (endptr != tmp.c_str() + (right-left)) {
                        std::cout << "Numeric parsing failed on '" << tmp << "'\n";
                        return false;
                    }
                    return true;
                } else if (util::is_varname_first(c)) {
                    // Variable name
                    result.ast.resize(result.ast.size() + 2, OpCode::ref);
                    const std::string varname =
                        expr.substr(left, right - left);
                    if ((result.ast.back()
                                = env.addr_of(varname, mode_explicit)) == -1) {
                        std::cout << "Undefined variable \"" << varname <<
                                    "\" (parser in explicit mode)\n";
                        return false;
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
                        std::cout << "Undefined variable \"" << varname <<
                                    "\", cannot use as constant (parser in explicit mode)\n";
                        return false;
                    }
                } else {
                        std::cout << "Unrecognized literal '" <<
                            expr.substr(left, right - left) << "'\n";
                    return false;
                }
        }
        return _parse(left, right, pri+1);
        return true;
    }
    Environment& env;
    const std::string& expr;
    Expr result;
    bool mode_explicit;

    // 'Token links':
    // Pos. of last char in token if at first char
    // Pos. of first char in token if at last char
    // -1 else
    std::vector<int64_t> tok_link;
};

Parser::Parser(){
    if (!is_bracket[lb[0]]){
        // Set lookup tables
        for (size_t i = 0; i < sizeof lb; ++i) {
            is_bracket[lb[i]] = true;
            is_bracket[rb[i]] = true;
        }
        is_operator['>'] = is_operator['='] = is_operator['<'] =
        is_operator['+'] = is_operator['-'] = is_operator['*'] =
        is_operator['/'] = is_operator['%'] = is_operator['^'] = true;

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

        func_opcodes["print"] = OpCode::print;
        func_opcodes["printchar"] = OpCode::printc;
    }
}

Expr Parser::operator()(const std::string& expr, Environment& env, bool mode_explicit) const {
    if (expr.empty()) return Expr();
    ParseSession sess(expr, env);
    return sess.parse(mode_explicit);
}

}  // namespace nivalis
