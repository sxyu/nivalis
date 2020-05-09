#pragma once
#ifndef _ENV_H_0C15810C_45B5_42D2_80B4_B4292F4A5E6C
#define _ENV_H_0C15810C_45B5_42D2_80B4_B4292F4A5E6C
#include<map>
#include<string>
#include<vector>
namespace nivalis {

// Nivalis environment
struct Environment {
    Environment();

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
    uint32_t addr_of(const std::string& var_name, bool mode_explicit = true);
    // Const version ignores explicit (always true)
    uint32_t addr_of(const std::string& var_name, bool mode_explicit = true) const;

    // Values of variables (by address)
    std::vector<double> vars;

    // Names of variables (by address)
    std::vector<std::string> varname;

private:
    // Free addresses on vars vector
    std::vector<uint32_t> free_addrs;
    // Map variable name to address
    std::map<std::string, uint32_t> vreg;
};

}  // namespace nivalis
#endif // ifndef _ENV_H_0C15810C_45B5_42D2_80B4_B4292F4A5E6C
