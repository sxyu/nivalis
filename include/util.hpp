#pragma once
#ifndef _UTIL_H_4EB09B11_F909_45C4_AD5D_8AA7A6644106
#define _UTIL_H_4EB09B11_F909_45C4_AD5D_8AA7A6644106
#include <string>
#include <vector>
#include <ostream>
#include <istream>
namespace nivalis {
namespace util {

// true if is literal char (a-zA-Z0-9_$'#&$)
constexpr bool is_literal(char c) {
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == '$' ||
        c == '\'' || c == '.' || c == '#' ||
        c == '&';
}

// true if can be first char of a variable name
constexpr bool is_varname_first(char c) {
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_' || c == '\'';
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
// checks if string is a nonnegative integer (only 0-9)
bool is_whole_number(const std::string& expr);
// returns position of = (or </> if allow_ineq) in string, or -1 else
// where = must:
// 1. not be at index 0 or expr.size()-1
// 2. at top bracket level wrt ([{
// 3. (if enforce_no_adj_comparison)
//     not be followed/preceded by any comparison operator or !
size_t find_equality(const std::string& expr,
                     bool allow_ineq = false,
                     bool enforce_no_adj_comparison = true);

// string trimming/strip
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);

// Put double in uint32_t vector
void push_dbl(std::vector<uint32_t>& v, double value);
// Read 2 uint32_t as a double
double as_double(const uint32_t* ast);

// Squared distance
int sqr_dist(int ax, int ay, int bx, int by);


template<class T>
/** Write binary to ostream */
inline void write_bin(std::ostream& os, T val) {
    os.write(reinterpret_cast<char*>(&val), sizeof(T));
}

template<class T>
/** Read binary from istream */
inline void read_bin(std::istream& is, T& val) {
    is.read(reinterpret_cast<char*>(&val), sizeof(T));
}

template<class Resizable>
/** Read a size_t from istream and resize v (vector/string) to it */
inline void resize_from_read_bin(std::istream& is, Resizable& v) {
    size_t sz; read_bin(is, sz);
    v.resize(sz);
}

}  // namespace util
}  // namespace nivalis
#endif // ifndef _UTIL_H_4EB09B11_F909_45C4_AD5D_8AA7A6644106
