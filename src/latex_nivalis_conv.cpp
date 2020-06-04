#include "parser.hpp"

#include<iostream>
#include<vector>
#include<map>
#include<string>
#include<sstream>
#include<cmath>
#include<cctype>
#include<regex>
#include "util.hpp"
namespace nivalis {

namespace {
char end_paren_for(char c) {
    switch(c) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
    }
    return 0;
}

struct ParenRule {
    std::string prefix;
    std::vector<char> parens;
    struct ParenRuleOutputItem {
        ParenRuleOutputItem() =default;
        ParenRuleOutputItem(const char* s) : idx(-1), s(s) {}
        ParenRuleOutputItem(int idx) : idx(idx) {}
        int idx;
        std::string s;
    };
    std::vector<ParenRuleOutputItem> out_order;

    std::string operator()(const std::string & s) const {
        if (s.size() < prefix.size()) return s;
        std::string tmp = prefix; tmp.push_back('\v'); tmp.append(s);
        std::vector<size_t> za(tmp.size());
        za.resize(tmp.size());
        za[0] = tmp.size();
        size_t left = 0, right = 0;
        for (size_t i = 1; i < tmp.size(); ++i) {
            if (right >= i && size_t(za[i - left]) < right - i + 1) za[i] = za[i - left];
            else {
                left = i; right = std::max(i, right);
                while (right < tmp.size() && tmp[right] == tmp[right - left]) ++right;
                za[i] = right - left; --right;
            }
        }
        return _replace(s, 0, s.size(), za);
    }

    std::string _replace(const std::string & s, size_t start, size_t end, const std::vector<size_t>& za) const {
        std::vector<char> stk;
        std::string out;
        size_t plen = prefix.size();
        size_t i = start;
        for (; i < end - plen; ++i) {
            if (za[plen + 1 + i] >= plen && (i == start ||
                    !std::isalpha(s[i-1]) || s[i] == '\\')) {
                std::vector<std::string> args(parens.size());
                size_t init_i = i;
                i += prefix.size();
                bool bad = false;
                for (size_t argi = 0; argi < parens.size(); ++argi) {
                    while (i < end && std::isspace(s[i])) ++i;
                    if (i < end && s[i] == parens[argi]) {
                        if (!util::is_open_bracket(parens[argi])) {
                            ++i;
                            while (i < end && std::isspace(s[i])) ++i;
                            if (!util::is_open_bracket(s[i])) {
                                bad = true;
                                break;
                            }
                        }
                        size_t stkh = 0;
                        size_t start = i+1;
                        while (++i < end) {
                            if (util::is_open_bracket(s[i])) ++ stkh;
                            else if (util::is_close_bracket(s[i])) {
                                -- stkh;
                                if (stkh == (size_t) -1) break;
                            }
                        }
                        args[argi] = _replace(s, start, i, za);
                        ++i;
                    } else {
                        bad = true;
                        break;
                    }
                }
                if (!bad) {
                    for (size_t i = 0; i < out_order.size(); ++i) {
                        size_t j = (size_t)out_order[i].idx;
                        if (~j) {
                            out.append(args[j]);
                        } else {
                            out.append(out_order[i].s);
                        }
                    }
                    --i;
                } else {
                    i = init_i;
                    out.push_back(s[i]);
                }
            } else {
                out.push_back(s[i]);
            }
        }
        if (i < end) out.append(s.substr(i, end - i));
        return out;
    }
};
}  // namespace

// Parse an expression
std::string latex_to_nivalis(const std::string& expr_in) {
#define NO_IMPLICIT_MULT "\v" // Special character used to indicate we should not add * here
    thread_local ParenRule
        l2n_nthroot { "\\sqrt", {'[', '{'},  // Input spec: prefix, paren types (end paren inferred)
            {"(", 1, ")^(1/(", 0, "))"} },   // Output spec: output arg order
        l2n_sqrt { "\\sqrt", {'{'},
            {"sqrt(", 0, ")"} },
        l2n_frac { "\\frac", {'{', '{'},
            {"((", 0, ")/(", 1, "))"} },
        l2n_logbase { "\\log_", {'{', '('},
            {"log(", 1, ", ", 0, ")"} },
        l2n_sum { "\\sum_", {'{', '^'},
            {"sum(", 0, ", ", 1, ")\v"} },
        l2n_prod { "\\prod_", {'{', '^'},
            {"prod(", 0, ", ", 1, ")\v"} },
        l2n_int { "\\int_", {'{', '^'},
            {"int(", 0, ", ", 1, ")\v"} },
        l2n_choose { "\\binom", {'{', '{'},
            {"choose(", 0, ", ", 1, ")"} };

    if (expr_in.empty()) return "";
    std::string expr = util::str_replace(expr_in, "\\left|", "abs(");
    expr = util::str_replace(expr, "\\right|", ")");
    expr = util::str_replace(expr, "\\lfloor", "floor(");
    expr = util::str_replace(expr, "\\rfloor", ")");
    expr = util::str_replace(expr, "\\lceil|", "ceil(");
    expr = util::str_replace(expr, "\\rceil|", ")");
    expr = util::str_replace(expr, "\\left", "");
    expr = util::str_replace(expr, "\\right", "");
    expr = util::str_replace(expr, "\\cdot", "*");
    expr = util::str_replace(expr, "\\le", "<=");
    expr = util::str_replace(expr, "\\ge", ">=");
    expr = util::str_replace(expr, "\\ne", "!=");
    expr = util::str_replace(expr, "\\wedge", "&");
    expr = util::str_replace(expr, "\\vee", "|");
    expr = util::str_replace(expr, "\\operatorname{at}", "@");

    static const char*  SPECIAL_COMMANDS[] = {
        "poly", "fpoly", "Fpoly",
        "rect", "frect", "Frect",
        "circ", "fcirc", "Fcirc",
        "ellipse", "fellipse", "Fellipse",
        "text"
    };

    // Remove operatorname
    expr = std::regex_replace(expr,
            std::regex(R"(\\operatorname\{(.+?)\})"), "\\$1 ");

    // Remove text{}
    expr = std::regex_replace(expr,
            std::regex(R"(\\text\{(.+?)\})"), "$1");

    // Remap drawing commands
    for (size_t i = 0; i < sizeof(SPECIAL_COMMANDS) / sizeof(SPECIAL_COMMANDS[0]); ++i) {
        const size_t len = strlen(SPECIAL_COMMANDS[i]);
        if (expr.size() > len + 1 &&
                expr[0] == '\\' && expr.compare(1, len, SPECIAL_COMMANDS[i]) == 0) {
            expr[0] = '%';
        }
    }

    {
        std::string tmp(1, expr[0]);
        // Add braces so that x^22 -> x^{2}2,
        //                  log_22 -> log_{2}2
        tmp.reserve(expr.size());
        for (size_t i = 1; i < expr.size(); ++i) {
            if (expr[i] != '{' && (expr[i-1] == '_' || expr[i-1] == '^')) {
                tmp.push_back('{');
                tmp.push_back(expr[i]);
                tmp.push_back('}');
            } else {
                tmp.push_back(expr[i]);
            }
        }
        expr = std::move(tmp);
    }

    // Map (sin/cos/tan)^{-1} -> arg(..)
    expr = std::regex_replace(expr,
            std::regex(R"(\\(sin|cos|tan)( *?)\^\{-1\})"), "\\arc$1 ");

    // Diff
    expr = std::regex_replace(expr,
            std::regex(R"(\\frac\{d\}\{d([a-zA-Z][a-zA-Z_0-9\{\}]*?)\})"), "diff($1)" NO_IMPLICIT_MULT);

    // Apply parenthesis-based rules
    expr = l2n_choose(l2n_frac(l2n_int(
                    l2n_prod(l2n_sum(l2n_sqrt(
                                l2n_nthroot(l2n_logbase(expr))
                    )))
            )));

    // Implicit multiply
    {
        std::string tmp(1, expr[0]); tmp.reserve(expr.size());
        char last_nonspace = expr[0];
        std::vector<bool> brace_subs;
        size_t sub_level = 0; // subscript level

        auto is_opchar = [](char c, bool allow_open_brkt, bool allow_close_brkt) {
            return util::is_operator(c) ||
                util::is_control(c) ||
                (!allow_open_brkt && util::is_open_bracket(c)) ||
                (!allow_close_brkt && util::is_close_bracket(c)) ||
                c == '_' ||
                c == NO_IMPLICIT_MULT[0];
        };
        size_t i = 1;
        if (expr.size() > 5 && expr.substr(0, 5) == "%text") {
            i = expr.find('@');
            tmp = expr.substr(0, i);
            if (i == std::string::npos) i = expr.size();
        } else if (expr.size() > 1 && expr[0] == '%') {
            i = expr.find(' ');
            tmp = expr.substr(0, i);
            if (i == std::string::npos) i = expr.size();
        }
        for (; i < expr.size(); ++i) {
            if (std::isspace(expr[i]) && expr[i] != NO_IMPLICIT_MULT[0]) {
                tmp.push_back(expr[i]);
                continue;
            }
            if (expr[i] == '{') {
                brace_subs.push_back(last_nonspace == '_');
                if (brace_subs.back()) sub_level++;
            } else if (expr[i] == '}') {
                if (brace_subs.size()) {
                    if (brace_subs.back()) sub_level--;
                    brace_subs.pop_back();
                }
            }
            if ((expr[i] == '\\' &&
                    (i + 1 == expr.size() ||
                     (expr[i+1] != '}' && expr[i+1] != ' ')) && // not '\}' or '\ '
                    !is_opchar(last_nonspace, false, true)) ||
                (util::is_close_bracket(last_nonspace) &&
                    !is_opchar(expr[i], true, false))) {
                tmp.push_back('*');
            } else if (util::is_open_bracket(expr[i]) &&
                    !is_opchar(last_nonspace, false, true)){
                if (!std::isalpha(last_nonspace) || expr[i] != '(')  // not beginning of call
                    tmp.push_back('*');
            } else if (sub_level == 0 &&
                       util::is_identifier(expr[i]) &&
                       util::is_identifier(last_nonspace) &&
                       util::is_numeric(expr[i]) !=
                       util::is_numeric(last_nonspace)) {
                tmp.push_back('*');
            }
            last_nonspace = expr[i];
            if (expr[i] != NO_IMPLICIT_MULT[0])
                tmp.push_back(expr[i]);
        }
        expr = std::move(tmp);
    }

    // Subscript
    expr = std::regex_replace(expr,
            std::regex(R"(_\{(.+?)\}\*?)"), "$1");

    // Remove any remaining backslashes, convert {}
    {
        std::string tmp; tmp.reserve(expr.size());
        for (size_t i = 0; i < expr.size(); ++i) {
            if (expr[i] == '{' && (i == 0 || expr[i-1] != '\\')) {
                tmp.push_back('(');
            } else if (expr[i] == '}' && (i == 0 || expr[i-1] != '\\')) {
                tmp.push_back(')');
            } else if (expr[i] != '\\') {
                tmp.push_back(expr[i]);
            }
        }
        expr = std::move(tmp);
    }
    return expr;
}

std::string nivalis_to_latex(const std::string& expr_in, Environment& env) {
    std::string copy = expr_in;
    util::trim(copy);
    if (copy.empty()) return "";
    if (copy[0] == '#') return "#\\text{" + copy.substr(1) + "}";
    Expr expr = parse(expr_in, env, false, true);

    std::ostringstream strm;
    expr.latex_repr(strm, env);
    std::string s = strm.str();
    if (s.size() >= 13 &&
            s.compare(0, 6, "\\left(") == 0 &&
            s.compare(s.size() - 7, 7, "\\right)") == 0) {
        s = s.substr(6, s.size() - 13);
    }
    return s;
}

std::string nivalis_to_latex_safe(const std::string& expr_in) {
    std::string expr = expr_in;
    util::trim(expr);
    if (expr.empty()) return "";
    if (expr[0] == '#') return "#\\text{" + expr.substr(1) + "}";
    expr = util::str_replace(expr, "*", "\\cdot ");
    expr = util::str_replace(expr, "<=", "\\le ");
    expr = util::str_replace(expr, ">=", "\\ge ");
    expr = util::str_replace(expr, "!=", "\\ne ");
    expr = util::str_replace(expr, "&", "\\wedge ");
    expr = util::str_replace(expr, "|", "\\vee ");
    expr = util::str_replace(expr, "{", "\\left\\{");
    expr = util::str_replace(expr, "}", "\\right\\}");
    expr = util::str_replace(expr, "@", "\\operatorname{at} ");

    thread_local ParenRule
        n2l_sqrt { "sqrt", {'('}, {"\\sqrt{", 0, "}"} },
        n2l_abs { "abs", {'('}, {"\\left|", 0, "\\right|"} },
        n2l_diff { "diff", {'('}, {"\\frac{d}{d", 0, "}"} };
    expr = n2l_diff(n2l_abs(n2l_sqrt(expr)));
    return expr;
}

}  // namespace nivalis
