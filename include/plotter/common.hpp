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
        FUNC_TYPE_EXPLICIT,             // normal func like x^2
        FUNC_TYPE_IMPLICIT,             // implicit func like abs(x)=abs(y)
        FUNC_TYPE_IMPLICIT_INEQ,        // implicit inequality >= 0
        FUNC_TYPE_IMPLICIT_INEQ_STRICT, // implicit inequality > 0
        FUNC_TYPE_POLYLINE,             // poly-lines (x,y) (x',y') (x'',y'')...
    } type;
    // Stores line points, if polyline-type
    std::vector<Expr> polyline;

    // Binary serialization
    std::ostream& to_bin(std::ostream& os) const;
    std::istream& from_bin(std::istream& is);
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

struct FuncDrawObj {
    std::vector<std::array<double, 2> > points;
    float thickness;
    color::color c;
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
 * Call draw_grid(), recalc(), swap(), draw() in drawing event/loop
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
     * void string(float x, float y, std::string s, const color::color&);                                           draw a string
     * Adapater may use internal caching and/or add optional arguments to above functions; when require_update is set the cache should be
     * cleared. */
    template<class GraphicsAdaptor>
    void draw_grid(GraphicsAdaptor& graph, const View& view) {
        int sx0 = 0, sy0 = 0;
        int cnt_visible_axis = 0;
        // Draw axes
        if (view.ymin <= 0 && view.ymax >= 0) {
            double y0 = view.ymax / (view.ymax - view.ymin);
            sy0 = static_cast<float>(view.shigh * y0);
            graph.line(0.f, sy0, view.swid, sy0, color::DARK_GRAY, 2.);
            ++cnt_visible_axis;
        }
        else if (view.ymin > 0) {
            sy0 = view.shigh - 26;
        }
        if (view.xmin <= 0 && view.xmax >= 0) {
            double x0 = - view.xmin / (view.xmax - view.xmin);
            sx0 = static_cast<float>(view.swid * x0);
            graph.line(sx0, 0.f, sx0, view.shigh, color::DARK_GRAY, 3.);
            ++cnt_visible_axis;
        }
        else if (view.xmax < 0) {
            sx0 = view.swid - 50;
        }

        // Draw lines
        double ystep, xstep, ymstep, xmstep;
        std::tie(ystep, ymstep) = util::round125((view.ymax - view.ymin) / view.shigh * 600 / 10.8);
        std::tie(xstep, xmstep) = util::round125((view.xmax - view.xmin) / view.swid * 1000 / 18.);
        double xli = std::ceil(view.xmin / xstep) * xstep;
        double xr = std::floor(view.xmax / xstep) * xstep;
        double ybi = std::ceil(view.ymin / ystep) * ystep;
        double yt = std::floor(view.ymax / ystep) * ystep;
        double yb = ybi, xl = xli;
        int idx = 0;
        while (xl <= xr) {
            float sxi = static_cast<float>(view.swid * (xl - view.xmin) / (view.xmax - view.xmin));
            graph.line(sxi,0, sxi, view.shigh, color::LIGHT_GRAY);
            xl = xstep * idx + xli;
            ++idx;
        }
        idx = 0;
        while (yb <= yt) {
            float syi = static_cast<float>(view.shigh * (view.ymax - yb) / (view.ymax - view.ymin));
            graph.line(0.f, syi, view.swid, syi, color::LIGHT_GRAY);
            yb = ystep * idx + ybi;
            ++idx;
        }
        // Larger lines + text
        double xmli = std::ceil(view.xmin / xmstep) * xmstep;
        double xmr = std::floor(view.xmax / xmstep) * xmstep;
        double ymbi = std::ceil(view.ymin / ymstep) * ymstep;
        double ymt = std::ceil(view.ymax / ymstep) * ymstep;
        double ymb = ymbi, xml = xmli;
        idx = 0;
        while (xml <= xmr) {
            float sxi = static_cast<float>(view.swid * (xml - view.xmin) / (view.xmax - view.xmin));
            graph.line(sxi, 0.f, sxi, view.shigh, color::GRAY);

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
            float syi = static_cast<float>(view.shigh * (view.ymax - ymb) / (view.ymax - view.ymin));
            graph.line(0, syi, view.swid, syi, color::GRAY);

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
    template<class GraphicsAdaptor>
    void draw_grid(GraphicsAdaptor& graph) {
        if (view.xmin >= view.xmax) view.xmax = view.xmin + 1e-9;
        if (view.ymin >= view.ymax) view.ymax = view.ymin + 1e-9;
        draw_grid(graph, view);
    }

    /** Draw all functions onto the back buffer and set up point markers
     * (for clicking on function/crit points)*/
    void recalc(const View& view);
    void recalc();

    // Swap back buffer with front buffer (note: get lock for this,
    // if recalc on separate process)
    void swap();

    template<class GraphicsAdaptor>
    // Draw front buffer to screen
    void draw(GraphicsAdaptor& graph) {
        if (view.xmin >= view.xmax) view.xmax = view.xmin + 1e-9;
        if (view.ymin >= view.ymax) view.ymax = view.ymin + 1e-9;

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

    // Reset the plotter's view (view.xmin, view.xmax, etc.) to initial view,
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

    // Binary serialization for functions
    std::ostream& funcs_to_bin(std::ostream& os) const;
    std::istream& funcs_from_bin(std::istream& is);

    // Binary serialization for draw_buf, pt_markers, grid
    std::ostream& bufs_to_bin(std::ostream& os) const;
    std::istream& bufs_from_bin(std::istream& is);
private:
    // Helper for detecting if px, py activates a marker (in grid);
    // if so sets marker_* and current function
    // no_passive: if set, ignores passive markers
    void detect_marker_click(int px, int py, bool no_passive = false);
    // Helpers for adding polylines/rectangles to the buffer
    void buf_add_polyline(const View& recalc_view,
            const std::vector<std::array<float, 2> >& points,
            const color::color& c,  float thickness = 1);
    void buf_add_rectangle(const View& recalc_view,
            float x, float y, float w, float h,
            bool fill, const color::color& c);
public:

    // Public plotter state data
    View view;                              // Curr view data
    size_t curr_func = 0;                   // Currently selected function

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

    // The environment object, contains defined functions and variables
    // (owned)
    Environment env;

#ifndef NIVALIS_EMSCRIPTEN
    // Mutex for concurrency
    std::mutex mtx;
#endif
private:
    Parser parser;                            // Parser instance, for parsing func expressions

    std::vector<FuncDrawObj> draw_back_buf;   // Function draw back buffer
                                              // recalc() adds shapes to here
                                              // swap() swaps this to draw_buf
    std::vector<FuncDrawObj> draw_buf;        // Function draw buffer
                                              // draw() draws these shapes to an adaptor
    std::vector<color::color> rect_opt_grid; // Grid of rectangle colors 
                                             // used by recalc,
                                             // for optimizing drawing

    std::vector<PointMarker> pt_markers;    // Point markers
    std::vector<size_t> grid;               // Grid containing marker id
                                            // at every pixel, row major
                                            // (-1 if no marker)
    std::vector<size_t> grid_back;               // Grid copy, filled  by recalc(), swapped to grid by swap()
    std::vector<PointMarker> pt_markers_back;    // Point markers copy, filled  by recalc(),
                                                 // swapped to grid by swap()

    std::queue<color::color> reuse_colors;    // Reusable colors
    size_t last_expr_color = 0;               // Next available color index if no reusable
                                              // one present(by color::from_int)
    uint32_t x_var, y_var;                    // x,y variable addresses

    // For use with mouth dragging
    bool dragdown, draglabel;
    int dragx, dragy;

    size_t next_func_name = 0;                // Next available function name

    bool loss_detail = false;                 // Whether some detail is lost (if set, will show error)
};


}  // namespace nivalis
#endif // ifndef _COMMON_H_54FCC6EA_4F60_4EBB_88F4_C6E918887C77
