#include "expr.hpp"

#include <numeric>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include "env.hpp"
#include "util.hpp"
namespace nivalis {

namespace {
// * Combine/wrap utils
// Combine two expressions by applying binary operator
// assumes opcode is binary
Expr combine_expr(uint32_t opcode, const Expr& a, const Expr& b) {
    Expr new_expr;
    new_expr.ast.resize(a.ast.size() + b.ast.size() + 1);
    new_expr.ast[0] = opcode;
    std::copy(a.ast.begin(), a.ast.end(), new_expr.ast.begin() + 1);
    std::copy(b.ast.begin(), b.ast.end(),
              new_expr.ast.begin() + a.ast.size() + 1);
    return new_expr;
}
// Apply unary operator to expression
Expr wrap_expr(uint32_t opcode, const Expr& a) {
    Expr new_expr;
    new_expr.ast.resize(a.ast.size() + 1);
    new_expr.ast[0] = opcode;
    std::copy(a.ast.begin(), a.ast.end(), new_expr.ast.begin() + 1);
    return new_expr;
}
}  // namespace

Expr::Expr() { ast.resize(1); }
Expr::Expr(const AST& ast) : ast(ast) {}

// void Expr::optimize(int num_passes) {
//     // Not implemented
// }

Expr Expr::operator+(const Expr& other) const {
    return combine_expr(OpCode::add, *this, other);
}
Expr Expr::operator-(const Expr& other) const {
    return combine_expr(OpCode::sub, *this, other);
}
Expr Expr::operator*(const Expr& other) const {
    return combine_expr(OpCode::mul, *this, other);
}
Expr Expr::operator/(const Expr& other) const {
    return combine_expr(OpCode::divi, *this, other);
}
Expr Expr::combine(uint32_t opcode, const Expr& other) const {
    return combine_expr(opcode, *this, other);
}
Expr Expr::operator-() const { return wrap_expr(OpCode::unaryminus, *this); }
Expr Expr::wrap(uint32_t opcode) const { return wrap_expr(opcode, *this); }

bool Expr::has_var(uint32_t addr) const {
    return detail::has_var_ast(ast, addr);
}
void Expr::sub_var(uint32_t addr, complex value) {
    return detail::sub_var_ast(ast, addr, value);
}
void Expr::sub_var(uint32_t addr, const Expr& expr) {
    return detail::sub_var_ast(ast, addr, expr);
}

Expr Expr::null() { return Expr(); }
Expr Expr::zero() { return constant(0); }
Expr Expr::constant(complex val) {
    Expr z;
    z.ast[0].opcode = OpCode::val;
    z.ast[0].val = val;
    return z;
}

std::ostream& Expr::repr(std::ostream& os, const Environment& env) const {
    detail::print_ast(os, ast, &env);
    return os;
}

std::ostream& Expr::latex_repr(std::ostream& os, const Environment& env) const {
    detail::print_ast_latex(os, ast, &env);
    return os;
}

std::ostream& Expr::to_bin(std::ostream& os) const {
    util::write_bin(os, ast.size());
    for (size_t i = 0; i < ast.size(); ++i) {
        util::write_bin(os, ast[i]);
    }
    return os;
}
std::istream& Expr::from_bin(std::istream& is) {
    util::resize_from_read_bin(is, ast);
    for (size_t i = 0; i < ast.size(); ++i) {
        util::read_bin(is, ast[i]);
    }
    return is;
}

bool Expr::is_null() const {
    return ast.empty() || ast[0].opcode == OpCode::null ||
           (ast[0].opcode == OpCode::val && std::isnan(ast[0].val.real()));
}

bool Expr::is_val() const {
    return ast.size() >= 1 && ast[0].opcode == OpCode::val;
}

bool Expr::is_ref() const {
    return ast.size() >= 1 && ast[0].opcode == OpCode::ref;
}

const Expr::ASTNode& Expr::operator[](int idx) const { return ast[idx]; }
Expr::ASTNode& Expr::operator[](int idx) { return ast[idx]; }

Expr::ASTNode::ASTNode() : opcode(OpCode::null) {}
Expr::ASTNode::ASTNode(uint32_t opcode, uint64_t ref)
    : opcode(opcode), ref(ref) {}
Expr::ASTNode::ASTNode(OpCode::_OpCode opcode) : opcode(opcode) {}
Expr::ASTNode::ASTNode(complex val) : opcode(OpCode::val), val(val) {}
Expr::ASTNode::ASTNode(double val) : opcode(OpCode::val), val(val) {}

Expr::ASTNode Expr::ASTNode::varref(uint64_t id) {
    ASTNode node(OpCode::ref, id);
    return node;
}
Expr::ASTNode Expr::ASTNode::call(uint32_t id, uint32_t n_arg) {
    ASTNode node(OpCode::call);
    node.call_info[0] = id;
    node.call_info[1] = n_arg;
    return node;
}

bool Expr::ASTNode::operator==(const ASTNode& other) const {
    if (other.opcode != opcode) return false;
    if ((OpCode::has_ref(opcode) || opcode == OpCode::val ||
         opcode == OpCode::thunk_jmp) &&
        ref != other.ref)
        return false;
    return true;
}
bool Expr::ASTNode::operator!=(const ASTNode& other) const {
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& os, const Expr& expr) {
    os << "nivalis::Expr[";
    detail::print_ast(os, expr.ast);
    os << "]";
    return os;
}
std::ostream& operator<<(std::ostream& os, const Expr::ASTNode& node) {
    os << node.opcode;
    if (node.opcode == OpCode::val) os << "#" << node.val;
    if (OpCode::has_ref(node.opcode)) os << "&" << node.ref;
    return os;
}

namespace detail {
size_t print_ast(std::ostream& os, const Expr::AST& ast, const Environment* env,
                 size_t idx) {
    std::string_view reprs = OpCode::repr(ast[idx].opcode);
    size_t n_idx = idx + 1;
    if (ast[idx].opcode == OpCode::call) {
        // Special handling for user function
        if (env != nullptr) {
            os << env->funcs[ast[idx].call_info[0]].name << "(";
        } else {
            os << "<function id=" << ast[idx].call_info[0] << ", "
               << ast[idx].call_info[1] << " args>(";
        }
        size_t n_args = ast[idx].call_info[1];
        for (size_t i = 0; i < n_args; ++i) {
            if (i) os << ", ";
            n_idx = print_ast(os, ast, env, n_idx);
        }
        os << ")";
    } else {
        for (char c : reprs) {
            switch (c) {
                case '@':
                    n_idx = print_ast(os, ast, env, n_idx);
                    break;  // subexpr
                case '\v':
                    util::print_complex(os, ast[idx].val);
                    break;  // value
                case '\r':
                    if (env != nullptr) {
                        if (ast[idx].ref >= env->vars.size()) {
                            os << "&NULL";
                            break;
                        }
                        os << env->varname.at(ast[idx].ref);
                    } else
                        os << "@" << ast[idx].ref;
                    break;  // ref
                case '\t':
                    // Not used
                    break;
                case '$':
                    os << "$" << ast[idx].ref;
                    break;
                default:
                    os << c;
            }
        }
    }
    return n_idx;
}

size_t print_ast_latex(std::ostream& os, const Expr::AST& ast,
                       const Environment* env, size_t idx) {
    std::string_view reprs = OpCode::latex_repr(ast[idx].opcode);
    size_t n_idx = idx + 1;
    if (ast[idx].opcode == OpCode::call) {
        // Special handling for user function
        if (env != nullptr) {
            os << env->funcs[ast[idx].call_info[0]].name;
        } else {
            os << "<function id=" << ast[idx].call_info[0] << ", "
               << ast[idx].call_info[1] << " args>";
        }
        size_t n_args = ast[idx].call_info[1];
        if (n_args) {
            os << "\\left(";
            for (size_t i = 0; i < n_args; ++i) {
                if (i) os << ",";
                n_idx = print_ast_latex(os, ast, env, n_idx);
            }
            os << "\\right)";
        }
    } else {
        for (char c : reprs) {
            switch (c) {
                case '@':
                    n_idx = print_ast_latex(os, ast, env, n_idx);
                    break;  // subexpr
                case '\v':
                    os << ast[idx].val;
                    break;  // value
                case '\r':
                    if (env != nullptr) {
                        if (ast[idx].ref >= env->vars.size()) {
                            os << "\\&NULL";
                            break;
                        }
                        os << env->varname.at(ast[idx].ref);
                    } else
                        os << "\\@" << ast[idx].ref;
                    break;  // ref
                case '\t':
                    // Not used
                    break;
                case '$':
                    os << "\\$" << ast[idx].ref;
                    break;
                default:
                    os << c;
            }
        }
    }
    return n_idx;
}
void sub_var_ast(Expr::AST& ast, int64_t addr, complex value) {
    for (size_t i = 0; i < ast.size(); ++i) {
        if (OpCode::has_ref(ast[i].opcode) && ast[i].opcode != OpCode::arg &&
            ast[i].ref == addr) {
            ast[i].opcode = OpCode::val;
            ast[i].val = value;
        }
    }
}

void sub_var_ast(Expr::AST& ast, int64_t addr, const Expr& expr) {
    Expr::AST new_ast;
    for (size_t i = 0; i < ast.size(); ++i) {
        if (ast[i].opcode == OpCode::ref && ast[i].ref == addr) {
            std::copy(expr.ast.begin(), expr.ast.end(),
                      std::back_inserter(new_ast));
        } else {
            new_ast.push_back(ast[i]);
        }
    }
    ast = new_ast;
}

bool has_var_ast(const Expr::AST& ast, uint32_t addr) {
    for (size_t i = 0; i < ast.size(); ++i) {
        if (OpCode::has_ref(ast[i].opcode) && ast[i].opcode != OpCode::arg &&
            ast[i].ref == addr) {
            return true;
        }
    }
    return false;
}
}  // namespace detail

}  // namespace nivalis
