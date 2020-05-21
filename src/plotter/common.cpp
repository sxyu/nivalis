#include "plotter/common.hpp"


namespace nivalis {
namespace util {
std::pair<double, double> round125(double step) {
    double fa = 1., fan;
    if (step < 1) {
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

Plotter::Plotter(Environment& expr_env, const std::string& init_expr,
            int win_width, int win_height)
    : env(expr_env), swid(win_width), shigh(win_height)
{
    curr_func = 0;
    {
        Function f;
        f.name = "f" + std::to_string(next_func_name++);
        f.expr_str = init_expr;
        f.line_color = color::from_int(last_expr_color++);
        f.type = Function::FUNC_TYPE_EXPLICIT;
        funcs.push_back(f);
    }
    draglabel = dragdown = false;

    x_var = env.addr_of("x", false);
    y_var = env.addr_of("y", false);
    set_curr_func(0);
}
void Plotter::reparse_expr(size_t idx) {
    if (idx == -1 ) idx = curr_func;
    auto& func = funcs[idx];
    auto& expr = func.expr;
    auto& expr_str = func.expr_str;
    func.polyline.clear();
    size_t eqpos;
    // Marks whether this is a vlaid polyline expr
    bool valid_polyline;
    util::trim(expr_str);
    // Determine if function type is polyline
    if (expr_str.size() &&
            expr_str[0] == '(' && expr_str.back() == ')') {
        // Only try if of form (...)
        valid_polyline = true;
        std::string polyline_err;

        // Try to parse function as polyline expr
        size_t last_begin = 0, stkh = 0;
        bool has_comma = false;
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
                        func.polyline.push_back(parser(
                                    expr_str.substr(last_begin,
                                        i - last_begin), env,
                                    true, true));
                        if (parser.error_msg.size()) polyline_err = parser.error_msg;
                        last_begin = i + 1;
                        has_comma = true;
                    }
                    break;
                case ')':
                    if (stkh == 0) {
                        if (!has_comma) {
                            // Must have comma
                            valid_polyline = false;
                            break;
                        }
                        func.polyline.push_back(parser(
                                    expr_str.substr(last_begin,
                                        i - last_begin), env,
                                    true, true));
                        if (parser.error_msg.size()) polyline_err = parser.error_msg;
                        has_comma = false;
                    }
                    break;
                default:
                    // Can't have things between ), (
                    if (stkh == 0) valid_polyline = false;
            }
            if (!valid_polyline || polyline_err.size()) break;
        }
        if (valid_polyline) {
            // Polyline.
            func.type = Function::FUNC_TYPE_POLYLINE;
            if (polyline_err.empty()) {
                for (Expr& e1 : func.polyline) {
                    if (e1.has_var(x_var) || e1.has_var(y_var)) {
                        // Can't have x,y, show warning
                        func.polyline.clear();
                        polyline_err = "x, y disallowed\n";
                        break;
                    }
                }
            }
            // Keep as polyline type but show error
            // so that the user can see info about why it failed to parse
            if (polyline_err.size())
                func_error = "Polyline expr error: " + polyline_err;
            else func_error.clear();
        }
    } else valid_polyline = false;
    if (!valid_polyline) {
        // If failed to parse as polyline expr, try to detect if
        // it is an implicit function
        eqpos = util::find_equality(expr_str);
        if (~eqpos) {
            func.type = Function::FUNC_TYPE_IMPLICIT;
            auto lhs = expr_str.substr(0, eqpos),
                 rhs = expr_str.substr(eqpos+1);
            util::trim(lhs); util::trim(rhs);
            if (lhs == "y" || rhs == "y") {
                expr = parser(lhs == "y" ? rhs : lhs, env,
                        true, // explicit
                        true  // quiet
                        );
                if (!expr.has_var(y_var)) {
                    // if one side is y and other side has no y,
                    // treat as explicit function
                    func.type = Function::FUNC_TYPE_EXPLICIT;
                }
            }
            if (func.type == Function::FUNC_TYPE_IMPLICIT) {
                // If still valid, set expression to difference
                // i.e. rearrange so RHS is 0
                expr = parser(lhs, env, true, true)
                    - parser(rhs, env, true, true);
            }
        } else {
            func.type = Function::FUNC_TYPE_EXPLICIT;
            expr = parser(expr_str, env, true, true);
        }
        if (!expr.is_null()) {
            // Compute derivatives
            expr.optimize();
            if (func.type == Function::FUNC_TYPE_EXPLICIT) {
                func.diff = expr.diff(x_var, env);
                if (!func.diff.is_null()) {
                    func.ddiff = func.diff.diff(x_var, env);
                }
                else func.ddiff.ast[0] = OpCode::null;
                func.recip = Expr::constant(1.) / func.expr;
                func.recip.optimize();
                func.drecip = func.recip.diff(x_var, env);
            }
        } else func.diff.ast[0] = OpCode::null;
        func_error = parser.error_msg;
    }

    if (parser.error_msg.empty()) func_error.clear();
    if (func.type == Function::FUNC_TYPE_EXPLICIT) {
        // Register a function in env
        env.def_func(func.name, func.expr, { x_var });
        if (env.error_msg.size()) {
            func_error = env.error_msg;
        }
    } else {
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
        if (!tmp.empty()) {
            Function f;
            f.type = Function::FUNC_TYPE_EXPLICIT;
            if (reuse_colors.empty()) {
                f.line_color =
                    color::from_int(last_expr_color++);
            } else {
                f.line_color = reuse_colors.front();
                reuse_colors.pop();
            }
            f.name = "f" + std::to_string(next_func_name++);
            funcs.push_back(std::move(f));
        } else {
            // If last function is empty,
            // then stay on it and do not create a new function
            curr_func = funcs.size() - 1;
        }
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
        reuse_colors.push(funcs[idx].line_color);
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
    util::trim(sl.var_name);
    sliders_vars.erase(sl.var_name);
    if (sl.var_name == "") {
        sl.var_addr = -1;
    } else if (sliders_vars.count(sl.var_name)) {
        // Already has slider
        slider_error = "Duplicate slider for " +
            sl.var_name  + "\n";
        sl.var_addr = -1;
        sl.var_name.clear();
    } else if (sl.var_name == "x" || sl.var_name == "y") {
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
    sl.var_addr = env.addr_of(sl.var_name, false);
    env.vars[sl.var_addr] = sl.lo;
    for (size_t t = 0; t < funcs.size(); ++t)
        reparse_expr(t);
    sliders_vars.insert(var_name);
}

void Plotter::delete_slider(size_t idx) {
    sliders_vars.erase(sliders[idx].var_name);
    sliders.erase(sliders.begin() + idx);
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
    double wf = (xmax - xmin) * (1.*width / swid - 1.) / 2;
    double hf = (ymax - ymin) * (1.*height / shigh - 1.) / 2;
    xmax += wf; xmin -= wf;
    ymax += hf; ymin -= hf;

    swid = width;
    shigh = height;
    require_update = true;
}

void Plotter::reset_view() {
    double wid = 10. * swid / shigh * (600./ 1000.);
    xmax = wid; xmin = -wid;
    ymax = 6.0; ymin = -6.0;
    require_update = true;
}

void Plotter::handle_key(int key, bool ctrl, bool alt) {
    switch(key) {
        case 37: case 39: case 262: case 263:
            // LR Arrow
            {
                auto delta = (xmax - xmin) * 0.003;
                if (key == 37 || key == 263) delta = -delta;
                xmin += delta; xmax += delta;
            }
            require_update = true;
            break;
        case 38: case 40: case 264: case 265:
            {
                // UD Arrow
                auto delta = (ymax - ymin) * 0.003;
                if (key == 40 || key == 264) delta = -delta;
                ymin += delta; ymax += delta;
            }
            require_update = true;
            break;
        case 61: case 45:
        case 187: case 189:
            // Zooming +-
            {
                auto fa = (key == 45 || key == 189) ? 1.013 : 0.987;
                auto dy = (ymax - ymin) * (fa - 1.) /2;
                auto dx = (xmax - xmin) * (fa - 1.) /2;
                if (ctrl) dy = 0.; // x-only
                if (alt) dx = 0.;  // y-only
                xmin -= dx; xmax += dx;
                ymin -= dy; ymax += dy;
                require_update = true;
            }
            break;
        case 48: case 72:
            // ctrl H: Home
            if (ctrl) {
                reset_view();
            }
            break;
        case 69:
            // E: Edit (focus tb)
            focus_on_editor = true;
            break;
    }
}

void Plotter::handle_mouse_down(int px, int py) {
    if (!dragdown) {
        if (px >= 0 && py >= 0 &&
                py * swid + px < grid.size() &&
                ~grid[py * swid + px]) {
            // Show marker
            detect_marker_click(px, py);
            draglabel = true;
        } else {
            // Begin dragging window
            dragx = px; dragy = py;
            dragdown = true;
            xmaxi = xmax; xmini = xmin;
            ymaxi = ymax; ymini = ymin;
        }
    }
}

void Plotter::handle_mouse_move(int px, int py) {
    if (dragdown) {
        // Dragging background
        marker_text.clear();
        int dx = px - dragx;
        int dy = py - dragy;
        double fx = (xmax - xmin) / swid * dx;
        double fy = (ymax - ymin) / shigh * dy;
        xmax = xmaxi - fx; xmin = xmini - fx;
        ymax = ymaxi + fy; ymin = ymini + fy;
        require_update = true;
    } else if (px >= 0 && py >= 0 &&
            py * swid + px < grid.size() &&
            ~grid[py * swid + px]) {
        // Show marker if point marker under cursor
        detect_marker_click(px, py, !draglabel);
    } else {
        marker_text.clear();
    }
}

void Plotter::handle_mouse_up(int px, int py) {
    // Stop dragging
    draglabel = dragdown = false;
    marker_text.clear();
}

void Plotter::handle_mouse_wheel(bool upwards, int distance, int px, int py) {
    dragdown = false;
    constexpr double multiplier = 0.012;
    double scaling;
    if (upwards) {
        scaling = exp(-log(distance) * multiplier);
    } else {
        scaling = exp(log(distance) * multiplier);
    }
    scaling = std::min(scaling, 100.);
    double xdiff = (xmax - xmin) * (scaling-1.);
    double ydiff = (ymax - ymin) * (scaling-1.);

    double focx = px * 1. / swid;
    double focy = py * 1./ shigh;
    xmax += xdiff * (1-focx);
    xmin -= xdiff * focx;
    ymax += ydiff * focy;
    ymin -= ydiff * (1-focy);
    require_update = true;
}

void Plotter::detect_marker_click(int px, int py, bool no_passive) {
    auto& ptm = pt_markers[grid[py * swid + px]];
    if (ptm.passive && no_passive) return;
    marker_posx = px; marker_posy = py + 20;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(4) <<
        PointMarker::label_repr(ptm.label) << ptm.x << ", " << ptm.y;
    marker_text = ss.str();
    if (~ptm.rel_func && ptm.rel_func != curr_func) {
        // Switch to function
        set_curr_func(ptm.rel_func);
        require_update = true;
    }
}
}  // namespace nivalis
