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

// Buffer management
// Add polyline/polygon to buffer (points in plot coords) to buffer
// automatically splits the line where consecutive points are near colinear
void buf_add_polyline(std::vector<DrawBufferObject>& draw_buf,
                      const Plotter::View& render_view,
                      const std::vector<std::array<double, 2>>& points,
                      const color::color& c, size_t rel_func,
                      float thickness = 1., bool closed = false,
                      bool line = true, bool filled = false) {
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
        obj.type =
            closed ? DrawBufferObject::POLYGON : DrawBufferObject::POLYLINE;
        obj.points.reserve(points.size());
        // draw_buf.push_back(std::move(obj_tmp));
        for (size_t i = 0; i < points.size(); ++i) {
            // if (std::isnan(points[i][0])) break;
            size_t j = obj.points.size();
            obj.points.push_back({points[i][0], points[i][1]});
            if (j >= 2) {
                double ax = (obj.points[j][0] - obj.points[j - 1][0]);
                double ay = (obj.points[j][1] - obj.points[j - 1][1]);
                double bx = (obj.points[j - 1][0] - obj.points[j - 2][0]);
                double by = (obj.points[j - 1][1] - obj.points[j - 2][1]);
                double theta_a =
                    std::fmod(std::atan2(ay, ax) + 2 * M_PI, 2 * M_PI);
                double theta_b =
                    std::fmod(std::atan2(by, bx) + 2 * M_PI, 2 * M_PI);
                double angle_between =
                    std::min(std::fabs(theta_a - theta_b),
                             std::fabs(theta_a + 2 * M_PI - theta_b));
                if (std::fabs(angle_between - M_PI) < 1e-1) {
                    // Near-colinear, currently ImGui's polyline drawing may
                    // will break in this case. We split the obj.points here.
                    draw_buf.push_back(obj);
                    obj.points[0] = obj.points[j - 1];
                    obj.points[1] = obj.points[j];
                    obj.points.resize(2);
                }
            }
        }
        draw_buf.push_back(obj);
    }
}

// Add polyline (points in screen coords) to buffer
void buf_add_screen_polyline(
    std::vector<DrawBufferObject>& draw_buf, const Plotter::View& render_view,
    const std::vector<std::array<float, 2>>& points_screen,
    const color::color& c, size_t rel_func, float thickness = 1.,
    bool closed = false, bool line = true, bool filled = false) {
    std::vector<std::array<double, 2>> points_conv;
    points_conv.resize(points_screen.size());
    for (size_t i = 0; i < points_screen.size(); ++i) {
        points_conv[i][0] = points_screen[i][0] * 1. / render_view.swid *
                                (render_view.xmax - render_view.xmin) +
                            render_view.xmin;
        points_conv[i][1] = (render_view.shigh - points_screen[i][1]) * 1. /
                                render_view.shigh *
                                (render_view.ymax - render_view.ymin) +
                            render_view.ymin;
    }
    buf_add_polyline(draw_buf, render_view, points_conv, c, rel_func, thickness,
                     closed, line, filled);
}

// Add an rectangle in screen coordinates to the buffer
// automatically merges adjacent filled rectangles of same color
// added consecutively, where possible
void buf_add_screen_rectangle(std::vector<DrawBufferObject>& draw_buf,
                              const Plotter::View& render_view, float x,
                              float y, float w, float h, bool fill,
                              const color::color& c, float thickness,
                              size_t rel_func) {
    if (w <= 0. || h <= 0.) return;
    DrawBufferObject rect;
    rect.type = fill ? DrawBufferObject::FILLED_RECT : DrawBufferObject::RECT;
    rect.points = {{(double)x, (double)y}, {(double)(x + w), (double)(y + h)}};
    rect.thickness = thickness, rect.rel_func = rel_func;
    auto xdiff = render_view.xmax - render_view.xmin;
    auto ydiff = render_view.ymax - render_view.ymin;
    for (size_t i = 0; i < rect.points.size(); ++i) {
        rect.points[i][0] = rect.points[i][0] * 1. / render_view.swid * xdiff +
                            render_view.xmin;
        rect.points[i][1] = (render_view.shigh - rect.points[i][1]) * 1. /
                                render_view.shigh * ydiff +
                            render_view.ymin;
    }
    if (fill && draw_buf.size()) {
        auto& last_rect = draw_buf.back();
        if (last_rect.type == DrawBufferObject::FILLED_RECT &&
            std::fabs(last_rect.points[0][1] - rect.points[0][1]) <
                1e-6 * ydiff &&
            std::fabs(last_rect.points[1][1] - rect.points[1][1]) <
                1e-6 * ydiff &&
            std::fabs(last_rect.points[1][0] - rect.points[0][0]) <
                1e-6 * xdiff &&
            last_rect.c == c) {
            // Reduce shape count by merging with rectangle to left
            last_rect.points[1][0] = rect.points[1][0];
            return;
        } else if (last_rect.type == DrawBufferObject::FILLED_RECT &&
                   std::fabs(last_rect.points[0][0] - rect.points[0][0]) <
                       1e-6 * xdiff &&
                   std::fabs(last_rect.points[1][0] - rect.points[1][0]) <
                       1e-6 * xdiff &&
                   std::fabs(last_rect.points[1][1] - rect.points[0][1]) <
                       1e-6 * ydiff &&
                   last_rect.c == c) {
            // Reduce shape count by merging with rectangle above
            last_rect.points[1][1] = rect.points[1][1];
            return;
        }
    }
    rect.c = c;
    draw_buf.push_back(std::move(rect));
}
}  // namespace

void Plotter::render(const View& view) {
    // Re-register some special vars, just in case they got deleted
    x_var = env.addr_of("x", false);
    y_var = env.addr_of("y", false);
    z_var = env.addr_of("z", false);
    t_var = env.addr_of("t", false);

    // * Constants
    // Number of different t values to evaluate for a parametric equation
    static const double PARAMETRIC_STEPS = 1e4;
    // Number of different t values to evaluate for a polar function
    static const double POLAR_STEP_SIZE = M_PI * 1e-3;
    // If parametric function moves more than this amount (square of l2),
    // disconnects the function line
    const double PARAMETRIC_DISCONN_THRESH =
        1e-2 *
        std::sqrt(util::sqr_dist(view.xmin, view.ymin, view.xmax, view.ymax));

    bool prev_loss_detail = loss_detail;
    loss_detail = false;  // Will set to show 'some detail may be lost'

    // * Clear back buffers
    pt_markers.clear();
    pt_markers.reserve(500);
    draw_buf.clear();

    // * Draw functions
    for (size_t funcid = 0; funcid < funcs.size(); ++funcid) {
        auto& func = funcs[funcid];
        if (func.type == Function::FUNC_TYPE_COMPLEX) {
            plot_complex(funcid);
        }
    }

    if (loss_detail) {
        func_error = "Warning: some detail may be lost";
    } else if (prev_loss_detail) {
        func_error.clear();
    }
}

void Plotter::plot_complex(size_t funcid) {
    int grid_reso_v = 100, grid_reso_h = 100;

    auto& func = funcs[funcid];

#ifndef NIVALIS_EMSCRIPTEN
    std::atomic<int> pt_id(0);
#else
    int pt_id(0);
#endif

    double ymin = -20.0, ymax = 20.0;
    double xmin = -20.0, xmax = 20.0;

    double vstep = (ymax - ymin) / grid_reso_v;
    double hstep = (xmax - xmin) / grid_reso_h;

    const int num_points = grid_reso_h * grid_reso_v;
    int pt_size = 2;

    std::vector<std::array<float, 2>> all_draws(num_points);
    double t = env.vars[t_var].real();

    auto worker = [&]() {
        std::vector<std::pair<int, std::array<float, 2>>> tdraws;
        Environment tenv = env;

        while (true) {
            int t_pt_id = pt_id++;
            if (t_pt_id >= num_points) break;

            const int ri = t_pt_id / grid_reso_h;
            const int ci = t_pt_id % grid_reso_h;

            const double y = ri * vstep + ymin;
            const double x = ci * hstep + xmin;
            tenv.vars[x_var] = x;
            tenv.vars[y_var] = y;
            tenv.vars[z_var] = complex(x, y);

            complex r = func.expr(tenv);
            const double ry = r.imag(), rx = r.real();

            const double my = y * (1.0 - t) + ry * t;
            const double mx = x * (1.0 - t) + rx * t;

            float sx = _X_TO_SX(mx);
            float sy = _Y_TO_SY(my);
            tdraws.push_back({t_pt_id, {sx, sy}});
        }

        {
#ifndef NIVALIS_EMSCRIPTEN
            std::lock_guard<std::mutex> lock(mtx);
#endif
            for (auto& p : tdraws) {
                all_draws[p.first] = std::move(p.second);
            }
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

    std::vector<std::array<float, 2>> points_screen;
    for (size_t row = 0; row < grid_reso_h; ++row) {
        for (size_t pt_id = row * grid_reso_h; pt_id < (row + 1) * grid_reso_h;
             ++pt_id) {
            points_screen.push_back(all_draws[pt_id]);
        }
        buf_add_screen_polyline(draw_buf, view, points_screen, func.line_color,
                                funcid);
        points_screen.clear();
    }
    for (size_t col = 0; col < grid_reso_h; ++col) {
        for (size_t pt_id = col; pt_id < all_draws.size();
             pt_id += grid_reso_h) {
            points_screen.push_back(all_draws[pt_id]);
        }
        buf_add_screen_polyline(draw_buf, view, points_screen, func.line_color,
                                funcid);
        points_screen.clear();
    }
}

void Plotter::render() { render(view); }

void Plotter::populate_grid() {
    const int r = marker_clickable_radius;
    grid.resize(view.swid * view.shigh);
    std::fill(grid.begin(), grid.end(), -1);

    thread_local std::vector<std::array<int, 4>  // y x id dist
                             >
        que;
    que.resize(view.swid * view.shigh);

    size_t lo = 0, hi = 0;
    auto bfs = [&] {
        // BFS to fill grid
        while (lo < hi) {
            const auto& v = que[lo++];
            int sy = v[0], sx = v[1], id = v[2], d = v[3];
            if (d >= marker_clickable_radius) continue;
            if (sy > 0 && grid[(sy - 1) * view.swid + sx] == -1) {
                que[hi++] = {sy - 1, sx, id, d + 1};
                grid[(sy - 1) * view.swid + sx] = id;
            }
            if (sx > 0 && grid[sy * view.swid + sx - 1] == -1) {
                que[hi++] = {sy, sx - 1, id, d + 1};
                grid[sy * view.swid + sx - 1] = id;
            }
            if (sx < view.swid - 1 && grid[sy * view.swid + sx + 1] == -1) {
                que[hi++] = {sy, sx + 1, id, d + 1};
                grid[sy * view.swid + sx + 1] = id;
            }
            if (sy < view.shigh - 1 && grid[(sy + 1) * view.swid + sx] == -1) {
                que[hi++] = {sy + 1, sx, id, d + 1};
                grid[(sy + 1) * view.swid + sx] = id;
            }
        }
    };

    // Pass 1: BFS from draggable markers
    for (size_t id = 0; id < pt_markers.size(); ++id) {
        auto& ptm = pt_markers[id];
        if (ptm.passive || (ptm.drag_var_x == -1 && ptm.drag_var_y == -1))
            continue;
        int sx = (int)(0.5 + _X_TO_SX(ptm.x));
        int sy = (int)(0.5 + _Y_TO_SY(ptm.y));
        if (sx < 0 || sy < 0 || sx >= view.swid || sy >= view.shigh) continue;
        if (~grid[sy * view.swid + sx]) continue;
        grid[sy * view.swid + sx] = id;
        que[hi++] = {sy, sx, (int)id, 0};
    }
    bfs();
    // Note: hi will be 0 here;

    // Pass 2: BFS from non-draggable, active (i.e. text-on-hover) markers
    // only goes through grid cells not already visited
    for (size_t id = 0; id < pt_markers.size(); ++id) {
        auto& ptm = pt_markers[id];
        if (ptm.passive || !(ptm.drag_var_x == -1 && ptm.drag_var_y == -1))
            continue;
        int sx = (int)(0.5 + _X_TO_SX(ptm.x));
        int sy = (int)(0.5 + _Y_TO_SY(ptm.y));
        if (sx < 0 || sy < 0 || sx >= view.swid || sy >= view.shigh) continue;
        if (~grid[sy * view.swid + sx]) continue;
        grid[sy * view.swid + sx] = id;
        que[hi++] = {sy, sx, (int)id, 0};
    }
    bfs();
    // Note: hi will be 0 here;

    // Pass 3: Construct and push passive markers (for clicking on
    // functions)
    for (size_t i = 0; i < draw_buf.size(); ++i) {
        const auto& obj = draw_buf[i];
        if (obj.type == DrawBufferObject::POLYLINE) {
            const auto& points = obj.points;
            for (size_t j = 1; j < points.size(); ++j) {
                const std::array<double, 2>& p = points[j - 1];
                const std::array<double, 2>& q = points[j];
                size_t id = pt_markers.size();
                {
                    PointMarker ptm;
                    ptm.x = p[0];
                    ptm.y = p[1];
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
                     r != std::min(std::max(qsy, 0), view.shigh - 1) + dir;
                     r += dir) {
                    if (~grid[r * view.swid + psx]) continue;
                    grid[r * view.swid + psx] = id;
                    que[hi++] = {r, psx, (int)id, 0};
                }
            }
        }
    }
    bfs();
}

}  // namespace nivalis
