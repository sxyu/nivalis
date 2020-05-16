#include "util.hpp"
#include <algorithm>
#include <cctype>
#include <locale>

#include "opcodes.hpp"
namespace nivalis {
namespace util {

bool is_varname(const std::string& expr) {
    if(!util::is_varname_first(expr[0])) return false;
    for (size_t k = 1; k < expr.size(); ++k) {
        if (!util::is_literal(expr[k])) return false;
    }
    return true;
}

bool is_whole_number(const std::string& expr) {
    for (size_t k = 0; k < expr.size(); ++k) {
        if (expr[k] < '0' || expr[k] > '9') return false;
    }
    return true;
}

size_t find_equality(const std::string& expr, char eqn_chr) {
    size_t stkh = 0;
    for (size_t i = 0; i < expr.size(); ++i) {
        const char c = expr[i];
        if (is_open_bracket(c)) {
            ++stkh;
        } else if (is_close_bracket(c)) {
            --stkh;
        } else if (stkh == 0 && c == eqn_chr &&
                   i > 0 && i < expr.size()-1 &&
                   !util::is_comp_operator(expr[i-1]) &&
                   expr[i-1] != '!' &&
                   !util::is_comp_operator(expr[i+1]) &&
                   expr[i+1] != '!') {
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
int sqr_dist(int ax, int ay, int bx, int by) {
    return (ax-bx)*(ax-bx) + (ay-by)*(ay-by);
}
}  // namespace util
}  // namespace nivalis

