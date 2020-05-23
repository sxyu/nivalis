#pragma once
#ifndef _ENV_H_0C15810C_45B5_42D2_80B4_B4292F4A5E6C
#define _ENV_H_0C15810C_45B5_42D2_80B4_B4292F4A5E6C
#include<map>
#include<string>
#include<vector>
#include<ostream>
#include<istream>
#include "expr.hpp"
namespace nivalis {

// Nivalis environment
struct Environment {
    Environment();
    Environment(const Environment& other) =default;
    Environment& operator=(const Environment& other) =default;

    // User function representation
    struct UserFunction {
        // Function name
        std::string name;
        // Function expression
        Expr expr;
        // Function dependencies
        // (list of other user functions called)
        std::vector<uint64_t> deps;
        // # arguments
        size_t n_args;
    };

    // Check if a variable is defined
    bool is_set(const std::string& var_name);
    // Set variable to value
    void set(const std::string& var_name, double val = 0.0);
    // Get variable value
    double get(const std::string& var_name) const;
    // Free variable
    bool del(const std::string& var_name);

    // Return address of variable (advanced)
    // explicit = false: if not var defined, then allocates space for it
    // explicit = true:  if not var defined, then returns -1
    uint64_t addr_of(const std::string& var_name, bool mode_explicit = true);
    // Const version ignores explicit (always true)
    uint64_t addr_of(const std::string& var_name, bool mode_explicit = true) const;

    // Define a function given name, expression, and argument
    // bindings (index i: ith argument's variable address in expr)
    uint64_t def_func(const std::string& func_name, const Expr& expr,
                  const std::vector<uint64_t>& arg_bindings);

    // Get a function's address (in funcs) by name; -1 if not present
    uint64_t addr_of_func(const std::string& func_name) const;

    // Delete function
    void del_func(const std::string& func_name);

    // Clear all vars/funcs
    void clear();

    // Binary serialization (only vars/funcs)
    std::ostream& to_bin(std::ostream& os) const;
    std::istream& from_bin(std::istream& is);

    // Values of variables (by address)
    std::vector<double> vars;

    // Names of variables (by address)
    std::vector<std::string> varname;

    // User functions
    std::vector<UserFunction> funcs;

    // Error message
    mutable std::string error_msg;

private:
    // Free addresses on vars vector
    std::vector<uint64_t> free_addrs;
    // Map variable name to address
    std::map<std::string, uint64_t> vreg;
    // Map function name to address
    std::map<std::string, uint64_t> freg;
};

}  // namespace nivalis
#endif // ifndef _ENV_H_0C15810C_45B5_42D2_80B4_B4292F4A5E6C
