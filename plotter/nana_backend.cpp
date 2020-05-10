#include "plotter/plot_gui.hpp"
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

#include "plotter/plotter.hpp"

#include "nana/gui/widgets/form.hpp"
#include "nana/gui/widgets/label.hpp"
#include "nana/gui/widgets/textbox.hpp"
#include "nana/paint/graphics.hpp"
#include "nana/gui/drawing.hpp"
#include "nana/gui.hpp"
#include "nana/gui/widgets/button.hpp"
#include <nana/threads/pool.hpp>

namespace nivalis {
using namespace nana;

namespace {

struct NanaGraphicsAdapter {
    void line(int ax, int ay, int bx, int by, color::color c) {
        graph.line(point(ax, ay), point(bx, by), nana::color(c.r, c.g, c.b));
    }
    void rectangle(int x, int y, int w, int h, bool fill, color::color c) {
        graph.rectangle(nana::rectangle(x,y,w,h),
                fill, nana::color(c.r, c.g, c.b));
    }
    void rectangle(bool fill, color::color c) {
        graph.rectangle(fill, nana::color(c.r, c.g, c.b));
    }
    void set_pixel(int x, int y, color::color c) {
        graph.set_pixel(x, y, nana::color(c.r, c.g, c.b));
    }
    void string(int x, int y, const std::string& s, color::color c) {
        graph.string(point(x, y), s, nana::color(c.r, c.g, c.b));
    }
    NanaGraphicsAdapter(nana::paint::graphics& graph) : graph(graph) {}
    nana::paint::graphics& graph;
};

struct NanaPlotGUI {
    std::chrono::high_resolution_clock::time_point lazy_start;

    NanaPlotGUI(Environment expr_env, const std::string& init_expr)
        : plot(*this, expr_env, init_expr, 1000, 600),
          fm(API::make_center(1000, 600)),
          dw(fm),
          label_func(fm, rectangle{20, 20, 250, 30}),
          label_err(fm, rectangle{20, 115, 250, 30}),
          label_point_marker(fm, rectangle{20, 20, 250, 30}),
          tb(fm, rectangle{20, 40, 250, 40}),
          btn_home(fm, rectangle{20, 80, 130, 30}),
          btn_prev(fm, rectangle{150, 80, 40, 30}),
          btn_next(fm, rectangle{190, 80, 40, 30}),
          btn_del(fm, rectangle{230, 80, 40, 30}),
          env(expr_env) {
        fm.caption("Nivalis Plotter");

        /* Editor UI */
        // Func label
        label_func.caption("Function 0");
        label_func.bgcolor(colors::white);
        // Error label
        label_err.transparent(true);
        // Point marker label
        label_point_marker.bgcolor(colors::white);
        label_point_marker.hide();
        // Textbook for editing functions
        plot.setup_editor();

        tb.events().key_char([&](const arg_keyboard& arg) {
            if (arg.key == keyboard::escape) {
                arg.ignore = true;
                fm.focus();
            }
        });
        tb.events().key_release([&](const nana::arg_keyboard& arg) {
            if (arg.key == 38 || arg.key == 40) {
                // Up/down: go to different funcs
                plot.reparse_expr();
                plot.set_curr_func(plot.curr_func +
                        (arg.key == 38 ? -1 : 1));
            } else if (arg.key == 127) {
                // Delete
                plot.delete_func();
            } else plot.reparse_expr();
        });

        // Home button
        btn_home.bgcolor(colors::white);
        btn_home.edge_effects(false);
        btn_home.caption(L"Reset View");
        btn_home.events().click([this](){ plot.reset_view(); });

        // Prev/next/del-func button
        btn_prev.bgcolor(colors::white);
        btn_prev.edge_effects(false);
        btn_prev.caption(L"<");
        btn_prev.events().click([this](){ plot.set_curr_func(plot.curr_func - 1); fm.focus(); });
        btn_next.bgcolor(colors::white);
        btn_next.edge_effects(false);
        btn_next.caption(L">");
        btn_next.events().click([this](){ plot.set_curr_func(plot.curr_func + 1); fm.focus(); });
        btn_del.bgcolor(colors::white);
        btn_del.edge_effects(false);
        btn_del.caption(L"x");
        btn_del.events().click([this](){ plot.delete_func(); fm.focus(); });

        // * Register form event handlers *
        // Form keyboard handle
        auto keyn_handle = fm.events().key_release([this](const arg_keyboard&arg)
                { plot.handle_key(arg.key, arg.ctrl, arg.alt); });

        // Dragging + marker handling
        auto down_handle = fm.events().mouse_down([this](const arg_mouse&arg)
                { plot.handle_mouse_down(arg.pos.x, arg.pos.y); });
        auto move_handle = fm.events().mouse_move([this](const arg_mouse&arg)
                { plot.handle_mouse_move(arg.pos.x, arg.pos.y); });
        auto up_handle = fm.events().mouse_up([this](const arg_mouse&arg)
                { plot.handle_mouse_up(arg.pos.x, arg.pos.y); });
        auto leave_handle = fm.events().mouse_leave([this](const arg_mouse&arg)
                { plot.handle_mouse_up(arg.pos.x, arg.pos.y); });

        // Scrolling
        auto scroll_handle = fm.events().mouse_wheel([this](const arg_wheel&arg)
                { plot.zoom(arg.upwards, arg.distance, arg.pos.x, arg.pos.y); });

        // Window resizing
        auto resize_handle = fm.events().resized([this](const arg_resized&arg)
                { plot.resize(arg.width, arg.height); });

        // *** Main drawing code ***
        dw.draw([this](paint::graphics& graph) { 
                NanaGraphicsAdapter graph_adapt(graph);
                plot.draw(graph_adapt);
            });

        // * Show the form *
        fm.show();
        exec();
    }

    void focus_editor() { tb.focus(); }
    void focus_background() { fm.focus(); }
    void close() { fm.close(); }

    void update(bool force = false) {
        // Delay the update
        auto finish = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - lazy_start).count();
        if (force || elapsed > 1000) {
            dw.update();
            lazy_start = std::chrono::high_resolution_clock::now();
        }
    }

    // Assumes func_id is curr_func !
    void update_editor(int func_id, std::string contents) { tb.caption(contents); }

    // Assumes func_id is curr_func !
    std::string read_editor(int func_id) { return tb.caption(); }
    void show_error(const std::string& txt) { label_err.caption(txt); }
    void set_func_name(const std::string& txt) { label_func.caption(txt); }

    void show_marker_at(const PointMarker& ptm, int px, int py) {
        label_point_marker.show();
        label_point_marker.move(px, py+20);
        label_point_marker.caption(
            PointMarker::label_repr(ptm.label) +
            std::to_string(ptm.x) + ", " + std::to_string(ptm.y));
    }

    void hide_marker() { label_point_marker.hide(); }

private:
    Plotter<NanaPlotGUI, NanaGraphicsAdapter> plot;

    form fm;
    drawing dw;
    label label_point_marker, label_func, label_err;
    textbox tb;
    button btn_home, btn_prev, btn_next, btn_del;

    Environment env;
    Parser parser;
    nana::threads::pool pool;
};
}  // namespace

PlotGUI::PlotGUI(Environment& env, const std::string& init_expr) {
    NanaPlotGUI nanagui(env, init_expr);
}
}  // namespace nivalis
