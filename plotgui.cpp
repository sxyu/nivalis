#include "plotgui.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <set>
#include <utility>
#include <queue>
#include <algorithm>

#include "expr.hpp"
#include "parser.hpp"
#include "util.hpp"

#include "nana/gui/widgets/form.hpp"
#include "nana/gui/widgets/label.hpp"
#include "nana/gui/widgets/textbox.hpp"
#include "nana/paint/graphics.hpp"
#include "nana/gui/drawing.hpp"
#include "nana/gui.hpp"
#include "nana/gui/widgets/button.hpp"
// #include <nana/gui/dragger.hpp>
// #include <nana/threads/pool.hpp>

namespace nivalis {
using namespace nana;

namespace {
const color& color_from_int(size_t color_index)
{
    static const color palette[] = {
        colors::red, colors::royal_blue, colors::green, colors::orange,
        colors::purple, colors::black,
        color(255, 220, 0), color(201, 13, 177), color(34, 255, 94),
        color(255, 65, 54), color(255, 255, 64), color(0, 116, 217),
        color(27, 133, 255), color(190, 18, 240), color(20, 31, 210),
        color(75, 20, 133), color(255, 219, 127), color(204, 204, 57),
        color(112, 153, 61), color(64, 204, 46), color(112, 255, 1),
        color(170, 170, 170), color(225, 30, 42)
    };
    return palette[color_index % (sizeof palette / sizeof palette[0])];
}
}  // namespace

struct Function {
    bool is_implicit;
    Expr expr;
    // Derivative, 2nd derivative
    Expr diff, ddiff;
    // Reciprocal, derivative of reciprocal
    Expr recip, drecip;
    color line_color;
    std::string expr_str;
    // Store x-positions of currently visible roots and extrema
    std::vector<double> roots_and_extrema;
};

struct PointMarker {
    double x, y;
    std::string label;
    size_t rel_func;
};

struct PlotGUI::impl {
    std::chrono::high_resolution_clock::time_point lazy_start;
    void update(bool force = false) {
        auto finish = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - lazy_start).count();
        if (force || elapsed > 1000) {
            dw.update();
            lazy_start = std::chrono::high_resolution_clock::now();
        }
    }

    impl(Environment expr_env, std::string init_expr)
        : fm(API::make_center(1000, 600)), dw(fm),
             env(expr_env) {
        fm.caption("Nivalis");
        // Func label
        label label_func(fm, rectangle{20, 20, 250, 30});
        label_func.caption("Function 0");
        label_func.bgcolor(colors::white);
        // Error label
        label label_err(fm, rectangle{20, 115, 250, 30});
        label_err.transparent(true);
        // Point marker label
        label label_point_marker(fm, rectangle{20, 20, 250, 30});
        label_point_marker.bgcolor(colors::white);
        label_point_marker.hide();
        // Textbook for editing functions
        textbox tb(fm, rectangle{20, 40, 250, 40});
        tb.caption(init_expr);

        size_t edit_expr_idx = 0;
        auto reparse_expr = [&]() {
            size_t idx = edit_expr_idx;
            auto& func = funcs[idx];
            auto& expr = func.expr;
            auto& expr_str = func.expr_str;
            expr_str = tb.caption();
            size_t eqpos = util::find_equality(expr_str);
            if ((func.is_implicit = ~eqpos)) {
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
                        func.is_implicit = false;
                    }
                }
                if (func.is_implicit) {
                    expr = parser(lhs, env, true, true) - parser(rhs, env, true, true);
                }
            } else {
                expr = parser(expr_str, env, true, true);
            }
            func.diff = expr.diff(x_var, env);
            func.ddiff = func.diff.diff(x_var, env);
            func.recip = Expr::constant(1.) / func.expr;
            func.drecip = func.recip.diff(x_var, env);
            label_err.caption(parser.error_msg);
            expr.optimize();
            update();
        };
        tb.events().key_char([&](const nana::arg_keyboard& arg) {
            switch(arg.key) {
            case keyboard::enter:
                arg.ignore = true;
                break;
            case keyboard::escape:
                arg.ignore = true;
                fm.focus();
                break;
            }
        });
        auto seek_func = [&](int delta){
            edit_expr_idx += delta;
            std::string suffix;
            if (edit_expr_idx == -1) {
                edit_expr_idx = 0;
                return;
            }
            else if (edit_expr_idx >= funcs.size()) {
                std::string tmp = funcs.back().expr_str;
                util::trim(tmp);
                if (!tmp.empty()) {
                    funcs.emplace_back();
                    funcs.back().is_implicit = false;
                    if (reuse_colors.empty()) {
                        funcs.back().line_color = color_from_int(last_expr_color++);
                    } else {
                        funcs.back().line_color = reuse_colors.front();
                        reuse_colors.pop();
                    }
                    suffix = " [new]";
                } else {
                    // If last function is empty,
                    // then stay on it and do not create a new function
                    edit_expr_idx = funcs.size()-1;
                    return;
                }
            }
            label_func.caption("Function " +
                    std::to_string(edit_expr_idx) + suffix);
            tb.caption(funcs[edit_expr_idx].expr_str);
            update(true);
        };
        auto delete_func = [&]() {
            if (funcs.size() > 1) {
                reuse_colors.push(funcs[edit_expr_idx].line_color);
                funcs.erase(funcs.begin() + edit_expr_idx);
                if (edit_expr_idx >= funcs.size()) {
                    edit_expr_idx--;
                }
            } else {
                funcs[0].expr_str = "";
            }
            seek_func(0);
            reparse_expr();
            update(true);
        };
        tb.events().key_release([&](const nana::arg_keyboard& arg) {
            reparse_expr();
            if (arg.key == 38 || arg.key == 40) {
                // Up/down: go to different funcs
                seek_func(arg.key == 38 ? -1 : 1);
            } else if (arg.key == 127) {
                // Delete
                delete_func();
            }
        });

        // Home button
        button btn_home(fm, rectangle{20, 80, 130, 30});
        btn_home.bgcolor(colors::white);
        btn_home.edge_effects(false);
        btn_home.caption(L"Reset View");
        btn_home.events().click([&](){
            xmax = 10.0; xmin = -10.0;
            ymax = 6.0; ymin = -6.0;
            update();
            fm.focus();
        });

        // Prev/next/del-func button
        button btn_prev(fm, rectangle{150, 80, 40, 30});
        btn_prev.bgcolor(colors::white);
        btn_prev.edge_effects(false);
        btn_prev.caption(L"<");
        btn_prev.events().click([&](){ seek_func(-1); fm.focus(); });
        button btn_next(fm, rectangle{190, 80, 40, 30});
        btn_next.bgcolor(colors::white);
        btn_next.edge_effects(false);
        btn_next.caption(L">");
        btn_next.events().click([&](){ seek_func(1); fm.focus(); });
        button btn_del(fm, rectangle{230, 80, 40, 30});
        btn_del.bgcolor(colors::white);
        btn_del.edge_effects(false);
        btn_del.caption(L"x");
        btn_del.events().click([&](){ delete_func(); fm.focus(); });

        // Form keyboard handle
        auto keyn_handle = fm.events().key_release([&](const nana::arg_keyboard&arg)
        {
            switch(arg.key) {
                case 81:
                    // q: quit
                    fm.close();
                    break;
                case 37: case 38: case 39: case 40:
                    // Arrows
                    if (arg.key & 1) {
                        // LR
                        auto delta = (xmax - xmin) * 0.02;
                        if (arg.key == 37) delta = -delta;
                        xmin += delta; xmax += delta;
                    } else {
                        // UD
                        auto delta = (ymax - ymin) * 0.02;
                        if (arg.key == 40) delta = -delta;
                        ymin += delta; ymax += delta;
                    }
                    update();
                    break;
                case 61: case 45:
                case 187: case 189:
                    // Zooming +-
                    {
                        auto fa = (arg.key == 45 || arg.key == 189) ? 1.05 : 0.95;
                        auto dy = (ymax - ymin) * (fa - 1.) /2;
                        auto dx = (xmax - xmin) * (fa - 1.) /2;
                        if (arg.ctrl) dy = 0.; // x-only
                        if (arg.alt) dx = 0.;  // y-only
                        xmin -= dx; xmax += dx;
                        ymin -= dy; ymax += dy;
                        update();
                    }
                    break;
                case 72:
                    // ctrl H: Home
                    if (arg.ctrl) {
                        if (!arg.alt) xmax = 10.0; xmin = -10.0;
                        ymax = 6.0; ymin = -6.0;
                        update();
                    }
                    break;
                case 69:
                    // E: Edit (focus tb)
                    tb.focus();
                    break;
            }
        });

        // Dragging + scrolling
        point dragpt, lastpt;
        bool dragdown = false;
        double xmaxi, xmini, ymaxi, ymini;
        auto down_handle = fm.events().mouse_down([&](const nana::arg_mouse&arg)
        {
            if (!dragdown) {
                lastpt = dragpt = arg.pos;
                dragdown = true;
                xmaxi = xmax; xmini = xmin;
                ymaxi = ymax; ymini = ymin;
            }
        });

        auto move_handle =
            fm.events().mouse_move(
                    [&](const nana::arg_mouse&arg) {
                    int px = arg.pos.x, py = arg.pos.y;
                if (dragdown) {
                    label_point_marker.hide();
                    lastpt = arg.pos;
                    double swid = fm.size().width;
                    double shigh = fm.size().height;
                    int dx = px - dragpt.x;
                    int dy = py - dragpt.y;
                    double fx = (xmax - xmin) / swid * dx;
                    double fy = (ymax - ymin) / shigh * dy;
                    xmax = xmaxi - fx; xmin = xmini - fx;
                    ymax = ymaxi + fy; ymin = ymini + fy;
                    update();
                } else if (px >= 0 && py >= 0 && py < grid.size() && px < grid[0].size() && ~grid[py][px]) {
                    label_point_marker.show();
                    auto& ptm = pt_markers[grid[py][px]];
                    label_point_marker.move(px, py+20);
                    label_point_marker.caption(
                            ptm.label + std::to_string(ptm.x) + ", " + std::to_string(ptm.y));
                } else {
                    label_point_marker.hide();
                }
            });

        auto up_handle = fm.events().mouse_up([&dragdown](const nana::arg_mouse&arg) { dragdown = false; });
        auto leave_handle = fm.events().mouse_leave([&dragdown](const nana::arg_mouse&arg) { dragdown = false; });
        int win_wid = 1000, win_hi = 600;
        auto resize_handle = fm.events().resized([this, &win_wid, &win_hi](const nana::arg_resized&arg) {
            double wf = (xmax - xmin) * (1.*arg.width / win_wid - 1.) / 2;
            double hf = (ymax - ymin) * (1.*arg.height / win_hi - 1.) / 2;
            xmax += wf; xmin -= wf;
            ymax += hf; ymin -= hf;

            win_wid = arg.width;
            win_hi = arg.height;
            update();
        });


        auto scroll_handle = fm.events().mouse_wheel([&](
                    const nana::arg_wheel&arg)
        {
            dragdown = false;
            constexpr double multiplier = 0.01;
            double scaling;
            if (arg.upwards) {
                scaling = exp(-log(arg.distance) * multiplier);
            } else {
                scaling = exp(log(arg.distance) * multiplier);
            }
            double xdiff = (xmax - xmin) * (scaling-1.);
            double ydiff = (ymax - ymin) * (scaling-1.);

            double focx = arg.pos.x * 1. / fm.size().width;
            double focy = arg.pos.y * 1./ fm.size().height;
            xmax += xdiff * (1-focx);
            xmin -= xdiff * focx;
            ymax += ydiff * focy;
            ymin -= ydiff * (1-focy);
            update();
        });

        x_var = env.addr_of("x", false);
        y_var = env.addr_of("y", false);

        funcs.emplace_back();
        funcs.back().expr_str = init_expr;
        funcs.back().line_color = color_from_int(last_expr_color++);
        funcs.back().is_implicit = false;
        reparse_expr();
        // *** Main drawing code ***
        dw.draw([this, &edit_expr_idx](paint::graphics& graph) {
            graph.rectangle(true, colors::white);
            int swid = fm.size().width, shigh = fm.size().height;
            double xdiff = xmax - xmin, ydiff = ymax - ymin;

            int sx0 = 0, sy0 = 0;
            int cnt_visible_axis = 0;
            // Draw axes
            if (ymin <= 0 && ymax >= 0) {
                double y0 = ymax / (ymax - ymin);
                sy0 = static_cast<int>(shigh * y0);
                graph.line(point(0, sy0-1), point(swid, sy0-1), colors::dark_gray);
                graph.line(point(0, sy0), point(swid, sy0), colors::dark_gray);
                graph.line(point(0, sy0+1), point(swid, sy0+1), colors::dark_gray);
                ++cnt_visible_axis;
            }
            else if (ymin > 0) {
                sy0 = shigh - 26;
            }
            if (xmin <= 0 && xmax >= 0) {
                double x0 = - xmin / (xmax - xmin);
                sx0 = static_cast<int>(swid * x0);
                graph.line(point(sx0-1,0), point(sx0-1, swid), colors::dark_gray);
                graph.line(point(sx0,0), point(sx0, shigh), colors::dark_gray);
                graph.line(point(sx0+1,0), point(sx0+1, swid), colors::dark_gray);
                ++cnt_visible_axis;
            }
            else if (xmax < 0) {
                sx0 = swid - 50;
            }

            // Draw lines
            auto round125 = [](double step) {
                int fa = 1, fan;
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
                    int subdiv = 5;
                    while(fa < step) {
                        fa *= 2; subdiv = 4;
                        if(fa >= step) break;
                        fa /= 2; fa *= 5; subdiv = 5;
                        if(fa >= step) break;
                        fa *= 2; subdiv = 5;
                    }
                    return std::pair<double, double>(fa * 1. / subdiv, fa);
                }
            };
            double ystep, xstep, ymstep, xmstep;
            std::tie(ystep, ymstep) = round125((ymax - ymin) / shigh * 600 / 10.8);
            std::tie(xstep, xmstep) = round125((xmax - xmin) / swid * 1000 / 18.);
            double xli = std::ceil(xmin / xstep) * xstep;
            double xr = std::floor(xmax / xstep) * xstep;
            double ybi = std::ceil(ymin / ystep) * ystep;
            double yt = std::floor(ymax / ystep) * ystep;
            double yb = ybi, xl = xli;
            int idx = 0;
            while (xl <= xr) {
                int sxi = static_cast<int>(swid * (xl - xmin) / (xmax - xmin));
                graph.line(point(sxi,0), point(sxi, shigh), colors::light_gray);
                xl = xstep * idx + xli;
                ++idx;
            }
            idx = 0;
            while (yb <= yt) {
                int syi = static_cast<int>(shigh * (ymax - yb) / (ymax - ymin));
                graph.line(point(0, syi), point(swid, syi), colors::light_gray);
                yb = ystep * idx + ybi;
                ++idx;
            }
            // Larger lines + text
            double xmli = std::ceil(xmin / xmstep) * xmstep;
            double xmr = std::floor(xmax / xmstep) * xmstep;
            double ymbi = std::ceil(ymin / ymstep) * ymstep;
            double ymt = std::ceil(ymax / ymstep) * ymstep;
            double ymb = ymbi, xml = xmli;
            idx = 0;
            while (xml <= xmr) {
                int sxi = static_cast<int>(swid * (xml - xmin) / (xmax - xmin));
                graph.line(point(sxi,0), point(sxi, shigh), colors::gray);

                if (xml != 0) {
                    std::stringstream sstm;
                    sstm << std::setprecision(4) << xml;
                    graph.string(point(sxi-7, sy0+5), sstm.str());
                }
                ++idx;
                xml = xmstep * idx + xmli;
            }
            idx = 0;
            while (ymb <= ymt) {
                int syi = static_cast<int>(shigh * (ymax - ymb) / (ymax - ymin));
                graph.line(point(0, syi), point(swid, syi), colors::gray);

                std::stringstream sstm;
                if (ymb != 0) {
                    sstm << std::setprecision(4) << ymb;
                    graph.string(point(sx0+5, syi-6), sstm.str());
                }
                ++idx;
                ymb = ymstep * idx + ymbi;
            }

            // Draw 0
            if (cnt_visible_axis == 2) {
                graph.string(point(sx0 - 12, sy0 + 5), "0");
            }

            // Draw functions
            std::vector<PointMarker> new_pt_markers;
            std::vector<std::vector<size_t> > new_grid;
            new_grid.resize(shigh, std::vector<size_t>(swid, -1));
            for (size_t exprid = 0; exprid < funcs.size(); ++exprid) {
                bool reinit = true;
                int psx = -1, psy = -1;
                auto& func = funcs[exprid];
                auto& expr = func.expr;
                const color& func_color = func.line_color;
                if (func.is_implicit) {
                    // implicit function
                    const int interval = 3;
                    std::vector<double> line(swid / interval + 2);
                    for (int sx = 0; sx < swid; sx += interval) {
                        int idx = 0;
                        for (int sy = 0; sy < shigh; sy += interval, ++idx) {
                            const double x = 1.*sx / swid * xdiff + xmin;
                            const double y = (shigh - sy)*1. / shigh * ydiff + ymin;
                            env.vars[x_var] = x; env.vars[y_var] = y;
                            double z = expr(env);
                            if (std::fabs(z) > 10.0) {
                                line[idx] = line[idx+1] = z;
                                ++idx; sy += interval;
                                continue;
                            }
                            if (sy > 0) {
                                double zup = line[idx-1];
                                if ((zup <= 0 && z >= 0) || (zup >= 0 && z <= 0)) {
                                    graph.set_pixel(sx-1, sy-1, func_color);
                                    graph.set_pixel(sx, sy-1, func_color);
                                    graph.set_pixel(sx+1, sy-1, func_color);
                                    if (edit_expr_idx == exprid) {
                                        // current function, thicken
                                        graph.set_pixel(sx-1, sy-2, func_color);
                                        graph.set_pixel(sx, sy-2, func_color);
                                        graph.set_pixel(sx+1, sy-2, func_color);
                                    }
                                }
                            }
                            if (sx > 0) {
                                double zleft = line[idx];
                                if ((zleft <= 0 && z >= 0) || (zleft >= 0 && z <= 0)) {
                                    graph.set_pixel(sx-2, sy-1, func_color);
                                    graph.set_pixel(sx-2, sy, func_color);
                                    graph.set_pixel(sx-2, sy+1, func_color);
                                    if (edit_expr_idx == exprid) {
                                        // current function, thicken
                                        graph.set_pixel(sx-1, sy-1, func_color);
                                        graph.set_pixel(sx-1, sy, func_color);
                                        graph.set_pixel(sx-1, sy+1, func_color);
                                    }
                                }
                            }
                            line[idx] = z;
                        }
                    }
                } else {
                    // explicit function
                    env.vars[y_var] = std::numeric_limits<double>::quiet_NaN();
                    std::set<double> roots_and_extrema, asymps;
                    size_t idx = 0;
                    static const double EPS_STEP  = 1e-8;
                    static const double EPS_ABS   = 1e-6;
                    static const double MIN_DIST_BETWEEN_ROOTS  = 1e-4;
                    static const int MAX_ITER     = 20;
                    auto push_if_valid = [this](double value, std::set<double>& st) {
                        if (!std::isnan(value) && !std::isinf(value) &&
                            value >= xmin && value <= xmax) {
                            auto it = st.lower_bound(value);
                            double cdist = 1.;
                            if (it != st.end()) cdist = *it - value;
                            if (it != st.begin()) {
                                auto itp = it; --itp;
                                cdist = std::min(cdist, value - *itp);
                            }
                            if (cdist >= MIN_DIST_BETWEEN_ROOTS) st.insert(value);
                        }
                    };
                    auto draw_line = [&](double psx, double psy, double sx, double sy) {
                        graph.line(point(psx, psy), point(sx,sy), func_color);
                        graph.line(point(psx+1, psy), point(sx+1,sy), func_color);
                        graph.line(point(psx, psy+1), point(sx,sy+1), func_color);
                        if (edit_expr_idx == exprid) {
                            // current function, thicken
                            graph.line(point(psx-1, psy), point(sx-1,sy), func_color);
                            graph.line(point(psx, psy-1), point(sx,sy-1), func_color);
                        }
                    };
                    // Find roots, asymptotes, extrema
                    const double SIDE_ALLOW = xdiff/20;
#define NEWTON_ARGS x_var, x, env, EPS_STEP, EPS_ABS, MAX_ITER, xmin - SIDE_ALLOW, xmax + SIDE_ALLOW
                    for (int sxd = 0; sxd < swid; sxd += 5) {
                        const double x = sxd*1. * xdiff / swid + xmin;
                        double root = expr.newton(NEWTON_ARGS, &func.diff); push_if_valid(root, roots_and_extrema);
                        double asymp = func.recip.newton(NEWTON_ARGS, &func.drecip); push_if_valid(asymp, asymps);
                        double extr = func.diff.newton(NEWTON_ARGS, &func.ddiff); push_if_valid(extr, roots_and_extrema);
                    }
                    // Copy into function
                    std::vector<double> tmp; tmp.resize(roots_and_extrema.size());
                    std::copy(roots_and_extrema.begin(), roots_and_extrema.end(), tmp.begin());
                    tmp.swap(func.roots_and_extrema);

                    asymps.insert(xmin); asymps.insert(xmax);
                    double prev_as = xmin, prev_asd, asd;
                    size_t as_idx = 0;
                    // Draw function from asymptote to asymptote
                    for (double as : asymps) {
                        asd = (as - xmin) / xdiff * swid;
                        if (as_idx > 0) {
                            bool connector = as_idx > 1;
                            // Draw func between asymptotes
                            for (double sxd = prev_asd + 1e-7; sxd < asd;) {
                                const double x = sxd / swid * xdiff + xmin;
                                env.vars[x_var] = x;
                                double y = expr(env);

                                if (!std::isnan(y)) {
                                    if (y> ymax + ydiff) y = ymax + ydiff;
                                    else if (y < ymin - ydiff) y = ymin - ydiff;
                                    int sy = static_cast<int>((ymax - y) / ydiff * shigh);
                                    int sx = static_cast<int>(sxd);
                                    if (reinit) {
                                        if (!(y > ymax || y < ymin)) {
                                            reinit = false;
                                        }
                                    }
                                    if (connector) {
                                        connector = false;
                                        env.vars[x_var] = prev_as + 1e-6;
                                        double yp = expr(env);
                                        if (yp > ymax && sy > 0) {
                                            psx = prev_asd;
                                            psy = 0;
                                            reinit = false;
                                        }
                                        if (yp < ymin && sy < shigh) {
                                            psx = prev_asd;
                                            psy = shigh;
                                            reinit = false;
                                        }
                                    }
                                    if (!reinit && ~psx) {
                                        draw_line(psx, psy, sx, sy);
                                        if (y > ymax || y < ymin) {
                                            reinit = true;
                                            psx = -1;
                                        }
                                    }
                                    psx = sx;
                                    psy = sy;
                                } else {
                                    reinit = true;
                                }
                                if (asymps.size() > 2 && asymps.size() < 100) {
                                    if ((as_idx > 1 && sxd - prev_asd < 1.) ||
                                        (as_idx < asymps.size() - 1 && asd - sxd < 1.)) {
                                        sxd += 0.1;
                                    } else if ((as_idx > 1 && sxd - prev_asd < 5.) ||
                                        (as_idx < asymps.size() - 1 && asd - sxd < 5.)) {
                                        sxd += 0.2;
                                    } else {
                                        sxd += 1.0;
                                    }
                                } else {
                                    sxd += 1.0;
                                }
                            }
                            // Connect next asymptote
                            if (as_idx != asymps.size() - 1) {
                                env.vars[x_var] = as - 1e-6;
                                double yp = expr(env);
                                int sx = -1, sy;
                                if (yp > ymax) {
                                    sx = asd;
                                    sy = 0;
                                    if (psy <= 0) sx = -1;
                                }
                                if (yp < ymin) {
                                    sx = asd;
                                    sy = shigh;
                                    if (psy >= shigh) sx = -1;
                                }
                                if (~sx) {
                                    draw_line(psx, psy, sx, sy);
                                }
                            }
                        }
                        ++as_idx;
                        prev_as = as;
                        prev_asd = asd;
                    }
                    // Draw roots/extrema/y-int
                    push_if_valid(0., roots_and_extrema); // y-int
                    for (double x : roots_and_extrema) {
                        env.vars[x_var] = x;
                        double y = expr(env);
                        double dy = func.diff(env);
                        double ddy = func.ddiff(env);

                        int sy = static_cast<int>((ymax - y) / ydiff * shigh);
                        int sx = static_cast<int>((x - xmin) / xdiff * swid);
                        static const int rect_rad = 3, click_rect_rad = 5;
                        graph.rectangle(rectangle(sx-rect_rad, sy-rect_rad, 2*rect_rad+1, 2*rect_rad+1), true, colors::light_gray);
                        graph.rectangle(rectangle(sx-rect_rad, sy-rect_rad, 2*rect_rad+1, 2*rect_rad+1), false, func.line_color);
                        size_t idx = new_pt_markers.size();
                        new_pt_markers.emplace_back();
                        auto& ptm = new_pt_markers.back();
                        ptm.label = 
                                    x == 0. ? "y-intercept\n" :
                                    std::fabs(dy) > 1e-6 ? "x-intercept\n" :
                                    ddy > 1e-6 ? "local minimum\n" :
                                    ddy < -1e-6 ? "local maximum\n":
                                    "";
                        ptm.x = x; ptm.y = y;
                        for (int r = std::max(sy - click_rect_rad, 0); r <= std::min(sy + click_rect_rad, shigh-1); ++r) {
                            for (int c = std::max(sx - click_rect_rad, 0); c <= std::min(sx + click_rect_rad, swid-1); ++c) {
                                new_grid[r][c] = idx;
                            }
                        }
                    }

                    // Function intersection
                    for (size_t exprid2 = 0; exprid2 < exprid; ++exprid2) {
                        auto& func2 = funcs[exprid2];
                        if (func2.is_implicit) continue; // not supported right now
                        Expr sub_expr = expr - func2.expr;
                        Expr diff_sub_expr = sub_expr.diff(x_var, env);
                        std::set<double> st;
                        for (int sxd = 0; sxd < swid; sxd += 5) {
                            const double x = sxd*1. * xdiff / swid + xmin;
                            double root = sub_expr.newton(NEWTON_ARGS, &diff_sub_expr);
                            push_if_valid(root, st);
                        }
                        for (double x : st) {
                            env.vars[x_var] = x;
                            double y = expr(env);
                            int sy = static_cast<int>((ymax - y) / ydiff * shigh);
                            int sx = static_cast<int>((x - xmin) / xdiff * swid);
                            static const int rect_rad = 3, click_rect_rad = 5;
                            graph.rectangle(rectangle(sx-rect_rad, sy-rect_rad, 2*rect_rad, 2*rect_rad), true, colors::light_gray);
                            graph.rectangle(rectangle(sx-rect_rad, sy-rect_rad, 2*rect_rad+1, 2*rect_rad+1), false, func.line_color);
                            graph.rectangle(rectangle(sx-rect_rad+1, sy-rect_rad+1, 2*rect_rad, 2*rect_rad), false, func2.line_color);
                            size_t idx = new_pt_markers.size();
                            new_pt_markers.emplace_back();
                            auto& ptm = new_pt_markers.back();
                            ptm.label = "intersection\n";
                            ptm.x = x; ptm.y = y;
                            for (int r = std::max(sy - click_rect_rad, 0); r <= std::min(sy + click_rect_rad, shigh-1); ++r) {
                                for (int c = std::max(sx - click_rect_rad, 0); c <= std::min(sx + click_rect_rad, swid-1); ++c) {
                                    new_grid[r][c] = idx;
                                }
                            }
                        }
                    }
                }
            }
            new_grid.swap(grid);
            new_pt_markers.swap(pt_markers);
        });
        update(true);

        fm.show();
        exec();
    }

    double xmax = 10, xmin = -10;
    double ymax = 6, ymin = -6;

private:
    form fm;
    drawing dw;

    Environment env;
    Parser parser;
    std::vector<Function> funcs;
    std::vector<PointMarker> pt_markers;
    std::vector<std::vector<size_t> > grid;
    std::queue<color> reuse_colors;
    size_t last_expr_color = 0;
    uint32_t x_var, y_var;

    // nana::threads::pool pool;
};

PlotGUI::PlotGUI(Environment& env, const std::string& init_expr)
    : pImpl(std::make_unique<impl>(env, init_expr)) {
}

PlotGUI::~PlotGUI() =default;

}  // namespace nivalis
