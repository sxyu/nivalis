#include "plotter/common.hpp"

#include "parser.hpp"
#include "util.hpp"

#include "json.hpp"
#include "shell.hpp"
#include <iomanip>
#include <iostream>

namespace {
using json = nlohmann::json;

const unsigned NUM_THREADS =
#ifdef NIVALIS_EMSCRIPTEN
    1;  // Multithreading not supported
#else
    std::thread::hardware_concurrency();
#endif

#define _X_TO_SX(x) static_cast<float>(((x) - view.xmin) * view.swid / (view.xmax - view.xmin))
#define _Y_TO_SY(y) static_cast<float>((view.ymax - (y)) * view.shigh / (view.ymax - view.ymin))

bool is_var_name_reserved(const std::string& var_name) {
    return var_name == "x" || var_name == "y" ||
           var_name == "t" || var_name == "r";
}
}

namespace nivalis {
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

    util::write_bin(os, polyline.size());
    for (size_t i = 0; i < polyline.size(); ++i) {
        polyline[i].to_bin(os);
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

    util::resize_from_read_bin(is, polyline);
    for (size_t i = 0; i < polyline.size(); ++i) {
        polyline[i].from_bin(is);
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
    draglabel = dragdown = false;

    x_var = env.addr_of("x", false);
    y_var = env.addr_of("y", false);
    t_var = env.addr_of("t", false);
    r_var = env.addr_of("r", false);
    set_curr_func(0);
}

void Plotter::render(const View& view) {
    double xdiff = view.xmax - view.xmin, ydiff = view.ymax - view.ymin;

    // * Constants
    // Newton's method parameters
    static const double EPS_STEP  = 1e-10 * ydiff;
    static const double EPS_ABS   = 1e-10 * ydiff;
    static const int    MAX_ITER  = 30;
    // Shorthand for Newton's method arguments
#define NEWTON_ARGS x_var, x, env, EPS_STEP, EPS_ABS, MAX_ITER, \
    view.xmin - NEWTON_SIDE_ALLOW, view.xmax + NEWTON_SIDE_ALLOW
    // Amount x-coordinate is allowed to exceed the display boundaries
    const double NEWTON_SIDE_ALLOW = xdiff / 20.;

    // Minimum x-distance between critical points
    static const double MIN_DIST_BETWEEN_ROOTS  = 1e-4;
    // Point marker display size / mouse selecting area size
    static const int MARKER_DISP_RADIUS = 3;

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

    // For explicit functions
    // We will draw a function segment between every pair of adjacent
    // discontinuities (including 'edge of screen')
    // x-coordinate after a discontinuity to begin drawing function
    static const double DISCONTINUITY_EPS = xdiff * 1e-3;

    // Number of different t values to evaluate for a parametric
    // equation
    static const double PARAMETRIC_STEPS = 2500.0;

    bool prev_loss_detail = loss_detail;
    loss_detail = false; // Will set to show 'some detail may be lost'

    // * Clear back buffers
    pt_markers.clear(); pt_markers.reserve(500);
    draw_buf.clear();

    // * Draw functions
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
                            float sx = _X_TO_SX(x), sy = _Y_TO_SY(y);
                            line.push_back({sx, sy});
                            buf_add_rectangle(view, sx - (float)mark_radius,
                                    sy - (float)mark_radius,
                                    (float)mark_radius*2 + 1.f,
                                    (float)mark_radius*2 + 1.f, true, func_color);
                            PointMarker ptm;
                            ptm.label = PointMarker::LABEL_NONE;
                            ptm.y = y; ptm.x = x;
                            ptm.passive = false;
                            ptm.rel_func = exprid;
                            pt_markers.push_back(std::move(ptm));
                        }
                        buf_add_polyline(view, line, func_color, exprid, (curr_func == exprid) ? 3.f : 2.f);
                    }
                }
                break;
            case Function::FUNC_TYPE_IMPLICIT:
            case Function::FUNC_TYPE_IMPLICIT_INEQ:
            case Function::FUNC_TYPE_IMPLICIT_INEQ_STRICT:
                {
                    // Implicit function
                    if (func.expr.is_null() ||
                        (func.expr.ast[0].opcode == OpCode::val &&
                         func.type == Function::FUNC_TYPE_IMPLICIT)) {
                        // Either 0=0 or c=0 for some c, do not draw
                        continue;
                    }

                    // Inequality color
                    color::color ineq_color(func_color.r, func_color.g, func_color.b, 0.25);

                    // Coarse interval
                    static const int COARSE_INTERVAL = 4;

                    // Function values in above line
                    std::vector<double> coarse_line(view.swid + 2);

                    // 'Interesting' squares from above line/to left
                    // When a square is painted, need to paint neighbor
                    // to below (if changes sign left)/
                    // right (if changes sign above) as well
                    std::vector<bool> coarse_below_interesting(view.swid + 2);
                    bool coarse_right_interesting = false;

                    // Interesting squares
                    using Square = std::tuple<int, int, int, int, double>;
                    std::vector<Square> interest_squares;

                    // Increase interval per x pixels
                    static const size_t HIGH_PIX_LIMIT =
                        std::max<size_t>(200000 / NUM_THREADS, 20000);
                    // Maximum number of pixels to draw (stops drawing)
                    static const size_t MAX_PIXELS = 300000;
                    // Epsilon for bisection
                    static const double BISECTION_EPS = 1e-4;

                    for (int csy = -1; csy < view.shigh + COARSE_INTERVAL - 1; csy += COARSE_INTERVAL) {
                        int cyi = COARSE_INTERVAL;
                        if (csy >= view.shigh) {
                            csy = view.shigh - 1;
                            cyi = (view.shigh-1) % COARSE_INTERVAL;
                            if (cyi == 0) break;
                        }
                        const double cy = (view.shigh - csy)*1. / view.shigh * ydiff + view.ymin;
                        coarse_right_interesting = false;
                        for (int csx = -1; csx < view.swid + COARSE_INTERVAL - 1; csx += COARSE_INTERVAL) {
                            int cxi = COARSE_INTERVAL;
                            if (csx >= view.swid) {
                                csx = view.swid - 1;
                                cxi = (view.swid-1) % COARSE_INTERVAL;
                                if (cxi == 0) break;
                            }
                            // Update interval based on point count
                            const double coarse_x = 1.*csx / view.swid * xdiff + view.xmin;
                            double precise_x = coarse_x, precise_y = cy;
                            const int xy_pos = csy * view.swid + csx;

                            env.vars[y_var] = cy;
                            env.vars[x_var] = coarse_x;
                            double z = expr(env);
                            if (csx >= cxi-1 && csy >= cyi-1) {
                                bool interesting_square = false;
                                int sgn_z = (z < 0 ? -1 : z == 0 ? 0 : 1);
                                bool interest_from_left = coarse_right_interesting;
                                bool interest_from_above = coarse_below_interesting[csx+1];
                                coarse_right_interesting = coarse_below_interesting[csx+1] = false;
                                double zleft = coarse_line[csx+1 - cxi];
                                int sgn_zleft = (zleft < 0 ? -1 : zleft == 0 ? 0 : 1);
                                if (sgn_zleft * sgn_z <= 0) {
                                    coarse_below_interesting[csx+1] = true;
                                    interesting_square = true;
                                }
                                double zup = coarse_line[csx+1];
                                int sgn_zup = (zup < 0 ? -1 : zup == 0 ? 0 : 1);
                                if (sgn_zup * sgn_z <= 0) {
                                    coarse_right_interesting = true;
                                    interesting_square = true;
                                }
                                if (interesting_square ||
                                    interest_from_left ||
                                    interest_from_above) {
                                    interest_squares.push_back(Square(
                                                csy - cyi, csy, csx - cxi, csx, z
                                                ));
                                } else if (sgn_z >= 0 &&
                                        func.type !=
                                        Function::FUNC_TYPE_IMPLICIT) {
                                    // Inequality region
                                    buf_add_rectangle(view,
                                            csx + (float)(- cxi + 1),
                                            csy + (float)(- cyi + 1),
                                            (float)cxi, (float)cyi, true, ineq_color);
                                }
                            }
                            coarse_line[csx+1] = z;
                        }
                    }

                    {
#ifndef NIVALIS_EMSCRIPTEN
                        std::atomic<int> sqr_id(0);
#else
                        int sqr_id(0);
#endif
                        int pad = (curr_func == exprid) ? 1 : 0;
                        auto worker = [&]() {
                            std::vector<double> line(view.swid + 2);
                            std::vector<bool> fine_paint_below(view.swid + 2);
                            std::vector<std::array<int, 3> > tdraws, tdraws_ineq;
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
                                fine_interval = static_cast<int>(tpix_cnt /
                                            HIGH_PIX_LIMIT) + 1;
                                if (fine_interval >= 3) fine_interval = 4;

                                int ylo, yhi, xlo, xhi; double z_at_xy_hi;
                                std::tie(ylo, yhi, xlo, xhi, z_at_xy_hi) = sqr;

                                std::fill(fine_paint_below.begin() + (xlo + 1),
                                        fine_paint_below.begin() + (xhi + 2), false);
                                for (int sy = ylo; sy <= yhi; sy += fine_interval) {
                                    fine_paint_right = false;
                                    const double y = (view.shigh - sy)*1. / view.shigh * ydiff + view.ymin;
                                    for (int sx = xlo; sx <= xhi; sx += fine_interval) {

                                        const double x = 1.*sx / view.swid * xdiff + view.xmin;
                                        // double precise_x = x, precise_y = y;

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
                                            bool paint_from_above = fine_paint_below[sx+1];
                                            fine_paint_right = fine_paint_below[sx+1] = false;
                                            double zup = line[sx+1];
                                            int sgn_zup = (zup < 0 ? -1 : zup == 0 ? 0 : 1);
                                            if (sgn_zup * sgn_z <= 0) {
                                            //     // Bisect up
                                            //     tenv.vars[x_var] = x;
                                            //     double lo = y;
                                            //     double hi = (view.shigh - (sy - fine_interval))*1. / view.shigh * ydiff + view.ymin;
                                            //     while (hi - lo > BISECTION_EPS) {
                                            //         double mi = (lo + hi) / 2;
                                            //         tenv.vars[y_var] = mi;
                                            //         double zmi = expr(tenv);
                                            //         int sgn_zmi = (zmi < 0 ? -1 : 1);
                                            //         if (sgn_z == sgn_zmi) lo = mi;
                                            //         else hi = mi;
                                            //     }
                                            //     precise_y = lo;
                                                paint_square = fine_paint_right = true;
                                            }
                                            double zleft = line[sx+1 - fine_interval];
                                            int sgn_zleft = (zleft < 0 ? -1 : zleft == 0 ? 0 : 1);
                                            if (sgn_zleft * sgn_z <= 0) {
                                                // if (!paint_square) {
                                                //     // Bisect left
                                                //     tenv.vars[y_var] = y;
                                                //     double lo = 1.*(sx - fine_interval) / view.swid * xdiff + view.xmin;
                                                //     double hi = x;
                                                //     while (hi - lo > BISECTION_EPS) {
                                                //         double mi = (lo + hi) / 2;
                                                //         tenv.vars[x_var] = mi;
                                                //         double zmi = expr(tenv);
                                                //         int sgn_zmi = (zmi < 0 ? -1 : 1);
                                                //         if (sgn_zleft == sgn_zmi) lo = mi;
                                                //         else hi = mi;
                                                //     }
                                                //     precise_x = lo;
                                                // }
                                                paint_square = fine_paint_below[sx+1] = true;
                                            }
                                            if (paint_square || paint_from_left || paint_from_above) {
                                                // float precise_sy =
                                                //     static_cast<float>(
                                                //             (view.ymax - y) / ydiff * view.shigh);
                                                // float precise_sx = static_cast<float>(
                                                //         (x - view.xmin) / xdiff * view.swid);
                                                tdraws.push_back({sx, sy, fine_interval});
                                                ++tpix_cnt;
                                            } else if (sgn_z >= 0 &&
                                                    func.type !=
                                                    Function::FUNC_TYPE_IMPLICIT) {
                                                // Inequality region
                                                tdraws_ineq.push_back({sx, sy, fine_interval});
                                            }
                                        }
                                        line[sx+1] = z;
                                    }
                                }
                            }

                            {
#ifndef NIVALIS_EMSCRIPTEN
                                std::lock_guard<std::mutex> lock(mtx);
#endif
                                if (func.type != Function::FUNC_TYPE_IMPLICIT_INEQ_STRICT) {
                                    // Draw function line (boundary
                                    for (auto& p : tdraws) {
                                        buf_add_rectangle(view,
                                                p[0] + (float)(- p[2] + 1 - pad),
                                                p[1] + (float)(- p[2] + 1 - pad),
                                                (float)(p[2] + pad),
                                                (float)(p[2] + pad),
                                                true, func_color);
                                    }
                                }
                                for (auto& p : tdraws_ineq) {
                                    // Draw inequality region
                                    buf_add_rectangle(view,
                                            p[0] + (float)(- p[2] + 1),
                                            p[1] + (float)(- p[2] + 1),
                                            (float)p[2],
                                            (float)p[2],
                                            true,
                                            ineq_color);
                                }

                                size_t sz = pt_markers.size();
                                pt_markers.resize(sz + tpt_markers.size());
                                std::move(tpt_markers.begin(), tpt_markers.end(),
                                        pt_markers.begin() + sz);

                                // Show detail lost warning
                                if (fine_interval > 1) loss_detail = true;
                            }
                        };

#ifndef NIVALIS_EMSCRIPTEN
                        if (NUM_THREADS <= 1) {
                            worker();
                        } else {
                            std::vector<std::thread> pool;
                            for (size_t i = 0; i < NUM_THREADS; ++i) {
                                pool.emplace_back(worker);
                            }
                            for (size_t i = 0; i < pool.size(); ++i) pool[i].join();
                        }
#else
                        worker();
#endif
                    }

                }
                break;
            case Function::FUNC_TYPE_PARAMETRIC:
            case Function::FUNC_TYPE_POLAR:
                {
                    if (func.type == Function::FUNC_TYPE_PARAMETRIC
                        && func.polyline.size() != 2) continue;
                    std::vector<std::array<float, 2> > curr_line;
                    double tmin = (double)func.tmin;
                    double tmax = (double)func.tmax;
                    if (tmin > tmax) std::swap(tmin, tmax);
                    double tstep = (tmax - tmin) / PARAMETRIC_STEPS;
                    for (double t = tmin; t <= tmax; t += tstep) {
                        env.vars[t_var] = t;
                        double x, y;
                        if (func.type == Function::FUNC_TYPE_PARAMETRIC) {
                            x = func.polyline[0](env);
                            y = func.polyline[1](env);
                        } else {
                            double r = func.expr(env);
                            x = r * cos(t); y = r * sin(t);
                        }
                        float sx = _X_TO_SX(x), sy = _Y_TO_SY(y);
                        curr_line.push_back({sx, sy});
                    }
                    buf_add_polyline(view, curr_line, func_color, exprid,
                            curr_func == exprid ? 3 : 2.);
                }
                break;
            case Function::FUNC_TYPE_EXPLICIT:
                {
                    // explicit function
                    env.vars[y_var] = std::numeric_limits<double>::quiet_NaN();
                    // Discontinuity/root/extremum type enum
                    enum {
                        DISCONT_ASYMPT = 0, // asymptote (in middle of domain, e.g. 0 of 1/x)
                        DISCONT_DOMAIN,     // domain boundary (possibly asymptote, e.g. 0 of ln(x))
                        DISCONT_SCREEN,     // edge of screen
                        ROOT,               // root
                        EXTREMUM,           // extremum
                        Y_INT,              // y-intercept
                    };
                    // CritPoint: discontinuity/root/extremum type
                    // first: x-position
                    // second: enum
                    using CritPoint = std::pair<double, int>;
                    // Store discontinuities
                    std::set<CritPoint> discont, roots_and_extrema;
                    size_t idx = 0;
                    // Push check helpers
                    // Push to st if no other item less than MIN_DIST_BETWEEN_ROOTS from value
                    auto push_if_valid = [this, &view](double value, std::set<double>& st) {
                        if (!std::isnan(value) && !std::isinf(value) &&
                                value >= view.xmin && value <= view.xmax) {
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
                    auto push_critpt_if_valid = [&](double value,
                            CritPoint::second_type type,
                            std::set<CritPoint>& st) {
                        auto vc = CritPoint(value, type);
                        if (!std::isnan(value) && !std::isinf(value) &&
                                value >= view.xmin && value <= view.xmax) {
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
                        // Draw the line
                        if (curr_line.empty() ||
                                (psx > curr_line.back()[0] || psy != curr_line.back()[1])) {
                            if (curr_line.size() > 1) {
                                buf_add_polyline(view, curr_line, func_color, exprid,
                                        curr_func == exprid ? 3 : 2.);
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
                        for (int sx = 0; sx < view.swid; sx += 10) {
                            const double x = sx*1. * xdiff / view.swid + view.xmin;
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
                    // Add screen edges to discontinuities list for convenience
                    discont.emplace(view.xmin, DISCONT_SCREEN);
                    discont.emplace(view.xmax, DISCONT_SCREEN);

                    // Previous discontinuity infop
                    double prev_discont_x = view.xmin;
                    float prev_discont_sx;
                    int prev_discont_type;
                    size_t as_idx = 0;

                    float psx = -1.f, psy = -1.f;
                    // ** Main explicit func drawing code: draw function from discont to discont
                    for (const auto& discontinuity : discont) {
                        double discont_x = discontinuity.first;
                        int discont_type = discontinuity.second;
                        float discont_sx = _X_TO_SX(discont_x);
                        if (as_idx > 0) {
                            bool connector = prev_discont_type != DISCONT_SCREEN;
                            // Draw func between asymptotes
                            for (float sxd = prev_discont_sx + DISCONTINUITY_EPS;
                                    sxd < discont_sx;) {
                                const double x = sxd / view.swid * xdiff + view.xmin;
                                env.vars[x_var] = x;
                                double y = expr(env);

                                if (!std::isnan(y)) {
                                    if (y > view.ymax + ydiff) y = view.ymax + ydiff;
                                    else if (y < view.ymin - ydiff) y = view.ymin - ydiff;
                                    float sy = _Y_TO_SY(y), sx = sxd;
                                    if (reinit) {
                                        if (!(y > view.ymax || y < view.ymin)) {
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
                                        } else if (yp - yp2 < -eps && sy < (float)view.shigh) {
                                            psx = prev_discont_sx;
                                            psy = static_cast<float>(view.shigh);
                                            reinit = false;
                                        }
                                    }
                                    if (!reinit && psx >= 0) {
                                        draw_line(psx, psy, sx, sy, x, y);
                                        if (y > view.ymax || y < view.ymin) {
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
                                if (yp - yp2 < -eps && psy < view.shigh) {
                                    sx = discont_sx;
                                    sy = static_cast<float>(view.shigh);
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
                        buf_add_polyline(view, curr_line, func_color, exprid,
                                curr_func == exprid ? 3 : 2.);
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

                        int sy = static_cast<int>((view.ymax - y) / ydiff * view.shigh);
                        int sx = static_cast<int>((x - view.xmin) / xdiff * view.swid);
                        buf_add_rectangle(view, sx-MARKER_DISP_RADIUS,
                                sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1,
                                2*MARKER_DISP_RADIUS+1, true, color::LIGHT_GRAY);
                        buf_add_rectangle(view, sx-MARKER_DISP_RADIUS, sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1, 2*MARKER_DISP_RADIUS+1, false, func.line_color);
                        PointMarker ptm;
                        ptm.label = label;
                        ptm.y = y; ptm.x = x;
                        ptm.passive = false;
                        ptm.rel_func = exprid;
                        pt_markers.push_back(std::move(ptm));
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
                            for (int sxd = 0; sxd < view.swid; sxd += 10) {
                                const double x = sxd * xdiff / view.swid + view.xmin;
                                double root = sub_expr.newton(NEWTON_ARGS, &diff_sub_expr);
                                push_if_valid(root, st);
                            }
                            for (double x : st) {
                                env.vars[x_var] = x;
                                double y = expr(env);
                                int sy = static_cast<int>((view.ymax - y) /
                                                          ydiff * view.shigh);
                                int sx = static_cast<int>((x - view.xmin) /
                                                          xdiff * view.swid);
                                buf_add_rectangle(view, sx-MARKER_DISP_RADIUS,
                                        sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS,
                                        2*MARKER_DISP_RADIUS, true, color::LIGHT_GRAY);
                                buf_add_rectangle(view, sx-MARKER_DISP_RADIUS-1,
                                        sy-MARKER_DISP_RADIUS-1, 2*MARKER_DISP_RADIUS+1,
                                        2*MARKER_DISP_RADIUS+1, false, func.line_color);
                                buf_add_rectangle(view, sx-MARKER_DISP_RADIUS,
                                        sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1,
                                        2*MARKER_DISP_RADIUS+1, false,
                                        func2.line_color);
                                size_t idx = pt_markers.size();
                                PointMarker ptm;
                                ptm.label = PointMarker::LABEL_INTERSECTION;
                                ptm.x = x; ptm.y = y;
                                ptm.passive = false;
                                ptm.rel_func = -1;
                                pt_markers.push_back(std::move(ptm));
                            } // for x : st
                        } // for exprid2
                    } // if (!func.diff.is_null())
                    // * End of function intersection code
                } // case
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
void Plotter::render() {
    render(view);
}

void Plotter::populate_grid() {
    const int r = marker_clickable_radius;
    grid.resize(view.swid * view.shigh);
    std::fill(grid.begin(), grid.end(), -1);

    thread_local std::deque<
            std::array<int, 4> // y x id dist
            > que;

    que.clear();
    auto bfs = [&]{
        // BFS to fill grid
        while(!que.empty()) {
            auto v = que.front();
            int sy = v[0], sx = v[1], id = v[2], d = v[3];
            grid[sy * view.swid + sx] = id;
            que.pop_front();
            if (d >= marker_clickable_radius) continue;
            if (sy > 0 && grid[(sy - 1) * view.swid + sx] == -1) {
                que.push_back({ sy - 1, sx, id, d + 1});
            }
            if (sx > 0 && grid[sy * view.swid + sx - 1] == -1) {
                que.push_back({ sy, sx - 1, id, d + 1});
            }
            if (sx < view.swid - 1 && grid[sy * view.swid + sx + 1] == -1) {
                que.push_back({ sy, sx + 1, id, d + 1});
            }
            if (sy < view.shigh - 1 && grid[(sy + 1) * view.swid + sx] == -1) {
                que.push_back({ sy + 1, sx, id, d + 1});
            }
        }
    };

    // Push non-passive markers (crit points, should cover other points)
    for (size_t id = 0; id < pt_markers.size(); ++id) {
        auto& ptm = pt_markers[id];
        int sx = (int)(0.5 + _X_TO_SX(ptm.x));
        int sy = (int)(0.5 + _Y_TO_SY(ptm.y));
        if (sx < 0 || sy < 0 || sx >= view.swid || sy >= view.shigh)
            continue;
        que.push_back({ sy, sx, (int)id, 0 });
    }
    bfs();

    // Construct and push passive markers
    for (size_t i = 0; i < draw_buf.size(); ++i) {
        const auto & obj = draw_buf[i];
        if (obj.type == FuncDrawObj::POLYLINE) {
            const auto& points = obj.points;
            for (size_t j = 1; j < points.size(); ++j) {
                const std::array<double, 2>& p = points[j - 1];
                const std::array<double, 2>& q = points[j];
                size_t id = pt_markers.size();
                {
                    PointMarker ptm;
                    ptm.x = p[0]; ptm.y = p[1];
                    ptm.label = PointMarker::LABEL_NONE;
                    ptm.rel_func = obj.rel_func;
                    ptm.passive = true;
                    pt_markers.push_back(std::move(ptm));
                }
                int psx = (int)(0.5 + _X_TO_SX(p[0]));
                int qsx = (int)(0.5 + _X_TO_SX(q[0]));
                if (psx < 0 || psx >= view.swid || qsx - psx > 1) continue;

                int psy = (int)(0.5 + _Y_TO_SY(p[1]));
                int qsy = (int)(0.5 + _Y_TO_SY(q[1]));
                int dir = qsy >= psy ? 1 : -1;
                for (int r = std::min(std::max(psy, 0), view.shigh - 1);
                        r != std::min(std::max(qsy, 0), view.shigh - 1) + dir; r += dir) {
                    que.push_back({ r, psx, (int)id - 1, 0 });
                }
            }
        }
    }
    bfs();
}

void Plotter::reparse_expr(size_t idx) {
    if (idx == -1 ) idx = curr_func;
    auto& func = funcs[idx];
    auto& expr = func.expr;
    auto& expr_str = func.expr_str;
    std::string parse_err, polyline_err;

    // Ugly code to try and determine the type of function
    func.polyline.clear();
    // Marks whether this is a vlaid polyline expr
    bool valid_polyline;
    util::trim(expr_str);
    // Determine if function type is polyline
    if (expr_str.size() &&
            expr_str[0] == '(' && expr_str.back() == ')') {
        // Only try if of form (...)
        valid_polyline = true;

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
                        func.polyline.push_back(parse(
                                    expr_str.substr(last_begin,
                                        i - last_begin), env,
                                    true, true, 0, &parse_err));
                        if (parse_err.size()) polyline_err = parse_err;
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
                        func.polyline.push_back(parse(
                                    expr_str.substr(last_begin,
                                        i - last_begin), env,
                                    true, true, 0, &parse_err));
                        if (parse_err.size()) polyline_err = parse_err;
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
                        polyline_err = "x, y disallowed in tuple "
                            "(polyline/parametric equation)\n";
                        break;
                    } else if (e1.has_var(t_var)) {
                        // Detect parametric equation
                        if (func.polyline.size() != 2) {
                            func.polyline.clear();
                            polyline_err = "Parametric equation can't have "
                                "more than one tuple\n";
                            break;
                        } else {
                            // Parametric
                            func.type = Function::FUNC_TYPE_PARAMETRIC;
                        }
                    }
                }
            }
            // Keep as polyline type but show error
            // so that the user can see info about why it failed to parse
            if (polyline_err.size())
                func_error = polyline_err;
            else func_error.clear();
        }
    } else valid_polyline = false;
    if (!valid_polyline) {
        // If failed to parse as polyline expr, try to detect if
        // it is an implicit function
        size_t eqpos = util::find_equality(expr_str, true, false);
        if (~eqpos) {
            func.type = Function::FUNC_TYPE_IMPLICIT;
            bool flip = false;
            size_t eqpos_next = eqpos + 1;
            const char ch = expr_str[eqpos];
            const char next_ch = expr_str[eqpos + 1];
            if (next_ch == '=') ++eqpos_next;
            if (ch == '>' || ch == '<') {
                // Inequality
                func.type = (eqpos_next != eqpos + 1) ?
                    func.type = Function::FUNC_TYPE_IMPLICIT_INEQ :
                    Function::FUNC_TYPE_IMPLICIT_INEQ_STRICT;
                flip = ch == '<';
            }
            auto lhs = expr_str.substr(0, eqpos),
                 rhs = expr_str.substr(eqpos_next);
            bool valid_implicit_func = true;
            util::trim(lhs); util::trim(rhs);
            if (func.type == Function::FUNC_TYPE_IMPLICIT
                    && (lhs == "y" || rhs == "y")) {
                expr = parse(lhs == "y" ? rhs : lhs, env,
                        true, // explicit
                        true,  // quiet
                        0, &parse_err);
                if (!expr.has_var(y_var)) {
                    // if one side is y and other side has no y,
                    // treat as explicit function
                    func.type = Function::FUNC_TYPE_EXPLICIT;
                    valid_implicit_func = false;
                }
            }
            if (func.type == Function::FUNC_TYPE_IMPLICIT
                    && (lhs == "r" || rhs == "r")) {
                expr = parse(lhs == "r" ? rhs : lhs, env,
                        true, // explicit
                        true,  // quiet
                        0, &parse_err
                      );
                if (!expr.has_var(x_var) &&
                    !expr.has_var(y_var)) {
                    // if one side is r and other side has no x,y,
                    // treat as polar function
                    func.type = Function::FUNC_TYPE_POLAR;
                    valid_implicit_func = false;
                }
            }
            if (valid_implicit_func) {
                // If none of these apply, set expression to difference
                // i.e. rearrange so RHS is 0
                if (flip) {
                    expr = parse("(" + rhs + ")-(" + lhs + ")",
                            env, true, true, 0, &parse_err);
                } else {
                    expr = parse("(" + lhs + ")-(" + rhs + ")",
                            env, true, true, 0, &parse_err);
                }
            }
        } else {
            func.type = Function::FUNC_TYPE_EXPLICIT;
            expr = parse(expr_str, env, true, true, 0, &parse_err);
        }
        if (!expr.is_null()) {
            // Optimize the main expression
            if (func.type != Function::FUNC_TYPE_PARAMETRIC)
                expr.optimize();

            // Compute derivatives, if explicit
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
        func_error = parse_err;
    }

    // Optimize any polyline/parametric point expressions
    for (auto& point_expr : func.polyline) {
        point_expr.optimize();
    }

    if (parse_err.empty()) func_error.clear();
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
    switch(key) {
        case 37: case 39: case 262: case 263:
            // LR Arrow
            {
                auto delta = (view.xmax - view.xmin) * 0.003;
                if (key == 37 || key == 263) delta = -delta;
                view.xmin += delta; view.xmax += delta;
            }
            require_update = true;
            break;
        case 38: case 40: case 264: case 265:
            {
                // UD Arrow
                auto delta = (view.ymax - view.ymin) * 0.003;
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
                auto fa = (key == 45 || key == 189 ||
                            key == 173) ? 1.013 : 0.987;
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
    if (!dragdown && !draglabel) {
        if (px >= 0 && py >= 0 &&
                py * view.swid + px < grid.size() &&
                ~grid[py * view.swid + px]) {
            // Show marker
            detect_marker_click(px, py);
            draglabel = true;
        } else {
            // Begin dragging window
            dragdown = true;
            dragx = px; dragy = py;
        }
    }
}

void Plotter::handle_mouse_move(int px, int py) {
    if (dragdown) {
        // Dragging background
        marker_text.clear();
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
        std::vector<int> slider_ids;
        for (auto& sl : sliders) {
            slider_ids.push_back(sl.var_addr);
        }
        std::sort(slider_ids.begin(), slider_ids.end());
        size_t j = 0;
        // Export variable values
        for (size_t i = 0; i < env.vars.size(); ++i) {
            // Do not store x,y,z, etc.
            if (is_var_name_reserved(env.varname[i])) continue;
            while (j < slider_ids.size() && slider_ids[j] < i) ++j;
            // Do not store slider variables
            if (j < slider_ids.size() && slider_ids[j] == i) continue;
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
                        tmpshell.eval_line(line.get<std::string>());
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
        funcs.clear();
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

void Plotter::detect_marker_click(int px, int py, bool no_passive) {
    auto& ptm = pt_markers[grid[py * view.swid + px]];
    if (ptm.passive && no_passive) return;
    marker_posx = px; marker_posy = py + 20;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4) <<
        PointMarker::label_repr(ptm.label) << ptm.x << ", " << ptm.y;
    marker_text = ss.str();
    if (ptm.passive && ~ptm.rel_func && ptm.rel_func != curr_func) {
        // Switch to function
        set_curr_func(ptm.rel_func);
        require_update = true;
    }
}

void Plotter::buf_add_polyline(const View& render_view,
        const std::vector<std::array<float, 2> >& points,
        const color::color& c, size_t rel_func, float thickness) {
    FuncDrawObj obj;
    obj.points.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        obj.points[i][0] = points[i][0]*1. / render_view.swid *
            (render_view.xmax - render_view.xmin) + render_view.xmin;
        obj.points[i][1] = (render_view.shigh - points[i][1])*1. /
            render_view.shigh * (render_view.ymax - render_view.ymin) + render_view.ymin;
    }
    obj.type = FuncDrawObj::POLYLINE;
    obj.rel_func = rel_func;
    obj.c = c;
    obj.thickness = thickness;
    draw_buf.push_back(std::move(obj));
}
void Plotter::buf_add_rectangle(const View& render_view,
        float x, float y, float w, float h, bool fill, const color::color& c) {
    FuncDrawObj rect;
    rect.type = fill ? FuncDrawObj::FILLED_RECT : FuncDrawObj::RECT;
    rect.points = {{(double)x, (double)y}, {(double)(x+w), (double)(y+h)}};
    rect.rel_func = -1; // Not used
    auto xdiff = render_view.xmax - render_view.xmin;
    auto ydiff = render_view.ymax - render_view.ymin;
    for (size_t i = 0; i < rect.points.size(); ++i) {
        rect.points[i][0] = rect.points[i][0]*1. / render_view.swid * xdiff + render_view.xmin;
        rect.points[i][1] = (render_view.shigh - rect.points[i][1])*1. / render_view.shigh * ydiff + render_view.ymin;
    }
    if (fill && draw_buf.size()) {
        auto& last_rect = draw_buf.back();
        if (last_rect.type == FuncDrawObj::FILLED_RECT &&
                std::fabs(last_rect.points[0][1] - rect.points[0][1]) < 1e-6 * ydiff &&
                std::fabs(last_rect.points[1][1] - rect.points[1][1]) < 1e-6 * ydiff &&
                std::fabs(last_rect.points[1][0] - rect.points[0][0]) < 1e-6 * xdiff &&
                last_rect.c == c) {
            // Reduce shape count by merging with rectangle to left
            last_rect.points[1][0] = rect.points[1][0];
            return;
        } else if (last_rect.type == FuncDrawObj::FILLED_RECT &&
                std::fabs(last_rect.points[0][0] - rect.points[0][0]) < 1e-6 * xdiff &&
                std::fabs(last_rect.points[1][0] - rect.points[1][0]) < 1e-6 * xdiff &&
                std::fabs(last_rect.points[1][1] - rect.points[0][1]) < 1e-6 * ydiff &&
                last_rect.c == c) {
            // Reduce shape count by merging with rectangle above
            last_rect.points[1][1] = rect.points[1][1];
            return;
        }
    }
    rect.c = c;
    draw_buf.push_back(std::move(rect));
}
}  // namespace nivalis
