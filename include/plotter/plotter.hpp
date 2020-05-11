#pragma once
#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#endif

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
#include <array>
#include <vector>
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

 * void update_editor(size_t func_id, std::string contents);               update the text in the (expression) editor for given function
 * std::string read_editor(size_t func_id);                                get text in the editor for given function

 * void show_error(std::string error_msg);                              show error message (empty = clear)
 * void set_func_name(std::string name);                                set the 'function name' label if available

 * void show_marker_at(const PointMarker& marker, int px, int py);     show the marker (point label) at the position, with given marker data
 * void hide_marker();                                                 hide the marker

 * * Required Graphics adapter API
 * void line(float ax, float ay, float bx, float by, color::color color, float thickness = 1.0);          draw line (ax, ay) -- (bx, by)
 *                                                                                                        thickness shall be an integer
 * void polyline(const std::vector<std::array<float, 2> >& points, color::color c, float thickness = 1.);   draw polyline
 * void rectangle(float x, float y, float w, float h, bool fill, color::color color);                     draw rectangle (filled or non-filled)
 * void clear(color::color color);                                                                        fill entire view (clear)
 * void set_pixel(float x, float y, color::color color);                                                  set single pixel
 * void string(float x, float y, std::string s, color::color c);                                          draw a string
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

    // Draw axes and grid
    void draw_grid(Graphics& graph) {
        int sx0 = 0, sy0 = 0;
        int cnt_visible_axis = 0;
        // Draw axes
        if (ymin <= 0 && ymax >= 0) {
            double y0 = ymax / (ymax - ymin);
            sy0 = static_cast<int>(shigh * y0);
            graph.line(0, sy0, swid, sy0, color::DARK_GRAY, 2.);
            ++cnt_visible_axis;
        }
        else if (ymin > 0) {
            sy0 = shigh - 26;
        }
        if (xmin <= 0 && xmax >= 0) {
            double x0 = - xmin / (xmax - xmin);
            sx0 = static_cast<int>(swid * x0);
            graph.line(sx0,0, sx0, shigh, color::DARK_GRAY, 3.);
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
    }

    // Draw grid and functions
    void draw(Graphics& graph) {
        if (xmin >= xmax) xmax = xmin + 1e-9;
        if (ymin >= ymax) ymax = ymin + 1e-9;
        graph.clear(color::WHITE);
        draw_grid(graph);
        double xdiff = xmax - xmin, ydiff = ymax - ymin;

        // * Constants
        // Newton's method parameters
        static const double EPS_STEP  = 1e-8;
        static const double EPS_ABS   = 1e-6;
        static const int    MAX_ITER     = 20;
        // Shorthand for Newton's method arguments
#define NEWTON_ARGS x_var, x, env, EPS_STEP, EPS_ABS, MAX_ITER, \
        xmin - NEWTON_SIDE_ALLOW, xmax + NEWTON_SIDE_ALLOW
        // Amount x-coordinate is allowed to exceed the display boundaries
        const double NEWTON_SIDE_ALLOW = xdiff / 20.;

        // Minimum x-distance between critical points
        static const double MIN_DIST_BETWEEN_ROOTS  = 1e-4;
        // Point marker display size / mouse selecting area size
        static const int MARKER_DISP_RADIUS = 3,
                         MARKER_MOUSE_RADIUS = 4;

        // x-epsilon for domain bisection
        // (finding cutoff where function becomes undefined)
        static const double DOMAIN_BISECTION_EPS = 1e-8;

        // Asymptote check constants:
        // Assume asymptote at discontinuity if slope between
        // x + ASYMPTOTE_CHECK_DELTA1, x + ASYMPTOTE_CHECK_DELTA2
        static const double ASYMPTOTE_CHECK_DELTA1 = 1e-8;
        static const double ASYMPTOTE_CHECK_DELTA2 = 1e-7;
        static const double ASYMPTOTE_CHECK_EPS    = 1e-9;
        // Special eps for boundary of domain
        static const double ASYMPTOTE_CHECK_BOUNDARY_EPS = 1e-2;

        // x-coordinate after a discontinuity to begin drawing
        static const double DISCONTINUITY_EPS = 1e-7;

        // * Draw functions
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
                // Store roots and extrema
                std::set<double> roots_and_extrema;
                // Discontinuity type
                // first: x-position
                // second: DISCONT_xxx
                using Discontinuity = std::pair<double, int>;
                // Store discontinuities
                std::set<Discontinuity> discont;
                size_t idx = 0;
                // Push check helpers
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
                auto push_discont_if_valid = [&](double value, Discontinuity::second_type type) {
                    auto vc = Discontinuity(value, type);
                    if (!std::isnan(value) && !std::isinf(value) &&
                            value >= xmin && value <= xmax) {
                        auto it = discont.lower_bound(vc);
                        double cdist = 1.;
                        if (it != discont.end())
                            cdist = it->first - value;
                        if (it != discont.begin()) {
                            auto itp = it; --itp;
                            cdist = std::min(cdist, value - itp->first);
                        }
                        if (cdist >= MIN_DIST_BETWEEN_ROOTS) {
                            discont.insert(vc);
                        }
                    }
                };
                std::vector<std::array<float, 2> > curr_line;
                // Draw a line and construct markers along the line
                // to allow clicking
                auto draw_line = [&](int psx, int psy, int sx, int sy, double x, double y) {
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
                        for (int r = std::max(syp - MARKER_MOUSE_RADIUS, 0);
                                r <= std::min(syp + MARKER_MOUSE_RADIUS,
                                             shigh-1); ++r) {
                            for (int c = std::max(
                                        sx - MARKER_MOUSE_RADIUS, 0);
                                    c <= std::min(sx + MARKER_MOUSE_RADIUS,
                                        swid-1); ++c) {
                                size_t existing_marker_idx = grid[r * swid + c];
                                if (~existing_marker_idx) {
                                    auto& existing_marker = pt_markers[existing_marker_idx];
                                    if (util::sqr_dist(existing_marker.sx,
                                                existing_marker.sy, c, r) <=
                                            util::sqr_dist(new_marker.sx,
                                                new_marker.sy, c, r)) {
                                        // If existing marker is closer, do not overwrite it
                                        continue;
                                    }
                                }
                                grid[r * swid + c] = new_marker_idx;
                            }
                        }
                    }
                    // Draw the line
                    if (curr_line.empty() ||
                        (psx > curr_line.back()[0] || psy != curr_line.back()[1])) {
                        if (curr_line.size() > 1) {
                            graph.polyline(curr_line, func_color, curr_func == exprid ? 3 : 2.);
                        }
                        curr_line.resize(2);
                        curr_line[0] = {(float)psx, (float)psy};
                        curr_line[1] = {(float)sx, (float)sy};
                    } else {
                        curr_line.push_back({(float)sx, (float)sy});
                    }
                };
                // Find roots, asymptotes, extrema
                if (!func.diff.is_null()) {
                    double prev_x, prev_y = 0.;
                    for (int sx = 0; sx < swid; sx += 5) {
                        const double x = sx*1. * xdiff / swid + xmin;
                        env.vars[x_var] = x;
                        double y = expr(env);
                        const bool is_y_nan = std::isnan(y);

                        if (!is_y_nan) {
                            double dy = func.diff(env);
                            if (!std::isnan(dy)) {
                                double root = expr.newton(NEWTON_ARGS, &func.diff, y, dy);
                                push_if_valid(root, roots_and_extrema);
                                double asymp = func.recip.newton(NEWTON_ARGS,
                                        &func.drecip, 1. / y, -dy / (y*y));
                                push_discont_if_valid(asymp, DISCONT_ASYMPT);

                                double ddy = func.diff(env);
                                if (!std::isnan(ddy)) {
                                    double extr = func.diff.newton(NEWTON_ARGS,
                                            &func.ddiff, dy, ddy);
                                    push_if_valid(extr, roots_and_extrema);
                                }
                            }
                        }
                        if (sx) {
                            const bool is_prev_y_nan = std::isnan(prev_y);
                            if (is_y_nan != is_prev_y_nan) {
                                // Search for cutoff via bisection
                                double lo = prev_x, hi = x;
                                while (hi - lo > DOMAIN_BISECTION_EPS) {
                                    double mi = (lo + hi) * 0.5;
                                    env.vars[x_var] = mi; double mi_y = expr(env);
                                    if (std::isnan(mi_y) == is_prev_y_nan) {
                                        lo = mi;
                                    } else {
                                        hi = mi;
                                    }
                                }
                                double boundary_x_not_nan_side = is_prev_y_nan ? hi : lo;
                                push_discont_if_valid(boundary_x_not_nan_side,
                                                      DISCONT_DOMAIN);
                            }
                        }
                        prev_x = x; prev_y = y;
                    }
                }
                // Copy into function
                func.roots_and_extrema.resize(roots_and_extrema.size());
                std::copy(roots_and_extrema.begin(),
                        roots_and_extrema.end(),
                        func.roots_and_extrema.begin());

                // Add screen edges to discontinuities list for convenience
                discont.emplace(xmin, DISCONT_SCREEN);
                discont.emplace(xmax, DISCONT_SCREEN);
                // std::cerr << discont.size() << ": ";
                // for (auto i : discont) std::cerr << i.first << ":" << i.second <<" ";
                // std::cerr <<  "\n";
                double prev_discont_x = xmin, prev_asd, asd;
                int prev_discont_type;
                size_t as_idx = 0;
                // Draw function from asymptote to asymptote
                for (const auto& discontinuity : discont) {
                    double discont_x = discontinuity.first;
                    int discont_type = discontinuity.second;
                    asd = (discont_x - xmin) / xdiff * swid;
                    if (as_idx > 0) {
                        bool connector = prev_discont_type != DISCONT_SCREEN;
                        // Draw func between asymptotes
                        for (double sxd = prev_asd + DISCONTINUITY_EPS; sxd < asd;) {
                            const double x = sxd / swid * xdiff + xmin;
                            env.vars[x_var] = x;
                            double y = expr(env);

                            if (!std::isnan(y)) {
                                if (y> ymax + ydiff) y = ymax + ydiff;
                                else if (y < ymin - ydiff) y = ymin - ydiff;
                                int sy = static_cast<int>(std::round((ymax - y) * shigh / ydiff));
                                int sx = static_cast<int>(std::round(sxd));
                                if (reinit) {
                                    if (!(y > ymax || y < ymin)) {
                                        reinit = false;
                                    }
                                }
                                if (connector) {
                                    // Check if asymptote at previous position;
                                    // if so then connect it
                                    connector = false;
                                    env.vars[x_var] = prev_discont_x + ASYMPTOTE_CHECK_DELTA1;
                                    double yp = expr(env);
                                    env.vars[x_var] = prev_discont_x + ASYMPTOTE_CHECK_DELTA2;
                                    double yp2 = expr(env);
                                    double eps = discont_type == DISCONT_ASYMPT ?
                                                ASYMPTOTE_CHECK_EPS:
                                                ASYMPTOTE_CHECK_BOUNDARY_EPS;
                                    if (yp - yp2 > eps && sy > 0) {
                                        psx = static_cast<int>(std::round(prev_asd));
                                        psy = 0;
                                        reinit = false;
                                    }
                                    if (yp - yp2 < -eps && sy < shigh) {
                                        psx = static_cast<int>(std::round(prev_asd));
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
                                psx = -1;
                            }
                            if (discont.size() > 2 && discont.size() < 100) {
                                if ((as_idx > 1 && sxd - prev_asd < 1.) ||
                                        (as_idx < discont.size() - 1 && asd - sxd < 5.)) {
                                    sxd += 0.1;
                                } else if ((as_idx > 1 && sxd - prev_asd < 5.) ||
                                        (as_idx < discont.size() - 1 && asd - sxd < 5.)) {
                                    sxd += 0.5;
                                } else {
                                    sxd += 1.0;
                                }
                            } else {
                                sxd += 1.0;
                            }
                        }
                        // Connect next asymptote
                        if (discont_type != DISCONT_SCREEN) {
                            env.vars[x_var] = discont_x - ASYMPTOTE_CHECK_DELTA1;
                            double yp = expr(env);
                            env.vars[x_var] = discont_x - ASYMPTOTE_CHECK_DELTA2;
                            double yp2 = expr(env);
                            int sx = -1, sy;
                            double eps = discont_type == DISCONT_ASYMPT ?
                                ASYMPTOTE_CHECK_EPS:
                                ASYMPTOTE_CHECK_BOUNDARY_EPS;
                            if (yp - yp2 > eps) {
                                sx = static_cast<int>(asd);
                                sy = 0;
                                if (psy <= 0) sx = -1;
                            }
                            if (yp - yp2 < -eps) {
                                sx = static_cast<int>(asd);
                                sy = shigh;
                                if (psy >= shigh) sx = -1;
                            }
                            if (~sx) {
                                draw_line(psx, psy, sx, sy, discont_x - 1e-6, yp);
                            }
                        }
                    }
                    ++as_idx;
                    prev_discont_x = discont_x;
                    prev_discont_type = discont_type;
                    prev_asd = asd;
                }

                // finish last line, if exists
                if (curr_line.size() > 1) {
                    graph.polyline(curr_line, func_color, curr_func == exprid ? 3 : 2.);
                }
                // Draw roots/extrema/y-int
                if (!func.expr.is_null() && !func.diff.is_null()) {
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
                    auto label =
                        x == 0. ? PointMarker::LABEL_Y_INT :
                        std::fabs(dy) > 1e-6 ? PointMarker::LABEL_X_INT :
                        ddy > 1e-6 ? PointMarker::LABEL_LOCAL_MIN :
                        ddy < -1e-6 ? PointMarker::LABEL_LOCAL_MAX:
                        PointMarker::LABEL_INFLECTION_PT;
                    // Do not show
                    if (label == PointMarker::LABEL_INFLECTION_PT) continue;

                    int sy = static_cast<int>((ymax - y) / ydiff * shigh);
                    int sx = static_cast<int>((x - xmin) / xdiff * swid);
                    graph.rectangle(sx-MARKER_DISP_RADIUS, sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1, 2*MARKER_DISP_RADIUS+1, true, color::LIGHT_GRAY);
                    graph.rectangle(sx-MARKER_DISP_RADIUS, sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1, 2*MARKER_DISP_RADIUS+1, false, func.line_color);
                    size_t idx = pt_markers.size();
                    pt_markers.emplace_back();
                    auto& ptm = pt_markers.back();
                    ptm.label == label;
                    ptm.y = y; ptm.x = x;
                    ptm.sx = sx; ptm.sy = sy;
                    ptm.passive = false;
                    ptm.rel_func = exprid;
                    for (int r = std::max(sy - MARKER_MOUSE_RADIUS, 0); r <= std::min(sy + MARKER_MOUSE_RADIUS, shigh-1); ++r) {
                        for (int c = std::max(sx - MARKER_MOUSE_RADIUS, 0); c <= std::min(sx + MARKER_MOUSE_RADIUS, swid-1); ++c) {
                            grid[r * swid + c] = idx;
                        }
                    }
                }

                // Function intersection
                if (!func.diff.is_null()) {
                    for (size_t exprid2 = 0; exprid2 < exprid; ++exprid2) {
                        auto& func2 = funcs[exprid2];
                        if (func2.diff.is_null()) continue;
                        if (func2.is_implicit) continue; // not supported right now
                        Expr sub_expr = expr - func2.expr;
                        Expr diff_sub_expr = func.diff - func2.diff;
                        diff_sub_expr.optimize();
                        if (diff_sub_expr.is_null()) continue;
                        std::set<double> st;
                        for (int sxd = 0; sxd < swid; sxd += 2) {
                            const double x = sxd*1. * xdiff / swid + xmin;
                            double root = sub_expr.newton(NEWTON_ARGS, &diff_sub_expr);
                            push_if_valid(root, st);
                        }
                        for (double x : st) {
                            env.vars[x_var] = x;
                            double y = expr(env);
                            int sy = static_cast<int>((ymax - y) / ydiff * shigh);
                            int sx = static_cast<int>((x - xmin) / xdiff * swid);
                            graph.rectangle(sx-MARKER_DISP_RADIUS, sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS, true, color::LIGHT_GRAY);
                            graph.rectangle(sx-MARKER_DISP_RADIUS-1, sy-MARKER_DISP_RADIUS-1, 2*MARKER_DISP_RADIUS+1, 2*MARKER_DISP_RADIUS+1, false, func.line_color);
                            graph.rectangle(sx-MARKER_DISP_RADIUS, sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1, 2*MARKER_DISP_RADIUS+1, false, func2.line_color);
                            size_t idx = pt_markers.size();
                            pt_markers.emplace_back();
                            auto& ptm = pt_markers.back();
                            ptm.label = PointMarker::LABEL_INTERSECTION;
                            ptm.x = x; ptm.y = y;
                            ptm.passive = false;
                            ptm.rel_func = -1;
                            for (int r = std::max(sy - MARKER_MOUSE_RADIUS, 0); r <= std::min(sy + MARKER_MOUSE_RADIUS, shigh-1); ++r) {
                                for (int c = std::max(sx - MARKER_MOUSE_RADIUS, 0); c <= std::min(sx + MARKER_MOUSE_RADIUS, swid-1); ++c) {
                                    grid[r * swid + c] = idx;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Re-parse expression in editor for function 'idx'
    // and update expression, derivatives, etc.
    // Also detects function type.
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
        if (!expr.is_null()) {
            expr.optimize();
            func.diff = expr.diff(x_var, env);
            if (!func.diff.is_null())
                func.ddiff = func.diff.diff(x_var, env);
            else func.ddiff.ast[0] = OpCode::null;
            func.recip = Expr::constant(1.) / func.expr;
            func.recip.optimize();
            func.drecip = func.recip.diff(x_var, env);
        } else func.diff.ast[0] = OpCode::null;
        be.show_error(parser.error_msg);
        be.update();
    }

    void set_curr_func(size_t func_id) {
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

    void delete_func(size_t idx = -1) {
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

    // Discontinuity type
    enum _DiscontType {
        DISCONT_ASYMPT = 0, // asymptote
        DISCONT_DOMAIN = 1, // domain boundary (possibly asymptote)
        DISCONT_SCREEN = 2, // edge of screen
    };
};

}  // namespace nivalis
#endif // ifndef _PLOTTER_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
