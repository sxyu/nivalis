#include "env.hpp"
#include <cmath>
#include <iostream>
namespace nivalis {
Environment::Environment() { }

bool Environment::is_set(const std::string& var_name) {
    return vreg.count(var_name);
}
uint32_t Environment::addr_of(const std::string& var_name, bool mode_explicit) {
    if (var_name[0] == '&') {
        // Address
        int64_t addr = std::atoll(var_name.substr(1).c_str());
        return (addr < 0 || static_cast<size_t>(addr) >= vars.size()) ? -1 : static_cast<uint32_t>(addr);
    }
    auto it = vreg.find(var_name);
    if (it != vreg.end()) {
        return it->second;
    } else {
        if (mode_explicit) return -1;
        uint32_t addr;
        if (free_addrs.empty()) {
            addr = static_cast<uint32_t>(vars.size());
            vars.push_back(std::numeric_limits<double>::quiet_NaN());
            varname.emplace_back();
        } else {
            addr = free_addrs.back();
            free_addrs.pop_back();
        }
        varname[addr] = var_name;
        return vreg[var_name] = addr;
    }
}
uint32_t Environment::addr_of(const std::string& var_name, bool mode_explicit) const {
    if (var_name[0] == '&') {
        // Address
        int64_t addr = std::atoll(var_name.substr(1).c_str());
        return (addr < 0 || static_cast<size_t>(addr) >= vars.size()) ? -1 : static_cast<uint32_t>(addr);
    }
    auto it = vreg.find(var_name);
    return it != vreg.end() ? it->second : -1;
}
void Environment::set(const std::string& var_name, double value) {
    auto idx = addr_of(var_name, false);
    if (~idx) vars[idx] = value;
}
double Environment::get(const std::string& var_name) const {
    auto idx = addr_of(var_name);
    if (~idx) return vars[idx];
    else {
        return std::numeric_limits<double>::quiet_NaN();
    }
}
bool Environment::del(const std::string& var_name) {
    auto idx = addr_of(var_name, true);
    if (~idx) {
        if (idx == vars.size()-1) {
            vars.pop_back();
            varname.pop_back();
        } else {
            free_addrs.push_back(idx);
            vars[idx] = std::numeric_limits<double>::quiet_NaN();
            varname[idx].clear();
            varname[idx].shrink_to_fit();
        }
        vreg.erase(var_name);
        return true;
    }
    else return false;
}
}  // namespace nivalis
