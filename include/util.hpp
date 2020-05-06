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
