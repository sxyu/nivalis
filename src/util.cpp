#include "util.hpp"
#include <algorithm>
#include <cctype>
#include <locale>

#include "opcodes.hpp"
namespace nivalis {
namespace util {

bool is_varname(std::string_view expr) {
    if (!util::is_varname_first(expr[0])) return false;
    for (size_t k = 1; k < expr.size(); ++k) {
        if (!util::is_identifier(expr[k])) return false;
    }
    return true;
}

bool is_whole_number(std::string_view expr) {
    for (size_t k = 0; k < expr.size(); ++k) {
        if (expr[k] < '0' || expr[k] > '9') return false;
    }
    return true;
}

size_t find_equality(std::string_view expr, bool allow_ineq,
                     bool enforce_no_adj_comparison) {
    if (expr.empty()) return -1;
    size_t stkh = 0;
    for (size_t i = 0; i < expr.size() - 1; ++i) {
        const char c = expr[i];
        if (is_open_bracket(c)) {
            ++stkh;
        } else if (is_close_bracket(c)) {
            --stkh;
        } else if (stkh == 0 &&
                   (c == '=' || (allow_ineq && (c == '<' || c == '>'))) &&
                   i > 0 &&
                   (!enforce_no_adj_comparison ||
                    (!util::is_comp_operator(expr[i - 1]) &&
                     expr[i - 1] != '!' &&
                     !util::is_comp_operator(expr[i + 1]) &&
                     expr[i + 1] != '!'))) {
            return i;
        }
    }
    return -1;
}

void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
                return !::std::isspace(ch);
            }));
}

void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](int ch) { return !::std::isspace(ch); })
                .base(),
            s.end());
}

void trim(std::string& s) {
    ltrim(s);
    rtrim(s);
}

void push_dbl(std::vector<uint32_t>& v, double value) {
    v.resize(v.size() + 3, OpCode::val);
    *reinterpret_cast<double*>(&v[0] + (v.size() - 2)) = value;
}

double as_double(const uint32_t* ast) {
    return *reinterpret_cast<const double*>(ast);
}

std::string str_replace(std::string_view src, std::string from,
                        std::string_view to) {
    size_t sz = from.size();
    from.push_back('\v');
    from.append(src);

    std::vector<size_t> za(from.size());
    za[0] = from.size();
    size_t left = 0, right = 0;
    for (size_t i = 1; i < from.size(); ++i) {
        if (right >= i && size_t(za[i - left]) < right - i + 1)
            za[i] = za[i - left];
        else {
            left = i;
            right = std::max(i, right);
            while (right < from.size() && from[right] == from[right - left])
                ++right;
            za[i] = right - left;
            --right;
        }
    }
    std::string out;
    out.reserve(src.size());
    for (size_t i = sz + 1; i < from.size(); ++i) {
        if (za[i] >= sz) {
            out.append(to);
            i += sz - 1;
        } else {
            out.push_back(from[i]);
        }
    }
    return out;
}

bool ends_with(std::string_view s, std::string_view ending) {
    if (s.length() >= ending.length()) {
        return (0 == s.compare(s.length() - ending.length(), ending.length(),
                               ending));
    } else {
        return false;
    }
}

void print_imag(std::ostream& os, double n) {
    if (n == 1.0) {
        os << "i";
    } else if (n == -1.0) {
        os << "-i";
    } else {
        os << n << "i";
    }
}

void print_complex(std::ostream& os, complex n) {
    if (n.imag() == 0) {
        os << n.real();
    } else if (n.real() == 0) {
        print_imag(os, n.imag());
    } else {
        os << n.real() << " " << (n.imag() < 0 ? '-' : '+') << " ";
        print_imag(os, std::fabs(n.imag()));
    }
}
}  // namespace util
}  // namespace nivalis
