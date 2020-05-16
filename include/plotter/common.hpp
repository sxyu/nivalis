#pragma once
#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#endif

#ifndef _COMMON_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
#define _COMMON_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
#include <cctype>
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
#include <algorithm>
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
        BLUE  = 0x0000FF,
        ROYAL_BLUE = 0x4169e1,
        GREEN = 0x008000,
        ORANGE = 0xffa500,
        PURPLE = 0x800080,
        YELLOW = 0xFFFF00
    };
    color from_int(size_t color_index);
}  // namespace color

// Represents function in plotter
struct Function {
    // Function name
    std::string name;
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
    enum FuncType {
        FUNC_TYPE_EXPLICIT, // normal func like x^2
        FUNC_TYPE_IMPLICIT, // implicit func like abs(x)=abs(y)
        FUNC_TYPE_POLYLINE,  // poly-lines (x,y) (x',y') (x'',y'')...
    } type;
    // Store x-positions of currently visible roots and extrema
    // does not include y-int (0)
    std::vector<double> roots_and_extrema;
    // Stores line points, if polyline-type
    std::vector<Expr> polyline;
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
 *  This design looks sort of messy now,
 *  but was originally intended to support different GUI backends
 *
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

 * void show_marker_at(const PointMarker& marker, int px, int py);     show the marker (point label) at the position, with given marker data
 * void hide_marker();                                                 hide the marker

 * * Required Graphics adapter API
 * void line(float ax, float ay, float bx, float by, color::color color, float thickness = 1.0);          draw line (ax, ay) -- (bx, by)
 *                                                                                                        thickness shall be an integer
 * void polyline(const std::vector<std::array<float, 2> >& points, color::color c, float thickness = 1.);   draw polyline
 * void rectangle(float x, float y, float w, float h, bool fill, color::color color);                     draw rectangle (filled or non-filled)
 * void clear(color::color color);                                                                        fill entire view (clear)
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
            sy0 = static_cast<float>(shigh * y0);
            graph.line(0.f, sy0, swid, sy0, color::DARK_GRAY, 2.);
            ++cnt_visible_axis;
        }
        else if (ymin > 0) {
            sy0 = shigh - 26;
        }
        if (xmin <= 0 && xmax >= 0) {
            double x0 = - xmin / (xmax - xmin);
            sx0 = static_cast<float>(swid * x0);
            graph.line(sx0, 0.f, sx0, shigh, color::DARK_GRAY, 3.);
            ++cnt_visible_axis;
        }
        else if (xmax < 0) {
            sx0 = swid - 50;
        }

        // Draw lines
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
            float sxi = static_cast<float>(swid * (xl - xmin) / (xmax - xmin));
            graph.line(sxi,0, sxi, shigh, color::LIGHT_GRAY);
            xl = xstep * idx + xli;
            ++idx;
        }
        idx = 0;
        while (yb <= yt) {
            float syi = static_cast<float>(shigh * (ymax - yb) / (ymax - ymin));
            graph.line(0.f, syi, swid, syi, color::LIGHT_GRAY);
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
            float sxi = static_cast<float>(swid * (xml - xmin) / (xmax - xmin));
            graph.line(sxi, 0.f, sxi, shigh, color::GRAY);

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
            float syi = static_cast<float>(shigh * (ymax - ymb) / (ymax - ymin));
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
        static const int    MAX_ITER  = 30;
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
        static const double ASYMPTOTE_CHECK_DELTA1 = xdiff * 1e-9;
        static const double ASYMPTOTE_CHECK_DELTA2 = xdiff * 1e-8;
        static const double ASYMPTOTE_CHECK_EPS    = xdiff * 1e-10;
        // Special eps for boundary of domain
        static const double ASYMPTOTE_CHECK_BOUNDARY_EPS = xdiff * 1e-3;

        // x-coordinate after a discontinuity to begin drawing
        static const double DISCONTINUITY_EPS = xdiff * 1e-3;

        bool prev_loss_detail = loss_detail;
        loss_detail = false;
        // * Draw function s
        pt_markers.clear(); pt_markers.reserve(500);
        grid.clear(); grid.resize(shigh * swid, -1);
        for (size_t exprid = 0; exprid < funcs.size(); ++exprid) {
            bool reinit = true;
            auto& func = funcs[exprid];
            auto& expr = func.expr;
            const color::color& func_color = func.line_color;
            switch (func.type) {
                case Function::FUNC_TYPE_POLYLINE:
                    {
                        // Draw polyline
                        if (func.polyline.size() && (func.polyline.size() & 1) == 0) {
                            std::vector<std::array<float, 2> > line;
                            double mark_radius = (curr_func == exprid) ? MARKER_DISP_RADIUS :
                                MARKER_DISP_RADIUS-1;
                            for (size_t i = 0; i < func.polyline.size(); i += 2) {
                                auto& expr_x = func.polyline[i],
                                    & expr_y = func.polyline[i+1];
                                double x = expr_x(env), y = expr_y(env);
                                if (std::isnan(x) || std::isnan(y) ||
                                        std::isinf(x) || std::isinf(y)) {
                                    continue;
                                }
                                float sx = static_cast<float>((x - xmin) * swid / xdiff);
                                float sy = static_cast<float>((ymax - y) * shigh / ydiff);
                                line.push_back({sx, sy});
                                graph.rectangle(sx - (float)mark_radius,
                                        sy - (float)mark_radius,
                                        (float)mark_radius*2 + 1.f,
                                        (float)mark_radius*2 + 1.f, true, func_color);
                                size_t new_marker_idx = pt_markers.size();
                                {
                                    PointMarker ptm;
                                    ptm.label = PointMarker::LABEL_NONE;
                                    ptm.y = y; ptm.x = x;
                                    ptm.passive = true;
                                    ptm.rel_func = exprid;
                                    pt_markers.push_back(std::move(ptm));
                                }
                                int cmin = std::max(static_cast<int>(sx - MARKER_MOUSE_RADIUS), 0);
                                int cmax = std::min(static_cast<int>(sx + MARKER_MOUSE_RADIUS), swid - 1);
                                int rmin = std::max(static_cast<int>(sy - MARKER_MOUSE_RADIUS), 0);
                                int rmax = std::min(static_cast<int>(sy + MARKER_MOUSE_RADIUS), shigh - 1);
                                if (cmin <= cmax) {
                                    for (int r = rmin; r <= rmax; ++r) {
                                        std::fill(grid.begin() + (r * swid + cmin),
                                                grid.begin() + (r * swid + cmax + 1),
                                                new_marker_idx);
                                    }
                                }

                            }
                            graph.polyline(line, func_color, (curr_func == exprid) ? 3.f : 2.f);
                        }
                    }
                    break;
                case Function::FUNC_TYPE_IMPLICIT:
                    {
                        // Implicit function
                        // Coarse interval
                        static const int COARSE_INTERVAL = 6;
                        // Fine interval (variable)
                        int fine_interval = 1;

                        // Number of pixels drawn, used to increase fine interval
                        size_t pix_cnt = 0;
                        // Function values in above line
                        std::vector<double> coarse_line(swid + 2);
                        std::vector<double> line(swid + 2);

                        // 'Interesting' squares from above line/to left
                        // When a square is painted, need to paint neighbor
                        // to below (if changes sign left)/
                        // right (if changes sign above) as well
                        std::vector<bool> coarse_below_interesting(swid + 2);
                        bool coarse_right_interesting = false;
                        std::vector<bool> fine_paint_below(swid + 2);
                        bool fine_paint_right;

                        // Increase interval per x pixels
                        static const size_t HIGH_PIX_LIMIT = 75000;
                        // Maximum number of pixels to draw (stops drawing)
                        static const size_t MAX_PIXELS = 300000;
                        // Epsilon for bisection
                        static const double BISECTION_EPS = 1e-4;

                        auto paint_square_fine = [&](int ylo, int yhi, int xlo, int xhi,
                                        double z_at_xy_hi) {
                            std::fill(fine_paint_below.begin() + xlo,
                                    fine_paint_below.begin() + (xhi + 1), false);
                            for (int sy = ylo; sy <= yhi; sy += fine_interval) {
                                fine_paint_right = false;
                                if (pix_cnt > MAX_PIXELS) break;
                                const double y = (shigh - sy)*1. / shigh * ydiff + ymin;
                                for (int sx = xlo; sx <= xhi; sx += fine_interval) {
                                    // Update interval based on point count
                                    if (pix_cnt > MAX_PIXELS) break;
                                    fine_interval = std::min(static_cast<int>(pix_cnt /
                                            HIGH_PIX_LIMIT) + 1, COARSE_INTERVAL);

                                    const double x = 1.*sx / swid * xdiff + xmin;
                                    double precise_x = x, precise_y = y;

                                    double z;
                                    if (sy == yhi && sx == xhi) {
                                        z = z_at_xy_hi;
                                    } else {
                                        env.vars[y_var] = y;
                                        env.vars[x_var] = x;
                                        z = expr(env);
                                    }
                                    if (sy > ylo && sx > xlo) {
                                        int sgn_z = (z < 0 ? -1 : z == 0 ? 0 : 1);
                                        bool paint_square = false;
                                        bool paint_from_left = fine_paint_right;
                                        bool paint_from_above = fine_paint_below[sx];
                                        fine_paint_right = fine_paint_below[sx] = false;
                                        double zup = line[sx];
                                        int sgn_zup = (zup < 0 ? -1 : zup == 0 ? 0 : 1);
                                        if (sgn_zup * sgn_z <= 0) {
                                            // Bisect up
                                            env.vars[x_var] = x;
                                            double lo = y;
                                            double hi = (shigh - (sy - fine_interval))*1. / shigh * ydiff + ymin;
                                            while (hi - lo > BISECTION_EPS) {
                                                double mi = (lo + hi) / 2;
                                                env.vars[y_var] = mi;
                                                double zmi = expr(env);
                                                int sgn_zmi = (zmi < 0 ? -1 : 1);
                                                if (sgn_z == sgn_zmi) lo = mi;
                                                else hi = mi;
                                            }
                                            precise_y = lo;
                                            paint_square = fine_paint_right = true;
                                        }
                                        double zleft = line[sx - fine_interval];
                                        int sgn_zleft = (zleft < 0 ? -1 : zleft == 0 ? 0 : 1);
                                        if (sgn_zleft * sgn_z <= 0) {
                                            if (!paint_square) {
                                                // Bisect left
                                                env.vars[y_var] = y;
                                                double lo = 1.*(sx - fine_interval) / swid * xdiff + xmin;
                                                double hi = x;
                                                while (hi - lo > BISECTION_EPS) {
                                                    double mi = (lo + hi) / 2;
                                                    env.vars[x_var] = mi;
                                                    double zmi = expr(env);
                                                    int sgn_zmi = (zmi < 0 ? -1 : 1);
                                                    if (sgn_zleft == sgn_zmi) lo = mi;
                                                    else hi = mi;
                                                }
                                                precise_x = lo;
                                            }
                                            paint_square = fine_paint_below[sx] = true;
                                        }
                                        if (paint_square || paint_from_left || paint_from_above) {
                                            int pad = (curr_func == exprid) ? 1 : 0;
                                            float precise_sy =
                                                static_cast<float>(
                                                        (ymax - y) / ydiff * shigh);
                                            float precise_sx = static_cast<float>(
                                                    (x - xmin) / xdiff * swid);
                                            graph.rectangle(
                                                    sx + (float)(- fine_interval + 1 - pad),
                                                    sy + (float)(- fine_interval + 1 - pad),
                                                    (float)(fine_interval + pad),
                                                    (float)(fine_interval + pad),
                                                    true,
                                                    func_color);
                                            // Add labels
                                            size_t new_marker_idx = pt_markers.size();
                                            PointMarker new_marker;
                                            new_marker.x = precise_x; new_marker.y = precise_y;
                                            new_marker.rel_func = exprid;
                                            new_marker.label = PointMarker::LABEL_NONE;
                                            new_marker.passive = true;
                                            pt_markers.push_back(std::move(new_marker));
                                            int cmin = std::max(sx - fine_interval + 1 - MARKER_MOUSE_RADIUS, 0);
                                            int cmax = std::min(sx + MARKER_MOUSE_RADIUS, swid - 1);
                                            if (cmin <= cmax) {
                                                for (int r = std::max(sy - fine_interval + 1 - MARKER_MOUSE_RADIUS, 0);
                                                        r <= std::min(sy + MARKER_MOUSE_RADIUS, shigh - 1); ++ r) {
                                                    std::fill(grid.begin() + (r * swid + cmin),
                                                            grid.begin() + (r * swid + cmax + 1),
                                                            new_marker_idx);
                                                }
                                            }
                                            ++pix_cnt;
                                        }
                                    }
                                    line[sx] = z;
                                }
                            }
                        };

                        for (int coarse_sy = 0; coarse_sy < shigh + COARSE_INTERVAL - 1; coarse_sy += COARSE_INTERVAL) {
                            int coarse_int_y = COARSE_INTERVAL;
                            if (coarse_sy >= shigh) {
                                coarse_sy = shigh - 1;
                                coarse_int_y = (shigh-1) % COARSE_INTERVAL;
                                if (coarse_int_y == 0) break;
                            }
                            const double coarse_y = (shigh - coarse_sy)*1. / shigh * ydiff + ymin;
                            coarse_right_interesting = false;
                            for (int coarse_sx = 0; coarse_sx < swid + COARSE_INTERVAL - 1; coarse_sx += COARSE_INTERVAL) {
                                int coarse_int_x = COARSE_INTERVAL;
                                if (coarse_sx >= swid) {
                                    coarse_sx = swid - 1;
                                    coarse_int_x = (swid-1) % COARSE_INTERVAL;
                                    if (coarse_int_x == 0) break;
                                }
                                // Update interval based on point count
                                const double coarse_x = 1.*coarse_sx / swid * xdiff + xmin;
                                double precise_x = coarse_x, precise_y = coarse_y;
                                const int xy_pos = coarse_sy * swid + coarse_sx;

                                env.vars[y_var] = coarse_y;
                                env.vars[x_var] = coarse_x;
                                double z = expr(env);
                                if (coarse_sx >= coarse_int_x && coarse_sy >= coarse_int_y) {
                                    bool interesting_square = false;
                                    int sgn_z = (z < 0 ? -1 : z == 0 ? 0 : 1);
                                    bool interest_from_left = coarse_right_interesting;
                                    bool interest_from_above = coarse_below_interesting[coarse_sx];
                                    coarse_right_interesting = coarse_below_interesting[coarse_sx] = false;
                                    double zleft = coarse_line[coarse_sx - coarse_int_x];
                                    int sgn_zleft = (zleft < 0 ? -1 : zleft == 0 ? 0 : 1);
                                    if (sgn_zleft * sgn_z <= 0) {
                                        coarse_below_interesting[coarse_sx] = true;
                                        interesting_square = true;
                                    }
                                    double zup = coarse_line[coarse_sx];
                                    int sgn_zup = (zup < 0 ? -1 : zup == 0 ? 0 : 1);
                                    if (sgn_zup * sgn_z <= 0) {
                                        coarse_right_interesting = true;
                                        interesting_square = true;
                                    }
                                    if (interesting_square || interest_from_left || interest_from_above) {
                                        // Debug
                                        // graph.rectangle(coarse_sx - coarse_int_x + 1,
                                        //         coarse_sy - coarse_int_y + 1,
                                        //         coarse_int_x,
                                        //         coarse_int_y,
                                        //         false,
                                        //         interesting_square?
                                        //         color::BLUE : color::GREEN);
                                        paint_square_fine(coarse_sy - coarse_int_y,
                                                coarse_sy,
                                                coarse_sx - coarse_int_x,
                                                coarse_sx, z);

                                    }
                                }
                                coarse_line[coarse_sx] = z;
                            }
                        }

                        // Show detail lost warning
                        if (fine_interval > 1) loss_detail = true;
                    }
                    break;
                case Function::FUNC_TYPE_EXPLICIT:
                    {
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
                        auto draw_line = [&](float psx, float psy, float sx, float sy, double x, double y) {
                            float miny = std::min(sy, psy), maxy = sy + psy - miny;

                            // Construct a (passive) point marker for this point,
                            // so that the user can click to see the position
                            size_t new_marker_idx = pt_markers.size();
                            pt_markers.emplace_back();
                            auto& new_marker = pt_markers.back();
                            new_marker.x = x; new_marker.y = y;
                            new_marker.rel_func = exprid;
                            new_marker.label = PointMarker::LABEL_NONE;
                            new_marker.sx = static_cast<int>(sx + psx) / 2;
                            new_marker.sy = static_cast<int>(sy + psy) / 2;
                            new_marker.passive = true;

                            int sxi = static_cast<int>(sx);
                            int syi = static_cast<int>(sy);
                            int minyi = std::max(static_cast<int>(miny) - MARKER_MOUSE_RADIUS, 0);
                            int maxyi = std::min(static_cast<int>(maxy) + MARKER_MOUSE_RADIUS, shigh-1);
                            for (int r = minyi; r <= maxyi; ++r) {
                                int cmin = std::max(sxi - MARKER_MOUSE_RADIUS, 0);
                                int cmax = std::min(sxi + MARKER_MOUSE_RADIUS, swid-1);
                                // Assign to grid
                                for (int c = cmin; c <= cmax; ++c) {
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
                            // Draw the line
                            if (curr_line.empty() ||
                                    (psx > curr_line.back()[0] || psy != curr_line.back()[1])) {
                                if (curr_line.size() > 1) {
                                    graph.polyline(curr_line, func_color, curr_func == exprid ? 3 : 2.);
                                }
                                curr_line.resize(2);
                                curr_line[0] = {psx, psy};
                                curr_line[1] = {sx, sy};
                            } else {
                                curr_line.push_back({sx, sy});
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
                        double prev_discont_x = xmin;
                        float prev_discont_sx;
                        int prev_discont_type;
                        size_t as_idx = 0;

                        float psx = -1.f, psy = -1.f;
                        // Draw function from discont to discont
                        for (const auto& discontinuity : discont) {
                            double discont_x = discontinuity.first;
                            int discont_type = discontinuity.second;
                            float discont_sx = static_cast<float>((discont_x - xmin) / xdiff * swid);
                            if (as_idx > 0) {
                                bool connector = prev_discont_type != DISCONT_SCREEN;
                                // Draw func between asymptotes
                                for (float sxd = prev_discont_sx + DISCONTINUITY_EPS;
                                        sxd < discont_sx;) {
                                    const double x = sxd / swid * xdiff + xmin;
                                    env.vars[x_var] = x;
                                    double y = expr(env);

                                    if (!std::isnan(y)) {
                                        if (y > ymax + ydiff) y = ymax + ydiff;
                                        else if (y < ymin - ydiff) y = ymin - ydiff;
                                        float sy = static_cast<float>((ymax - y) * shigh / ydiff);
                                        float sx = sxd;
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
                                            if (yp - yp2 > eps && sy > 0.f) {
                                                psx = prev_discont_sx;
                                                psy = 0.f;
                                                reinit = false;
                                            } else if (yp - yp2 < -eps && sy < (float)shigh) {
                                                psx = prev_discont_sx;
                                                psy = static_cast<float>(shigh);
                                                reinit = false;
                                            }
                                        }
                                        if (!reinit && psx >= 0) {
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
                                        if ((as_idx > 1 && sxd - prev_discont_sx < 1.) ||
                                                (as_idx < discont.size() - 1 && discont_sx - sxd < 5.)) {
                                            sxd += 0.1f;
                                        } else if ((as_idx > 1 && sxd - prev_discont_sx < 5.) ||
                                                (as_idx < discont.size() - 1 && discont_sx - sxd < 5.)) {
                                            sxd += 0.5f;
                                        } else {
                                            sxd += 1.0f;
                                        }
                                    } else {
                                        sxd += 1.0f;
                                    }
                                }
                                // Connect next asymptote
                                if (discont_type != DISCONT_SCREEN) {
                                    env.vars[x_var] = discont_x - ASYMPTOTE_CHECK_DELTA1;
                                    double yp = expr(env);
                                    env.vars[x_var] = discont_x - ASYMPTOTE_CHECK_DELTA2;
                                    double yp2 = expr(env);
                                    float sx = -1, sy;
                                    double eps = discont_type == DISCONT_ASYMPT ?
                                        ASYMPTOTE_CHECK_EPS:
                                        ASYMPTOTE_CHECK_BOUNDARY_EPS;
                                    if (yp - yp2 > eps && psy > 0) {
                                        sx = static_cast<float>(discont_sx);
                                        sy = 0.f;
                                    }
                                    if (yp - yp2 < -eps && psy < shigh) {
                                        sx = discont_sx;
                                        sy = static_cast<float>(shigh);
                                    }
                                    if (sx >= 0.f) {
                                        draw_line(psx, psy, sx, sy, discont_x - 1e-6f, yp);
                                    }
                                }
                            }
                            ++as_idx;
                            prev_discont_x = discont_x;
                            prev_discont_type = discont_type;
                            prev_discont_sx = discont_sx;
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

                            int sy = static_cast<int>((ymax - y) / ydiff * shigh);
                            int sx = static_cast<int>((x - xmin) / xdiff * swid);
                            graph.rectangle(sx-MARKER_DISP_RADIUS, sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1, 2*MARKER_DISP_RADIUS+1, true, color::LIGHT_GRAY);
                            graph.rectangle(sx-MARKER_DISP_RADIUS, sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1, 2*MARKER_DISP_RADIUS+1, false, func.line_color);
                            size_t idx = pt_markers.size();
                            {
                                PointMarker ptm;
                                ptm.label = label;
                                ptm.y = y; ptm.x = x;
                                ptm.sx = sx; ptm.sy = sy;
                                ptm.passive = false;
                                ptm.rel_func = exprid;
                                pt_markers.push_back(std::move(ptm));
                            }
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
                                if (func2.type != Function::FUNC_TYPE_EXPLICIT) continue; // not supported right now
                                Expr sub_expr = expr - func2.expr;
                                Expr diff_sub_expr = func.diff - func2.diff;
                                diff_sub_expr.optimize();
                                if (diff_sub_expr.is_null()) continue;
                                std::set<double> st;
                                for (int sxd = 0; sxd < swid; sxd += 2) {
                                    const double x = sxd * xdiff / swid + xmin;
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
                                    {
                                        PointMarker ptm;
                                        ptm.label = PointMarker::LABEL_INTERSECTION;
                                        ptm.x = x; ptm.y = y;
                                        ptm.passive = false;
                                        ptm.rel_func = -1;
                                        pt_markers.push_back(std::move(ptm));
                                    }
                                    for (int r = std::max(sy - MARKER_MOUSE_RADIUS, 0); r <= std::min(sy + MARKER_MOUSE_RADIUS, shigh-1); ++r) {
                                        for (int c = std::max(sx - MARKER_MOUSE_RADIUS, 0); c <= std::min(sx + MARKER_MOUSE_RADIUS, swid-1); ++c) {
                                            grid[r * swid + c] = idx;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
            }
        }
        if (loss_detail) {
            be.show_error("Warning: some detail may be lost");
        } else if (prev_loss_detail) {
            be.show_error("");
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
        func.polyline.clear();
        expr_str = be.read_editor(idx);
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
                    be.show_error("Polyline expr error: " + polyline_err);
                else
                    be.show_error(""); // Clear
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
            be.show_error(parser.error_msg);
        }

        if (parser.error_msg.empty()) be.show_error("");
        if (func.type == Function::FUNC_TYPE_EXPLICIT) {
            // Register a function in env
            env.def_func(func.name, func.expr, { x_var });
            if (env.error_msg.size()) {
                be.show_error(env.error_msg);
            }
        } else {
            env.del_func(func.name);
        }
        loss_detail = false;
        be.update();
    }

    void set_curr_func(size_t func_id) {
        if (func_id != curr_func)
            be.show_error("");
        reparse_expr(curr_func);
        curr_func = func_id;
        if (curr_func == -1) {
            curr_func = 0;
            return;
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
                curr_func = funcs.size()-1;
                return;
            }
        }
        be.update_editor(func_id, funcs[func_id].expr_str);
        be.update(true);
    }

    void delete_func(size_t idx = -1) {
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
    // Helper for finding grid line sizes fo a given step size
    // rounds to increments of 1,2,5
    // e.g. 0.1, 0.2, 0.5, 1, 2, 5 (used for grid lines)
    // returns (small gridline size, big gridline size)
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
    Backend& be;
    Parser parser;

    std::queue<color::color> reuse_colors;    // Reusable colors
    size_t last_expr_color = 0;               // Next available color index if no reusable
                                              // one present(by color::from_int)
    uint32_t x_var, y_var;                    // x,y variable addresses
    bool dragdown, draglabel;
    int dragx, dragy;
    double xmaxi, xmini, ymaxi, ymini;

    size_t next_func_name = 0;                // Next available function name

    // whether some detail is lost
    bool loss_detail = false;

    // Discontinuity type
    enum _DiscontType {
        DISCONT_ASYMPT = 0, // asymptote
        DISCONT_DOMAIN = 1, // domain boundary (possibly asymptote)
        DISCONT_SCREEN = 2, // edge of screen
    };
};

}  // namespace nivalis
#endif // ifndef _COMMON_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
