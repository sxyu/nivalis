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
#include <cmath>
#include <string>
#include <sstream>
#include <queue>
#include <array>
#include <vector>
#include <set>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include "expr.hpp"
#include "parser.hpp"
#include "color.hpp"
#include "util.hpp"

#include <chrono>
// #include "test_common.hpp"
//
namespace nivalis {
namespace util {
// * Plotter-related utils

// Helper for finding grid line sizes fo a given step size
// rounds to increments of 1,2,5
// e.g. 0.1, 0.2, 0.5, 1, 2, 5 (used for grid lines)
// returns (small gridline size, big gridline size)
std::pair<double, double> round125(double step);
}  // namespace util

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
    // Should be set by GUI implementation: expression string from editor.
    std::string expr_str;
    enum {
        FUNC_TYPE_EXPLICIT, // normal func like x^2
        FUNC_TYPE_IMPLICIT, // implicit func like abs(x)=abs(y)
        FUNC_TYPE_POLYLINE,  // poly-lines (x,y) (x',y') (x'',y'')...
    } type;
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

// Data for each slider in Slider window
struct SliderData {
    // Should be set by implementation when variable name changes
    std::string var_name;
    // Should be set by implementation when value, bounds change
    float val, lo = -10.0, hi = 10.0;
    // Internal
    uint32_t var_addr;
};

/** Nivalis GUI plotter logic, decoupled from GUI implementation
 * Register GUI event handlers to call handle_xxx
 * Register resize handler to call resize
 * Call draw() in drawing event/loop
 * Call reset_view() to reset view, delete_func() delete function,
 * set_curr_func() to set current function, etc.
* */
class Plotter {
public:
    const unsigned NUM_THREADS = std::thread::hardware_concurrency();
    // Construct a Plotter.
    // expr_env: environment to store variables/evaluate function expressions in
    // init_expr: initial expression for first function (which is automatically added)
    // win_width/win_height: initial plotting window size
    Plotter(Environment& expr_env, const std::string& init_expr,
            int win_width, int win_height);

    // Draw axes and grid onto given graphics adaptor
    // for Graphics adaptor API see draw()
    template<class GraphicsAdaptor>
    void draw_grid(GraphicsAdaptor& graph) {
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
        std::tie(ystep, ymstep) = util::round125((ymax - ymin) / shigh * 600 / 10.8);
        std::tie(xstep, xmstep) = util::round125((xmax - xmin) / swid * 1000 / 18.);
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

            if (ymb != 0) {
                std::stringstream sstm;
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

    /** Draw axes, grid AND all functions onto given graphics adaptor
     * * Required Graphics adaptor API
     * void line(float ax, float ay, float bx, float by, const color::color&, float thickness = 1.0);               draw line (ax, ay) -- (bx, by)
     * void polyline(const std::vector<std::array<float, 2> >& points, const color::color&, float thickness = 1.);  draw polyline (not closed)
     * void rectangle(float x, float y, float w, float h, bool fill, const color::color&);                          draw rectangle (filled or non-filled)
     * void clear(color::const color color&);                                                                       fill entire view (clear)
     * void string(float x, float y, std::string s, const color::color&);                                           draw a string
     * Adapater may use internal caching and/or add optional arguments to above functions; when require_update is set the cache should be
     * cleared. */
    template<class GraphicsAdaptor>
    void draw(GraphicsAdaptor& graph) {
        if (xmin >= xmax) xmax = xmin + 1e-9;
        if (ymin >= ymax) ymax = ymin + 1e-9;
        graph.clear(color::WHITE);
        draw_grid<GraphicsAdaptor>(graph);
        double xdiff = xmax - xmin, ydiff = ymax - ymin;

        // * Constants
        // Newton's method parameters
        static const double EPS_STEP  = 1e-10 * ydiff;
        static const double EPS_ABS   = 1e-10 * ydiff;
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
                     MARKER_CLICKABLE_RADIUS = 5;

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
        // BEGIN_PROFILE;
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
                                int cmin = std::max(static_cast<int>(sx - MARKER_CLICKABLE_RADIUS), 0);
                                int cmax = std::min(static_cast<int>(sx + MARKER_CLICKABLE_RADIUS), swid - 1);
                                int rmin = std::max(static_cast<int>(sy - MARKER_CLICKABLE_RADIUS), 0);
                                int rmax = std::min(static_cast<int>(sy + MARKER_CLICKABLE_RADIUS), shigh - 1);
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

                        // Function values in above line
                        std::vector<double> coarse_line(swid + 2);

                        // 'Interesting' squares from above line/to left
                        // When a square is painted, need to paint neighbor
                        // to below (if changes sign left)/
                        // right (if changes sign above) as well
                        std::vector<bool> coarse_below_interesting(swid + 2);
                        bool coarse_right_interesting = false;

                        // Interesting squares
                        using Square = std::tuple<int, int, int, int, double>;
                        std::vector<Square> interest_squares;

                        // Increase interval per x pixels
                        static const size_t HIGH_PIX_LIMIT = 75000;
                        // Maximum number of pixels to draw (stops drawing)
                        static const size_t MAX_PIXELS = 300000;
                        // Epsilon for bisection
                        static const double BISECTION_EPS = 1e-4;

                        for (int csy = 0; csy < shigh + COARSE_INTERVAL - 1; csy += COARSE_INTERVAL) {
                            int coarse_int_y = COARSE_INTERVAL;
                            if (csy >= shigh) {
                                csy = shigh - 1;
                                coarse_int_y = (shigh-1) % COARSE_INTERVAL;
                                if (coarse_int_y == 0) break;
                            }
                            const double cy = (shigh - csy)*1. / shigh * ydiff + ymin;
                            coarse_right_interesting = false;
                            for (int csx = 0; csx < swid + COARSE_INTERVAL - 1; csx += COARSE_INTERVAL) {
                                int cxi = COARSE_INTERVAL;
                                if (csx >= swid) {
                                    csx = swid - 1;
                                    cxi = (swid-1) % COARSE_INTERVAL;
                                    if (cxi == 0) break;
                                }
                                // Update interval based on point count
                                const double coarse_x = 1.*csx / swid * xdiff + xmin;
                                double precise_x = coarse_x, precise_y = cy;
                                const int xy_pos = csy * swid + csx;

                                env.vars[y_var] = cy;
                                env.vars[x_var] = coarse_x;
                                double z = expr(env);
                                if (csx >= cxi && csy >= coarse_int_y) {
                                    bool interesting_square = false;
                                    int sgn_z = (z < 0 ? -1 : z == 0 ? 0 : 1);
                                    bool interest_from_left = coarse_right_interesting;
                                    bool interest_from_above = coarse_below_interesting[csx];
                                    coarse_right_interesting = coarse_below_interesting[csx] = false;
                                    double zleft = coarse_line[csx - cxi];
                                    int sgn_zleft = (zleft < 0 ? -1 : zleft == 0 ? 0 : 1);
                                    if (sgn_zleft * sgn_z <= 0) {
                                        coarse_below_interesting[csx] = true;
                                        interesting_square = true;
                                    }
                                    double zup = coarse_line[csx];
                                    int sgn_zup = (zup < 0 ? -1 : zup == 0 ? 0 : 1);
                                    if (sgn_zup * sgn_z <= 0) {
                                        coarse_right_interesting = true;
                                        interesting_square = true;
                                    }
                                    if (interesting_square || interest_from_left || interest_from_above) {
                                        interest_squares.push_back(Square(
                                             csy - coarse_int_y, csy, csx - cxi, csx, z
                                        ));
                                    }
                                }
                                coarse_line[csx] = z;
                            }
                        }

                        {
                            std::atomic<int> sqr_id(0);
                            int pad = (curr_func == exprid) ? 1 : 0;
                            auto worker = [&]() {
                                std::vector<double> line(swid + 2);
                                std::vector<bool> fine_paint_below(swid + 2);
                                std::vector<std::array<int, 2> > tdraws;
                                std::vector<PointMarker> tpt_markers;
                                Environment tenv = env;
                                bool fine_paint_right;
                                // Number of pixels drawn, used to increase fine interval
                                size_t tpix_cnt = 0;
                                // Fine interval (variable)
                                int fine_interval = 1;
                                while (true) {
                                    int i = sqr_id++;
                                    if (i >= interest_squares.size()) break;
                                    auto& sqr = interest_squares[i];

                                    if (tpix_cnt > MAX_PIXELS) break;
                                    // Update interval based on point count
                                    fine_interval = std::min(static_cast<int>(tpix_cnt /
                                                HIGH_PIX_LIMIT) + 1, COARSE_INTERVAL);

                                    int ylo, yhi, xlo, xhi; double z_at_xy_hi;
                                    std::tie(ylo, yhi, xlo, xhi, z_at_xy_hi) = sqr;

                                    std::fill(fine_paint_below.begin() + xlo,
                                            fine_paint_below.begin() + (xhi + 1), false);
                                    for (int sy = ylo; sy <= yhi; sy += fine_interval) {
                                        fine_paint_right = false;
                                        const double y = (shigh - sy)*1. / shigh * ydiff + ymin;
                                        for (int sx = xlo; sx <= xhi; sx += fine_interval) {

                                            const double x = 1.*sx / swid * xdiff + xmin;
                                            double precise_x = x, precise_y = y;

                                            double z;
                                            if (sy == yhi && sx == xhi) {
                                                z = z_at_xy_hi;
                                            } else {
                                                tenv.vars[y_var] = y;
                                                tenv.vars[x_var] = x;
                                                z = expr(tenv);
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
                                                    tenv.vars[x_var] = x;
                                                    double lo = y;
                                                    double hi = (shigh - (sy - fine_interval))*1. / shigh * ydiff + ymin;
                                                    while (hi - lo > BISECTION_EPS) {
                                                        double mi = (lo + hi) / 2;
                                                        tenv.vars[y_var] = mi;
                                                        double zmi = expr(tenv);
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
                                                        tenv.vars[y_var] = y;
                                                        double lo = 1.*(sx - fine_interval) / swid * xdiff + xmin;
                                                        double hi = x;
                                                        while (hi - lo > BISECTION_EPS) {
                                                            double mi = (lo + hi) / 2;
                                                            tenv.vars[x_var] = mi;
                                                            double zmi = expr(tenv);
                                                            int sgn_zmi = (zmi < 0 ? -1 : 1);
                                                            if (sgn_zleft == sgn_zmi) lo = mi;
                                                            else hi = mi;
                                                        }
                                                        precise_x = lo;
                                                    }
                                                    paint_square = fine_paint_below[sx] = true;
                                                }
                                                if (paint_square || paint_from_left || paint_from_above) {
                                                    float precise_sy =
                                                        static_cast<float>(
                                                                (ymax - y) / ydiff * shigh);
                                                    float precise_sx = static_cast<float>(
                                                            (x - xmin) / xdiff * swid);
                                                    tdraws.push_back({sx, sy});
                                                    // Add labels
                                                    PointMarker new_marker;
                                                    new_marker.x = precise_x; new_marker.y = precise_y;
                                                    new_marker.sx = sx; new_marker.sy = sy;
                                                    new_marker.rel_func = exprid;
                                                    new_marker.label = PointMarker::LABEL_NONE;
                                                    new_marker.passive = true;
                                                    tpt_markers.push_back(std::move(new_marker));
                                                    ++tpix_cnt;
                                                }
                                            }
                                            line[sx] = z;
                                        }
                                    }
                                }

                                {
                                    std::lock_guard<std::mutex> lock(mtx);
                                    for (auto& p : tdraws) {
                                        graph.rectangle(
                                                p[0] + (float)(- fine_interval + 1 - pad),
                                                p[1] + (float)(- fine_interval + 1 - pad),
                                                (float)(fine_interval + pad),
                                                (float)(fine_interval + pad),
                                                true,
                                                func_color);
                                    }

                                    for (auto& pm : tpt_markers) {
                                        const int sx = pm.sx, sy = pm.sy;
                                        size_t new_marker_idx = pt_markers.size();
                                        pt_markers.push_back(pm);
                                        int cmin = std::max(sx - fine_interval + 1 - MARKER_CLICKABLE_RADIUS, 0);
                                        int cmax = std::min(sx + MARKER_CLICKABLE_RADIUS, swid - 1);
                                        if (cmin <= cmax) {
                                            for (int r = std::max(sy - fine_interval + 1 - MARKER_CLICKABLE_RADIUS, 0);
                                                    r <= std::min(sy + MARKER_CLICKABLE_RADIUS, shigh - 1); ++ r) {
                                                std::fill(grid.begin() + (r * swid + cmin),
                                                        grid.begin() + (r * swid + cmax + 1),
                                                        new_marker_idx);
                                            }
                                        }
                                    }

                                    // Show detail lost warning
                                    if (fine_interval > 1) loss_detail = true;
                                }
                            };

                            if (NUM_THREADS <= 1) {
                                worker();
                            } else {
                                std::vector<std::thread> pool;
                                for (size_t i = 0; i < NUM_THREADS; ++i) {
                                    pool.emplace_back(worker);
                                }
                                for (size_t i = 0; i < pool.size(); ++i) pool[i].join();
                            }
                        }

                    }
                    break;
                case Function::FUNC_TYPE_EXPLICIT:
                    {
                        // explicit function
                        env.vars[y_var] = std::numeric_limits<double>::quiet_NaN();
                        // Discontinuity type
                        // first: x-position
                        // second: DISCONT_xxx
                        // Discontinuity/root/extremum type
                        enum {
                            DISCONT_ASYMPT = 0, // asymptote (in middle of domain, e.g. 0 of 1/x)
                            DISCONT_DOMAIN,     // domain boundary (possibly asymptote, e.g. 0 of ln(x))
                            DISCONT_SCREEN,     // edge of screen
                            ROOT,               // root
                            EXTREMUM,           // extremum
                            Y_INT,              // y-intercept
                        };
                        using CritPoint = std::pair<double, int>;
                        // Store discontinuities
                        std::set<CritPoint> discont, roots_and_extrema;
                        size_t idx = 0;
                        // Push check helpers
                        // Push to st if no other item less than MIN_DIST_BETWEEN_ROOTS from value
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
                        // Push (value, type) to discont if no other distcontinuity's first item
                        // is less than MIN_DIST_BETWEEN_ROOTS from value
                        auto push_critpt_if_valid = [&](double value, CritPoint::second_type type, std::set<CritPoint>& st) {
                            auto vc = CritPoint(value, type);
                            if (!std::isnan(value) && !std::isinf(value) &&
                                    value >= xmin && value <= xmax) {
                                auto it = st.lower_bound(vc);
                                double cdist = 1.;
                                if (it != st.end())
                                    cdist = it->first - value;
                                if (it != st.begin()) {
                                    auto itp = it; --itp;
                                    cdist = std::min(cdist, value - itp->first);
                                }
                                if (cdist >= MIN_DIST_BETWEEN_ROOTS) {
                                    st.insert(vc);
                                }
                            }
                        };

                        // Stores points in current line, which will be drawn as a polyline
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
                            int minyi = std::max(static_cast<int>(miny) - MARKER_CLICKABLE_RADIUS, 0);
                            int maxyi = std::min(static_cast<int>(maxy) + MARKER_CLICKABLE_RADIUS, shigh-1);
                            for (int r = minyi; r <= maxyi; ++r) {
                                int cmin = std::max(sxi - MARKER_CLICKABLE_RADIUS, 0);
                                int cmax = std::min(sxi + MARKER_CLICKABLE_RADIUS, swid-1);
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
                        // ** Find roots, asymptotes, extrema
                        if (!func.diff.is_null()) {
                            double prev_x, prev_y = 0.;
                            for (int sx = 0; sx < swid; sx += 10) {
                                const double x = sx*1. * xdiff / swid + xmin;
                                env.vars[x_var] = x;
                                double y = expr(env);
                                const bool is_y_nan = std::isnan(y);

                                if (!is_y_nan) {
                                    double dy = func.diff(env);
                                    if (!std::isnan(dy)) {
                                        double root = expr.newton(NEWTON_ARGS, &func.diff, y, dy);
                                        push_critpt_if_valid(root, ROOT, roots_and_extrema);
                                        double asymp = func.recip.newton(NEWTON_ARGS,
                                                &func.drecip, 1. / y, -dy / (y*y));
                                        push_critpt_if_valid(asymp, DISCONT_ASYMPT, discont);

                                        double ddy = func.ddiff(env);
                                        if (!std::isnan(ddy)) {
                                            double extr = func.diff.newton(NEWTON_ARGS,
                                                    &func.ddiff, dy, ddy);
                                            push_critpt_if_valid(extr, EXTREMUM, roots_and_extrema);
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
                                        push_critpt_if_valid(boundary_x_not_nan_side,
                                                DISCONT_DOMAIN, discont);
                                    }
                                }
                                prev_x = x; prev_y = y;
                            }
                        }
                        // cout << endl;
                        // Add screen edges to discontinuities list for convenience
                        discont.emplace(xmin, DISCONT_SCREEN);
                        discont.emplace(xmax, DISCONT_SCREEN);

                        // Previous discontinuity infop
                        double prev_discont_x = xmin;
                        float prev_discont_sx;
                        int prev_discont_type;
                        size_t as_idx = 0;

                        float psx = -1.f, psy = -1.f;
                        // ** Main explicit func drawing code: draw function from discont to discont
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
                                                (as_idx < discont.size() - 1 && discont_sx - sxd < 1.)) {
                                            sxd += 0.25f;
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

                        // Finish last line, if exists
                        if (curr_line.size() > 1) {
                            graph.polyline(curr_line, func_color, curr_func == exprid ? 3 : 2.);
                        }
                        std::vector<CritPoint> to_erase; // Save dubious points to delete from roots_and_extrama
                        // Helper to draw roots/extrema/y-int and add a marker for it
                        auto draw_extremum = [&](const CritPoint& cpt, double y) {
                            double dy = func.diff(env);
                            double ddy = func.ddiff(env);
                            double x; int type;
                            std::tie(x, type) = cpt;
                            auto label =
                                type == Y_INT ? PointMarker::LABEL_Y_INT :
                                type == ROOT ? PointMarker::LABEL_X_INT :
                                (type == EXTREMUM && ddy > 2e-7 * ydiff) ? PointMarker::LABEL_LOCAL_MIN :
                                (type == EXTREMUM && ddy < -2e-7 * ydiff) ? PointMarker::LABEL_LOCAL_MAX:
                                type == EXTREMUM ? PointMarker::LABEL_INFLECTION_PT:
                                PointMarker::LABEL_NONE;
                            // Do not show
                            if (label == PointMarker::LABEL_INFLECTION_PT ||
                                label == PointMarker::LABEL_NONE) {
                                to_erase.push_back(cpt);
                                return;
                            }

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
                            for (int r = std::max(sy - MARKER_CLICKABLE_RADIUS, 0); r <= std::min(sy + MARKER_CLICKABLE_RADIUS, shigh-1); ++r) {
                                for (int c = std::max(sx - MARKER_CLICKABLE_RADIUS, 0); c <= std::min(sx + MARKER_CLICKABLE_RADIUS, swid-1); ++c) {
                                    grid[r * swid + c] = idx;
                                }
                            }
                        };
                        for (const CritPoint& cpt : roots_and_extrema) {
                            env.vars[x_var] = cpt.first;
                            double y = expr(env);
                            draw_extremum(cpt, y);
                        }
                        // Delete the "dubious" points
                        for (const CritPoint& x : to_erase) {
                            roots_and_extrema.erase(x);
                        }
                        if (!func.expr.is_null() && !func.diff.is_null()) {
                            env.vars[x_var] = 0;
                            double y = expr(env);
                            if (!std::isnan(y) && !std::isinf(y)) {
                                push_critpt_if_valid(0., Y_INT, roots_and_extrema); // y-int
                                auto cpt = CritPoint(0., Y_INT);
                                if (roots_and_extrema.count(cpt)) {
                                    draw_extremum(cpt, y);
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
                                for (int sxd = 0; sxd < swid; sxd += 10) {
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
                                    for (int r = std::max(sy - MARKER_CLICKABLE_RADIUS, 0); r <= std::min(sy + MARKER_CLICKABLE_RADIUS, shigh-1); ++r) {
                                        for (int c = std::max(sx - MARKER_CLICKABLE_RADIUS, 0); c <= std::min(sx + MARKER_CLICKABLE_RADIUS, swid-1); ++c) {
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
        // PROFILE(all);
        if (loss_detail) {
            func_error = "Warning: some detail may be lost";
        } else if (prev_loss_detail) {
            func_error.clear();
        }
    }

    // Re-parse expression from expr_str into expr, etc. for function 'idx'
    // and update expression, derivatives, etc.
    // Also detects function type.
    void reparse_expr(size_t idx = -1);

    // Set the current function (drawn thicker than other functions) to func_id.
    // If func_id is just beyond last current function (i.e., = funcs.size()),
    // adds a new function
    void set_curr_func(size_t func_id);

    // Add a new function to the end of funcs.
    void add_func();

    // Delete the func at idx. If idx == -1, then deletes current function.
    void delete_func(size_t idx = -1);

    // Add a new slider to the end of sliders
    void add_slider();

    // Delete the slider at index idx in sliders
    void delete_slider(size_t idx);

    // Call this when slider's variable name (var_name) changes:
    // Using var_name update var_addr of slider at idx.
    // If var_name is invalid, sets slider_error
    void update_slider_var(size_t idx);

    // Call this when slider's variable value (val) changes:
    // checks if val is outside bounds (in which case updates bounds)
    // then copies the slider or float input value into the environment
    void copy_slider_value_to_env(size_t idx);

    // Call this when plotter window/widget is resizing:
    // Resize the plotter area to width x height
    void resize(int width, int height);

    // Reset the plotter's view (xmin, xmax, etc.) to initial view,
    // accounting for current window size
    void reset_view();

    // * Keyboard/mouse handlers
    // A basic key handler: key code, ctrl pressed?, alt pressed?
    void handle_key(int key, bool ctrl, bool alt);

    // Mouse down handler (any key)
    void handle_mouse_down(int px, int py);

    // Mouse move handler (any key)
    void handle_mouse_move(int px, int py);

    // Mouse up handler (any key)
    void handle_mouse_up(int px, int py);

    // Mouse wheel handler:
    // upwards = whether scrolling up,
    // distance = magnitude of amount scrolled
    void handle_mouse_wheel(bool upwards, int distance, int px, int py);

    // Public plotter state data
    int swid, shigh;                        // Screen size

    double xmax = 10, xmin = -10;           // Function area bounds: x
    double ymax = 6, ymin = -6;             // Function area bounds: y
    size_t curr_func = 0;                   // Currently selected function

    std::vector<Function> funcs;            // Functions
    std::string func_error;                 // Function parsing error str
    std::vector<PointMarker> pt_markers;    // Point markers
    std::vector<size_t> grid;               // Grid containing marker id
                                            // at every pixel, row major
                                            // (-1 if no marker)
    // Slider data
    std::vector<SliderData> sliders;        // Sliders to show
    std::set<std::string> sliders_vars;     // Variables which have a slider
    std::string slider_error;               // Error from slider

    // If set, GUI implementation should:
    // focus on the editor for the current function AND
    // set focus_on_editor = false
    bool focus_on_editor = true;

    // If set, requires GUI to update lines and functions
    // (using draw).
    // - Implementation does not need to set this to false.
    // - Implementation may set this to true, if e.g. xmin/xmax
    //   are changed by code
    bool require_update = false;

    // Marker data
    std::string marker_text;
    int marker_posx, marker_posy;

    // The environment object, contains defined functions and variables
    Environment& env;

    // Mutex for concurrency
    std::mutex mtx;
private:
    // Helper for detecting if px, py activates a marker (in grid);
    // if so sets marker_* and current function
    // no_passive: if set, ignores passive markers
    void detect_marker_click(int px, int py, bool no_passive = false);
    Parser parser;                            // Parser instance, for parsing func expressions

    std::queue<color::color> reuse_colors;    // Reusable colors
    size_t last_expr_color = 0;               // Next available color index if no reusable
                                              // one present(by color::from_int)
    uint32_t x_var, y_var;                    // x,y variable addresses
    bool dragdown, draglabel;
    int dragx, dragy;
    double xmaxi, xmini, ymaxi, ymini;

    size_t next_func_name = 0;                // Next available function name

    bool loss_detail = false;                 // Whether some detail is lost (if set, will show error)
};

}  // namespace nivalis
#endif // ifndef _COMMON_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
