#include "util.hpp"
#include <algorithm>
#include <cctype>
#include <locale>

#include "expr.hpp"
#define white_space(c) ((c) == ' ' || (c) == '\t')
#define valid_digit(c) ((c) >= '0' && (c) <= '9')

namespace nivalis {
namespace util {

// 09-May-2009 Tom Van Baak (tvb) www.LeapSecond.com
// From https://gist.github.com/LihuaWu/08b00f239e7443006bb6
double atof(const char* p, const char* end) {
    if (p == end) return 0.0;
    int frac;
    double sign, value, scale;

    // Skip leading white space, if any.
    while (white_space(*p)) {
        p += 1;
    }

    // Get sign, if any.
    sign = 1.0;
    if (*p == '-') {
        sign = -1.0;
        p += 1;
    } else if (*p == '+') {
        p += 1;
    }

    // Get digits before decimal point or exponent, if any.

    for (value = 0.0; valid_digit(*p) && p != end; p += 1) {
        value = value * 10.0 + (*p - '0');
    }

    // Get digits after decimal point, if any.
    if (*p == '.') {
        double pow10 = 10.0;
        p += 1;
        while (valid_digit(*p) && p != end) {
            value += (*p - '0') / pow10;
            pow10 *= 10.0;
            p += 1;
        }
    }

    // Handle exponent, if any.
    frac = 0;
    scale = 1.0;
    if ((*p == 'e') || (*p == 'E')) {
        unsigned int expon;

        // Get sign of exponent, if any.
        p += 1;
        if (*p == '-') {
            frac = 1;
            p += 1;

        } else if (*p == '+') {
            p += 1;
        }

        // Get digits of exponent, if any.
        for (expon = 0; valid_digit(*p) && p != end; p += 1) {
            expon = expon * 10 + (*p - '0');
        }
        if (expon > 308) expon = 308;

        // Calculate scaling factor.
        while (expon >= 50) { scale *= 1E50; expon -= 50; }
        while (expon >=  8) { scale *= 1E8;  expon -=  8; }
        while (expon >   0) { scale *= 10.0; expon -=  1; }
    }

    // Return signed and scaled floating point result.
    return sign * (frac ? (value / scale) : (value * scale));
}

bool is_varname(const std::string& expr) {
    if(!util::is_varname_first(expr[0])) return false;
    for (size_t k = 1; k < expr.size(); ++k) {
        if (!util::is_literal(expr[k])) return false;
    }
    return true;
}
size_t find_equality(const std::string& expr) {
    size_t stkh = 0;
    for (size_t i = 0; i < expr.size(); ++i) {
        const char c = expr[i];
        if (is_open_bracket(c)) {
            ++stkh;
        } else if (is_close_bracket(c)) {
            --stkh;
        } else if (stkh == 0 && c == '=' && 
                   i > 0 && i < expr.size()-1 &&
                   !util::is_comp_operator(expr[i-1]) &&
                   !util::is_comp_operator(expr[i+1])) {
            return i;
        }
    }
    return -1;
}

void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

void push_dbl(std::vector<uint32_t>& v, double value) {
    v.resize(v.size() + 3, OpCode::val);
    *reinterpret_cast<double*>(&v[0] + (v.size()-2)) = value;
}

double as_double(const uint32_t* ast) {
    return *reinterpret_cast<const double*>(ast);
}
}  // namespace util
}  // namespace nivalis

