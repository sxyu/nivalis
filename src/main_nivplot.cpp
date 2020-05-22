#include "parser.hpp"

#include "version.hpp"
#include "util.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cctype>
#include <utility>

#include "plotter/common.hpp"
#include "imgui.h"
#include "imstb_textedit.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "imgui_stdlib.h"

#ifdef NIVALIS_EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#define GLFW_INCLUDE_ES3

#else // NIVALIS_EMSCRIPTEN
#include <thread>
#include <GL/glew.h>
#include "plotter/draw_worker.hpp"
#endif // NIVALIS_EMSCRIPTEN

#include <GLFW/glfw3.h>

#include "shell.hpp"
#include "util.hpp"
#ifdef NIVALIS_EMSCRIPTEN
EM_JS(int, canvas_get_width, (), {
        return Module.canvas.width;
        });

EM_JS(int, canvas_get_height, (), {
        return Module.canvas.height;
        });

EM_JS(void, resizeCanvas, (), {
        js_resizeCanvas();
        });
worker_handle draw_worker_handle;
bool ret_from_worker;
#endif // NIVALIS_EMSCRIPTEN


namespace {
using namespace nivalis;
// Graphics adaptor for Plotter, with caching
struct ImGuiDrawListGraphicsAdaptor {
    void line(float ax, float ay, float bx, float by,
            const color::color& c, float thickness = 1.) {
        draw_list->AddLine(ImVec2(ax, ay), ImVec2(bx, by),
                ImColor(c.r, c.g, c.b, c.a), thickness);
    }
    void polyline(const std::vector<std::array<float, 2> >& points,
            const color::color& c, float thickness = 1.) {
        std::vector<ImVec2> line(points.size());
        for (size_t i = 0; i < line.size(); ++i) {
            line[i].x = (float) points[i][0];
            line[i].y = (float) points[i][1];
        }
        draw_list->AddPolyline(&line[0], (int)line.size(), ImColor(c.r, c.g, c.b, c.a), false, thickness);
    }
    void rectangle(float x, float y, float w, float h, bool fill, const color::color& c) {
        if (fill) {
            draw_list->AddRectFilled(ImVec2(x,y), ImVec2(x+w, y+h),
                    ImColor(c.r, c.g, c.b, c.a));
        } else {
            draw_list->AddRect(ImVec2(x,y), ImVec2(x+w, y+h),
                    ImColor(c.r, c.g, c.b, c.a));
        }
    }
    void string(float x, float y, const std::string& s, const color::color& c) {
        // String using ImGui API
        draw_list->AddText(ImVec2(x, y),
                ImColor(c.r, c.g, c.b, c.a), s.c_str());
    }

    ImDrawList* draw_list = nullptr;
};

// * Constants
// FPS restriction when not moving (to reduce CPU usage)
const int RESTING_FPS = 12;
// Frames to stay active after update
const int ACTIVE_FRAMES = 60;

Plotter plot; // Main plotter
Environment& env = plot.env; // Main environment
GLFWwindow* window; // GLFW window

// MAIN LOOP: Run single step of main loop
void main_loop_step() {
    // Add scaled default font (Dear ImGui)
    static auto AddDefaultFont = [](float pixel_size) -> ImFont* {
        ImGuiIO &io = ImGui::GetIO();
        ImFontConfig config;
        config.SizePixels = pixel_size;
        config.OversampleH = config.OversampleV = 1;
        config.PixelSnapH = true;
        ImFont *font = io.Fonts->AddFontDefault(&config);
        return font;
    };
    static ImFont *font_sm = AddDefaultFont(12);
    static ImFont *font_md = AddDefaultFont(14);
    // * State
    // Do not throttle the FPS for active_counter frames
    // (decreases 1 each frame)
    static int active_counter;
    // Only true on first loop (places and resizes windows)
    static bool init = true;

    // Set to open popups
    static bool open_color_picker = false,
                open_reference = false,
                open_shell = false;

    // Main graphics adaptor for plot.draw
    static ImGuiDrawListGraphicsAdaptor adaptor;

    // Color picker func index: function whose color
    // the color editor is changing (not necessarily curr_func)
    static size_t curr_edit_color_idx;

#ifdef NIVALIS_EMSCRIPTEN
    int ems_js_canvas_width = canvas_get_width();
    int ems_js_canvas_height = canvas_get_height();
    glfwSetWindowSize(window, ems_js_canvas_width, ems_js_canvas_height);
#else
    double frame_time = glfwGetTime();
#endif

    glfwPollEvents();
    // Clear
    glClear(GL_COLOR_BUFFER_BIT);
#ifndef NIVALIS_EMSCRIPTEN
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
#endif
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float ratio = width / (float) height;
#ifndef NIVALIS_EMSCRIPTEN
    glOrtho(0, width, height, 0, 0, 1);
#endif

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

    static ImDrawList* draw_list_pre = nullptr;
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    adaptor.draw_list = draw_list;
    if (plot.require_update || plot.marker_text.size() || open_shell) {
        // Reset the active counter
        // (do not throttle the FPS for ACTIVE_FRAMES frames)
        active_counter = ACTIVE_FRAMES;
    }
    if (plot.require_update) {
        plot.require_update = false;
        // Redraw the grid and functions
        glClearColor(1., 1., 1., 1.); // Clear white
        plot.draw_grid(adaptor);      // Draw axes and grid
#ifndef NIVALIS_EMSCRIPTEN
        {
            // Need lock since worker thread asynchroneously
            // swaps back buffer to front
            std::lock_guard<std::mutex> lock(worker_mtx);
            plot.draw(adaptor);       // Draw functions
        }
        // Run worker if not already running AND either:
        // this update was not from the worker or
        // view has changed since last worker run
        maybe_run_worker(plot);
        // In Emscripten threads are not supported, and we use
        // WebWorker instead; this is not need then
#else
        // Synchronous, since threading not supported
        plot.recalc();
        plot.swap();
        plot.draw(adaptor);       // Draw functions
#endif
        // Cache the draw list
        draw_list_pre = draw_list->CloneOutput();
    } else {
        // No update, load draw list from cache
        if (draw_list_pre) *draw_list = *draw_list_pre;
        if (active_counter > 0) {
            --active_counter;
        } else {
            // Sleep to throttle FPS
#ifdef NIVALIS_EMSCRIPTEN
            // emscripten_sleep(1000 / RESTING_FPS);
#else
            std::this_thread::sleep_for(std::chrono::microseconds(
                        1000000 / RESTING_FPS
                        ));
#endif

        }
    }

    // Set to set current function to 'change_curr_func' at next loop step
    static int change_curr_func = -1;
    // Change current function
    if (~change_curr_func) {
        // Seek to previous/next function after up/down arrow on textbox
        plot.set_curr_func(change_curr_func);
        change_curr_func = -1;
    }

    // Render GUI
    if (init) {
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowSize(ImVec2(400, 130));
    }
    ImGui::Begin("Functions", NULL);
    ImGui::PushFont(font_md);

    for (size_t fidx = 0; fidx < plot.funcs.size(); ++fidx) {
        if (plot.focus_on_editor && fidx == plot.curr_func) {
            ImGui::SetKeyboardFocusHere(0);
        }
        const std::string fid = plot.funcs[fidx].name;
        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 100);
        if (ImGui::InputText((fid +
                        "##funcedit-" + fid).c_str(),
                    &plot.funcs[fidx].expr_str,
                    ImGuiInputTextFlags_CallbackHistory,
                    [](ImGuiTextEditCallbackData* data) -> int {
                        // Handle up/down arrow keys in textboxes
                        change_curr_func = plot.curr_func +
                            (data->EventKey == ImGuiKey_UpArrow ? -1 : 1);
                        return 0;
                    })) {
            plot.reparse_expr(fidx);
        }
        if (ImGui::IsItemActive() && !plot.focus_on_editor) {
            if (fidx != plot.curr_func) change_curr_func = fidx;
        }
        ImGui::SameLine();
        auto& col = plot.funcs[fidx].line_color;
        if (ImGui::ColorButton(("c##colfun" +fid).c_str(),
                    ImVec4(col.r, col.g, col.b, col.a),
                    ImGuiColorEditFlags_NoAlpha)) {
            open_color_picker = true;
            curr_edit_color_idx = fidx;
        }
        ImGui::SameLine();
        if (ImGui::Button(("x##delfun-" + fid).c_str())) {
            plot.delete_func(fidx--);
        }
    }
    plot.focus_on_editor = false;
    std::string tmp = plot.funcs.back().expr_str;
    util::trim(tmp);
    if (tmp.size()) {
        if (ImGui::Button("+ New function")) plot.add_func();
        ImGui::SameLine();
    }
    if (ImGui::Button("? Help")) {
        open_reference = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("# Shell")) {
        open_shell = true;
    }
    ImGui::PushFont(font_sm);
    ImGui::TextColored(ImColor(255, 50, 50, 255), "%s",
            plot.func_error.c_str());
    ImGui::PushFont(font_md);
    ImGui::End(); //  Functions

    if (init) {
        ImGui::SetNextWindowPos(ImVec2(10,
                    static_cast<float>(plot.view.shigh - 140)));
        ImGui::SetNextWindowSize(ImVec2(333, 130));
    }
    ImGui::Begin("Sliders", NULL);
    if (~pwwidth && !init) {
        // Outer window was resized
        ImVec2 pos = ImGui::GetWindowPos();
        ImGui::SetWindowPos(ImVec2(pos.x,
                    (float)wheight - ((float)pwheight - pos.y)));
    }

    for (size_t sidx = 0; sidx < plot.sliders.size(); ++sidx) {
        ImGui::PushItemWidth(50.);
        auto& sl = plot.sliders[sidx];
        std::string slid = std::to_string(sidx);
        if (ImGui::InputText(("=##vsl-" + slid).c_str(), &sl.var_name)) {
            plot.update_slider_var(sidx);
        }

        ImGui::SameLine();
        if (ImGui::InputFloat(("##vslinput-" + slid).c_str(),
                    &sl.val)) {
            plot.copy_slider_value_to_env(sidx);
        }

        ImGui::SameLine(0., 10.0);
        ImGui::InputFloat(("min##vsl-"+slid).c_str(), &sl.lo);
        ImGui::SameLine();
        ImGui::InputFloat(("max##vsl-"+slid).c_str(), &sl.hi);

        ImGui::SameLine();
        if (ImGui::Button(("x##delvsl-" + slid).c_str())) {
            plot.delete_slider(sidx--);
            continue;
        }

        ImGui::PushItemWidth(318.);
        if (ImGui::SliderFloat(("##vslslider" + slid).c_str(),
                    &sl.val, sl.lo, sl.hi)) {
            plot.copy_slider_value_to_env(sidx);
        }
    }

    if (ImGui::Button("+ New slider")) plot.add_slider();
    ImGui::PushFont(font_sm);
    ImGui::TextColored(ImColor(255, 50, 50, 255), "%s",
            plot.slider_error.c_str());
    ImGui::PushFont(font_md);

    ImGui::End(); // Sliders

    if (init) {
        ImGui::SetNextWindowPos(ImVec2(
                    static_cast<float>(10),
                    static_cast<float>(10)));
        ImGui::SetNextWindowSize(ImVec2(350, 150));
    }

    if (init) {
        ImGui::SetNextWindowPos(ImVec2(
                    static_cast<float>(plot.view.swid - 182), 10));
        ImGui::SetNextWindowSize(ImVec2(175, 105));
    }
    ImGui::Begin("View", NULL, ImGuiWindowFlags_NoResize);
    if (~pwwidth && !init) {
        // Outer window was resized
        ImVec2 pos = ImGui::GetWindowPos();
        ImGui::SetWindowPos(ImVec2(
                    wwidth - ((float) pwwidth - pos.x),
                    pos.y));
    }
    ImGui::PushItemWidth(60.);
    if (ImGui::InputDouble(" <x<", &plot.view.xmin)) plot.require_update = true;
    ImGui::SameLine();
    ImGui::PushItemWidth(60.);
    if(ImGui::InputDouble("##xm", &plot.view.xmax)) plot.require_update = true;
    ImGui::PushItemWidth(60.);
    if (ImGui::InputDouble(" <y<", &plot.view.ymin)) plot.require_update = true;
    ImGui::SameLine();
    ImGui::PushItemWidth(60.);
    if(ImGui::InputDouble("##ym", &plot.view.ymax)) plot.require_update = true;
    if (ImGui::Button("Reset view")) plot.reset_view();
    ImGui::End(); // View

    // Show the marker window
    if (plot.marker_text.size()) {
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(plot.marker_posx),
                    static_cast<float>(plot.marker_posy)));
        ImGui::SetNextWindowSize(ImVec2(250, 50));
        ImGui::Begin("Marker", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextUnformatted(plot.marker_text.c_str());
        ImGui::End();
    }
    if(open_color_picker) ImGui::OpenPopup("Color picker");
    if(open_reference) ImGui::OpenPopup("Reference");
    if(open_shell) ImGui::OpenPopup("Shell");

    if (ImGui::BeginPopupModal("Color picker", &open_color_picker,
                ImGuiWindowFlags_AlwaysAutoResize)) {
        // Color picker dialog
        auto* sel_col = plot.funcs[curr_edit_color_idx].line_color.data;
        ImGui::TextUnformatted("Presets:");
        for (size_t i = 0; i < 8; ++i) {
            ImGui::SameLine();
            auto col = color::from_int(i);
            if (ImGui::ColorButton(("##cpick-preset-"
                            + std::to_string(i)).c_str(),
                        ImVec4(col.r, col.g, col.b, col.a),
                        ImGuiColorEditFlags_NoAlpha)) {
                plot.funcs[curr_edit_color_idx].line_color = col;
                plot.require_update = true;
            }
        }
        if (ImGui::ColorPicker3("color", sel_col,
                    ImGuiColorEditFlags_PickerHueWheel)) {
            plot.require_update = true;
        }
        // Close
        if (ImGui::Button("Ok##cpickok", ImVec2(100.f, 0.0f))) {
            open_color_picker = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(std::min(600, plot.view.swid),
                std::min(400, plot.view.shigh)));
    if (ImGui::BeginPopupModal("Reference", &open_reference,
                ImGuiWindowFlags_NoResize)) {
        // Reference popup
        ImGui::TextUnformatted("GUI: Function editor");
        ImGui::Indent();
        ImGui::BulletText("%s", "The function editor is the window initially on top-left with\ntextboxes. You can enter expressions here to draw:");
        ImGui::Indent();
        ImGui::BulletText("%s", "Explicit functions: Simply enter an expression with x in the textbox\ne.g. x^2");
        ImGui::BulletText("%s", "Implicit functions: Enter an equation with x, y in the textbox, e.g. cos(x*y)=0");
        ImGui::BulletText("%s", "'Polylines' (points and lines)");
        ImGui::Indent();
        ImGui::BulletText("%s", "To draw a single point, write (<x-coord>,<y-coord>)\ne.g. (1, 2). Coords can have variables.");
        ImGui::BulletText("%s", "To draw a series of points connected in order, write\n(<x1>,<y1>)(<x2>,<y2>)... e.g. (1, 1)(2,2)(3,2)");
        ImGui::Unindent();
        ImGui::Unindent();
        ImGui::BulletText("%s", "Press '+ New function' to add more functions.\nPress 'x' to delete a function.\nPress the colored button to the left to change line color.");
        ImGui::Unindent();

        ImGui::TextUnformatted("GUI: Keyboard shortcuts");
        ImGui::Indent();
        ImGui::BulletText("On background\n");
        ImGui::Indent();
        ImGui::BulletText("%s", "E to focus on functions editor");
        ImGui::BulletText("%s", "-= keys to zoom, arrow keys to move");
        ImGui::BulletText("%s", "Ctrl/Alt + -= keys to zoom asymmetrically");
        ImGui::Unindent();
        ImGui::BulletText("On function editor\n");
        ImGui::Indent();
        ImGui::BulletText("%s", "Up/down arrow to move between functions\n"
                "Down from the bottomost function to create new function");
        ImGui::Unindent();
        ImGui::Unindent();

        ImGui::TextUnformatted("Expressions: Operators");
        ImGui::Indent();
        ImGui::BulletText("%s", "+- */% ^\nWhere ^ is exponentiation (right-assoc)");
        ImGui::BulletText("%s", "Parentheses: () and [] are equivalent (but match separately)");
        ImGui::BulletText("%s", "Comparison: < > <= >= == output 0,1\n(= equivalent to == except in assignment/equality statement)");
        ImGui::Unindent();

        ImGui::TextUnformatted("Expressions: Special Forms");
        ImGui::Indent();
        ImGui::BulletText("%s", "Conditional special form (piecewise function):\n"
                "{<predicate>: <expr>[, <elif-pred>: "
                "<expr>[, ... [, <else-expr>]]]}\n"
                "ex. {x<0: x, x>=0 : x^2}  ex. {x<2: exp(x)}");
        ImGui::BulletText("%s", "Sum special form:\n"
                "sum(<var>: <begin>, <end>)[<expr>]\n"
                "ex. sum(x: 0, 100)[x]");
        ImGui::BulletText("%s", "Prod special form:\n"
                "prod(<var>: <begin>, <end>)[<expr>]\n"
                "ex. prdo(x: 0, 100)[1/x]");
        ImGui::BulletText("%s", "Diff (derivative) special form:\n"
                "prod(<var>: <begin>, <end>)[<expr>]\n"
                "ex. prdo(x: 0, 100)[1/x]");
        ImGui::Unindent();

        ImGui::TextUnformatted("Expressions: Functions");
        ImGui::Indent();
        ImGui::BulletText("%s", "<func_name>(<args>) to call a function");
        ImGui::BulletText("%s", "Most function names are self-explanatory; some hints:\n"
                "N(x) is standard normal pdf\n"
                "Functions take a fixed number of arguments, except\n"
                "log(x,b) (log x base b) is special: can take 1 or 2 args, where log(x) = ln(x)");
        ImGui::BulletText("%s", "List of functions available:");
        ImGui::Indent();
        const auto& mp = OpCode::funcname_to_opcode_map();
        for (const auto& pr : mp) {
            std::string fnrepr = pr.first + "(";
            if (pr.second == -1) {
                fnrepr.push_back('_');
            } else {
                for (size_t i = 0; i < OpCode::n_args(
                            pr.second); ++i) {
                    if (i) fnrepr.append(", ");
                    fnrepr.push_back('_');
                }
            }
            ImGui::BulletText("%s)", fnrepr.c_str());
        }

        ImGui::Unindent();
        ImGui::Unindent();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(std::min(700, plot.view.swid),
                std::min(500, plot.view.shigh)));
    if (ImGui::BeginPopupModal("Shell", &open_shell,
                ImGuiWindowFlags_NoResize)) {
        // Shell popup
        if (init) ImGui::SetWindowCollapsed(true);

        ImGui::BeginChild("Scrolling", ImVec2(0, ImGui::GetWindowHeight() - 85));
        // * Virtual shell state
        // Shell output stream (replaces cout)
        static std::stringstream shell_ss;
        // Hidden shell backend
        static Shell shell(env, shell_ss);
        // Shell command buffer
        static std::string shell_curr_cmd;
        // If true, scrolls the shell to bottom on next frame
        // (used after command exec to scroll to bottom)
        static bool shell_scroll = false;
        // Shell history implementation
        static std::vector<std::string> shell_hist;
        static size_t shell_hist_pos = -1;
        // * End virtual shell state

        ImGui::TextWrapped("%s\n", shell_ss.str().c_str());
        if (shell_scroll) {
            shell_scroll = false;
            ImGui::SetScrollHere(1.0f);
        }
        ImGui::EndChild();

        if (!ImGui::IsAnyItemActive())
            ImGui::SetKeyboardFocusHere(0);
        auto exec_shell = [&]{
            if (shell_curr_cmd.empty()) return;
            shell_ss << ">>> " << shell_curr_cmd << "\n";
            // Push history
            if (shell_hist.empty() ||
                    shell_hist.back() != shell_curr_cmd){
                // Don't push if same as last
                shell_hist.push_back(shell_curr_cmd);
            }
            shell_hist_pos = -1;
            shell.eval_line(shell_curr_cmd);
            shell_curr_cmd.clear();
            shell_scroll = true;
        };
        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 80.);
        if (ImGui::InputText("##ShellCommand",
                    &shell_curr_cmd,
                    ImGuiInputTextFlags_EnterReturnsTrue |
                    ImGuiInputTextFlags_CallbackHistory,
                    [](ImGuiTextEditCallbackData* data) -> int {
                        // Handle up/down arrow keys in textboxes
                        const int prev_history_pos = shell_hist_pos;
                        if (data->EventKey == ImGuiKey_UpArrow)
                        {
                            if (shell_hist_pos == -1)
                                shell_hist_pos = shell_hist.size() - 1;
                            else if (shell_hist_pos > 0)
                                shell_hist_pos--;
                        } else if (data->EventKey == ImGuiKey_DownArrow) {
                            if (~shell_hist_pos)
                            if (++shell_hist_pos >= shell_hist.size())
                                shell_hist_pos = -1;
                        }
                        if (prev_history_pos != shell_hist_pos) {
                            data->DeleteChars(0, data->BufTextLen);
                            if (shell_hist_pos != -1) {
                                data->InsertChars(0,
                                        shell_hist[
                                            shell_hist_pos].c_str());
                            }
                        }
                        return 0;
                    })) {
                // If part
                exec_shell();
            }
        ImGui::SameLine();
        if (ImGui::Button("Submit")) exec_shell();
        ImGui::EndPopup();
    }

    // * Handle IO events
    ImGuiIO &io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        int mouse_x = static_cast<int>(io.MousePos[0]);
        int mouse_y = static_cast<int>(io.MousePos[1]);
        if (io.MouseDown[0]) {
            plot.handle_mouse_down(mouse_x, mouse_y);
        }
        if (io.MouseReleased[0]) {
            plot.handle_mouse_up(mouse_x, mouse_y);
        }
        static int mouse_prev_x = -1, mouse_prev_y = -1;
        if (mouse_x != mouse_prev_x || mouse_y != mouse_prev_y) {
            plot.handle_mouse_move(mouse_x, mouse_y);
        }
        if (io.MouseWheel) {
            plot.handle_mouse_wheel(
                    io.MouseWheel > 0,
                    static_cast<int>(std::fabs(io.MouseWheel) * 120),
                    mouse_x, mouse_y);
        }
        mouse_prev_x = mouse_x;
        mouse_prev_y = mouse_y;
    }

    if (!io.WantCaptureKeyboard) {
        for (size_t i = 0; i < IM_ARRAYSIZE(io.KeysDown); ++i) {
            if (ImGui::IsKeyDown(i)) {
                plot.handle_key(i,
                        io.KeyCtrl, io.KeyAlt);
            }
        }
    }

    // Render dear imgui into screen
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
    init = false;
}  // void main_loop_step()
}  // namespace

// Need to use global state since
// emscripten main loop takes a C function ptr...
extern "C" {
#ifdef NIVALIS_EMSCRIPTEN
    void emscripten_loop() {
        main_loop_step();
    }
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
#endif  // NIVALIS_EMSCRIPTEN

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

#ifdef NIVALIS_EMSCRIPTEN
        // Resize the canvas to fill the browser window
        resizeCanvas();
#else
        // Init glew context
        if (glewInit() != GLEW_OK)
        {
            fprintf(stderr, "Failed to initialize OpenGL loader!\n");
            return false;
        }
#endif
        return true;
    }

    // Main method
    int main(int argc, char ** argv) {
        using namespace nivalis;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) plot.add_func();
            // Load the expression
            plot.funcs[i - 1].expr_str = argv[i];
            plot.reparse_expr(i);
        }
        if (!init_gl()) return 1; // GL initialization failed

        // Setup Dear ImGUI context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        char* glsl_version = NULL;
        ImGui_ImplOpenGL3_Init(glsl_version);
        // Setup Dear ImGui style
        ImGui::StyleColorsLight();

#ifdef NIVALIS_EMSCRIPTEN
        // Set initial window size according to HTML canvas size
        int ems_js_canvas_width = canvas_get_width();
        int ems_js_canvas_height = canvas_get_height();
        glfwSetWindowSize(window, ems_js_canvas_width, ems_js_canvas_height);

        // Set handlers and start emscripten main loop
        emscripten_set_main_loop(emscripten_loop, 0, 1);
#else
        // Start worker thread
        std::thread thd(draw_worker, std::ref(plot));
        thd.detach();

        // Main GLFW loop (desktop)
        while (!glfwWindowShouldClose(window)) {
            main_loop_step();
        } // Main loop
#endif
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
} // extern "C"
