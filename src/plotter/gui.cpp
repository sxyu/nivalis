#include "plotter/plotter.hpp"
#include "plotter/internal.hpp"

#include "parser.hpp"
#include "util.hpp"

#include "json.hpp"
#include "shell.hpp"
#include <iomanip>
#include <iostream>

namespace nivalis {

namespace {
using json = nlohmann::json;

bool is_var_name_reserved(const std::string& var_name) {
    return var_name == "x" || var_name == "y" ||
           var_name == "t" || var_name == "r";
}

constexpr char var_name_for_type(int func_type) {
    switch(func_type & ~Function::FUNC_TYPE_MOD_ALL) {
        case Function::FUNC_TYPE_EXPLICIT: return 'x';
        case Function::FUNC_TYPE_EXPLICIT_Y: return 'y';
        case Function::FUNC_TYPE_POLAR:
        case Function::FUNC_TYPE_PARAMETRIC:
             return 't';
        default: return 0;
    }
}
constexpr char result_var_name_for_type(int func_type) {
    switch(func_type & ~Function::FUNC_TYPE_MOD_ALL) {
        case Function::FUNC_TYPE_EXPLICIT: return 'y';
        case Function::FUNC_TYPE_EXPLICIT_Y: return 'x';
        case Function::FUNC_TYPE_POLAR: return 'r';
        default: return 0;
    }
}
int detect_func_type(const std::string& expr_str, std::string& lhs, std::string& rhs) {
    if (expr_str.empty()) return Function::FUNC_TYPE_EXPLICIT;
    if (expr_str[0] == '(' && expr_str.back() == ')') {
        int close = expr_str[expr_str.size() - 2] == '(';
        int stkh = 0;
        bool has_comma = false;
        for (size_t i = 0; i < expr_str.size() - (close ? 2 : 0); ++i) {
            char c = expr_str[i];
            if (util::is_open_bracket(c)) {
                ++stkh;
            } else if (util::is_close_bracket(c)) {
                --stkh;
            } else if (stkh == 1) {
                if (c == ',') has_comma = true;
            } else if (stkh == 0) {
                // Invalid since symbol between )(
                break;
            }
        }
        if (has_comma) {
            // POLYLINE or POLYGON or PARAMETRIC
            lhs = expr_str;
            if (close) lhs.resize(lhs.size() - 2);
            return Function::FUNC_TYPE_POLYLINE + close;
        }
    }
    int lhs_end = util::find_equality(
            expr_str, true  /* allow_ineq */ ,
            false /* enforce_no_adj_comparison */);
    if (~lhs_end) {
        // If has equality/inequality at root level
        int rhs_start = lhs_end + 1;
        const char eq_ch = expr_str[lhs_end];
        const char next_ch = expr_str[rhs_start];
        if (next_ch == '=') ++rhs_start; // <=, >= detection
        int type_mod = 0;
        bool ineq_is_less = 0;
        if (eq_ch == '>' || eq_ch == '<') {
            // Inequality modifier
            type_mod = Function::FUNC_TYPE_MOD_INEQ;
            if (rhs_start == lhs_end + 1) type_mod |= Function::FUNC_TYPE_MOD_INEQ_STRICT;
            if (eq_ch == '<') type_mod |= Function::FUNC_TYPE_MOD_INEQ_LESS;
        }
        lhs = expr_str.substr(0, lhs_end),
        rhs = expr_str.substr(rhs_start);
        if (type_mod == 0 && lhs.size() && lhs.back() == ')') {
            // A function definition?
            for (size_t i = 0; i < lhs.size(); ++i) {
                if (lhs[i] == '(') {
                    auto fname = lhs.substr(0, i);
                    if (util::is_varname(fname) &&
                        OpCode::funcname_to_opcode_map().count(fname) == 0) {
                        // Not name of a built-in function like sin,
                        // so see it as a a function definition.
                        return Function::FUNC_TYPE_FUNC_DEFINITION;
                    }
                }
            }
        } else if (lhs == "y" || rhs == "y") {
            Environment env;
            env.addr_of("y", false);
            if (lhs == "y") lhs = rhs;
            if (!parse(lhs, env, true, /* explicit */ true,
                        /* quiet */ 0).has_var(0)) {
                // if one side is y and other side has no y,
                // treat as explicit function
                return Function::FUNC_TYPE_EXPLICIT | type_mod;
            }
        } else if (lhs == "x" || rhs == "x") {
            Environment env;
            env.addr_of("x", false);
            if (lhs == "x") lhs = rhs;
            if (!parse(lhs, env, true, /* explicit */ true,
                        /* quiet */ 0).has_var(0)) {
                // if one side is x and other side has no x,
                // treat as explicit function in y
                return Function::FUNC_TYPE_EXPLICIT_Y | type_mod;
            }
        } else if (lhs == "r" || rhs == "r") {
            Environment env;
            if (lhs == "r") lhs = rhs;
            env.addr_of("x", false); env.addr_of("y", false);
            Expr expr = parse(lhs, env, true /* explicit */, true  /* quiet */, 0);
            if (!(expr.has_var(0) || expr.has_var(1))) {
                // if one side is r and other side has no x, y
                // treat as polar
                return Function::FUNC_TYPE_POLAR | type_mod;
            }
        } else if (type_mod == 0 && util::is_varname(lhs)) {
            // A function definition with no args
            lhs.append("()");
            return Function::FUNC_TYPE_FUNC_DEFINITION;
        }
        // o.w. it is implicit
        // here we can just swap the lhs/rhs so the
        // func types < <= are not used
        if (eq_ch == '<') std::swap(lhs, rhs);
        type_mod &= ~Function::FUNC_TYPE_MOD_INEQ_LESS;
        return Function::FUNC_TYPE_IMPLICIT | type_mod;
    }
    // o.w. explicit
    lhs = expr_str; rhs = "y";
    return Function::FUNC_TYPE_EXPLICIT;
}

bool parse_polyline_expr(const std::string& expr_str, Function& func,
                         Environment& env, int x_var, int y_var, int t_var,
                         std::string& parse_err) {
    // Try to parse function as polyline expr
    size_t last_begin = 0, stkh = 0;
    bool has_comma = false;
    bool want_reparse = false;
    for (size_t i = 0; i < expr_str.size(); ++i) {
        const char c = expr_str[i];
        if (std::isspace(c)) continue;
        // Handle nested brackets
        if (util::is_open_bracket(c)) ++stkh;
        else if (util::is_close_bracket(c)) --stkh;
        switch(c) {
            case '(':
                if (stkh == 1) {
                    last_begin = i + 1;
                }
                break;
            case ',':
                if (stkh == 1) {
                    std::string tmp_err,
                                s = expr_str.substr(last_begin,
                                        i - last_begin);
                    Expr e1 = parse(s, env,
                                true, true, 0, &tmp_err);
                    util::rtrim(tmp_err);
                    if (tmp_err.size()) {
                        func.exprs.push_back(parse(s, env,
                                    false, true, 0, &parse_err));
                        want_reparse = true;
                    } else func.exprs.push_back(e1);
                    last_begin = i + 1;
                    has_comma = true;
                }
                break;
            case ')':
                if (stkh == 0) {
                    if (!has_comma) {
                        // Must have comma
                        parse_err.append("Polyline/parametric expression missing ,");
                        return false;
                    }
                    std::string tmp_err,
                                s = expr_str.substr(last_begin,
                                        i - last_begin);
                    Expr e1 = parse(s, env,
                                true, true, 0, &tmp_err);
                    util::rtrim(tmp_err);
                    if (tmp_err.size()) {
                        func.exprs.push_back(parse(s, env,
                                    false, true, 0, &parse_err));
                        want_reparse = true;
                    } else func.exprs.push_back(e1);
                    has_comma = false;
                }
                break;
            default:
                // Can't have things between ), (
                // shouldn't happen since should not detect as polyline
                if (stkh == 0) return false;
        }
    }
    // Polyline.
    if (parse_err.empty()) {
        for (Expr& e1 : func.exprs) {
            if (e1.has_var(x_var) || e1.has_var(y_var)) {
                // Can't have x,y, show warning
                func.exprs.clear();
                parse_err.append("x, y disallowed in tuple "
                    "(polyline/parametric equation)\n");
                break;
            } else if (e1.has_var(t_var)) {
                // Detect parametric equation
                if (func.exprs.size() != 2) {
                    func.exprs.clear();
                    parse_err.append("Parametric equation can't have "
                        "more than one tuple\n");
                    break;
                } else {
                    // Parametric detection: only single
                    // point and has t
                    func.type = Function::FUNC_TYPE_PARAMETRIC;
                }
            }
        }
    }
    return want_reparse;
}
}  // namespace

namespace util {
// Deduce gridline distances (plot coordinates) from normalized size:
// (input) step = (plot size / screen size) * initial screen size
// returns minor gridline distance, major gridline distance
std::pair<double, double> round125(double step) {
    double fa = 1.;
    if (step < 1) {
        double fan;
        int subdiv = 5;
        while(1./fa > step) {
            fan = fa; fa *= 2; subdiv = 5;
            if(1./fa <= step) break;
            fan = fa; fa /= 2; fa *= 5; subdiv = 5;
            if(1./fa <= step) break;
            fan = fa; fa *= 2; subdiv = 4;
        }
        return std::pair<double, double>(1./fan/subdiv, 1./fan);
    } else {
        double subdiv = 5.;
        while(fa < step) {
            fa *= 2; subdiv = 4;
            if(fa >= step) break;
            fa /= 2; fa *= 5; subdiv = 5;
            if(fa >= step) break;
            fa *= 2; subdiv = 5;
        }
        return std::pair<double, double>(fa * 1. / subdiv, fa);
    }
}
}  // namespace util

std::ostream& Function::to_bin(std::ostream& os) const {
    util::write_bin(os, name.size());
    os.write(name.c_str(), name.size());
    util::write_bin(os, type);
    util::write_bin(os, expr_str.size());
    os.write(expr_str.c_str(), expr_str.size());

    expr.to_bin(os); diff.to_bin(os);
    ddiff.to_bin(os); recip.to_bin(os);
    drecip.to_bin(os);
    for (int i = 0; i < 4; ++i) util::write_bin(os, line_color.data[i]);
    util::write_bin(os, tmin);
    util::write_bin(os, tmax);

    util::write_bin(os, exprs.size());
    for (size_t i = 0; i < exprs.size(); ++i) {
        exprs[i].to_bin(os);
    }
    return os;
}
std::istream& Function::from_bin(std::istream& is) {
    util::resize_from_read_bin(is, name);
    is.read(&name[0], name.size());
    util::read_bin(is, type);
    util::resize_from_read_bin(is, expr_str);
    is.read(&expr_str[0], expr_str.size());

    expr.from_bin(is); diff.from_bin(is);
    ddiff.from_bin(is); recip.from_bin(is);
    drecip.from_bin(is);
    for (int i = 0; i < 4; ++i) util::read_bin(is, line_color.data[i]);
    util::read_bin(is, tmin);
    util::read_bin(is, tmax);

    util::resize_from_read_bin(is, exprs);
    for (size_t i = 0; i < exprs.size(); ++i) {
        exprs[i].from_bin(is);
    }
    return is;
}

std::ostream& FuncDrawObj::to_bin(std::ostream& os) const {
    util::write_bin(os, points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        util::write_bin(os, points[i]);
    }
    util::write_bin(os, thickness);
    for (int i = 0; i < 4; ++i) util::write_bin(os, c.data[i]);
    util::write_bin(os, rel_func);
    util::write_bin(os, type);
    return os;
}

std::istream& FuncDrawObj::from_bin(std::istream& is) {
    util::resize_from_read_bin(is, points);
    for (size_t i = 0; i < points.size(); ++i) {
        util::read_bin(is, points[i]);
    }

    util::read_bin(is, thickness);
    for (int i = 0; i < 4; ++i) util::read_bin(is, c.data[i]);
    util::read_bin(is, rel_func);
    util::read_bin(is, type);

    return is;
}

bool Function::uses_parameter_t() const{
 return type == Function::FUNC_TYPE_POLAR ||
            type == Function::FUNC_TYPE_PARAMETRIC;
}

bool Plotter::View::operator==(const View& other) const {
    return shigh == other.shigh && swid == other.swid && xmin == other.xmin && xmax == other.xmax &&
           ymin == other.ymin && ymax == other.ymax;
}

bool Plotter::View::operator!=(const View& other) const {
    return !(other == *this);
}

Plotter::Plotter() : view{SCREEN_WIDTH, SCREEN_HEIGHT, 0., 0., 0., 0.}
{
    reset_view();
    curr_func = 0;
    {
        Function f;
        f.name = "f" + std::to_string(next_func_name++);
        f.expr_str = "";
        f.line_color = color::from_int(last_expr_color++);
        f.type = Function::FUNC_TYPE_EXPLICIT;
        funcs.push_back(f);
    }
    drag_trace = drag_view = false;
    drag_marker = -1;

    x_var = env.addr_of("x", false);
    y_var = env.addr_of("y", false);
    t_var = env.addr_of("t", false);
    r_var = env.addr_of("r", false);
    set_curr_func(0);
}

void Plotter::reparse_expr(size_t idx) {
    // Re-register some special vars, just in case they got deleted
    x_var = env.addr_of("x", false);
    y_var = env.addr_of("y", false);
    t_var = env.addr_of("t", false);

    if (idx == -1 ) idx = curr_func;
    auto& func = funcs[idx];
    func_error.clear();
    func.exprs.clear();
    {
        // Remove all spaces, unless comment
        if (func.expr_str.size() && func.expr_str[0] != '#') {
            std::string tmp = func.expr_str;
            func.expr_str.clear();
            for (auto c : tmp) {
                if (std::isspace(c)) continue;
                func.expr_str.push_back(c);
            }
        }
    }
    std::string lhs, rhs;
    func.type = detect_func_type(func.expr_str, lhs, rhs);
    if (func.expr_str.size() && func.expr_str[0] == '#') {
        // Comment
        lhs.clear(); rhs.clear();
    }

    int ftype_nomod = func.type & ~Function::FUNC_TYPE_MOD_ALL;
    bool want_reparse_all = false;
    switch(ftype_nomod) {
        case Function::FUNC_TYPE_EXPLICIT:
        case Function::FUNC_TYPE_EXPLICIT_Y:
        case Function::FUNC_TYPE_POLAR:
            func.expr = parse((rhs.size() == 1 &&
                            rhs[0] == result_var_name_for_type(func.type)) ?
                            lhs : rhs,
                        env,
                        true, // mode explicit
                        true, // quiet
                        0, &func_error);
            if (ftype_nomod == Function::FUNC_TYPE_POLAR &&
                    ftype_nomod != func.type) {
                func_error.append("Polar inequalities not currently supported");
            }
            break;
        case Function::FUNC_TYPE_IMPLICIT:
            func.expr = parse("(" + lhs + ")-(" + rhs + ")",
                    env, true, true, 0, &func_error);
            break;
        case Function::FUNC_TYPE_POLYLINE_CLOSED:
        case Function::FUNC_TYPE_POLYLINE:
            // This is either polyline or parametric
            {
                want_reparse_all = parse_polyline_expr(lhs, func, env,
                        x_var, y_var, t_var, func_error);
                // New variable may have been registered implicitly:
                // (a,b) creates variables a,b
                // so have to reparse all
            }
            break;
        case Function::FUNC_TYPE_FUNC_DEFINITION:
            {
                size_t funname_end = lhs.find('(');
                std::string funname = lhs.substr(0, funname_end);
                int64_t stkh = 0, last_begin = funname_end + 1;
                size_t argcount = 0, non_space_count = 0;
                std::vector<uint64_t> bindings;
                bool bad = false;
                for (int64_t i = funname_end + 1; i < lhs.size()- 1; ++i) {
                    const char cc = lhs[i];
                    if (cc == '(') {
                        ++stkh;
                    } else if (cc == ')') {
                        --stkh;
                    } else if (cc == ',') {
                        if (stkh == 0) {
                            std::string arg = lhs.substr(last_begin, i - last_begin);
                            if (!util::is_varname(arg)) {
                                bad = true;
                                break;
                            }
                            auto addr = env.addr_of(arg, false);
                            bindings.push_back(addr);
                            last_begin = i+1;
                            ++argcount;
                        }
                    } else if (!std::isspace(cc)) {
                        ++non_space_count;
                    }
                }
                if (!bad && non_space_count) {
                    ++argcount;
                    std::string arg = lhs.substr(last_begin, lhs.size() - last_begin - 1);
                    if (!util::is_varname(arg)) {
                        bad = true;
                    } else {
                        auto addr = env.addr_of(arg, false);
                        bindings.push_back(addr);
                    }
                } // else: function call with no args
                if (bad) {
                    func_error = "Invalid argument name in function definition " + lhs;
                } else {
                    Expr expr = parse(rhs, env,  true, true, 0, &func_error);
                    if (env.addr_of_func(funname) == -1) {
                        // Since new function defined, other expressions may change meaning,
                        // so must reparse all
                        want_reparse_all = true;
                    }
                    auto addr = env.def_func(funname, expr, bindings);
                    if (env.error_msg.size()) {
                        func_error = env.error_msg;
                    }
                }
            }
            break;
    }
    if (want_reparse_all) {
        for (size_t i = 0; i < funcs.size(); ++i) {
            if (i != idx) {
                reparse_expr(i);
            }
        }
        require_update = true;
    }

    if (!func.expr.is_null()) {
        // Optimize the main expression
        if (ftype_nomod != Function::FUNC_TYPE_PARAMETRIC &&
            ftype_nomod != Function::FUNC_TYPE_FUNC_DEFINITION)
            func.expr.optimize();

        // Compute derivatives, if explicit
        if (ftype_nomod == Function::FUNC_TYPE_EXPLICIT ||
            ftype_nomod == Function::FUNC_TYPE_EXPLICIT_Y) {
            uint64_t var = ftype_nomod == Function::FUNC_TYPE_EXPLICIT_Y ?  y_var : x_var;
            func.diff = func.expr.diff(var, env);
            if (!func.diff.is_null()) {
                func.ddiff = func.diff.diff(var, env);
            }
            else func.ddiff.ast[0] = OpCode::null;
            func.recip = Expr::constant(1.) / func.expr;
            func.recip.optimize();
            func.drecip = func.recip.diff(var, env);
        }
    } else func.diff.ast[0] = OpCode::null;

    // Optimize any polyline/parametric point expressions
    for (auto& point_expr : func.exprs) {
        point_expr.optimize();
    }

    if (ftype_nomod == Function::FUNC_TYPE_EXPLICIT
        || ftype_nomod == Function::FUNC_TYPE_EXPLICIT_Y) {
        // Register a function in env
        env.def_func(func.name, func.expr, { ftype_nomod == Function::FUNC_TYPE_EXPLICIT_Y ? y_var : x_var });
        if (env.error_msg.size()) {
            func_error = env.error_msg;
        }
    } else if (ftype_nomod != Function::FUNC_TYPE_FUNC_DEFINITION) {
        env.del_func(func.name);
    }
    loss_detail = false;
    require_update = true;
}

void Plotter::set_curr_func(size_t func_id) {
    if (func_id != curr_func)
        func_error.clear();
    reparse_expr(curr_func);
    curr_func = func_id;
    if (curr_func == -1) {
        curr_func = 0;
    }
    else if (curr_func >= funcs.size()) {
        // New function
        std::string tmp = funcs.back().expr_str;
        util::trim(tmp);
        Function f;
        f.type = Function::FUNC_TYPE_EXPLICIT;
        if (reuse_colors.empty()) {
            f.line_color =
                color::from_int(last_expr_color++);
        } else {
            f.line_color = reuse_colors.front();
            reuse_colors.pop_front();
        }
        f.name = "f" + std::to_string(next_func_name++);
        funcs.push_back(std::move(f));
    }
    focus_on_editor = true;
    require_update = true;
}

void Plotter::add_func() {
    set_curr_func(funcs.size());
}

void Plotter::delete_func(size_t idx) {
    if (idx == -1) idx = curr_func;
    if (idx >= funcs.size()) return;
    env.del_func(funcs[idx].name);
    if (funcs.size() > 1) {
        reuse_colors.push_back(funcs[idx].line_color);
        funcs.erase(funcs.begin() + idx);
        if (curr_func > idx || curr_func >= funcs.size()) {
            curr_func--;
        }
    } else {
        funcs[0].expr_str = "";
    }
    if (idx == curr_func) {
        set_curr_func(curr_func); // Update text without changing index
        reparse_expr();
    }
    focus_on_editor = true;
    require_update = true;
}

void Plotter::update_slider_var(size_t idx) {
    auto& sl = sliders[idx];
    if (sl.var_name_pre.size()) {
        sliders_vars.erase(sl.var_name_pre);
        sl.var_name_pre.clear();
    }
    util::trim(sl.var_name);
    if (sl.var_name == "") {
        sl.var_addr = -1;
    } else if (sliders_vars.count(sl.var_name)) {
        // Already has slider
        slider_error = "Duplicate slider for " +
            sl.var_name  + "\n";
        sl.var_addr = -1;
        sl.var_name.clear();
    } else if (is_var_name_reserved(sl.var_name)) {
        // Not allowed to set in slider (reserved)
        slider_error = sl.var_name  + " is reserved\n";
        sl.var_addr = -1;
        sl.var_name.clear();
    } else {
        sl.var_addr = env.addr_of(sl.var_name, false);
        sliders_vars.insert(sl.var_name);
        slider_error.clear();
        copy_slider_value_to_env(idx);
        for (size_t t = 0; t < funcs.size(); ++t)
            reparse_expr(t);
        require_update = true;
        sl.var_name_pre = sl.var_name;
    }
}

void Plotter::add_slider() {
    sliders.emplace_back();
    auto& sl = sliders.back();
    std::string var_name = "a";
    while (var_name[0] < 'z' &&
            sliders_vars.count(var_name)) {
        // Try to find next unused var name
        ++var_name[0];
    }
    sl.var_name = var_name;
    sl.var_name_pre = var_name;
    sl.var_addr = env.addr_of(sl.var_name, false);
    sl.val = 1.0;
    env.vars[sl.var_addr] = 1.0;
    for (size_t t = 0; t < funcs.size(); ++t)
        reparse_expr(t);
    sliders_vars.insert(var_name);
}

void Plotter::delete_slider(size_t idx) {
    sliders_vars.erase(sliders[idx].var_name);
    sliders.erase(sliders.begin() + idx);
    slider_error.clear();
}

void Plotter::copy_slider_value_to_env(size_t idx) {
    auto& sl = sliders[idx];
    if (sl.val > sl.hi) sl.hi = sl.val;
    if (sl.val < sl.lo) sl.lo = sl.val;
    if (~sl.var_addr) {
        env.vars[sl.var_addr] = sl.val;
        require_update = true;
    }
}

void Plotter::resize(int width, int height) {
    double wf = (view.xmax - view.xmin) * (1.*width / view.swid - 1.) / 2;
    double hf = (view.ymax - view.ymin) * (1.*height / view.shigh - 1.) / 2;
    view.xmax += wf; view.xmin -= wf;
    view.ymax += hf; view.ymin -= hf;

    view.swid = width;
    view.shigh = height;
    require_update = true;
}

void Plotter::reset_view() {
    double wid = 10. * view.swid /
        view.shigh * (SCREEN_HEIGHT * 1./ SCREEN_WIDTH);
    view.xmax = wid; view.xmin = -wid;
    view.ymax = 6.0; view.ymin = -6.0;
    require_update = true;
}

void Plotter::handle_key(int key, bool ctrl, bool shift, bool alt) {
#ifdef NIVALIS_EMSCRIPTEN
    static const double scale = 2.0;
#else
    static const double scale = 1.0;
#endif
    switch(key) {
        case 37: case 39: case 262: case 263:
            // LR Arrow
            {
                auto delta = (view.xmax - view.xmin) * 0.003 * scale;
                if (key == 37 || key == 263) delta = -delta;
                view.xmin += delta; view.xmax += delta;
            }
            require_update = true;
            break;
        case 38: case 40: case 264: case 265:
            {
                // UD Arrow
                auto delta = (view.ymax - view.ymin) * 0.003 * scale;
                if (key == 40 || key == 264) delta = -delta;
                view.ymin += delta; view.ymax += delta;
            }
            require_update = true;
            break;
        case 61: case 45:
        case 187: case 189:
        case 173:
            // Zooming +-
            {
                auto fa = 1 + 0.013 * ((key == 45 || key == 189 ||
                            key == 173) ? scale : -scale);
                auto dy = (view.ymax - view.ymin) * (fa - 1.) /2;
                auto dx = (view.xmax - view.xmin) * (fa - 1.) /2;
                if (shift) dy = 0.; // x-only
                if (alt) dx = 0.;  // y-only
                view.xmin -= dx; view.xmax += dx;
                view.ymin -= dy; view.ymax += dy;
                require_update = true;
            }
            break;
        case 48: case 72:
            // ctrl 0/H: Home
            if (ctrl) {
                reset_view();
            }
            break;
        case 80:
            // P: Polar grid
            if (!polar_grid) {
                polar_grid = true;
                require_update = true;
            }
            break;
        case 79:
            // O: Cartesian grid
            if (polar_grid) {
                polar_grid = false;
                require_update = true;
            }
            break;
        case 69:
            // E: Edit (focus editor)
            focus_on_editor = true;
            break;
    }
}

void Plotter::handle_mouse_down(int px, int py) {
    if (!drag_view && !drag_trace) {
        if (px >= 0 && py >= 0 &&
                py * view.swid + px < grid.size() &&
                ~grid[py * view.swid + px]) {
            // Show marker and either trace or drag view
            detect_marker_click(px, py, false, true);
            if (passive_marker_click_behavior ==
                    PASSIVE_MARKER_CLICK_DRAG_TRACE) {
                drag_trace = true;
            } else {
                // Drag view
                drag_view = true;
                dragx = px; dragy = py;
            }
        } else {
            // Begin dragging view
            drag_view = true;
            dragx = px; dragy = py;
        }
    }
}

void Plotter::handle_mouse_move(int px, int py) {
    if (~drag_marker &&
        drag_marker < pt_markers.size() &&
            (~pt_markers[drag_marker].drag_var_x ||
             ~pt_markers[drag_marker].drag_var_y)) {
        // Draggable marker
        auto& ptm = pt_markers[drag_marker];
        if (~ptm.drag_var_x) {
            env.vars[ptm.drag_var_x] = _SX_TO_X(px);
        }
        if (~ptm.drag_var_y) {
            env.vars[ptm.drag_var_y] = _SY_TO_Y(py);
        }
        int sx = (int)(std::min(std::max(_X_TO_SX(ptm.x), 0.f), (float) view.swid - 1.f) + 0.5f);
        int sy = (int)(std::min(std::max(_Y_TO_SY(ptm.y), 0.f), (float) view.shigh - 1.f) + 0.5f);
        if (~grid[sy * view.swid + sx]) detect_marker_click(sx, sy, true, false);
        require_update = true;
        return;
    }
    if (drag_view) {
        // Dragging background
        int dx = px - dragx;
        int dy = py - dragy;
        dragx = px; dragy = py;
        double fx = (view.xmax - view.xmin) / view.swid * dx;
        double fy = (view.ymax - view.ymin) / view.shigh * dy;
        view.xmax -= fx; view.xmin -= fx;
        view.ymax += fy; view.ymin += fy;
        require_update = true;
    } else if (px >= 0 && py >= 0 &&
            py * view.swid + px < grid.size() &&
            ~grid[py * view.swid + px]) {
        // Trace drag mode
        // Show marker if point marker under cursor
        detect_marker_click(px, py, !drag_trace, false);
    } else {
        marker_text.clear();
    }
}

void Plotter::handle_mouse_up(int px, int py) {
    // Stop dragging
    drag_trace = drag_view = false;
    drag_marker = -1;
    marker_text.clear();
}

void Plotter::handle_mouse_wheel(bool upwards, int distance, int px, int py) {
    drag_view = false;
    constexpr double multiplier = 0.012;
    double scaling;
    if (upwards) {
        scaling = exp(-log(distance) * multiplier);
    } else {
        scaling = exp(log(distance) * multiplier);
    }
    scaling = std::max(std::min(scaling, 100.), 0.01);
    double xdiff = (view.xmax - view.xmin) * (scaling-1.);
    double ydiff = (view.ymax - view.ymin) * (scaling-1.);

    double focx = std::min(std::max(px * 1. / view.swid, 0.0), 1.0);
    double focy = std::min(std::max(py * 1./ view.shigh, 0.0), 1.0);
    view.xmax += xdiff * (1-focx);
    view.xmin -= xdiff * focx;
    view.ymax += ydiff * focy;
    view.ymin -= ydiff * (1-focy);
    require_update = true;
}

std::ostream& Plotter::export_json(std::ostream& os, bool pretty) const {
    std::vector<json> jshell, jfuncs, jsliders;
    {
        size_t j = 0;
        // Export variable values
        for (size_t i = 0; i < env.vars.size(); ++i) {
            // Do not store x,y,z, etc.
            if (is_var_name_reserved(env.varname[i])) continue;
            // Do not store nan-valued variables
            if (std::isnan(env.vars[i]) ||
                    std::isinf(env.vars[i])) continue;
            std::ostringstream ss;
            ss << std::setprecision(16) << env.vars[i];
            jshell.push_back(env.varname[i] + " = " + ss.str());
        }
    }
    std::vector<int> fids;
    for (auto& func : funcs) {
        fids.push_back(std::atoi(func.name.substr(1).c_str()));
    }
    for (size_t i = 0; i < env.funcs.size(); ++i) {
        auto& f = env.funcs[i];
        if (f.name.size() > 1 &&
                f.name[0] == 'f' && f.n_args == 1) {
            // Do not store functions from editor like f0(x)
            int fid = std::atoi(f.name.substr(1).c_str());
            if (std::binary_search(fids.begin(), fids.end(), fid)) {
                continue;
            }
        }
        // Do not store null functions
        if (f.expr.is_null()) continue;
        std::string out = f.name + "(";
        for (size_t j = 0; j < f.n_args; ++j) {
            if (j) out.append(", ");
            out.append("$");
        }
        out.append(") = ");
        std::ostringstream ss;
        f.expr.repr(ss, env);
        jshell.push_back(out + ss.str());
    }
    // Export functions
    jfuncs.reserve(funcs.size());
    for (size_t i = 0; i < funcs.size(); ++i) {
        auto& func = funcs[i];
        json f {{"expr", func.expr_str},
                {"color", func.line_color.to_hex() },
                {"id", fids[i]} };
        if (func.uses_parameter_t()) {
            f["tmin"] = func.tmin;
            f["tmax"] = func.tmax;
        }
        jfuncs.push_back(f);
    }
    // Export sliders
    jsliders.reserve(sliders.size());
    for (auto& slider : sliders) {
        jsliders.push_back(json {
                {"var", slider.var_name},
                {"min", slider.lo },
                {"max", slider.hi },
                {"val", env.vars[slider.var_addr] }});
    }
    os << std::setprecision(17);
    if (pretty) os << std::setw(4);

    json jinternal = {
        {"next_color", last_expr_color },
        {"curr_func", curr_func},
    };
    if (reuse_colors.size()) {
        std::vector<json> jreuse_colors;
        // Export color list
        jreuse_colors.reserve(reuse_colors.size());
        for (const auto& col : reuse_colors) {
            jreuse_colors.push_back(col.to_hex());
        }
        jinternal["color_queue"] = jreuse_colors;

    }
    json j {
        {"view", // Export view data
            json {
                {"xmin", view.xmin},
                {"xmax", view.xmax},
                {"ymin", view.ymin},
                {"ymax", view.ymax},
                {"width", view.swid},
                {"height", view.shigh},
                {"polar", polar_grid},
            }
        }
    };
    if (jshell.size()) j["shell"] = jshell;
    if (jsliders.size()) j["sliders"] = jsliders;
    if (jfuncs.size()) j["funcs"] = jfuncs;
    return os << j;
}
std::istream& Plotter::import_json(std::istream& is, std::string* error_msg) {
    if (error_msg) {
        error_msg->clear();
    }
    try {
        funcs.clear();

        json j; is >> j;
        if (j.is_array()) {
            // Interpret as function list
            j = json {
                {"funcs",  j}
            };
        }

        {
            // Load environment
            env.clear();
            x_var = env.addr_of("x", false);
            y_var = env.addr_of("y", false);
            t_var = env.addr_of("t", false);
            r_var = env.addr_of("r", false);
            std::ostringstream ss;
            Shell tmpshell(env, ss);
            if (j.count("shell") && j["shell"].is_array()) {
                for (auto& line : j["shell"]) {
                    if (line.is_string()) {
                        if (!tmpshell.eval_line(line.get<std::string>())) {
                            std::cout << "json_load_err " << ss.str() << "\n";
                        }
                    }
                }
            }
        }

        // Load view
        polar_grid = false;
        if (j.count("view")) {
            json& jview = j["view"];
            if (jview.is_object()) {
                if (jview.count("xmin")) view.xmin = jview["xmin"].get<double>();
                if (jview.count("xmax")) view.xmax = jview["xmax"].get<double>();
                if (jview.count("xmin")) view.ymin = jview["ymin"].get<double>();
                if (jview.count("xmax")) view.ymax = jview["ymax"].get<double>();
                int old_swid = view.swid, old_shigh = view.shigh;
                if (jview.count("width")) view.swid = jview["width"].get<double>();
                if (jview.count("height")) view.shigh = jview["height"].get<double>();
                // Will resize bounds to fit screen
                // else will become too distorted
                resize(old_swid, old_shigh);
                if (jview.count("polar")) polar_grid = jview["polar"].get<bool>();
            }
        }

        // Load sliders
        sliders.clear();
        sliders_vars.clear();

        if (j.count("sliders") && j["sliders"].is_array()) {
            for (auto& slider : j["sliders"]) {
                if (!slider.is_object()) continue;
                size_t idx = sliders.size();
                sliders.emplace_back();
                if (slider.count("min"))
                    sliders[idx].lo = slider["min"].get<double>();
                if (slider.count("max"))
                    sliders[idx].hi = slider["max"].get<double>();
                if (slider.count("val")) {
                    sliders[idx].val = slider["val"].get<double>();
                }
                if (slider.count("var")) {
                    sliders[idx].var_name = slider["var"].get<std::string>();
                    update_slider_var(idx);
                    if (!slider.count("val")) {
                        sliders[idx].val = env.get(sliders[idx].var_name);
                    }
                }
            }
        }
        if (j.count("funcs") && j["funcs"].is_array()) {
            size_t idx = 0;
            for (auto& jfunc : j["funcs"]) {
                funcs.emplace_back();
                auto& f = funcs[idx];
                if (jfunc.is_object()) {
                    // Object form
                    if (jfunc.count("expr"))
                        f.expr_str = jfunc["expr"].get<std::string>();
                    if (jfunc.count("color")) {
                        auto& jcol = jfunc["color"];
                        if (jcol.is_string()) {
                            f.line_color = color::from_hex(
                                    jcol.get<std::string>());
                        } else if (jcol.is_number_integer()) {
                            f.line_color = color::from_int(
                                    (size_t)jcol.get<int>());
                        }
                    } else {
                        f.line_color = color::from_int(idx);
                        last_expr_color = idx+1;
                    }
                    if (jfunc.count("id")) {
                        int id = jfunc["id"].get<int>();
                        next_func_name = std::max(next_func_name, (size_t)id+1);
                        f.name = "f" + std::to_string(id);
                    } else {
                        f.name = "f" + std::to_string(next_func_name++);
                    }
                    if (jfunc.count("tmin")) {
                        f.tmin = jfunc["tmin"].get<double>();
                    }
                    if (jfunc.count("tmax")) {
                        f.tmax = jfunc["tmax"].get<double>();
                    }
                } else if (jfunc.is_string()) {
                    // Only expression
                    f.expr_str = jfunc.get<std::string>();
                    f.name = "f" + std::to_string(next_func_name++);
                    f.line_color = color::from_int(idx);
                    last_expr_color = idx+1;
                }
                reparse_expr(idx);
                ++idx;
            }
            for (size_t i = 0; i < funcs.size(); ++i) {
                // Reparse again in case of reference to other functions
                reparse_expr(i);
            }
        } else {
            // Ensure there is at lease 1 function left
            funcs.resize(1);
            funcs[0].expr_str = "";
            funcs[0].type = Function::FUNC_TYPE_EXPLICIT;
            funcs[0].line_color = color::from_int(last_expr_color++);
            funcs[0].name = "f" + std::to_string(next_func_name++);
        }

        reuse_colors.clear();
        if (j.count("internal")) {
            json& jint = j["internal"];
            if (jint.is_object()) {
                if (jint.count("curr_func")) {
                    int cf = jint["curr_func"].get<int>();
                    if (cf < 0 || cf >= funcs.size()) {
                        set_curr_func(0);
                    } else {
                        set_curr_func(cf);
                    }
                } else {
                    set_curr_func(0);
                }
                if (jint.count("next_color")) last_expr_color =
                    jint["next_color"].get<double>();
                if (jint.count("color_queue") &&
                        jint["color_queue"].is_array()) {
                    for (auto& jcol : jint["color_queue"]) {
                        reuse_colors.push_back(color::from_hex(
                                    jcol.get<std::string>()));
                    }
                }
            }
        }
    } catch (const json::parse_error& e) {
        if (error_msg != nullptr) {
            *error_msg = e.what();
        }
    }
    return is;
}

std::ostream& Plotter::export_binary_func_and_env(std::ostream& os) const {
    util::write_bin(os, curr_func);
    util::write_bin(os, funcs.size());
    for (size_t i = 0; i < funcs.size(); ++i) {
        funcs[i].to_bin(os);
    }
    util::write_bin(os, view);
    util::write_bin(os, x_var);
    util::write_bin(os, y_var);
    util::write_bin(os, t_var);
    util::write_bin(os, r_var);
    env.to_bin(os);
    return os;
}

std::istream& Plotter::import_binary_func_and_env(std::istream& is) {
    util::read_bin(is, curr_func);
    util::resize_from_read_bin(is, funcs);
    for (size_t i = 0; i < funcs.size(); ++i) {
        funcs[i].from_bin(is);
    }
    util::read_bin(is, view);
    util::read_bin(is, x_var);
    util::read_bin(is, y_var);
    util::read_bin(is, t_var);
    util::read_bin(is, r_var);
    env.from_bin(is);
    return is;
}

std::ostream& Plotter::export_binary_render_result(std::ostream& os) const {
    util::write_bin(os, draw_buf.size());
    for (size_t i = 0; i < draw_buf.size(); ++i) {
        draw_buf[i].to_bin(os);
    }
    util::write_bin(os, pt_markers.size());
    for (size_t i = 0; i < pt_markers.size(); ++i) {
        util::write_bin(os, pt_markers[i]);
    }
    util::write_bin(os, func_error.size());
    os.write(func_error.c_str(), func_error.size());
    util::write_bin(os, loss_detail);
    return os;
}
std::istream& Plotter::import_binary_render_result(std::istream& is) {
    util::resize_from_read_bin(is, draw_buf);
    for (size_t i = 0; i < draw_buf.size(); ++i) {
        draw_buf[i].from_bin(is);
    }
    util::resize_from_read_bin(is, pt_markers);
    for (size_t i = 0; i < pt_markers.size(); ++i) {
        util::read_bin(is, pt_markers[i]);
    }
    std::string func_error_tmp;
    util::resize_from_read_bin(is, func_error_tmp);
    is.read(&func_error_tmp[0], func_error_tmp.size());
    bool loss_detail_tmp;
    util::read_bin(is, loss_detail_tmp);
    if (loss_detail_tmp) {
        func_error = func_error_tmp;
    } else if (loss_detail) {
        func_error.clear();
    }
    loss_detail = loss_detail_tmp;
    require_update = true;
    return is;
}

void Plotter::detect_marker_click(int px, int py, bool no_passive, bool drag_var) {
    auto& ptm = pt_markers[grid[py * view.swid + px]];
    if (ptm.passive && no_passive) return;
    marker_posx = px; marker_posy = py + 20;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4) <<
        PointMarker::label_repr(ptm.label) << ptm.x << ", " << ptm.y;
    marker_text = ss.str();
    if (drag_var) {
        drag_marker = grid[py * view.swid + px];
    }
    if (ptm.passive && ~ptm.rel_func && ptm.rel_func != curr_func) {
        // Switch to function
        set_curr_func(ptm.rel_func);
        require_update = true;
    }
}
}  // namespace nivalis
