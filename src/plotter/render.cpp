#include "plotter/plotter.hpp"
#include "plotter/internal.hpp"
#include <iostream>

namespace nivalis {

namespace {
const unsigned NUM_THREADS =
#ifdef NIVALIS_EMSCRIPTEN
    1;  // Multithreading not supported
#else
    std::thread::hardware_concurrency();
#endif

color::color get_ineq_color(const color::color& func_line_color) {
    color::color ret = func_line_color;
    ret.a = 0.25;
    return ret;
}

// Buffer anagement
// Add polyline/polygon to buffer (points in plot coords) to buffer
// automatically splits the line where consecutive points are near colinear
void buf_add_polyline(
        std::vector<DrawBufferObject>& draw_buf,
        const Plotter::View& render_view,
        const std::vector<std::array<double, 2> >& points,
        const color::color& c, size_t rel_func, float thickness = 1.,
        bool closed = false, bool line = true, bool filled = false) {
    DrawBufferObject obj_fill;
    obj_fill.rel_func = rel_func;
    obj_fill.thickness = thickness;
    if (filled) {
        obj_fill.points = points;
        obj_fill.type = DrawBufferObject::FILLED_POLYGON;
        obj_fill.c = get_ineq_color(c);
        draw_buf.push_back(std::move(obj_fill));
    }
    if (line) {
        // Detect colinearities
        DrawBufferObject obj;
        obj = obj_fill;
        obj.c = c;
        obj.type = closed ? DrawBufferObject::POLYGON :
            DrawBufferObject::POLYLINE;
        obj.points.reserve(points.size());
        // draw_buf.push_back(std::move(obj_tmp));
        for (size_t i = 0; i < points.size(); ++i) {
            // if (std::isnan(points[i][0])) break;
            size_t j = obj.points.size();
            obj.points.push_back({points[i][0], points[i][1]});
            if (j >= 2) {
                double ax = (obj.points[j][0] - obj.points[j-1][0]);
                double ay = (obj.points[j][1] - obj.points[j-1][1]);
                double bx = (obj.points[j-1][0] - obj.points[j-2][0]);
                double by = (obj.points[j-1][1] - obj.points[j-2][1]);
                double theta_a = std::fmod(std::atan2(ay, ax) + 2*M_PI, 2*M_PI);
                double theta_b = std::fmod(std::atan2(by, bx) + 2*M_PI, 2*M_PI);
                double angle_between = std::min(std::fabs(theta_a - theta_b),
                        std::fabs(theta_a + 2*M_PI - theta_b));
                if (std::fabs(angle_between - M_PI) < 1e-1) {
                    // Near-colinear, currently ImGui's polyline drawing may will break
                    // in this case. We split the obj.points here.
                    draw_buf.push_back(obj);
                    obj.points[0] = obj.points[j-1]; obj.points[1] = obj.points[j];
                    obj.points.resize(2);
                }
            }
        }
        draw_buf.push_back(obj);
    }
}

// Add polyline (points in screen coords) to buffer
void buf_add_screen_polyline(
        std::vector<DrawBufferObject>& draw_buf,
        const Plotter::View& render_view,
        const std::vector<std::array<float, 2> >& points_screen,
        const color::color& c, size_t rel_func, float thickness = 1.,
        bool closed = false, bool line = true, bool filled = false) {
    std::vector<std::array<double, 2> > points_conv;
    points_conv.resize(points_screen.size());
    for (size_t i = 0; i < points_screen.size(); ++i) {
        points_conv[i][0] = points_screen[i][0]*1. / render_view.swid *
            (render_view.xmax - render_view.xmin) + render_view.xmin;
        points_conv[i][1] = (render_view.shigh - points_screen[i][1])*1. /
            render_view.shigh * (render_view.ymax - render_view.ymin) + render_view.ymin;
    }
    buf_add_polyline(draw_buf, render_view, points_conv, c, rel_func,
            thickness, closed, line, filled) ;
}

// Add an rectangle in screen coordinates to the buffer
// automatically merges adjacent filled rectangles of same color
// added consecutively, where possible
void buf_add_screen_rectangle(
        std::vector<DrawBufferObject>& draw_buf,
        const Plotter::View& render_view,
        float x, float y, float w, float h, bool fill, const color::color& c,
        float thickness, size_t rel_func) {
    if (w <= 0. || h <= 0.) return;
    DrawBufferObject rect;
    rect.type = fill ? DrawBufferObject::FILLED_RECT : DrawBufferObject::RECT;
    rect.points = {{(double)x, (double)y}, {(double)(x+w), (double)(y+h)}};
    rect.thickness = thickness,
    rect.rel_func = rel_func;
    auto xdiff = render_view.xmax - render_view.xmin;
    auto ydiff = render_view.ymax - render_view.ymin;
    for (size_t i = 0; i < rect.points.size(); ++i) {
        rect.points[i][0] = rect.points[i][0]*1. /
            render_view.swid * xdiff + render_view.xmin;
        rect.points[i][1] = (render_view.shigh - rect.points[i][1])*1. /
            render_view.shigh * ydiff + render_view.ymin;
    }
    if (fill && draw_buf.size()) {
        auto& last_rect = draw_buf.back();
        if (last_rect.type == DrawBufferObject::FILLED_RECT &&
                std::fabs(last_rect.points[0][1] - rect.points[0][1]) < 1e-6 * ydiff &&
                std::fabs(last_rect.points[1][1] - rect.points[1][1]) < 1e-6 * ydiff &&
                std::fabs(last_rect.points[1][0] - rect.points[0][0]) < 1e-6 * xdiff &&
                last_rect.c == c) {
            // Reduce shape count by merging with rectangle to left
            last_rect.points[1][0] = rect.points[1][0];
            return;
        } else if (last_rect.type == DrawBufferObject::FILLED_RECT &&
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
} // namespace

void Plotter::render(const View& view) {
    // Re-register some special vars, just in case they got deleted
    x_var = env.addr_of("x", false);
    y_var = env.addr_of("y", false);
    t_var = env.addr_of("t", false);

    // * Constants
    // Number of different t values to evaluate for a parametric equation
    static const double PARAMETRIC_STEPS = 1e4;
    // Number of different t values to evaluate for a polar function
    static const double POLAR_STEP_SIZE = M_PI * 1e-3;
    // If parametric function moves more than this amount (square of l2),
    // disconnects the function line
    const double PARAMETRIC_DISCONN_THRESH = 1e-2 *
        std::sqrt(util::sqr_dist(view.xmin, view.ymin, view.xmax, view.ymax));

    bool prev_loss_detail = loss_detail;
    loss_detail = false; // Will set to show 'some detail may be lost'

    // * Clear back buffers
    pt_markers.clear(); pt_markers.reserve(500);
    draw_buf.clear();

    for (size_t funcid = 0; funcid < funcs.size(); ++funcid) {
        auto& func = funcs[funcid];
        auto ftype_nomod = func.type & ~Function::FUNC_TYPE_MOD_ALL;
        if (ftype_nomod == Function::FUNC_TYPE_EXPLICIT ||
                ftype_nomod == Function::FUNC_TYPE_EXPLICIT_Y) {
            bool has_call = false;
            for (size_t i = 0 ; i < funcs[funcid].expr.ast.size(); ++i) {
                if (funcs[funcid].expr.ast[i].opcode == OpCode::call) {
                    has_call = true;
                    break;
                }
            }
            if (has_call && funcs.size() <= max_functions_find_crit_points) {
                // Re-compute derivatives, if explicit and has
                // at least one user-function call
                // This is needed since the user function may have been changed
                if (ftype_nomod == Function::FUNC_TYPE_EXPLICIT ||
                        ftype_nomod == Function::FUNC_TYPE_EXPLICIT_Y) {
                    uint64_t var = ftype_nomod == Function::FUNC_TYPE_EXPLICIT_Y ?  y_var : x_var;
                    func.diff = func.expr.diff(var, env);
                    if (!func.diff.is_null()) {
                        func.ddiff = func.diff.diff(var, env);
                    }
                    else func.ddiff.ast[0] = OpCode::null;
                    func.drecip = func.recip.diff(var, env);
                }
            }
        }
    }

    // * Draw functions
    // BEGIN_PROFILE;
    for (size_t funcid = 0; funcid < funcs.size(); ++funcid) {
        auto& func = funcs[funcid];
        if ((func.type & ~Function::FUNC_TYPE_MOD_ALL) ==
                Function::FUNC_TYPE_GEOM_POLYLINE) {
            // Draw polyline
            // Do this first since it can affect other functions in the same frame
            // e.g (p, q) moved
            if (func.exprs.size() && (func.exprs.size() & 1) == 0) {
                std::vector<std::array<double, 2> > line;
                double mark_radius = MARKER_DISP_RADIUS - 1;
                for (size_t i = 0; i < func.exprs.size(); i += 2) {
                    PointMarker ptm;
                    auto& expr_x = func.exprs[i],
                    & expr_y = func.exprs[i+1];
                    ptm.drag_var_x = ptm.drag_var_y = -1;
                    if (expr_x.is_ref()) {
                        ptm.drag_var_x = expr_x[0].ref;
                        if (std::isnan(env.vars[ptm.drag_var_x])) {
                            env.vars[ptm.drag_var_x] = 1.;
                        }
                    }
                    if (expr_y.is_ref()) {
                        ptm.drag_var_y = expr_y[0].ref;
                        if (std::isnan(env.vars[ptm.drag_var_y])) {
                            env.vars[ptm.drag_var_y] = 1.;
                        }
                    }
                    double x = expr_x(env), y = expr_y(env);
                    if (std::isnan(x) || std::isnan(y) ||
                            std::isinf(x) || std::isinf(y)) {
                        continue;
                    }
                    line.push_back({x, y});
                    ptm.label = PointMarker::LABEL_NONE;
                    ptm.y = y; ptm.x = x;
                    ptm.passive = false;
                    ptm.rel_func = funcid;
                    pt_markers.push_back(std::move(ptm));
                }
                buf_add_polyline(draw_buf, view, line, func.line_color,
                        funcid, 2.f,
                        func.type & Function::FUNC_TYPE_MOD_CLOSED,
                        (func.type & Function::FUNC_TYPE_MOD_NOLINE) == 0,
                        func.type & Function::FUNC_TYPE_MOD_FILLED);
            }
        }
    }
    // Draw all other functions
    for (size_t funcid = 0; funcid < funcs.size(); ++funcid) {
        auto& func = funcs[funcid];
        auto ftype_nomod = func.type & ~Function::FUNC_TYPE_MOD_ALL;
        switch (ftype_nomod) {
            case Function::FUNC_TYPE_IMPLICIT:
                plot_implicit(funcid); break;
            case Function::FUNC_TYPE_PARAMETRIC:
                {
                    if (func.exprs.size() != 2) continue;
                    std::vector<std::array<float, 2> > curr_line;
                    double tmin = (double)func.tmin;
                    double tmax = (double)func.tmax;
                    if (tmin > tmax) std::swap(tmin, tmax);
                    double tstep = std::max((tmax - tmin) / PARAMETRIC_STEPS, 1e-12);
                    double px, py;
                    for (double t = tmin; t <= tmax; t += tstep) {
                        env.vars[t_var] = t;
                        double x = func.exprs[0](env),
                               y = func.exprs[1](env);
                        if (t > tmin && util::sqr_dist(x, y, px, py) > PARAMETRIC_DISCONN_THRESH) {
                            buf_add_screen_polyline(draw_buf, view, curr_line, func.line_color, funcid, 2.f);
                            curr_line.clear();
                        }
                        float sx = _X_TO_SX(x), sy = _Y_TO_SY(y);
                        curr_line.push_back({sx, sy});
                        px = x; py = y;
                    }
                    buf_add_screen_polyline(draw_buf, view, curr_line, func.line_color, funcid, 2.f);
                }
                break;
            case Function::FUNC_TYPE_POLAR:
                {
                    std::vector<std::array<double, 2> > curr_line;
                    std::vector<double> vals;
                    double tmin = (double)func.tmin;
                    double tmax = (double)func.tmax;
                    if (tmin > tmax) std::swap(tmin, tmax);
                    bool has_line = ((func.type & Function::FUNC_TYPE_MOD_INEQ_STRICT) == 0);
                    bool is_ineq = (func.type & Function::FUNC_TYPE_MOD_INEQ) != 0;
                    bool is_ineq_less = (func.type & Function::FUNC_TYPE_MOD_INEQ_LESS) != 0;
                    if (is_ineq) {
                        vals.reserve((tmax - tmin) / POLAR_STEP_SIZE + 1);
                    }
                    if (has_line) {
                        curr_line.reserve((tmax - tmin) / POLAR_STEP_SIZE + 1);
                    }
                    for (double t = tmin; t <= tmax; t += POLAR_STEP_SIZE) {
                        env.vars[t_var] = t;
                        double r = func.expr(env);
                        if (is_ineq) vals.push_back(r);
                        if (has_line) {
                            double x = r * cos(t), y = r * sin(t);
                            curr_line.push_back({x, y});
                        }
                    }
                    if (has_line) {
                        buf_add_polyline(draw_buf, view, curr_line, func.line_color, funcid, 2.f,
                                false, true, false);
                    }
                    if (is_ineq) {
                        const float INTERVAL = 1.5f;
                        double beg_mod = std::fmod(tmin, 2*M_PI);
                        double end_mod = std::fmod(tmax, 2*M_PI);
                        if (beg_mod < 0) beg_mod += 2 * M_PI;
                        if (end_mod < 0) end_mod += 2 * M_PI;
                        double t_start = tmin - beg_mod;
                        double t_end = tmax + (end_mod == 0.0 ? 0.0 : (2*M_PI - end_mod));
                        for (float sy = 0; sy < view.shigh; sy += INTERVAL) {
                            double y = _SY_TO_Y(sy);
                            for (float sx = 0; sx < view.swid; sx += INTERVAL) {
                                double x = _SX_TO_X(sx);
                                double theta_mod = std::atan2(y, x);
                                double r = std::sqrt(x*x + y*y);
                                if (theta_mod < 0) theta_mod += 2 * M_PI;
                                double alpha = 0.;
                                for (double t = t_start; t <= tmax + t_end; t += 2*M_PI) {
                                    double theta = t + theta_mod;
                                    if (theta < tmin || theta > tmax) continue;
                                    size_t near_idx = std::min((size_t)std::max(
                                                std::round((theta - tmin) / POLAR_STEP_SIZE), 0.),
                                                vals.size() - 1);
                                    double thresh_r = vals[near_idx];
                                    if (r < thresh_r == is_ineq_less) {
                                        alpha = 0.25 + 0.75 * alpha; // Simulate blend
                                    }
                                }
                                if (alpha > 0.) {
                                    color::color c = func.line_color;
                                    c.a = alpha;
                                    buf_add_screen_rectangle(draw_buf, view, sx, sy,
                                            INTERVAL, INTERVAL, true, c, 0.0f, funcid);
                                }
                            }
                        }
                    }
                }
                break;
            case Function::FUNC_TYPE_EXPLICIT:
                plot_explicit(funcid, false); break;
            case Function::FUNC_TYPE_EXPLICIT_Y:
                plot_explicit(funcid, true); break;

            // Geometry (other than polyline)
            case Function::FUNC_TYPE_GEOM_RECT:
                {
                    if (func.exprs.size() != 4) break;
                    DrawBufferObject obj;
                    obj.rel_func = funcid;
                    obj.c = func.line_color;
                    obj.thickness = 2.;
                    obj.type = DrawBufferObject::RECT;
                    obj.points.resize(2);
                    auto& a = obj.points[0], &b = obj.points[1];
                    a[0] = func.exprs[0](env);
                    a[1] = func.exprs[1](env);
                    b[0] = func.exprs[2](env);
                    b[1] = func.exprs[3](env);
                    if (b[0] < a[0]) std::swap(a[0], b[0]);
                    if (b[1] < a[1]) std::swap(a[1], b[1]);
                    if (func.type & Function::FUNC_TYPE_MOD_FILLED) {
                        DrawBufferObject obj2 = obj;
                        obj2.type = DrawBufferObject::FILLED_RECT;
                        obj2.c = get_ineq_color(func.line_color);
                        draw_buf.push_back(std::move(obj2));
                    }
                    if ((func.type & Function::FUNC_TYPE_MOD_NOLINE) == 0) {
                        draw_buf.push_back(std::move(obj));
                    }
                }
                break;
            case Function::FUNC_TYPE_GEOM_CIRCLE:
            case Function::FUNC_TYPE_GEOM_ELLIPSE:
                {
                    size_t is_ellipse = (ftype_nomod == Function::FUNC_TYPE_GEOM_ELLIPSE);
                    if (func.exprs.size() != 3 + is_ellipse) break;
                    DrawBufferObject obj;
                    obj.rel_func = funcid;
                    obj.c = func.line_color;
                    obj.thickness = 2.;
                    obj.type = is_ellipse ? DrawBufferObject::ELLIPSE : DrawBufferObject::CIRCLE;
                    obj.points.resize(2);
                    auto& a = obj.points[0], &right = obj.points[1];
                    a[0] = func.exprs[0](env);
                    a[1] = func.exprs[1](env);
                    right[0] = a[0] + func.exprs[2](env);
                    right[1] = a[1];
                    if (is_ellipse) right[1] += func.exprs[3](env);
                    if (func.type & Function::FUNC_TYPE_MOD_FILLED) {
                        DrawBufferObject obj2 = obj;
                        obj2.type = is_ellipse ? DrawBufferObject::FILLED_ELLIPSE :
                            DrawBufferObject::FILLED_CIRCLE;
                        obj2.c = get_ineq_color(func.line_color);
                        draw_buf.push_back(std::move(obj2));
                    }
                    if ((func.type & Function::FUNC_TYPE_MOD_NOLINE) == 0) {
                        draw_buf.push_back(std::move(obj));
                    }
                }
            case Function::FUNC_TYPE_GEOM_TEXT:
                {
                    if (func.exprs.size() != 2) break;
                    DrawBufferObject obj;
                    obj.rel_func = funcid;
                    obj.c = func.line_color;
                    obj.type = DrawBufferObject::TEXT;
                    obj.points = { { func.exprs[0](env), func.exprs[1](env) } };
                    obj.str = func.str;
                    draw_buf.push_back(std::move(obj));
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

void Plotter::render() {
    render(view);
}

void Plotter::populate_grid() {
    const int r = marker_clickable_radius;
    grid.resize(view.swid * view.shigh);
    std::fill(grid.begin(), grid.end(), -1);

    thread_local std::vector<
            std::array<int, 4> // y x id dist
            > que;
    que.resize(view.swid * view.shigh);

    size_t lo = 0, hi = 0;
    auto bfs = [&]{
        // BFS to fill grid
        while(lo < hi) {
            const auto& v = que[lo++];
            int sy = v[0], sx = v[1], id = v[2], d = v[3];
            if (d >= marker_clickable_radius) continue;
            if (sy > 0 && grid[(sy - 1) * view.swid + sx] == -1) {
                que[hi++] = { sy - 1, sx, id, d + 1};
                grid[(sy - 1) * view.swid + sx] = id;
            }
            if (sx > 0 && grid[sy * view.swid + sx - 1] == -1) {
                que[hi++] = { sy, sx - 1, id, d + 1};
                grid[sy * view.swid + sx - 1] = id;
            }
            if (sx < view.swid - 1 && grid[sy * view.swid + sx + 1] == -1) {
                que[hi++] = { sy, sx + 1, id, d + 1};
                grid[sy * view.swid + sx + 1] = id;
            }
            if (sy < view.shigh - 1 && grid[(sy + 1) * view.swid + sx] == -1) {
                que[hi++] = { sy + 1, sx, id, d + 1};
                grid[(sy + 1) * view.swid + sx] = id;
            }
        }
    };

    // Pass 1: BFS from draggable markers
    for (size_t id = 0; id < pt_markers.size(); ++id) {
        auto& ptm = pt_markers[id];
        if (ptm.passive || (ptm.drag_var_x == -1 && ptm.drag_var_y == -1)) continue;
        int sx = (int)(0.5 + _X_TO_SX(ptm.x));
        int sy = (int)(0.5 + _Y_TO_SY(ptm.y));
        if (sx < 0 || sy < 0 || sx >= view.swid || sy >= view.shigh)
            continue;
        if (~grid[sy * view.swid + sx]) continue;
        grid[sy * view.swid + sx] = id;
        que[hi++] = { sy, sx, (int)id, 0 };
    }
    bfs();
    // Note: hi will be 0 here;

    // Pass 2: BFS from non-draggable, active (i.e. text-on-hover) markers
    // only goes through grid cells not already visited
    for (size_t id = 0; id < pt_markers.size(); ++id) {
        auto& ptm = pt_markers[id];
        if (ptm.passive || !(ptm.drag_var_x == -1 && ptm.drag_var_y == -1)) continue;
        int sx = (int)(0.5 + _X_TO_SX(ptm.x));
        int sy = (int)(0.5 + _Y_TO_SY(ptm.y));
        if (sx < 0 || sy < 0 || sx >= view.swid || sy >= view.shigh)
            continue;
        if (~grid[sy * view.swid + sx]) continue;
        grid[sy * view.swid + sx] = id;
        que[hi++] = { sy, sx, (int)id, 0 };
    }
    bfs();
    // Note: hi will be 0 here;

    // Pass 3: Construct and push passive markers (for clicking on functions)
    for (size_t i = 0; i < draw_buf.size(); ++i) {
        const auto & obj = draw_buf[i];
        if (obj.type == DrawBufferObject::POLYLINE) {
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
                    ptm.drag_var_x = ptm.drag_var_y = -1;
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
                    if (~grid[r * view.swid + psx]) continue;
                    grid[r * view.swid + psx] = id;
                    que[hi++] = { r, psx, (int)id, 0 };
                }
            }
        }
    }
    bfs();
}

void Plotter::plot_implicit(size_t funcid) {
    auto& func = funcs[funcid];
    // Implicit function
    if (func.expr.is_null() ||
            (func.expr.ast[0].opcode == OpCode::val &&
             func.type == Function::FUNC_TYPE_IMPLICIT)) {
        // Either 0=0 or c=0 for some c, do not draw
        return;
    }

    // Inequality color
    color::color ineq_color = get_ineq_color(func.line_color);

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
        const double cy = _SY_TO_Y(csy);
        coarse_right_interesting = false;
        for (int csx = -1; csx < view.swid + COARSE_INTERVAL - 1; csx += COARSE_INTERVAL) {
            int cxi = COARSE_INTERVAL;
            if (csx >= view.swid) {
                csx = view.swid - 1;
                cxi = (view.swid-1) % COARSE_INTERVAL;
                if (cxi == 0) break;
            }
            // Update interval based on point count
            const double coarse_x = _SX_TO_X(csx);
            double precise_x = coarse_x, precise_y = cy;
            const int xy_pos = csy * view.swid + csx;

            env.vars[y_var] = cy;
            env.vars[x_var] = coarse_x;
            double z = func.expr(env);
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
                if (interesting_square || interest_from_left ||
                        interest_from_above) {
                    interest_squares.push_back(Square(
                                csy - cyi, csy, csx - cxi, csx, z));
                } else if (sgn_z >= 0 &&
                        func.type !=
                        Function::FUNC_TYPE_IMPLICIT) {
                    // Inequality region
                    buf_add_screen_rectangle(draw_buf, view,
                            csx + (float)(- cxi + 1),
                            csy + (float)(- cyi + 1),
                            (float)cxi, (float)cyi,
                            true, ineq_color, 0.0, funcid);
                }
            }
            coarse_line[csx+1] = z;
        }
    }

#ifndef NIVALIS_EMSCRIPTEN
    std::atomic<int> sqr_id(0);
#else
    int sqr_id(0);
#endif
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
                const double y = _SY_TO_Y(sy);
                for (int sx = xlo; sx <= xhi; sx += fine_interval) {

                    const double x = _SX_TO_X(sx);
                    double z;
                    if (sy == yhi && sx == xhi) {
                        z = z_at_xy_hi;
                    } else {
                        tenv.vars[y_var] = y;
                        tenv.vars[x_var] = x;
                        z = func.expr(tenv);
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
            if ((func.type & Function::FUNC_TYPE_MOD_INEQ_STRICT) == 0) {
                // Draw function line (boundary)
                for (auto& p : tdraws) {
                    buf_add_screen_rectangle(draw_buf, view,
                            p[0] + (float)(- p[2] + 1),
                            p[1] + (float)(- p[2] + 1),
                            (float)(p[2]),
                            (float)(p[2]),
                            true, func.line_color, 0.0, funcid);
                }
            }
            for (auto& p : tdraws_ineq) {
                // Draw inequality region
                buf_add_screen_rectangle(draw_buf, view,
                        p[0] + (float)(- p[2] + 1),
                        p[1] + (float)(- p[2] + 1),
                        (float)p[2],
                        (float)p[2],
                        true,
                        ineq_color, 0.0, funcid);
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
} // void plot_implicit

void Plotter::plot_explicit(size_t funcid, bool reverse_xy) {
    const bool find_all_crit_pts = funcs.size() <= max_functions_find_all_crit_points
                                     || funcid == curr_func;

    double xdiff = view.xmax - view.xmin, ydiff = view.ymax - view.ymin;
    float swid = view.swid, shigh = view.shigh;
    double xmin = view.xmin, xmax = view.xmax;
    double ymin = view.ymin, ymax = view.ymax;
    uint64_t var = x_var;
    if (reverse_xy) {
        std::swap(xdiff, ydiff);
        std::swap(xmin, ymin);
        std::swap(xmax, ymax);
        std::swap(swid, shigh);
        var = y_var;
    }
    auto& func = funcs[funcid];
    color::color ineq_color = get_ineq_color(func.line_color);


    // Constants
    // Newton's method parameters
    const double EPS_STEP  = 1e-7 * xdiff;
    const double EPS_ABS   = 1e-10 * ydiff;
    static const int MAX_ITER  = 100;
    // Shorthand for Newton's method arguments
#define NEWTON_ARGS var, x, env, EPS_STEP, EPS_ABS, MAX_ITER, \
    xmin - NEWTON_SIDE_ALLOW, xmax + NEWTON_SIDE_ALLOW
    // Amount x-coordinate is allowed to exceed the display boundaries
    const double NEWTON_SIDE_ALLOW = xdiff / 20.;


    // x-epsilon for domain bisection
    // (finding cutoff where function becomes undefined)
    const double DOMAIN_BISECTION_EPS = 1e-9 * xdiff;

    // Asymptote check constants:
    // Assume asymptote at discontinuity if slope between
    // x + ASYMPTOTE_CHECK_DELTA1, x + ASYMPTOTE_CHECK_DELTA2
    const double ASYMPTOTE_CHECK_DELTA1 = xdiff * 1e-6;
    const double ASYMPTOTE_CHECK_DELTA2 = xdiff * 1e-5;
    const double ASYMPTOTE_CHECK_EPS    = ydiff * 1e-9;
    // Special eps for boundary of domain
    const double ASYMPTOTE_CHECK_BOUNDARY_EPS = ydiff * 1e-3;

    // For explicit functions
    // We will draw a function segment between every pair of adjacent
    // discontinuities (including 'edge of screen')
    // x-coordinate after a discontinuity to begin drawing function
    const float DISCONTINUITY_EPS = 1e-4f * xdiff;

    // Minimum x-distance between critical points
    const double MIN_DIST_BETWEEN_ROOTS  = 1e-7 * xdiff;

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
    // first: x-position second: enum
    using CritPoint = std::pair<double, int>;
    // Store discontinuities
    std::set<CritPoint> discont, roots_and_extrema;
    size_t idx = 0;
    // Push check helpers
    // Push to st if no other item less than MIN_DIST_BETWEEN_ROOTS from value
    auto push_if_valid = [&](double value, std::set<double>& st) {
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
    auto push_critpt_if_valid = [&](double value,
            CritPoint::second_type type,
            std::set<CritPoint>& st) {
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
    auto draw_line = [&](float psx, float psy, float sx, float sy) {
        if (reverse_xy) {
            std::swap(sx, sy);
            std::swap(psx, psy);
        }
        // Draw the line
        if (curr_line.empty() ||
                (psx > curr_line.back()[0] || psy != curr_line.back()[1])) {
            if (curr_line.size() > 1 &&
                    (func.type & Function::FUNC_TYPE_MOD_INEQ_STRICT) == 0) {
                buf_add_screen_polyline(draw_buf, view, curr_line, func.line_color, funcid, 2.);
            }
            curr_line.resize(2);
            curr_line[0] = {psx, psy};
            curr_line[1] = {sx, sy};
        } else {
            curr_line.push_back({sx, sy});
        }
        // Draw inequality region
        if (func.type & Function::FUNC_TYPE_MOD_INEQ) {
            if (func.type & Function::FUNC_TYPE_MOD_INEQ_LESS) {
                if (reverse_xy) {
                    buf_add_screen_rectangle(draw_buf, view,
                            0, psy, sx, sy - psy, true, ineq_color, 0.0f, funcid);
                } else {
                    buf_add_screen_rectangle(draw_buf, view,
                            psx, sy, sx - psx, view.shigh - sy, true, ineq_color,
                            0.0f, funcid);
                }
            } else {
                if (reverse_xy) {
                    buf_add_screen_rectangle(draw_buf, view,
                            psx, psy, view.swid - psx, sy - psy, true, ineq_color,
                            0.0f, funcid);
                } else {
                    buf_add_screen_rectangle(draw_buf, view,
                            psx, 0, sx - psx, sy, true, ineq_color, 0.0f,
                            funcid);
                }
            }
        }
    };
    // ** Find roots, asymptotes, extrema
    if (!func.diff.is_null() && funcs.size() <= max_functions_find_crit_points) {
        double prev_x, prev_y = 0.;
        for (int sx = 0; sx < swid; sx += 4) {
            const double x = reverse_xy ? _SY_TO_Y(sx) : _SX_TO_X(sx);
            env.vars[var] = x;
            double y = func.expr(env);
            const bool is_y_nan = std::isnan(y);

            if (!is_y_nan) {
                env.vars[var] = x;
                double dy = func.diff(env);
                if (!std::isnan(dy)) {
                    if (find_all_crit_pts) {
                        double root = func.expr.newton(NEWTON_ARGS, &func.diff, y, dy);
                        push_critpt_if_valid(root, ROOT, roots_and_extrema);
                    }
                    double asymp = func.recip.newton(NEWTON_ARGS,
                            &func.drecip, 1. / y, -dy / (y*y));
                    push_critpt_if_valid(asymp, DISCONT_ASYMPT, discont);

                    if (find_all_crit_pts) {
                        env.vars[var] = x;
                        double ddy = func.ddiff(env);
                        if (!std::isnan(ddy)) {
                            double extr = func.diff.newton(NEWTON_ARGS,
                                    &func.ddiff, dy, ddy);
                            push_critpt_if_valid(extr, EXTREMUM, roots_and_extrema);
                        }
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
                        env.vars[var] = mi; double mi_y = func.expr(env);
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
    discont.emplace(xmin, DISCONT_SCREEN);
    discont.emplace(xmax, DISCONT_SCREEN);

    // Previous discontinuity infop
    double prev_discont_x = xmin;
    float prev_discont_sx;
    int prev_discont_type;
    size_t as_idx = 0;

    // ** Main explicit func drawing code: draw function from discont to discont
    for (const auto& discontinuity : discont) {
        float psx = -1.f, psy = -1.f;
        double discont_x = discontinuity.first;
        int discont_type = discontinuity.second;
        float discont_sx = reverse_xy ? _Y_TO_SY(discont_x) : _X_TO_SX(discont_x);

        if (as_idx > 0) {
            bool connector = prev_discont_type != DISCONT_SCREEN;
            double x_begin = prev_discont_x, x_end = discont_x;
            float sx_begin = prev_discont_sx, sx_end = discont_sx;
            if (reverse_xy) {
                std::swap(sx_begin, sx_end);
                std::swap(x_begin, x_end);
            }
            // Draw func between asymptotes
            for (float sxd = sx_begin + DISCONTINUITY_EPS; sxd < sx_end - DISCONTINUITY_EPS;) {
                const double x = reverse_xy ? _SY_TO_Y(sxd) : _SX_TO_X(sxd);
                env.vars[var] = x;
                double y = func.expr(env);

                if (!std::isnan(y)) {
                    if (y > ymax + ydiff) y = ymax + ydiff;
                    else if (y < ymin - ydiff) y = ymin - ydiff;
                    float sy = reverse_xy ?  _X_TO_SX(y) : _Y_TO_SY(y), sx = sxd;
                    if (connector) {
                        // Check if asymptote at previous position;
                        // if so then connect it
                        connector = false;
                        env.vars[var] = x_begin + ASYMPTOTE_CHECK_DELTA1;
                        double yp = func.expr(env);
                        env.vars[var] = x_begin + ASYMPTOTE_CHECK_DELTA2;
                        double yp2 = func.expr(env);
                        double eps = discont_type == DISCONT_ASYMPT ?
                            ASYMPTOTE_CHECK_EPS:
                            ASYMPTOTE_CHECK_BOUNDARY_EPS;
                        if (yp - yp2 > eps && sy > 0.f) {
                            psx = sx_begin;
                            psy = 0.f;
                        } else if (yp - yp2 < -eps && sy < (float)shigh) {
                            psx = sx_begin;
                            psy = static_cast<float>(shigh);
                        }
                    }
                    if (psx >= 0.0) draw_line(psx, psy, sx, sy);
                    psx = sx;
                    psy = sy;
                } else {
                    psx = -1;
                }
                if (discont.size() > 2 && discont.size() < 100) {
                    if ((as_idx > 1 && sxd - prev_discont_sx < 1.) ||
                            (as_idx < discont.size() - 1 && discont_sx - sxd < 1.)) {
                        sxd += 0.1f;
                    } else {
                        sxd += 0.2f;
                    }
                } else {
                    sxd += 0.2f;
                }
            }
            // Connect next asymptote
            if (discont_type != DISCONT_SCREEN) {
                env.vars[var] = x_end - ASYMPTOTE_CHECK_DELTA1;
                double yp = func.expr(env);
                env.vars[var] = x_end - ASYMPTOTE_CHECK_DELTA2;
                double yp2 = func.expr(env);
                float sx = -1, sy;
                double eps = discont_type == DISCONT_ASYMPT ?
                    ASYMPTOTE_CHECK_EPS:
                    ASYMPTOTE_CHECK_BOUNDARY_EPS;
                if (yp - yp2 > eps && psy > 0) {
                    sx = static_cast<float>(sx_end);
                    sy = 0.f;
                }
                if (yp - yp2 < -eps && psy < shigh) {
                    sx = sx_end;
                    sy = static_cast<float>(shigh);
                }
                if (psx >= 0.f && sx >= 0.f) {
                    draw_line(psx, psy, sx, sy);
                }
            }
        }
        ++as_idx;
        prev_discont_x = discont_x;
        prev_discont_type = discont_type;
        prev_discont_sx = discont_sx;
    }

    // Finish last line, if exists
    if (curr_line.size() > 1 && (func.type & Function::FUNC_TYPE_MOD_INEQ_STRICT) == 0) {
        buf_add_screen_polyline(draw_buf, view, curr_line, func.line_color, funcid, 2.);
    }
    if (funcs.size() <= max_functions_find_crit_points) {
        if (find_all_crit_pts) {
            std::vector<CritPoint> to_erase; // Save dubious points to delete from roots_and_extrama
            // Helper to draw roots/extrema/y-int and add a marker for it
            auto draw_extremum = [&](const CritPoint& cpt, double y) {
                env.vars[var] = cpt.first;
                double dy = func.diff(env);
                double ddy = func.ddiff(env);
                double x; int type;
                std::tie(x, type) = cpt;
                auto label =
                    type == Y_INT ? (reverse_xy ? PointMarker::LABEL_X_INT : PointMarker::LABEL_Y_INT) :
                    type == ROOT ? (reverse_xy ? PointMarker::LABEL_Y_INT : PointMarker::LABEL_X_INT) :
                    (type == EXTREMUM && ddy > 1e-100 * ydiff) ? PointMarker::LABEL_LOCAL_MIN :
                    (type == EXTREMUM && ddy < -1e-100 * ydiff) ? PointMarker::LABEL_LOCAL_MAX:
                    type == EXTREMUM ? PointMarker::LABEL_INFLECTION_PT:
                    PointMarker::LABEL_NONE;
                // Do not show
                if (label == PointMarker::LABEL_INFLECTION_PT ||
                        label == PointMarker::LABEL_NONE) {
                    to_erase.push_back(cpt);
                    return;
                }

                PointMarker ptm;
                ptm.label = label;
                ptm.y = y; ptm.x = x;
                if (reverse_xy) std::swap(ptm.x, ptm.y);
                if (type == Y_INT) label = PointMarker::LABEL_X_INT;
                else if (type == ROOT) label = PointMarker::LABEL_Y_INT;
                ptm.passive = false;
                ptm.rel_func = funcid;
                ptm.drag_var_x = ptm.drag_var_y = -1;
                pt_markers.push_back(std::move(ptm));
            };
            for (const CritPoint& cpt : roots_and_extrema) {
                env.vars[var] = cpt.first;
                double y = func.expr(env);
                draw_extremum(cpt, y);
            }
            // Delete the "dubious" points
            for (const CritPoint& x : to_erase) {
                roots_and_extrema.erase(x);
            }
            if (!func.expr.is_null() && !func.diff.is_null()) {
                env.vars[var] = 0;
                double y = func.expr(env);
                if (!std::isnan(y) && !std::isinf(y)) {
                    push_critpt_if_valid(0., Y_INT, roots_and_extrema); // y-int
                    auto cpt = CritPoint(0., Y_INT);
                    if (roots_and_extrema.count(cpt)) {
                        draw_extremum(cpt, y);
                    }
                }
            }
        }

        // Function intersection
        if (!func.diff.is_null() && funcs.size() <= max_functions_find_crit_points) {
            if (find_all_crit_pts) {
                for (size_t funcid2 = 0; funcid2 < (funcs.size() <= max_functions_find_all_crit_points
                                                    ? funcid : funcs.size()); ++funcid2) {
                    auto& func2 = funcs[funcid2];
                    if (func2.diff.is_null()) continue;
                    int ft_nomod = func.type & ~Function::FUNC_TYPE_MOD_ALL;
                    int f2t_nomod = func2.type & ~Function::FUNC_TYPE_MOD_ALL;
                    if (ft_nomod == f2t_nomod) {
                        // Same direction, use Newton on difference.
                        Expr sub_expr = func.expr - func2.expr;
                        Expr diff_sub_expr = func.diff - func2.diff;
                        // diff_sub_expr.optimize();
                        if (diff_sub_expr.is_null()) continue;
                        std::set<double> st;
                        for (int sxd = 0; sxd < swid; sxd += 10) {
                            const double x = reverse_xy ? _SY_TO_Y(sxd) : _SX_TO_X(sxd);
                            double root = sub_expr.newton(NEWTON_ARGS, &diff_sub_expr);
                            push_if_valid(root, st);
                        }
                        for (double x : st) {
                            env.vars[var] = x;
                            double y = func.expr(env);
                            size_t idx = pt_markers.size();
                            PointMarker ptm;
                            ptm.label = PointMarker::LABEL_INTERSECTION;
                            ptm.x = x; ptm.y = y;
                            if (reverse_xy) std::swap(ptm.x, ptm.y);
                            ptm.passive = false;
                            ptm.rel_func = -1;
                            ptm.drag_var_x = ptm.drag_var_y = -1;
                            pt_markers.push_back(std::move(ptm));
                        } // for x : st
                    } else if (ft_nomod + f2t_nomod == 8) {
                        // Inverse direction, use Newton on composition.
                        Expr comp_expr = func2.expr;
                        auto other_var = var == x_var ? y_var  : x_var;
                        comp_expr.sub_var(other_var, func.expr);
                        comp_expr = comp_expr - Expr::AST{Expr::ASTNode::varref(var)};
                        comp_expr.optimize();

                        Expr diff_comp_expr = func2.diff;
                        diff_comp_expr.sub_var(other_var, func.expr);
                        diff_comp_expr = diff_comp_expr * func.diff - Expr::constant(1.);
                        diff_comp_expr.optimize();
                        if (diff_comp_expr.is_null()) continue;
                        std::set<double> st;
                        for (int sxd = 0; sxd < swid; sxd += 2) {
                            const double x = reverse_xy ? _SY_TO_Y(sxd) : _SX_TO_X(sxd);
                            double root = comp_expr.newton(
                                    var, x, env, EPS_STEP * 10.f, EPS_ABS * 10.f, MAX_ITER,
                                    xmin - NEWTON_SIDE_ALLOW, xmax + NEWTON_SIDE_ALLOW,
                                    &diff_comp_expr);
                            push_if_valid(root, st);
                        }
                        for (double x : st) {
                            env.vars[var] = x;
                            double y = func.expr(env);
                            size_t idx = pt_markers.size();
                            PointMarker ptm;
                            ptm.label = PointMarker::LABEL_INTERSECTION;
                            ptm.x = x; ptm.y = y;
                            if (reverse_xy) std::swap(ptm.x, ptm.y);
                            ptm.passive = false;
                            ptm.rel_func = -1;
                            ptm.drag_var_x = ptm.drag_var_y = -1;
                            pt_markers.push_back(std::move(ptm));
                        } // for x : st
                    } // if
                    // o.w. not supported.
                } // for funcid2
            } // if (find_all_intersections || funcid == curr_func)
        } // if (!func.diff.is_null())
    } // if (funcs.size() <= max_functions_find_crit_points)
    // * End of function intersection code
} // void plot_explicit

}  // namespace nivalis
