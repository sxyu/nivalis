#include "parser.hpp"

#include "version.hpp"
#include "util.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cctype>
#include <utility>

#include "plotter/plotter.hpp"
#include "plotter/imgui_adaptor.hpp"
#include "imstb_textedit.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "imgui_stdlib.h"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>

#include "shell.hpp"
#include "util.hpp"
EM_JS(double, get_ver, (), { return nivalis_ver_num; });
EM_JS(int, canvas_get_width, (), { return Module.canvas.width; });
EM_JS(int, canvas_get_height, (), { return Module.canvas.height; });
EM_JS(void, notify_js_focus_editor, (int x), { cppNotifyFocusEditor(x); });
EM_JS(void, notify_js_marker, (int x, int y), { cppNotifyMarker(x, y); });
EM_JS(void, notify_js_func_error_changed, (), { cppNotifyFuncErrorChanged(); });
EM_JS(void, notify_js_slider_error_changed, (), { cppNotifySliderErrorChanged(); });
worker_handle draw_worker_handle;
bool ret_from_worker;


namespace nivalis {
namespace {
Plotter plot; // Main plotter
Environment& env = plot.env; // Main environment
GLFWwindow* window; // GLFW window
std::ostringstream shell_strm; // Shell stream (replaces cout);
Shell shell(env, shell_strm); // Hidden shell

std::stringstream state_encoding_strm;
std::string state_encoding;

worker_handle worker; // WebWorker

bool redraw_canvas(bool);
// WebWorker callback
void webworker_cback(char* data, int size, void* arg) {
    state_encoding_strm.str("");
    state_encoding_strm.write(data, size);
    plot.import_binary_render_result(state_encoding_strm);
    redraw_canvas(true);                // Redraw
    plot.populate_grid();               // Populate grid of point
                                        // markers for mouse events
    notify_js_func_error_changed();
}

// Emscripten access methods, ret true if success
bool redraw_canvas(bool worker_req_update) {
    bool success = true;
    static int missed_messages = 0;
    // * State
    // Main graphics adaptor for plot.draw
    static ImGuiDrawListGraphicsAdaptor adaptor;

    // Resize window
    int ems_js_canvas_width = canvas_get_width();
    int ems_js_canvas_height = canvas_get_height();
    glfwSetWindowSize(window, ems_js_canvas_width, ems_js_canvas_height);

    glfwPollEvents();

    // Clear plot
    glClear(GL_COLOR_BUFFER_BIT);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float ratio = width / (float) height;

    // feed inputs to dear imgui, start new frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Handle resize
    int wwidth, wheight, pwwidth = -1, pwheight;
    glfwGetWindowSize(window, &wwidth, &wheight);
    if (wwidth != plot.view.swid || wheight != plot.view.shigh) {
        pwwidth = plot.view.swid; pwheight = plot.view.shigh;
        plot.resize(wwidth, wheight);
    }

    static ImDrawList draw_list_pre(ImGui::GetDrawListSharedData());
    static Plotter::View plot_view_pre = plot.view;

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    adaptor.draw_list = draw_list;
    if (plot.require_update) {
        // Redraw
        plot.require_update = false;
        // Redraw the grid and functions
        plot.draw_grid(adaptor, plot_view_pre);  // Draw axes and grid
        plot.draw(adaptor, plot_view_pre);       // Draw functions

        state_encoding_strm.str("");
        plot.export_binary_func_and_env(state_encoding_strm);
        state_encoding = state_encoding_strm.str();
        if (emscripten_get_worker_queue_size(worker) > 1) {
            success = false;
            ++ missed_messages;
        }
        if (!(worker_req_update && plot.view == plot_view_pre &&
              missed_messages == 0) && success) {
            if (missed_messages && worker_req_update)
                --missed_messages;
            emscripten_call_worker(worker, "webworker_sync",
                    &state_encoding[0],
                    state_encoding.size(), webworker_cback, nullptr);
        }

        // Note view is delayed by one frame, so that if worker finishes updating
        // in a single frame, we don't get border artifacts due to user movement
        plot_view_pre = plot.view;

        // Cache the draw list
        draw_list_pre.CmdBuffer = draw_list->CmdBuffer;
        draw_list_pre.IdxBuffer = draw_list->IdxBuffer;
        draw_list_pre.VtxBuffer = draw_list->VtxBuffer;
        draw_list_pre.Flags = draw_list->Flags;
    } else {
        // Load cache
        // No update, load draw list from cache
        *draw_list = draw_list_pre;
    }

    auto& io = ImGui::GetIO();
    if (io.MousePosPrev[0] != io.MousePos[0] ||
            io.MousePosPrev[1] != io.MousePos[1]) {
        plot.handle_mouse_move(io.MousePos[0], io.MousePos[1]);
    }

    // Set to set current function to 'change_curr_func' at next redraw
    static int change_curr_func = -1;
    // Change current function
    if (~change_curr_func) {
        // Seek to previous/next function after up/down arrow on textbox
        plot.set_curr_func(change_curr_func);
        change_curr_func = -1;
    }

    // Render dear imgui into screen
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
    return success;
}  // void redraw_canvas()
void redraw_canvas_not_worker() {
    // Expose 0-argument version to JS
    redraw_canvas(false);
}

void on_key(int key, bool ctrl, bool shift, bool alt) {
    plot.handle_key(key, ctrl, shift, alt);
    if (key == 'E') {
        notify_js_focus_editor(plot.curr_func);
    }
}

void on_mousedown(int x, int y) {
    plot.handle_mouse_down(x, y);
    notify_js_marker(plot.marker_posx, plot.marker_posy);
}
void on_mousemove(int x, int y) {
    bool was_empty = plot.marker_text.empty();
    plot.handle_mouse_move(x, y);
    if (!was_empty || !plot.marker_text.empty()) {
        notify_js_marker(plot.marker_posx, plot.marker_posy);
    }
}
void on_mouseup(int x, int y) {
    plot.handle_mouse_up(x, y);
    notify_js_marker(plot.marker_posx, plot.marker_posy);
}
void on_mousewheel(bool upwards, int distance, int x, int y) {
    plot.handle_mouse_wheel(upwards, distance, x, y); }

int emscripten_keypress(int k, int delv) {
    if (delv == 0) {
        ImGui::GetIO().AddInputCharacter(k);
    } else if (delv == 1) {
        ImGui::GetIO().KeysDown[
            ImGui::GetIO().KeyMap[ImGuiKey_Backspace]] = k;

    } else if (delv == 2) {
        ImGui::GetIO().KeysDown[
            ImGui::GetIO().KeyMap[ImGuiKey_Delete]] = k;
    }
    return 0;
}
std::string export_json() {
    std::ostringstream ss;
    plot.export_json(ss);
    return ss.str();
}
void set_marker_clickable_radius(int radius) {
    plot.marker_clickable_radius = radius;
}
void set_passive_marker_click_drag_view(bool val) {
    plot.passive_marker_click_behavior =
        val ? Plotter::PASSIVE_MARKER_CLICK_DRAG_VIEW :
              Plotter::PASSIVE_MARKER_CLICK_DRAG_TRACE;
}

// Returns error
std::string import_json(const std::string& data) {
    std::stringstream ss;
    ss.write(data.c_str(), data.size());
    std::string err;
    plot.import_json(ss, &err);
    return err;
}

// Function management
void set_func_expr(int idx, const std::string & s) {
    plot.funcs[idx].expr_str = s;
    plot.reparse_expr(idx);
    notify_js_func_error_changed();
}
std::string get_func_expr(int idx) {
    return plot.funcs[idx].expr_str;
}
std::string get_func_name(int idx) {
    return plot.funcs[idx].name;
}
// Function t bounds
bool get_func_uses_t(int idx) {
    return plot.funcs[idx].uses_parameter_t();
}
double get_func_tmin(int idx) { return plot.funcs[idx].tmin; }
double get_func_tmax(int idx) { return plot.funcs[idx].tmax; }
void set_func_tmin(int idx, double val) {
    if (std::isnan(val) || val >= plot.funcs[idx].tmax ||
            std::isinf(val)) return; // refuse
    plot.funcs[idx].tmin = val;
    plot.require_update = true;
}
void set_func_tmax(int idx, double val) {
    if (std::isnan(val) || val <= plot.funcs[idx].tmin ||
            std::isinf(val)) return; // refuse
    plot.funcs[idx].tmax = val;
    plot.require_update = true;
}
// Function color
void set_func_color(int idx, const std::string& color) {
    plot.funcs[idx].line_color = color::from_hex(color);
    plot.require_update = true;
}
std::string get_func_color(int idx) {
    return plot.funcs[idx].line_color.to_hex();
}
int get_curr_function() { return plot.curr_func; }
void set_curr_function(int idx) {
    if (idx != plot.curr_func) {
        plot.set_curr_func(idx);
    }
}
bool add_function() {
    size_t num_fn = plot.funcs.size();
    plot.add_func();
    notify_js_func_error_changed();
    return plot.funcs.size() > num_fn;
}
void delete_func(int idx) {
    plot.delete_func((size_t)idx);
    notify_js_func_error_changed();
}
int num_funcs() { return (int)plot.funcs.size(); }


// View
void set_view(double xmin, double xmax,
              double ymin, double ymax) {
    plot.view.xmin = xmin;
    plot.view.ymin = ymin;
    plot.view.xmax = xmax;
    plot.view.ymax = ymax;
    plot.require_update = true;
}
double get_xmin() { return plot.view.xmin; }
double get_ymin() { return plot.view.ymin; }
double get_xmax() { return plot.view.xmax; }
double get_ymax() { return plot.view.ymax; }
void reset_view() {
    plot.reset_view();
}
bool get_is_polar_grid() { return plot.polar_grid; }
void set_is_polar_grid(bool val) {
    plot.polar_grid = val;
    plot.require_update = true;
}

// Sliders
void add_slider() {
    plot.add_slider();
    notify_js_slider_error_changed();
    notify_js_func_error_changed();
}
std::string get_slider_var(int idx) { return plot.sliders[idx].var_name; }
double get_slider_val(int idx) { return plot.sliders[idx].val; }
double get_slider_lo(int idx) { return plot.sliders[idx].lo; }
double get_slider_hi(int idx) { return plot.sliders[idx].hi; }
void set_slider_var(int idx, const std::string& varname) {
    plot.sliders[idx].var_name = varname;
    plot.update_slider_var(idx);
    notify_js_func_error_changed();
    notify_js_slider_error_changed();
}
void set_slider_val(int idx, double val) {
    plot.sliders[idx].val = (float)val;
    plot.copy_slider_value_to_env(idx);
}
void set_slider_lo_hi(int idx, double lo, double hi) {
    auto& sl = plot.sliders[idx];
    sl.lo = (float)lo; sl.hi = (float)hi;
}
void delete_slider(int idx) {
    plot.delete_slider((size_t)idx);
    notify_js_slider_error_changed();
    notify_js_func_error_changed();
}
int num_sliders() { return (int) plot.sliders.size(); }

// Markers
std::string get_marker_text() { return plot.marker_text; }

// Error
std::string get_func_error() { return plot.func_error; }
std::string get_slider_error() { return plot.slider_error; }

// Shell
// Returns true iff no error
bool shell_exec(const std::string& line) {
    shell_strm.str("");
    return shell.eval_line(line);
}
std::string get_shell_output() { return shell_strm.str(); }

EMSCRIPTEN_BINDINGS(nivplot) {
    using namespace emscripten;
    function("on_keypress", &emscripten_keypress);
    function("export_json", &export_json);
    function("import_json", &import_json);
    function("redraw", &redraw_canvas_not_worker);

    function("set_marker_clickable_radius", &set_marker_clickable_radius);
    function("set_passive_marker_click_drag_view", &set_passive_marker_click_drag_view);

    // Function Editor API
    function("add_func", &add_function);
    function("delete_func", &delete_func);
    function("get_curr_func", &get_curr_function);
    function("get_func_name", &get_func_name);
    function("get_func_color", &get_func_color); // Returns hex
    function("get_func_expr", &get_func_expr);
    function("set_curr_func", &set_curr_function);
    function("set_func_color", &set_func_color); // Takes hex
    function("set_func_expr", &set_func_expr);
    function("num_funcs", &num_funcs);

    // Function Editor: Parametric/polar bound API
    function("get_func_uses_t", &get_func_uses_t);
    function("get_func_tmin", &get_func_tmin);
    function("get_func_tmax", &get_func_tmax);
    function("set_func_tmin", &set_func_tmin);
    function("set_func_tmax", &set_func_tmax);

    // View API
    function("set_view", &set_view);
    function("reset_view", &reset_view);
    function("get_xmin", &get_xmin);
    function("get_ymin", &get_ymin);
    function("get_xmax", &get_xmax);
    function("get_ymax", &get_ymax);
    function("get_is_polar_grid", &get_is_polar_grid);
    function("set_is_polar_grid", &set_is_polar_grid);

    // Sliders API
    function("add_slider", &add_slider);
    function("get_slider_var", &get_slider_var);
    function("get_slider_val", &get_slider_val);
    function("get_slider_lo", &get_slider_lo);
    function("get_slider_hi", &get_slider_hi);
    function("set_slider_var", &set_slider_var);
    function("set_slider_val", &set_slider_val);
    function("set_slider_lo_hi", &set_slider_lo_hi);
    function("delete_slider", &delete_slider);
    function("num_sliders", &num_sliders);

    // Marker
    function("get_marker_text", &get_marker_text);

    // Error API
    function("get_func_error", &get_func_error);
    function("get_slider_error", &get_slider_error);

    // Shell API
    function("shell_exec", &shell_exec);
    function("get_shell_output", &get_shell_output);

    // C++ event handlers
    function("on_key", &on_key);
    function("on_mousedown", &on_mousedown);
    function("on_mousemove", &on_mousemove);
    function("on_mouseup", &on_mouseup);
    function("on_mousewheel", &on_mousewheel);

}

// Helper for Initializing OpenGL
bool init_gl() {
    /* Initialize the library */
    if (!glfwInit()) return false;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(plot.view.swid, plot.view.shigh,
            "Nivalis Plotter", NULL, NULL);
    // Init glfw
    if (!window)
    {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
    glClearColor(1., 1., 1., 1.); // Clear white
    return true;
}
}  // namespace
}  // namespace nivalis

    // Main method
int main(int argc, char ** argv) {
    using namespace nivalis;
    if (!init_gl()) return 1; // GL initialization failed

    // Setup Dear ImGUI context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    char* glsl_version = NULL;
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Set initial window size according to HTML canvas size
    int ems_js_canvas_width = canvas_get_width();
    int ems_js_canvas_height = canvas_get_height();
    glfwSetWindowSize(window, ems_js_canvas_width, ems_js_canvas_height);

    uint64_t ver_str = (uint64_t)get_ver();
    worker = emscripten_create_worker(("worker.js?" + std::to_string(ver_str)).c_str());
}
