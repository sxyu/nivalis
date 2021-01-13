#include "env.hpp"
#include <cmath>
#include <unordered_set>
#include <algorithm>
#include "util.hpp"
namespace nivalis {
namespace {
bool check_for_cycle(const std::vector<Environment::UserFunction>& funcs,
                     uint64_t fid) {
    thread_local std::unordered_set<uint64_t> seen;
    if (seen.count(fid)) return true;
    seen.insert(fid);
    for (uint64_t dep_fid : funcs[fid].deps) {
        if (check_for_cycle(funcs, dep_fid)) {
            seen.erase(fid);
            return true;
        }
    }
    seen.erase(fid);
    return false;
}

}  // namespace

Environment::Environment() {}

bool Environment::is_set(const std::string& var_name) {
    error_msg.clear();
    return vreg.count(var_name);
}
uint64_t Environment::addr_of(const std::string& var_name, bool mode_explicit) {
    error_msg.clear();
    if (var_name[0] == '@') {
        // Address
        int64_t addr = std::atoll(var_name.substr(1).c_str());
        return (addr < 0 || static_cast<size_t>(addr) >= vars.size())
                   ? -1
                   : static_cast<uint64_t>(addr);
    }
    auto it = vreg.find(var_name);
    if (it != vreg.end()) {
        return it->second;
    } else {
        if (mode_explicit) return -1;
        uint64_t addr;
        if (free_addrs.empty()) {
            addr = vars.size();
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
uint64_t Environment::addr_of(const std::string& var_name,
                              bool mode_explicit) const {
    error_msg.clear();
    if (var_name[0] == '&') {
        // Address
        int64_t addr = std::atoll(var_name.substr(1).c_str());
        return (addr < 0 || static_cast<size_t>(addr) >= vars.size())
                   ? -1
                   : static_cast<uint64_t>(addr);
    }
    auto it = vreg.find(var_name);
    return it != vreg.end() ? it->second : -1;
}
void Environment::set(const std::string& var_name, complex value) {
    auto idx = addr_of(var_name, false);
    if (~idx) vars[idx] = value;
}
complex Environment::get(const std::string& var_name) const {
    auto idx = addr_of(var_name);
    if (~idx)
        return vars[idx];
    else {
        return std::numeric_limits<double>::quiet_NaN();
    }
}
bool Environment::del(const std::string& var_name) {
    auto idx = addr_of(var_name, true);
    if (~idx) {
        if (idx == vars.size() - 1) {
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
    } else
        return false;
}

uint64_t Environment::def_func(const std::string& func_name, const Expr& expr,
                               const std::vector<uint64_t>& arg_bindings) {
    error_msg.clear();
    size_t idx = 0;
    auto it = freg.find(func_name);
    if (it == freg.end()) {
        // New function
        idx = freg[func_name] = funcs.size();
        funcs.emplace_back();
        funcs.back().name = func_name;
    } else {
        idx = it->second;
    }
    UserFunction& func = funcs[idx];
    func.deps.clear();
    Expr func_expr = expr;
    func_expr.optimize();  // Optimize function expression
    auto& ast = func_expr.ast;
    // Invert argument mapping
    std::vector<uint64_t> arg_vars(vars.size(), -1);
    for (size_t i = 0; i < arg_bindings.size(); ++i) {
        if (~arg_bindings[i]) {
            arg_vars[arg_bindings[i]] = i;
        }
    }
    // Sub arguments
    for (size_t i = 0; i < ast.size(); ++i) {
        auto& nd = ast[i];
        if (OpCode::has_ref(nd.opcode) && nd.opcode != OpCode::arg &&
            ~arg_vars[nd.ref]) {
            // Set argument
            nd.opcode = OpCode::arg;
            nd.ref = arg_vars[nd.ref];
        }
        if (nd.opcode == OpCode::call) {
            // Set dependency on other function
            func.deps.push_back(nd.call_info[0]);
        }
    }
    std::sort(func.deps.begin(), func.deps.end());
    func.deps.resize(std::unique(func.deps.begin(), func.deps.end()) -
                     func.deps.begin());
    func.n_args = arg_bindings.size();
    func.expr = std::move(func_expr);

    // Check for recursion
    if (check_for_cycle(funcs, idx)) {
        // Found cycle
        func.expr.ast = {OpCode::null};
        func.deps.clear();
        error_msg = "Cycle found in definition of " + func_name + "(...)\n";
        return -1;
    }
    return idx;
}

uint64_t Environment::addr_of_func(const std::string& func_name) const {
    error_msg.clear();
    auto it = freg.find(func_name);
    if (it != freg.end()) {
        return it->second;
    }
    return -1;
}

bool Environment::del_func(const std::string& func_name) {
    error_msg.clear();
    auto it = freg.find(func_name);
    if (it != freg.end()) {
        // Won't actually delete, but try to save some memory
        funcs[it->second].name.clear();
        funcs[it->second].name.shrink_to_fit();
        funcs[it->second].expr.ast.resize(1);
        funcs[it->second].expr.ast[0] = OpCode::null;
        funcs[it->second].expr.ast.shrink_to_fit();
        funcs[it->second].deps.clear();
        funcs[it->second].deps.shrink_to_fit();
        freg.erase(it);
        return true;
    }
    return false;
}

void Environment::clear() {
    funcs.clear();
    vars.clear();
    varname.clear();
    vreg.clear();
    freg.clear();
    error_msg.clear();
    free_addrs.clear();
}

std::ostream& Environment::to_bin(std::ostream& os) const {
    util::write_bin(os, vars.size());
    for (size_t i = 0; i < vars.size(); ++i) {
        util::write_bin(os, vars[i]);
    }
    util::write_bin(os, funcs.size());
    for (size_t i = 0; i < funcs.size(); ++i) {
        funcs[i].expr.to_bin(os);
        util::write_bin(os, funcs[i].n_args);
    }
    return os;
}
std::istream& Environment::from_bin(std::istream& is) {
    util::resize_from_read_bin(is, vars);
    for (size_t i = 0; i < vars.size(); ++i) {
        util::read_bin(is, vars[i]);
    }
    varname.resize(vars.size());
    util::resize_from_read_bin(is, funcs);
    for (size_t i = 0; i < funcs.size(); ++i) {
        funcs[i].expr.from_bin(is);
        util::read_bin(is, funcs[i].n_args);
    }
    return is;
}
}  // namespace nivalis
