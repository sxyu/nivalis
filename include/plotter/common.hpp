#pragma once
#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#endif

#ifndef _COMMON_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
#define _COMMON_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
#include "version.hpp"
#include <cctype>
#include <cstddef>
#include <utility>
#include <iomanip>
#include <cmath>
#include <string>
#include <sstream>
#include <deque>
#include <array>
#include <vector>
#include <set>
#include <algorithm>
#ifndef NIVALIS_EMSCRIPTEN
#include <thread>
#include <mutex>
#include <atomic>
#endif
#include "env.hpp"
#include "expr.hpp"
#include "color.hpp"
#include "util.hpp"

#include <chrono>
namespace nivalis {
namespace util {
// * Plotter-related utils

// Helper for finding grid line sizes fo a given step size
// rounds to increments of 1,2,5
// e.g. 0.1, 0.2, 0.5, 1, 2, 5 (used for grid lines)
// returns (small gridline size, big gridline size)
std::pair<double, double> round125(double step);
}  // namespace util

// Initial screen size
const int SCREEN_WIDTH = 1000, SCREEN_HEIGHT = 600;

const int MARKER_DISP_RADIUS = 3;

// Represents function in plotter
struct Function {
    // Function name (f0 f1 etc)
    std::string name;
    // Function expression
    Expr expr;
    // Derivative, 2nd derivative (only for explicit)
    Expr diff, ddiff;
    // Reciprocal, derivative of reciprocal (only for explicit)
    Expr recip, drecip;
    // Color of function's line; made translucent for inequality region
    color::color line_color;
    // Should be set by GUI implementation: expression string from editor.
    std::string expr_str;
    enum {
        FUNC_TYPE_EXPLICIT,             // normal func like x^2
        FUNC_TYPE_IMPLICIT,             // implicit func like abs(x)=abs(y)
        FUNC_TYPE_IMPLICIT_INEQ,        // implicit inequality >= 0
        FUNC_TYPE_IMPLICIT_INEQ_STRICT, // implicit inequality > 0
        FUNC_TYPE_PARAMETRIC,           // parameteric equation
        FUNC_TYPE_POLAR,                // polar equation
        FUNC_TYPE_POLYLINE,             // poly-lines (x,y) (x',y') (x'',y'')...
    } type;
    // Polyline type: stores line point expressions,
    // Parameteric type: polyline[0] is x, ..[1] is y
    std::vector<Expr> polyline;

    // Bounds on t, for parametric and polar types only
    float tmin = 0.f, tmax = float(2.f * M_PI);

    // Binary serialization
    std::ostream& to_bin(std::ostream& os) const;
    std::istream& from_bin(std::istream& is);
    // Return true if function makes use of parameter t (tmin/tmax),
    // as opposed to just x/y.
    // Currently true for polar/parametric types.
    bool uses_parameter_t() const;
};

// Marks a single point on the plot which can be clicked
struct PointMarker {
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

    double x, y; // position
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
};

// Data for each slider in Slider window
struct SliderData {
    // Should be set by implementation when variable name changes
    std::string var_name;
    // Should be set by implementation when value, bounds change
    float val, lo = -10.0, hi = 10.0;
    // Internal
    uint32_t var_addr;
    std::string var_name_pre;
};

struct FuncDrawObj {
    std::vector<std::array<double, 2> > points;
    float thickness;
    color::color c;
    size_t rel_func;
    enum{
        POLYLINE,
        FILLED_RECT,
        RECT
    } type;
    // Binary serialization
    std::ostream& to_bin(std::ostream& os) const;
    std::istream& from_bin(std::istream& is);
};

/** Nivalis GUI plotter logic, decoupled from GUI implementation
 * Register GUI event handlers to call handle_xxx
 * Register resize handler to call resize
 *  * Single Thread: Call draw_grid()/render()/draw()/populate_grid()
 *                   in drawing loop
 *  * Two-thread:  draw_grid()/ (under lock)draw() in drawing thread
 *                 worker thread runs render() on a second plotter
 *                 with functions copied with export_binary_func_and_env()
 *                 (under lock) on each frame
 * Call reset_view() to reset view, delete_func() delete function,
 * set_curr_func() to set current function, etc.
* */
class Plotter {
public:
    struct View {
        int swid, shigh;                    // Screen size
        double xmax, xmin;                  // Function area bounds: x
        double ymax, ymin;                  // Function area bounds: y
        bool operator==(const View& other) const;
        bool operator!=(const View& other) const;
    };

    // Construct a Plotter.
    // init_expr: initial expression for first function (which is automatically added)
    Plotter();
    Plotter(const Plotter& other) =default;
    Plotter& operator=(const Plotter& other) =default;

    /* Draw axes and grid onto given graphics adaptor
     * * Required Graphics adaptor API
     * void line(float ax, float ay, float bx, float by, const color::color&, float thickness = 1.0);               draw line (ax, ay) -- (bx, by)
     * void polyline(const std::vector<std::array<float, 2> >& points, const color::color&, float thickness = 1.);  draw polyline (not closed)
     * void rectangle(float x, float y, float w, float h, bool fill, const color::color&);                          draw rectangle (filled or non-filled)
     * void circle(float x, float y, float r, bool fill, const color::color&);                                      draw circle (filled or non-filled)
     * void ellipse(float x, float y, float rx, float ry bool fill, const color::color&);                           draw axis-aligned ellipse (filled or non-filled)
     * void string(float x, float y, std::string s, const color::color&);                                           draw a string
     * Adapater may use internal caching and/or add optional arguments to above functions; when require_update is set the cache should be
     * cleared. */
    template<class GraphicsAdaptor>
    void draw_grid(GraphicsAdaptor& graph, const View& view) {
        int sx_min = 0, sy_min = 0;
        int cnt_visible_axis = 0;
        double y0 = view.ymax / (view.ymax - view.ymin);
        float sy0 = static_cast<float>(view.shigh * y0);
        double x0 = - view.xmin / (view.xmax - view.xmin);
        float sx0 = static_cast<float>(view.swid * x0);
        // Draw axes
        if (view.ymin <= 0 && view.ymax >= 0) {
            sy_min = sy0;
            graph.line(0.f, sy_min, view.swid, sy_min, color::DARK_GRAY, 2.);
            ++cnt_visible_axis;
        }
        else if (view.ymin > 0) {
            sy_min = view.shigh - 26;
        }
        if (view.xmin <= 0 && view.xmax >= 0) {
            sx_min = sx0;
            graph.line(sx_min, 0.f, sx_min, view.shigh, color::DARK_GRAY, 3.);
            ++cnt_visible_axis;
        }
        else if (view.xmax < 0) {
            sx_min = view.swid - 50;
        }

        // round
        thread_local auto prec4 = [](double v) -> std::string {
           thread_local std::ostringstream sstm;
            sstm.str("");
            sstm << std::setprecision(4) << v;
            return sstm.str();
        };

        // Draw lines (step = minor line, mstep = major line)
        if (polar_grid) {
            double rstep, rmstep;
            auto xmins = view.xmin*view.xmin,
                 ymins = view.ymin*view.ymin,
                 xmaxs = view.xmax*view.xmax,
                 ymaxs = view.ymax*view.ymax;
            // Determine minimum distance from origin
            // anywhere on screen
            double rmin;
            if (cnt_visible_axis == 2) rmin = 0.;
            else if (cnt_visible_axis == 1) {
                rmin = 0;
                if (view.xmin <= 0. && view.xmax >= 0.) {
                    rmin = sqrt(std::min(ymins, ymaxs));
                } else {
                    rmin = sqrt(std::min(xmins, xmaxs));
                }
            } else {
                rmin = std::sqrt(std::min(
                    std::min(xmins + ymins, xmins + ymaxs),
                    std::min(xmaxs + ymins, xmaxs + ymaxs)));
            }
            // Determine maximum distance from origin
            double rmax = std::sqrt(std::max(
                    std::max(xmins + ymins, xmins + ymaxs),
                    std::max(xmaxs + ymins, xmaxs + ymaxs)));
            // Compute minor/major step sizes
            static const double INITIAL_SCREEN_DIAM =
                sqrt(SCREEN_WIDTH * SCREEN_WIDTH +
                        SCREEN_HEIGHT * SCREEN_HEIGHT);
            double view_diam =
                sqrt((view.xmax - view.xmin) * (view.xmax - view.xmin) +
                     (view.ymax - view.ymin) * (view.ymax - view.ymin));
            double screen_diam =
                sqrt(view.swid * view.swid + view.shigh * view.shigh);
            std::tie(rstep, rmstep) =
                util::round125(view_diam / screen_diam *
                    INITIAL_SCREEN_DIAM / 20.);
            // End compute minor/major step sizes
            // Draw minor (elliptical) gridlines
            double rloi = std::ceil(rmin / rstep) * rstep;
            double rhi = std::floor(rmax / rstep) * rstep;
            double rmloi = std::ceil(rmin / rmstep) * rmstep;
            double rmhi = std::floor(rmax / rmstep) * rmstep;

            int idx = 0;
            double rlo = rloi;
            while (rlo <= rhi) {
                graph.ellipse(sx0, sy0,
                        rlo / (view.xmax - view.xmin) * view.swid,
                        rlo / (view.ymax - view.ymin) * view.shigh,
                        false, color::LIGHT_GRAY);
                rlo = rstep * idx + rloi;
                ++idx;
            }
            idx = 0;
            double x_plot_to_screen = view.swid * 1. / (view.xmax - view.xmin);
            double y_plot_to_screen = view.shigh * 1. / (view.ymax - view.ymin);
            // Major (elliptical) gridlines with text
            double rmlo = rmloi;
            while (rmlo <= rmhi) {
                double sxr = rmlo * x_plot_to_screen;
                double syr = rmlo * y_plot_to_screen;
                graph.ellipse(sx0, sy0,
                        sxr, syr,
                        false, color::GRAY);
                // Draw labels on axes
                if (rmlo > 0.) {
                    auto num_label = prec4(rmlo);
                    graph.string(sx0 + sxr-7,
                            sy0+5, num_label, color::BLACK);
                    graph.string(sx0 - sxr-7,
                            sy0+5, num_label, color::BLACK);
                    graph.string(sx0+5, sy0 + syr -6,
                            num_label, color::BLACK);
                    graph.string(sx0+5, sy0 - syr -6,
                            num_label, color::BLACK);
                }
                rmlo = rmstep * idx + rmloi;
                ++idx;
            }
            // Angle (straight) gridlines
            // 0, pi/12, pi/6 ...
            for (int i = 0; i < 24; ++i) {
                double angle = i * M_PI / 12;
                double uxs = cos(angle) * x_plot_to_screen;
                double uys = -sin(angle) * y_plot_to_screen;
                graph.line(sx0 + uxs * rmin,
                        sy0 + uys * rmin,
                        sx0 + uxs * rmax,
                        sy0 + uys * rmax,
                        color::LIGHT_GRAY);
                // Draw labels
                ++idx;
            }

            // Angle label text
            static const char* angle_labels[] = {
                "0", "pi/6", "pi/3",
                "pi/2", "2pi/3", "5pi/6",
                "pi", "7pi/6", "4pi/3",
                "3pi/2", "5pi/3", "11pi/6"
            };
            double angle_text_disp_r = rmloi + rmstep * 1.8;
            // 0, pi/6, ...
            for (int i = 0; i < sizeof(angle_labels) / sizeof(angle_labels[0]);
                    ++i) {
                double angle = i * M_PI / 6;
                double ux = cos(angle), uy = -sin(angle);
                double uxs = ux * x_plot_to_screen;
                double uys = uy * y_plot_to_screen;
                double posx = sx0 + uxs * angle_text_disp_r - ux * 5. - 12.;
                double posy = sy0 + uys * angle_text_disp_r - uy * 5. - 12.;
                if (i == 0 || i == 6) posy -= 10; // Clear x-axis
                if (i == 3 || i == 9) posx += 20; // Clear y-axis
                if (i == 0) posx += 10; // Adjust spacing
                graph.string(posx, posy,
                        angle_labels[i],
                        color::GRAY);
                ++idx;
            }
        } else {
            double ystep, xstep, ymstep, xmstep;
            std::tie(ystep, ymstep) =
                util::round125((view.ymax - view.ymin) /
                        view.shigh * 1. * SCREEN_HEIGHT / 10.8);
            std::tie(xstep, xmstep) =
                util::round125((view.xmax - view.xmin) /
                        view.swid *1. * SCREEN_WIDTH / 18.);
            {
                // Minor gridlines
                double xli = std::ceil(view.xmin / xstep) * xstep;
                double xr = std::floor(view.xmax / xstep) * xstep;
                double ybi = std::ceil(view.ymin / ystep) * ystep;
                double yt = std::floor(view.ymax / ystep) * ystep;
                double yb = ybi, xl = xli;
                int idx = 0;
                while (xl <= xr) {
                    float sxi = static_cast<float>(view.swid *
                            (xl - view.xmin) / (view.xmax - view.xmin));
                    graph.line(sxi,0, sxi, view.shigh, color::LIGHT_GRAY);
                    xl = xstep * idx + xli;
                    ++idx;
                }
                idx = 0;
                while (yb <= yt) {
                    float syi = static_cast<float>(view.shigh *
                            (view.ymax - yb) / (view.ymax - view.ymin));
                    graph.line(0.f, syi, view.swid, syi, color::LIGHT_GRAY);
                    yb = ystep * idx + ybi;
                    ++idx;
                }
            }
            {
                // Major gridlines (with label text)
                double xmli = std::ceil(view.xmin / xmstep) * xmstep;
                double xmr = std::floor(view.xmax / xmstep) * xmstep;
                double ymbi = std::ceil(view.ymin / ymstep) * ymstep;
                double ymt = std::ceil(view.ymax / ymstep) * ymstep;
                double ymb = ymbi, xml = xmli;
                int idx = 0;
                // On X axis
                while (xml <= xmr) {
                    float sxi = static_cast<float>(view.swid *
                            (xml - view.xmin) / (view.xmax - view.xmin));
                    graph.line(sxi, 0.f, sxi, view.shigh, color::GRAY);

                    // Draw text
                    if (xml != 0) {
                        graph.string(sxi-7, sy_min+5, prec4(xml), color::BLACK);
                    }
                    ++idx;
                    xml = xmstep * idx + xmli;
                }
                idx = 0;
                // On Y axis
                while (ymb <= ymt) {
                    float syi = static_cast<float>(view.shigh *
                            (view.ymax - ymb) / (view.ymax - view.ymin));
                    if (!polar_grid) {
                        graph.line(0, syi, view.swid, syi, color::GRAY);
                    }

                    // Draw text
                    if (ymb != 0) {
                        graph.string(sx_min+5, syi-6, prec4(ymb), color::BLACK);
                    }
                    ++idx;
                    ymb = ymstep * idx + ymbi;
                }
            }
        }

        // Draw 0
        if (cnt_visible_axis == 2) {
            graph.string(sx_min - 12, sy_min + 5, "0", color::BLACK);
        }
    }
    template<class GraphicsAdaptor>
    void draw_grid(GraphicsAdaptor& graph) {
        if (view.xmin >= view.xmax) view.xmax = view.xmin + 1e-9;
        if (view.ymin >= view.ymax) view.ymax = view.ymin + 1e-9;
        draw_grid(graph, view);
    }

    /** Draw all functions onto the buffer and set up point markers
     * (for clicking on function/crit points)*/
    void render(const View& view);
    void render();

    template<class GraphicsAdaptor>
    // Draw buffer populated by render to screen
    void draw(GraphicsAdaptor& graph, const View& view) {
        if (view.xmin >= view.xmax) {
            draw(graph, View{view.swid, view.shigh, view.xmin + 1e-9, view.xmin, view.ymax, view.ymin});
            return;
        }
        if (view.ymin >= view.ymax) {
            draw(graph, View{view.swid, view.shigh, view.xmax, view.xmin, view.ymin + 1e-9, view.ymin});
            return;
        }

        std::vector<std::array<float, 2> > points;
        for (auto& obj : draw_buf) {
            points.resize(obj.points.size());
            // Coordinate conversion: re-position all points in obj.points
            // onto current view in  output to points
            // (in case user moved view/zoomed the points should be moved)
            for (size_t i = 0; i < points.size(); ++i) {
                auto& pt = obj.points[i]; auto & npt = points[i];
                npt[0] = static_cast<float>((pt[0] - view.xmin) * view.swid / (view.xmax - view.xmin));
                npt[1] = static_cast<float>((view.ymax - pt[1]) * view.shigh / (view.ymax - view.ymin));
            }
            // Draw the object
            if (obj.type == FuncDrawObj::POLYLINE) {
                // Polyline
                graph.polyline(points, obj.c, obj.thickness);
            } else {
                // Rectangle
                graph.rectangle(points[0][0], points[0][1],
                                  points[1][0] - points[0][0], points[1][1] - points[0][1],
                                  obj.type == FuncDrawObj::FILLED_RECT, obj.c);
            }
        }
        for (auto& ptm : pt_markers) {
            if (ptm.passive) continue;
            int sy = static_cast<int>((view.ymax - ptm.y) / (view.ymax - view.ymin) * view.shigh);
            int sx = static_cast<int>((ptm.x - view.xmin) / (view.xmax - view.xmin) * view.swid);
            graph.rectangle(sx-MARKER_DISP_RADIUS,
                     sy-MARKER_DISP_RADIUS, 2*MARKER_DISP_RADIUS+1,
                     2*MARKER_DISP_RADIUS+1, true, color::LIGHT_GRAY);
            graph.rectangle(sx-MARKER_DISP_RADIUS,
                    sy-MARKER_DISP_RADIUS,
                    2*MARKER_DISP_RADIUS+1, 2*MARKER_DISP_RADIUS+1,
                    false,
                    (~ptm.rel_func ? funcs[ptm.rel_func].line_color : color::GRAY));
        }
    }

    template<class GraphicsAdaptor>
    // Draw front buffer to screen
    void draw(GraphicsAdaptor& graph) {
        draw(graph, view);
    }

    // Populate the marker grid (grid)
    // and add passive markers for all polylines
    void populate_grid();

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

    // Call this when slider's variable name (var_name) changes
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

    // Reset the plotter's view (view.xmin, view.xmax, etc.) to initial view,
    // accounting for current window size
    void reset_view();

    // * Keyboard/mouse handlers
    // A basic key handler: key code, ctrl pressed?, alt pressed?
    void handle_key(int key, bool ctrl, bool shift, bool alt);

    // Mouse down handler (left key)
    void handle_mouse_down(int px, int py);

    // Mouse move handler (left key)
    void handle_mouse_move(int px, int py);

    // Mouse up handler (left key)
    void handle_mouse_up(int px, int py);

    // Mouse wheel handler:
    // upwards = whether scrolling up,
    // distance = magnitude of amount scrolled
    void handle_mouse_wheel(bool upwards, int distance, int px, int py);

    // JSON serialization
    std::ostream& export_json(std::ostream& os, bool pretty = false) const;
    std::istream& import_json(std::istream& is, std::string* error_msg = nullptr);

    // Binary serialization, only functions, view, and env (used to sync data to worker before render)
    std::ostream& export_binary_func_and_env(std::ostream& os) const;
    std::istream& import_binary_func_and_env(std::istream& is);

    // Binary serialization, only draw_buf, pt_markers (used to sync data from worker after render)
    std::ostream& export_binary_render_result(std::ostream& os) const;
    std::istream& import_binary_render_result(std::istream& is);
private:
    // Helper for detecting if px, py activates a marker (in grid);
    // if so sets marker_* and current function
    // no_passive: if set, ignores passive markers
    void detect_marker_click(int px, int py, bool no_passive = false);
    // Helpers for adding polylines/rectangles to the buffer
    void buf_add_polyline(const View& recalc_view,
            const std::vector<std::array<float, 2> >& points,
            const color::color& c, size_t rel_func,  float thickness = 1);
    void buf_add_rectangle(const View& recalc_view,
            float x, float y, float w, float h,
            bool fill, const color::color& c);
public:
    // Public plotter state data
    View view;                              // Curr view data
    size_t curr_func = 0;                   // Currently selected function
    bool polar_grid = false;                // If true, draws polar grid

    std::vector<Function> funcs;            // Functions
    std::string func_error;                 // Function parsing error str

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

    // Radius around a point for which a marker is clickable
    // should be increased for mobile.
    int marker_clickable_radius = 8;

    // The environment object, contains defined functions and variables
    // (owned)
    Environment env;

#ifndef NIVALIS_EMSCRIPTEN
    // Mutex for concurrency
    std::mutex mtx;
#endif

    std::vector<FuncDrawObj> draw_buf;       // Function draw buffer
                                             // render() populates it
                                             // draw() draws these shapes to
                                             // an adaptor

    std::vector<PointMarker> pt_markers;    // Point markers
                                            // render() populates non-passive
                                            // markers, populate_grid()
                                            // adds passive markers
                                            // Used on mouse events

    std::vector<size_t> grid;               // Grid containing marker index
                                            // at each pixel (in pt_markers),
                                            // row major (-1 if no marker)
                                            // Populated by populate_grid()
                                            // used on mouse events

    bool loss_detail = false;                 // Whether some detail is lost (if set, will show error)
private:
    std::deque<color::color> reuse_colors;    // Reusable colors
    size_t last_expr_color = 0;               // Next available color index if no reusable
                                              // one present(by color::from_int)
    uint32_t x_var, y_var, t_var, r_var;      // x,y,t,r variable addresses

    // For use with mouth dragging
    bool dragdown, draglabel;
    int dragx, dragy;

    size_t next_func_name = 0;                // Next available function name
};


}  // namespace nivalis
#endif // ifndef _COMMON_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
