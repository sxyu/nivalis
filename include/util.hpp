#pragma once
#ifndef _UTIL_H_4EB09B11_F909_45C4_AD5D_8AA7A6644106
#define _UTIL_H_4EB09B11_F909_45C4_AD5D_8AA7A6644106
#include <string>
#include <vector>
namespace nivalis {
namespace util {

// atof for part of string (C++ committee fail)
double atof(const char* p, const char* end);

// true if is literal char (a-zA-Z0-9_$')
constexpr bool is_literal(char c) {
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == '$' ||
        c == '\'' || c == '.' || c == '#';
}

// true if can be variable name first char
constexpr bool is_varname_first(char c) {
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_' || c == '$' || c == '\'';
}

// true if is comparison operator character
constexpr bool is_comp_operator(char c) {
    return c == '>' || c == '=' || c == '<';
}
// true if is arithmetic operator character
constexpr bool is_arith_operator(char c) {
    return c == '+' || c == '-' || c == '*' ||
           c == '/' || c == '%' || c == '^';
}

// true if is operator character
constexpr bool is_operator(char c) {
    return is_comp_operator(c) || is_arith_operator(c);
}

// true if is open bracket
constexpr bool is_open_bracket(char c) {
    return c == '(' || c == '[' || c == '{';
}
// true if is close bracket
constexpr bool is_close_bracket(char c) {
    return c == ')' || c == ']' || c == '}';
}

// true if is bracket
constexpr bool is_bracket(char c) {
    return is_open_bracket(c) || is_close_bracket(c);
}

// checks if string is valid variable name
bool is_varname(const std::string& expr);
// returns position of = in equality, or -1 else
// where = must:
// 1. not be at index 0 or expr.size()-1
// 2. at top bracket level wrt ([{
// 3. not be followed/preceded by any comparison operator
size_t find_equality(const std::string& expr);

// string trimming/strip
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);

// Put double in uint32_t vector
void push_dbl(std::vector<uint32_t>& v, double value);
// Read 2 uint32_t as a double
double as_double(const uint32_t* ast);

}  // namespace util
}  // namespace nivalis
#endif // ifndef _UTIL_H_4EB09B11_F909_45C4_AD5D_8AA7A6644106
