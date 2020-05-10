#pragma once
#ifndef _PLOTTER_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
#define _PLOTTER_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
#include <cstddef>
#include <utility>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <string>
#include <sstream>
#include <queue>
#include <set>
#include "expr.hpp"
#include "parser.hpp"
#include "util.hpp"

namespace nivalis {

namespace color {
    // RGB color
    struct color {
        color();
        color(unsigned clr);
        color(uint8_t, uint8_t, uint8_t);
        uint8_t r, g, b;
    };
    enum _colors {
        // Hex codes of common colors
        WHITE = 0xFFFFFF,
        BLACK = 0x000000,
        DARK_GRAY = 0xa9a9a9,
        GRAY = 0x808080,
        LIGHT_GRAY = 0xd3d3d3,
        DIM_GRAY = 0x696969,
        RED  = 0xFF0000,
        ROYAL_BLUE = 0x4169e1,
        GREEN = 0x008000,
        ORANGE = 0xffa500,
        PURPLE = 0x800080
    };
    const color from_int(size_t color_index);
}  // namespace color

// Represents function in plotter
struct Function {
    // Function expression
    Expr expr;
    // Derivative, 2nd derivative
    Expr diff, ddiff;
    // Reciprocal, derivative of reciprocal
    Expr recip, drecip;
    // Color of function's line
    color::color line_color;
    // Original expression string in editor
    std::string expr_str;
    // Whether function is implicit
    bool is_implicit;
    // Store x-positions of currently visible roots and extrema
    // does not include y-int (0)
    std::vector<double> roots_and_extrema;
};

// Marks a single point on the plot which can be clicked
struct PointMarker {
    double x, y; // position
    int sx, sy;  // location on screen (if applicable)
    enum {
        // Label to show when hovered over/clicked
        LABEL_NONE,
        LABEL_X_INT,
        LABEL_Y_INT,
        LABEL_LOCAL_MIN,
        LABEL_LOCAL_MAX,
        LABEL_INTERSECTION,
        LABEL_INFLECTION_PT,
    } label;
    size_t rel_func; // associated function, -1 if N/A
    // passive: show marker with title,position on click
    // else: show .. on hover
    bool passive;
    // Get string corresponding to each label enum entry
    static constexpr const char* label_repr(int lab) {
        switch(lab) {
            case LABEL_X_INT: return "x-intercept\n";
            case LABEL_Y_INT: return "y-intercept\n";
            case LABEL_LOCAL_MIN: return "local minimum\n";
            case LABEL_LOCAL_MAX: return "local maximum\n";
            case LABEL_INTERSECTION: return "intersection\n";
            case LABEL_INFLECTION_PT: return "inflection point\n";
            default: return "";
        }
    }
};

/** Generic GUI plotter logic; used in plot_gui.hpp with appropriate backend
 * Register GUI event handlers to call handle_xxx
 * Register resize handler to call resize
 * Call draw() in drawing event/loop
 * Call reset_view() to reset view, delete_func() delete function,
 * set_curr_func() to set current function, etc.
 *
 * * Required Backend API
 * void close();                                                        close GUI window
 * void focus_editor();                                                 focus on current function's editor, if applicable
 * void focus_background();                                             focus on background of window (off editor), if applicable

 * void update(bool force);                                             update the GUI (redraw); force: force draw, else may ignore

 * void update_editor(int func_id, std::string contents);               update the text in the (expression) editor for given function
 * std::string read_editor(int func_id);                                get text in the editor for given function

 * void show_error(std::string error_msg);                              show error message (empty = clear)
 * void set_func_name(std::string name);                                set the 'function name' label if available

 * void show_marker_at(const PointMarker& marker, int px, int py);     show the marker (point label) at the position, with given marker data
 * void hide_marker();                                                 hide the marker

 * * Required Graphics adapter API
 * void line(int ax, int ay, int bx, int by, color::color color);                 draw line
 * void rectangle(int x, int y, int w, int h, bool fill, color::color color);     draw rectangle (filled or non-filled)
 * void rectangle(bool fill, color::color color);                                 fill entire view
 * void set_pixel(int x, int y, color::color color);                              set single pixel
 * void string(int x, int y, std::string s, color::color c);                      draw a string
 * */
template<class Backend, class Graphics>
class Plotter {
public:
    Plotter(Backend& backend,
            const Environment& expr_env, std::string init_expr,
            int win_width, int win_height) :
            env(expr_env), be(backend), 
            swid(win_width), shigh(win_height) {
        curr_func = 0;
        funcs.emplace_back();
        auto& f = funcs.back();
        f.expr_str = init_expr;
        f.line_color = color::from_int(last_expr_color++);
        f.is_implicit = false;
        draglabel = dragdown = false;

        x_var = env.addr_of("x", false);
        y_var = env.addr_of("y", false);

        be.update_editor(curr_func, funcs[curr_func].expr_str);
        set_curr_func(0);
    }

    void draw(Graphics& graph) {
        if (xmin >= xmax) xmax = xmin + 1e-9;
        if (ymin >= ymax) ymax = ymin + 1e-9;
        graph.rectangle(true, color::WHITE);
        double xdiff = xmax - xmin, ydiff = ymax - ymin;

        int sx0 = 0, sy0 = 0;
        int cnt_visible_axis = 0;
        // Draw axes
        if (ymin <= 0 && ymax >= 0) {
            double y0 = ymax / (ymax - ymin);
            sy0 = static_cast<int>(shigh * y0);
            graph.line(0, sy0-1, swid, sy0-1, color::DARK_GRAY);
            graph.line(0, sy0, swid, sy0, color::DARK_GRAY);
            graph.line(0, sy0+1, swid, sy0+1, color::DARK_GRAY);
            ++cnt_visible_axis;
        }
        else if (ymin > 0) {
            sy0 = shigh - 26;
        }
        if (xmin <= 0 && xmax >= 0) {
            double x0 = - xmin / (xmax - xmin);
            sx0 = static_cast<int>(swid * x0);
            graph.line(sx0-1,0, sx0-1, swid, color::DARK_GRAY);
            graph.line(sx0,0, sx0, shigh, color::DARK_GRAY);
            graph.line(sx0+1,0, sx0+1, swid, color::DARK_GRAY);
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
            graph.line(sxi,0, sxi, shigh, color::LIGHT_GRAY);
            xl = xstep * idx + xli;
            ++idx;
        }
        idx = 0;
        while (yb <= yt) {
            int syi = static_cast<int>(shigh * (ymax - yb) / (ymax - ymin));
            graph.line(0, syi, swid, syi, color::LIGHT_GRAY);
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
            graph.line(sxi,0, sxi, shigh, color::GRAY);

            if (xml != 0) {
                std::stringstream sstm;
                sstm << std::setprecision(4) << xml;
                graph.string(sxi-7, sy0+5, sstm.str(), color::BLACK);
            }
            ++idx;
            xml = xmstep * idx + xmli;
        }
        idx = 0;
        while (ymb <= ymt) {
            int syi = static_cast<int>(shigh * (ymax - ymb) / (ymax - ymin));
            graph.line(0, syi, swid, syi, color::GRAY);

            std::stringstream sstm;
            if (ymb != 0) {
                sstm << std::setprecision(4) << ymb;
                graph.string(sx0+5, syi-6, sstm.str(), color::BLACK);
            }
            ++idx;
            ymb = ymstep * idx + ymbi;
        }

        // Draw 0
        if (cnt_visible_axis == 2) {
            graph.string(sx0 - 12, sy0 + 5, "0", color::BLACK);
        }

        // Draw functions
        pt_markers.clear(); pt_markers.reserve(500);
        grid.clear(); grid.resize(shigh* swid, -1);
        for (size_t exprid = 0; exprid < funcs.size(); ++exprid) {
            bool reinit = true;
            int psx = -1, psy = -1;
            auto& func = funcs[exprid];
            auto& expr = func.expr;
            const color::color& func_color = func.line_color;
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
                                if (curr_func == exprid) {
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
                                if (curr_func == exprid) {
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
                static const int rect_rad = 3, click_rect_rad = 4;
                // Draw a line and construct markers along the line
                // to allow clicking
                auto draw_line = [&](double psx, double psy, double sx, double sy, double x, double y) {
                    graph.line(psx, psy, sx,sy, func_color);
                    graph.line(psx+1, psy, sx+1,sy, func_color);
                    graph.line(psx, psy+1, sx,sy+1, func_color);
                    int miny, maxy, minx, maxx;
                    if (sy < psy) {
                        miny = sy; maxy = psy; minx = sx; maxx = psx;
                    } else {
                        miny = psy; maxy = sy; minx = psx; maxx = sx;
                    }

                    // Construct a (passive) point marker for this point,
                    // so that the user can click to see the position
                    size_t new_marker_idx = pt_markers.size();
                    pt_markers.emplace_back();
                    auto& new_marker = pt_markers.back();
                    new_marker.x = x; new_marker.y = y;
                    new_marker.rel_func = exprid;
                    new_marker.label = PointMarker::LABEL_NONE;
                    new_marker.sx = (sx + psx) / 2; new_marker.sy = (sy + psy) / 2;
                    new_marker.passive = true;

                    int dfx = maxx-minx, dfy = maxy-miny;
                    for (int syp = std::max(miny, 0); syp <= std::min(maxy, shigh-1); syp += 4) {
                        // Assign to grid
                        for (int r = std::max(syp - click_rect_rad, 0); r <= std::min(syp + click_rect_rad, shigh-1); ++r) {
                            for (int c = std::max<int>(sx - click_rect_rad, 0); c <= std::min<int>(sx + click_rect_rad, swid-1); ++c) {
                                int existing_marker_idx = grid[r * swid + c];
                                if (~existing_marker_idx) {
                                    auto& existing_marker = pt_markers[existing_marker_idx];
                                    if (util::sqr_dist(existing_marker.sx, existing_marker.sy, c, r) <=
                                            util::sqr_dist(new_marker.sx, new_marker.sy, c, r)) {
                                        // If existing marker is closer, do not overwrite it
                                        continue;
                                    }
                                }
                                grid[r * swid + c] = new_marker_idx;
                            }
                        }
                    }
                    if (curr_func == exprid) {
                        // current function, thicken
                        graph.line(psx-1, psy, sx-1,sy, func_color);
                        graph.line(psx, psy-1, sx,sy-1, func_color);
                    }
                };
                // Find roots, asymptotes, extrema
                const double SIDE_ALLOW = xdiff/20;
#define NEWTON_ARGS x_var, x, env, EPS_STEP, EPS_ABS, MAX_ITER, xmin - SIDE_ALLOW, xmax + SIDE_ALLOW
                if (func.diff.ast[0] != OpCode::null) {
                    for (int sxd = 0; sxd < swid; sxd += 5) {
                        const double x = sxd*1. * xdiff / swid + xmin;
                        double root = expr.newton(NEWTON_ARGS, &func.diff); push_if_valid(root, roots_and_extrema);
                        double asymp = func.recip.newton(NEWTON_ARGS, &func.drecip); push_if_valid(asymp, asymps);
                        double extr = func.diff.newton(NEWTON_ARGS, &func.ddiff); push_if_valid(extr, roots_and_extrema);
                    }
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
                                    draw_line(psx, psy, sx, sy, x, y);
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
                                draw_line(psx, psy, sx, sy, as - 1e-6, yp);
                            }
                        }
                    }
                    ++as_idx;
                    prev_as = as;
                    prev_asd = asd;
                }
                // Draw roots/extrema/y-int
                if (func.expr.ast[0] != OpCode::null) {
                    env.vars[x_var] = 0;
                    double y = expr(env);
                    if (!std::isnan(y) && !std::isinf(y)) 
                        push_if_valid(0., roots_and_extrema); // y-int
                }
                for (double x : roots_and_extrema) {
                    env.vars[x_var] = x;
                    double y = expr(env);
                    double dy = func.diff(env);
                    double ddy = func.ddiff(env);

                    int sy = static_cast<int>((ymax - y) / ydiff * shigh);
                    int sx = static_cast<int>((x - xmin) / xdiff * swid);
                    graph.rectangle(sx-rect_rad, sy-rect_rad, 2*rect_rad+1, 2*rect_rad+1, true, color::LIGHT_GRAY);
                    graph.rectangle(sx-rect_rad, sy-rect_rad, 2*rect_rad+1, 2*rect_rad+1, false, func.line_color);
                    size_t idx = pt_markers.size();
                    pt_markers.emplace_back();
                    auto& ptm = pt_markers.back();
                    ptm.sx = sx; ptm.sy = sy;
                    ptm.label =
                        x == 0. ? PointMarker::LABEL_Y_INT :
                        std::fabs(dy) > 1e-6 ? PointMarker::LABEL_X_INT :
                        ddy > 1e-6 ? PointMarker::LABEL_LOCAL_MIN :
                        ddy < -1e-6 ? PointMarker::LABEL_LOCAL_MAX:
                        PointMarker::LABEL_INFLECTION_PT;
                    ptm.passive = false;
                    ptm.rel_func = exprid;
                    for (int r = std::max(sy - click_rect_rad, 0); r <= std::min(sy + click_rect_rad, shigh-1); ++r) {
                        for (int c = std::max(sx - click_rect_rad, 0); c <= std::min(sx + click_rect_rad, swid-1); ++c) {
                            grid[r * swid + c] = idx;
                        }
                    }
                }

                // Function intersection
                if (func.diff.ast[0] != OpCode::null) {
                    for (size_t exprid2 = 0; exprid2 < exprid; ++exprid2) {
                        auto& func2 = funcs[exprid2];
                        if (func2.diff.ast[0] == OpCode::null) continue;
                        if (func2.is_implicit) continue; // not supported right now
                        Expr sub_expr = expr - func2.expr;
                        Expr diff_sub_expr = sub_expr.diff(x_var, env);
                        if (diff_sub_expr.ast[0] == OpCode::null) continue;
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
                            graph.rectangle(sx-rect_rad, sy-rect_rad, 2*rect_rad, 2*rect_rad, true, color::LIGHT_GRAY);
                            graph.rectangle(sx-rect_rad-1, sy-rect_rad-1, 2*rect_rad+1, 2*rect_rad+1, false, func.line_color);
                            graph.rectangle(sx-rect_rad, sy-rect_rad, 2*rect_rad+1, 2*rect_rad+1, false, func2.line_color);
                            size_t idx = pt_markers.size();
                            pt_markers.emplace_back();
                            auto& ptm = pt_markers.back();
                            ptm.label = PointMarker::LABEL_INTERSECTION;
                            ptm.x = x; ptm.y = y;
                            ptm.passive = false;
                            ptm.rel_func = -1;
                            for (int r = std::max(sy - click_rect_rad, 0); r <= std::min(sy + click_rect_rad, shigh-1); ++r) {
                                for (int c = std::max(sx - click_rect_rad, 0); c <= std::min(sx + click_rect_rad, swid-1); ++c) {
                                    grid[r * swid + c] = idx;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void reparse_expr(size_t idx = -1) {
        if (idx == -1 ) idx = curr_func;
        auto& func = funcs[idx];
        auto& expr = func.expr;
        auto& expr_str = func.expr_str;
        expr_str = be.read_editor(idx);
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
        be.show_error(parser.error_msg);
        expr.optimize();
        be.update();
    }

    void set_curr_func(int func_id) {
        if (func_id != curr_func) 
            be.show_error("");
        reparse_expr(curr_func);
        curr_func = func_id;
        std::string suffix;
        if (curr_func == -1) {
            curr_func = 0;
            return;
        }
        else if (curr_func >= funcs.size()) {
            std::string tmp = funcs.back().expr_str;
            util::trim(tmp);
            if (!tmp.empty()) {
                funcs.emplace_back();
                funcs.back().is_implicit = false;
                if (reuse_colors.empty()) {
                    funcs.back().line_color =
                        color::from_int(last_expr_color++);
                } else {
                    funcs.back().line_color = reuse_colors.front();
                    reuse_colors.pop();
                }
                suffix = " [new]";
            } else {
                // If last function is empty,
                // then stay on it and do not create a new function
                curr_func = funcs.size()-1;
                return;
            }
        }
        be.set_func_name("Function " + std::to_string(curr_func) + suffix);
        be.update_editor(func_id, funcs[func_id].expr_str);
        be.update(true);
    }

    void delete_func(int idx = -1) {
        if (idx == -1) idx = curr_func;
        if (funcs.size() > 1) {
            reuse_colors.push(funcs[idx].line_color);
            funcs.erase(funcs.begin() + idx);
            if (curr_func >= funcs.size()) {
                curr_func--;
            }
        } else {
            funcs[0].expr_str = "";
            be.update_editor(0,
                    funcs[0].expr_str);
        }
        if (idx == curr_func) {
            set_curr_func(curr_func); // Update text without changing index
            reparse_expr();
        } else {
            be.update_editor(idx, funcs[idx].expr_str);
        }
        be.update(true);
    }

    void resize(int width, int height) {
        double wf = (xmax - xmin) * (1.*width / swid - 1.) / 2;
        double hf = (ymax - ymin) * (1.*height / shigh - 1.) / 2;
        xmax += wf; xmin -= wf;
        ymax += hf; ymin -= hf;

        swid = width;
        shigh = height;
        be.update();
    }

    void reset_view() {
        xmax = 10.0; xmin = -10.0;
        ymax = 6.0; ymin = -6.0;
        be.update(true);
        be.focus_background();
    }

    void handle_key(int key, bool ctrl, bool alt) {
        switch(key) {
            case 81:
                // q: quit
                be.close();
                break;
            case 37: case 39: case 262: case 263:
                    // LR Arrow
                {
                    auto delta = (xmax - xmin) * 0.02;
                    if (key == 37 || key == 263) delta = -delta;
                    xmin += delta; xmax += delta;
                }
                be.update();
                break;
            case 38: case 40: case 264: case 265:
                {
                    auto delta = (ymax - ymin) * 0.02;
                    if (key == 40 || key == 264) delta = -delta;
                    ymin += delta; ymax += delta;
                }
                be.update();
                break;
            case 61: case 45:
            case 187: case 189:
                // Zooming +-
                {
                    auto fa = (key == 45 || key == 189) ? 1.05 : 0.95;
                    auto dy = (ymax - ymin) * (fa - 1.) /2;
                    auto dx = (xmax - xmin) * (fa - 1.) /2;
                    if (ctrl) dy = 0.; // x-only
                    if (alt) dx = 0.;  // y-only
                    xmin -= dx; xmax += dx;
                    ymin -= dy; ymax += dy;
                    be.update();
                }
                break;
            case 72:
                // ctrl H: Home
                if (ctrl) {
                    if (!alt) xmax = 10.0; xmin = -10.0;
                    ymax = 6.0; ymin = -6.0;
                    be.update();
                }
                break;
            case 69:
                // ctrl E: Edit (focus tb)
                if (ctrl) {
                    be.focus_editor();
                }
                break;
        }
    }

    void handle_mouse_down(int px, int py) {
        if (!dragdown) {
            if (px >= 0 && py >= 0 &&
                    py * swid + px < grid.size() &&
                    ~grid[py * swid + px]) {
                // Show marker
                auto& ptm = pt_markers[grid[py * swid + px]];
                be.show_marker_at(ptm, px, py);
                draglabel = true;
                if (~ptm.rel_func && ptm.rel_func != curr_func) {
                    // Switch to function
                    set_curr_func(ptm.rel_func);
                    be.update(true);
                }
            } else {
                // Begin dragging window
                dragx = px; dragy = py;
                dragdown = true;
                xmaxi = xmax; xmini = xmin;
                ymaxi = ymax; ymini = ymin;
            }
        }
    }

    void handle_mouse_move(int px, int py) {
        if (dragdown) {
            // Dragging background
            be.hide_marker();
            int dx = px - dragx;
            int dy = py - dragy;
            double fx = (xmax - xmin) / swid * dx;
            double fy = (ymax - ymin) / shigh * dy;
            xmax = xmaxi - fx; xmin = xmini - fx;
            ymax = ymaxi + fy; ymin = ymini + fy;
            be.update();
        } else if (px >= 0 && py >= 0 &&
                py * swid + px < grid.size() &&
                ~grid[py * swid + px]) {
            // Show marker if point marker under cursor
            auto& ptm = pt_markers[grid[py * swid + px]];
            if (ptm.passive && !draglabel) return;
            be.show_marker_at(ptm, px, py);
        } else {
            be.hide_marker();
        }
    }

    void handle_mouse_up(int width, int height) {
        // Stop dragging
        draglabel = dragdown = false;
        be.hide_marker();
    }

    void handle_mouse_wheel(bool upwards, int distance, int px, int py) {
        dragdown = false;
        constexpr double multiplier = 0.01;
        double scaling;
        if (upwards) {
            scaling = exp(-log(distance) * multiplier);
        } else {
            scaling = exp(log(distance) * multiplier);
        }
        double xdiff = (xmax - xmin) * (scaling-1.);
        double ydiff = (ymax - ymin) * (scaling-1.);

        double focx = px * 1. / swid;
        double focy = py * 1./ shigh;
        xmax += xdiff * (1-focx);
        xmin -= xdiff * focx;
        ymax += ydiff * focy;
        ymin -= ydiff * (1-focy);
        be.update();
    }

    int swid, shigh;            // Screen size

    double xmax = 10, xmin = -10;
    double ymax = 6, ymin = -6;             // Function area bounds
    size_t curr_func = 0;                   // Currently selected function

    std::vector<Function> funcs;            // Functions
    std::vector<PointMarker> pt_markers;    // Point markers
    std::vector<size_t> grid;               // Grid containing marker id
                                            // at every pixel, row major
                                            // (-1 if no marker)

    Environment env;
private:
    Backend& be;
    Parser parser;

    std::queue<color::color> reuse_colors;   // Reusable colors
    size_t last_expr_color = 0;               // Next available color index if no reusable
                                              // one present(by color::from_int)
    uint32_t x_var, y_var;                    // x,y variable addresses
    bool dragdown, draglabel;
    int dragx, dragy;
    double xmaxi, xmini, ymaxi, ymini;
};

}  // namespace nivalis
#endif // ifndef _PLOTTER_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
