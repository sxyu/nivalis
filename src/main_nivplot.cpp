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
#include <algorithm>

#include "plotter/plotter.hpp"
#include "plotter/imgui_adaptor.hpp"
#include "imstb_textedit.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "imgui_stdlib.h"

// Font
#include "resources/roboto.h"

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <GL/glew.h>
#include "imfilebrowser.h"

#include <GLFW/glfw3.h>

#include "shell.hpp"
#include "util.hpp"

namespace {
using namespace nivalis;

Plotter plot; // Main plotter
Plotter worker_plot; // Worker plotter
Environment& env = plot.env; // Main environment
GLFWwindow* window; // GLFW window

// Worker state
bool run_worker_flag;
volatile bool worker_quit_flag;
std::condition_variable worker_cv;
std::mutex worker_mtx;
std::stringstream state_encoding;

// Fonts
ImFont* font_sm, * font_md;

// If true, then the redraw_canvas request is from WebWorker.
// to prevent infinite update loop, do not call WebWorker in that case
bool worker_req_update;

// Draw worker thread entry point
void draw_worker(nivalis::Plotter& plot) {
    while (!worker_quit_flag) {
        Plotter::View view;
        {
            std::unique_lock<std::mutex> lock(worker_mtx);
            worker_cv.wait(lock, []{return run_worker_flag;});
            if (worker_quit_flag) break;
            view = worker_plot.view;
            run_worker_flag = false;
            worker_plot.import_binary_func_and_env(state_encoding);
            state_encoding.str("");
        }
        worker_plot.render(view);
        std::lock_guard<std::mutex> lock(worker_mtx);
        // worker_plot.export_binary_render_result(output_encoding);
        // plot.import_binary_render_result(output_encoding);
        // Use swap rather than messaging for better performacne
        plot.draw_buf.swap(worker_plot.draw_buf);
        plot.pt_markers.swap(worker_plot.pt_markers);
        plot.require_update = true;
        if (worker_plot.loss_detail) {
            plot.func_error = worker_plot.func_error;
        } else if (plot.loss_detail) {
            plot.func_error.clear();
        }
        plot.loss_detail = worker_plot.loss_detail;
        worker_req_update = true;
    }
}

// Run worker if not already running AND either:
// this update was not from the worker or
// view has changed since last worker run
void maybe_run_worker(nivalis::Plotter& plot) {
    using namespace nivalis;
    {
        std::lock_guard<std::mutex> lock(worker_mtx);
        run_worker_flag = !(worker_req_update &&
                worker_plot.view == plot.view);
        state_encoding.str("");
        plot.export_binary_func_and_env(state_encoding);
        worker_plot.view = plot.view;
        worker_req_update = false;
    }
    worker_cv.notify_one();
}

// MAIN LOOP: Run single step of main loop
void main_loop_step() {
    // * State
    // Do not wait for input event for active_counter frames
    // (decreases 1 each frame)
    static int active_counter;

    // Set to open popups
    static bool open_color_picker = false,
                open_reference = false,
                open_shell = false;

    // Main graphics adaptor for plot.draw
    static ImGuiDrawListGraphicsAdaptor adaptor;

    // Color picker func index: function whose color
    // the color editor is changing (not necessarily curr_func)
    static size_t curr_edit_color_idx;

    // File dialog
    static ImGui::FileBrowser open_file_dialog;
    static ImGui::FileBrowser save_file_dialog(
            ImGuiFileBrowserFlags_EnterNewFilename);
    if (open_file_dialog.GetTitle().empty()) {
        open_file_dialog.SetTypeFilters({ ".json" });
        open_file_dialog.SetTitle("Import JSON");
        save_file_dialog.SetTypeFilters({ ".json" });
        save_file_dialog.SetTitle("Export JSON");
    }

    if (active_counter > 0 || plot.animating_sliders.size()) {
        // After move, keep updating for a while to
        // prevent UI freeze
        // Also do so if there are animating sliders
        glfwPollEvents();
        --active_counter;
    } else {
        // Wait until mouse/keyboard event
        // to reduce CPU usage
        glfwWaitEvents();
        active_counter = 40;
    }
    // Clear plot
    glClear(GL_COLOR_BUFFER_BIT);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glOrtho(0, width, height, 0, 0, 1);

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
    adaptor.shigh = plot.view.shigh;
    adaptor.swid = plot.view.swid;
    if (plot.require_update) {
        // Redraw
        plot.require_update = false;
        // Redraw the grid and functions
        plot.draw_grid(adaptor, plot_view_pre);      // Draw axes and grid
        {
            // Need lock since worker thread asynchroneously
            // swaps back buffer to front
            std::lock_guard<std::mutex> lock(worker_mtx);
            plot.draw(adaptor, plot_view_pre);       // Draw functions
            plot.populate_grid();                    // Populate grid of point markers for mouse events
        }
        // Run worker if not already running AND either:
        // this update was not from the worker or
        // view has changed since last worker run
        maybe_run_worker(plot);

        // Note view is delayed by one frame, so that if worker finishes updating
        // in a single frame, we don't get border artifacts due to user movement
        plot_view_pre = plot.view;

        // Cache the draw list
        draw_list_pre.CmdBuffer = draw_list->CmdBuffer;
        draw_list_pre.IdxBuffer = draw_list->IdxBuffer;
        draw_list_pre.VtxBuffer = draw_list->VtxBuffer;
        draw_list_pre.Flags = draw_list->Flags;
    } else {
        // No update, load draw list from cache
        // (used if e.g. mouse moved without dragging)
        *draw_list = draw_list_pre;
    }

    // Set to set current function to 'change_curr_func' at next loop step
    static size_t change_curr_func = (size_t)-1;
    // Change current function
    if (~change_curr_func) {
        // Seek to previous/next function after up/down arrow on textbox
        plot.set_curr_func(change_curr_func);
        change_curr_func = (size_t)-1;
    }
    plot.slider_animation_step();

    // Render GUI
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(412, 130), ImGuiCond_Once);
    ImGui::PushFont(font_md);
    ImGui::Begin("Functions", NULL);

    for (size_t fidx = 0; fidx < plot.funcs.size(); ++fidx) {
        auto& func = plot.funcs[fidx];
        if (plot.focus_on_editor && fidx == plot.curr_func) {
            ImGui::SetKeyboardFocusHere(0);
        }
        const std::string fid = func.name;
        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 100);
        if (ImGui::InputText((fid +
                        "##funcedit-" + fid).c_str(),
                    &func.expr_str,
                    ImGuiInputTextFlags_CallbackHistory,
                    [](ImGuiTextEditCallbackData* data) -> int {
                        // Handle up/down arrow keys in textboxes
                        if (!ImGui::GetIO().KeyCtrl) {
                            change_curr_func = plot.curr_func +
                                (data->EventKey == ImGuiKey_UpArrow ? -1 : 1);
                        }
                        return 0;
                    })) {
            std::lock_guard<std::mutex> lock(worker_mtx);
            plot.reparse_expr(fidx);
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemActive() && !plot.focus_on_editor) {
            if (fidx != plot.curr_func) change_curr_func = fidx;
        }
        ImGui::SameLine();
        auto& col = func.line_color;
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
        if (func.uses_parameter_t()) {
            ImGui::PushItemWidth(50);
            if (ImGui::DragFloat(("##tmins" + fid).c_str(), &func.tmin, 0.05f,
                       0.0f, 0.0f,  "t min")) {
                plot.require_update = true;
                if (func.tmax <= func.tmin) func.tmin = func.tmax - 1e-9f;
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(100);
            if (ImGui::InputFloat(("##tmin" + fid).c_str(), &func.tmin)) {
                plot.require_update = true;
                if (func.tmax <= func.tmin) func.tmin = func.tmax - 1e-9f;
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::PushItemWidth(50);
            if (ImGui::DragFloat(("##tmaxs" + fid).c_str(), &func.tmax, 0.05f,
                       0.0f, 0.0f, "t max")) {
                plot.require_update = true;
                if (func.tmax <= func.tmin) func.tmax = func.tmin + 1e-9;
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::PushItemWidth(100);
            if (ImGui::InputFloat(("##tmax" + fid).c_str(), &func.tmax)) {
                plot.require_update = true;
                if (func.tmax <= func.tmin) func.tmax = func.tmin + 1e-9f;
            }
            ImGui::PopItemWidth();

        }
    }
    plot.focus_on_editor = false;
    if (ImGui::Button("+ New function")) plot.add_func();
    ImGui::SameLine();
    if (ImGui::Button("Import")) {
        open_file_dialog.Open();
    }
    ImGui::SameLine();
    if (ImGui::Button("Export")) {
        save_file_dialog.Open();
    }
    ImGui::SameLine();
    if (ImGui::Button("# Shell")) {
        open_shell = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("? Help")) {
        open_reference = true;
    }
    ImGui::PushFont(font_sm);
    ImGui::TextColored(ImColor(255, 50, 50, 255), "%s",
            plot.func_error.c_str());
    ImGui::PopFont();
    ImGui::End(); //  Functions

    ImGui::SetNextWindowPos(ImVec2(10,
                static_cast<float>(
                    (~pwwidth ? pwheight : plot.view.shigh) - 140)),
            ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(385, 130), ImGuiCond_Once);
    ImGui::Begin("Sliders", NULL);
    if (~pwwidth) {
        // Outer window was resized
        ImVec2 pos = ImGui::GetWindowPos();
        ImGui::SetWindowPos(ImVec2(pos.x,
                    (float)wheight - ((float)pwheight - pos.y)));
    }

    for (size_t sidx = 0; sidx < plot.sliders.size(); ++sidx) {
        auto& sl = plot.sliders[sidx];
        std::string slid = std::to_string(sidx);
        ImGui::PushItemWidth(50.);
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
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button(("ani##anivsl-" + slid).c_str())) {
            if (plot.sliders[sidx].animation_dir) {
                plot.end_slider_animation(sidx);
            } else {
                plot.begin_slider_animation(sidx);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(("x##delvsl-" + slid).c_str())) {
            plot.delete_slider(sidx--);
            continue;
        }

        ImGui::PushItemWidth(355.);
        if (ImGui::SliderFloat(("##vslslider" + slid).c_str(),
                    &sl.val, sl.lo, sl.hi)) {
            plot.copy_slider_value_to_env(sidx);
        }
        ImGui::PopItemWidth();
    }

    if (ImGui::Button("+ New slider")) plot.add_slider();
    ImGui::PushFont(font_sm);
    ImGui::TextColored(ImColor(255, 50, 50, 255), "%s",
            plot.slider_error.c_str());
    ImGui::PopFont();

    ImGui::End(); // Sliders

    ImGui::SetNextWindowPos(ImVec2(
                static_cast<float>(
                    (~pwwidth ? pwwidth : plot.view.swid) - 208), 10),
            ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(200, 175), ImGuiCond_Once);
    ImGui::Begin("View", NULL, ImGuiWindowFlags_NoResize);
    if (~pwwidth) {
        // Outer window was resized
        ImVec2 pos = ImGui::GetWindowPos();
        ImGui::SetWindowPos(ImVec2(
                    wwidth - ((float) pwwidth - pos.x),
                    pos.y));
    }
    ImGui::PushItemWidth(65.);
    if (ImGui::InputDouble(" <x<", &plot.view.xmin)) plot.require_update = true;
    ImGui::SameLine();
    if(ImGui::InputDouble("##xm", &plot.view.xmax)) plot.require_update = true;
    if (ImGui::InputDouble(" <y<", &plot.view.ymin)) plot.require_update = true;
    ImGui::SameLine();
    if(ImGui::InputDouble("##ym", &plot.view.ymax)) plot.require_update = true;
    if (ImGui::Button("Reset view")) plot.reset_view();
    if (ImGui::Checkbox("Axes", &plot.enable_axes)) plot.require_update = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Grid", &plot.enable_grid)) plot.require_update = true;
    if (ImGui::Checkbox("Polar grid", &plot.polar_grid)) plot.require_update = true;
    ImGui::PopItemWidth();
    ImGui::End(); // View

    // Show the marker window
    if (plot.marker_text.size()) {
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(plot.marker_posx),
                    static_cast<float>(plot.marker_posy)));
        ImGui::SetNextWindowSize(ImVec2(250, 60));
        ImGui::Begin("Marker", NULL, ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
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

    ImGui::SetNextWindowSize(ImVec2(std::min<int>(600, plot.view.swid),
                std::min<int>(400, plot.view.shigh)));
    if (ImGui::BeginPopupModal("Reference", &open_reference,
                ImGuiWindowFlags_NoResize)) {
        // Reference popup
        ImGui::TextUnformatted("GUI: Function editor");
        ImGui::Indent();
            ImGui::BulletText("%s", "The function editor is the window initially on top-left with\ntextboxes. You can enter expressions in textboxes here to draw:");
            ImGui::Indent();
            ImGui::BulletText("%s", "Explicit functions: Simply enter an expression with x\ne.g. x^2, gamma(x). Equivalently, enter y=<expr>");
            ImGui::BulletText("%s", "Implicit functions: Enter an equation with x, y in the textbox\ne.g. cos(x*y)=0, sin(x)=cos(y)");
            ImGui::BulletText("%s", "Inequalities (implicit): e.g. x<y, cos(y)<sin(y), x^2>y");
            ImGui::BulletText("%s", "Parametric: (<x-expr>, <y-expr>), where expressions should be\nin terms of t e.g. (4*sin(4*t),3*sin(3*t))");
            ImGui::BulletText("%s", "Polar: r=<expr>, where <expr> should be in terms of angle t\ne.g. r = 1-cos(t)");
            ImGui::Indent();
                ImGui::BulletText("%s", "After entering a parametric/polar function, "
                    "inputs will\nappear to allow adjusting bounds on t "
                    "(you can directly set\nthe value or drag 't min', 't max' to change)");
            ImGui::Unindent();
            ImGui::BulletText("%s", "'Polylines' (points and lines)");
            ImGui::Indent();
                ImGui::BulletText("%s", "To draw a single point, write (<x-coord>,<y-coord>)\ne.g. (1, 2). Coords can have slider variables.");
                ImGui::BulletText("%s", "To draw a series of points connected in order, write\n(<x1>,<y1>),(<x2>,<y2>)... e.g. (1,1),(2,2),(3,2)");
            ImGui::Unindent();
            ImGui::BulletText("%s", "2D Geometry Primitives");
            ImGui::Indent();
                ImGui::BulletText("%s", "Line/polygon: %poly [polyline expression], e.g. %poly (0,0),(1,1),(2,0)\nSimilar to polyline but closes the path automatically");
                ImGui::BulletText("%s", "Rectangle: %rect (ax, ay), (bx, by) e.g. %rect (0,0),(1,1)");
                ImGui::BulletText("%s", "Circle: %circ radius @ (cenx, ceny)");
                ImGui::BulletText("%s", "Ellipse: %ellipse (rx, ry) @ (cenx, ceny)");
                ImGui::BulletText("%s", "*Filling*: replace %<command> with %f<command> to fill the\nprimitive, for example %fpoly or %frect");
                ImGui::BulletText("%s", "*Borderless filling*: replace %<command> with %F<command> to fill\nthe primitive AND remove the border, for example %Fpoly or %Frect");
                ImGui::BulletText("%s", "%circle is an alias for %circ and %rectangle for %rect");
            ImGui::Unindent();
            ImGui::BulletText("%s", "Text: %text text-to-draw  @ (posx, posy) e.g. %text hello world @ (0,0)\nThe text will be center-aligned. Left/right whitespaces will be trimmed.");
        ImGui::Unindent();
        ImGui::BulletText("%s", "Each function is given a unique name, e.g. f0, f1,\nshown to its right. You can use it in other function:\ne.g. expression of another function may be f0(x^2)");
        ImGui::BulletText("%s", "Press the colored button to the right of the function name\nto change function color.");
        ImGui::BulletText("%s", "Press 'x' to the right of that to delete the function.");
        ImGui::BulletText("%s", "Press '+ New function' to add more functions.\nPress 'x' to delete a function.\nPress the colored button to the left to change line color.");
        ImGui::BulletText("%s", "Press '# Shell' to get a virtual nivalis shell\n(see 'Shell' section below for usage)");
        ImGui::Unindent();

        ImGui::TextUnformatted("GUI: Sliders");
        ImGui::Indent();
        ImGui::BulletText("%s", "The slider window is initially on the bottom-left.\nClick '+ Add slider' to create a slider.");
        ImGui::BulletText("%s", "The top-left box above the slider is the variable name.\nThis can be any string consisting a-z, A-Z, 0-9, _, or ',\nnot starting with a number; it cannot be any of x,y,t,r.\nFor example, xy_3' is valid.\nTo use the slider value, enter this variable in a function expression\nin the function editor.\ne.g. if the string here is 'a',"
                "you can use 'a*x' in a function");
        ImGui::BulletText("%s", "The boxes to the right are for setting the variable's value manually\nand the lower/upper bounds of the slider resp.\n");
        ImGui::BulletText("%s", "Drag on the slider to change the variable's value.\n");
        ImGui::Unindent();

        ImGui::TextUnformatted("GUI: Keyboard shortcuts");
        ImGui::Indent();
        ImGui::BulletText("On background\n");
        ImGui::Indent();
        ImGui::BulletText("%s", "E to focus on functions editor");
        ImGui::BulletText("%s", "-= keys to zoom, arrow keys to move");
        ImGui::BulletText("%s", "Shift/Alt + -= keys to zoom asymmetrically");
        ImGui::BulletText("%s", "O to switch to cartesian grid");
        ImGui::BulletText("%s", "P to switch to polar grid");
        ImGui::Unindent();
        ImGui::BulletText("On function editor\n");
        ImGui::Indent();
        ImGui::BulletText("%s", "Up/down arrow to move between functions\n"
                "Down from the bottomost function to create new function");
        ImGui::BulletText("%s", "Ctrl + <background hotkey> e.g. Ctrl + Arrow/Ctrl + P to invoke background hotkey");
        ImGui::Unindent();
        ImGui::Unindent();

        ImGui::TextUnformatted("Shell");
        ImGui::Indent();
        ImGui::BulletText("%s", "Define variable: for example, a = 3+4,\nthen you can use a in any function in the function editor.");
        ImGui::BulletText("%s",
                "Operator assignment: a+=3, a*=3, etc., as in usual languages");
        ImGui::BulletText("%s",
        "Define custom function: <name>(<args>) = <expr>\ne.g. sec(x) = 1/cos(x) or f(x,y,z) = x+y+z\nYou can use this function in the function editor.");
        ImGui::BulletText("%s", "Symbolic operations");
        ImGui::Indent();
        ImGui::BulletText("%s", "Simplify expression (not super reliable): s <expr>\ne.g. s (1+x)^2 + 2*(x+1)^2, s exp(x)*exp(2*x)");
        ImGui::BulletText("%s", "Differentiate a function: diff <var> <expr>\ne.g. diff x sin(x)*cos(2*x); outputs the derivative expression");
        ImGui::BulletText("%s", "(Alternatively, use s diff(<var>)[<expr>])");
        ImGui::Unindent();
        ImGui::Unindent();

        ImGui::TextUnformatted("Expressions: Operators");
        ImGui::Indent();
        ImGui::BulletText("%s", "+- */% ^\nWhere ^ is exponentiation (right-assoc)");
        ImGui::BulletText("%s", "Logical and: &; logical or: |; logical not: not(x)");
        ImGui::BulletText("%s", "Parentheses: () and [] are equivalent (but match separately)");
        ImGui::BulletText("%s", "Comparison: < > <= >= == output 0,1\n(= equivalent to == except in assignment/equality statement)");
        ImGui::Unindent();

        ImGui::TextUnformatted("Expressions: Special Forms");
        ImGui::Indent();
        ImGui::BulletText("%s", "Conditional special form (piecewise function):\n"
                "{<predicate>: <expr>[, <elif-pred>:\n"
                " <expr>[, ... [, <else-expr>]]]}\n"
                "ex. {x<0: x, x>=0 : x^2}  ex. {x<2: exp(x)}");
        ImGui::BulletText("%s", "Sum special form:\n"
                "sum(<var> = <begin>, <end>)[<expr>]\n"
                "ex. sum(x=0, 100)[x]");
        ImGui::BulletText("%s", "Prod special form:\n"
                "prod(<var> = <begin>, <end>)[<expr>]\n"
                "ex. prod(x=0, 100)[1/x]");
        ImGui::BulletText("%s", "Diff (derivative) special form (evaluated at parse-time):\n"
                "diff(<var>)[<expr>]\n"
                "ex. diff(x)[1/x]");
        ImGui::BulletText("%s", "Higher-order derivative special forms:\n"
                "diff<ord>(<var>)[<expr>]\n"
                "ex. diff2(x)[gamma(x)]");
        ImGui::Unindent();

        ImGui::TextUnformatted("Expressions: Functions");
        ImGui::Indent();
        ImGui::BulletText("%s", "<func_name>(<args>) to call a function");
        ImGui::BulletText("%s", "Most function names are self-explanatory; some hints:\n"
                "N(x) is standard normal pdf\n"
                "rifact(x, b) is rising factorial\n"
                "falact(x, b) is falling factorial\n"
                "ifact(x) is integer version of factorial\n(fact(x) is gamma(x+1))\n"
                "Functions take a fixed number of arguments, except\n"
                "log(x,b) (log x base b) is special: can take 1 or 2 args,\nwhere log(x) = ln(x)");
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

    ImGui::SetNextWindowSize(ImVec2(std::min<int>(700, plot.view.swid),
                std::min<int>(500, plot.view.shigh)));
    if (ImGui::BeginPopupModal("Shell", &open_shell,
                ImGuiWindowFlags_NoResize)) {
        // Shell popup
        ImGui::BeginChild("Scrolling", ImVec2(0, ImGui::GetWindowHeight() - 70));
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
            shell_ss << ">> " << shell_curr_cmd << "\n";
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
        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 85.);
        if (ImGui::InputText("##ShellCommand",
                    &shell_curr_cmd,
                    ImGuiInputTextFlags_EnterReturnsTrue |
                    ImGuiInputTextFlags_CallbackHistory,
                    [](ImGuiTextEditCallbackData* data) -> int {
                        // Handle up/down arrow keys in textboxes
                        const size_t prev_history_pos = shell_hist_pos;
                        if (data->EventKey == ImGuiKey_UpArrow)
                        {
                            if (shell_hist_pos == (size_t)-1)
                                shell_hist_pos = shell_hist.size() - 1;
                            else if (shell_hist_pos)
                                shell_hist_pos--;
                        } else if (data->EventKey == ImGuiKey_DownArrow) {
                            if (~shell_hist_pos)
                            if (++shell_hist_pos >= shell_hist.size())
                                shell_hist_pos = (size_t)-1;
                        }
                        if (prev_history_pos != shell_hist_pos) {
                            data->DeleteChars(0, data->BufTextLen);
                            if (shell_hist_pos != (size_t)-1) {
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
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Submit")) exec_shell();
        ImGui::EndPopup();
    }
    // File dialogs; not available on web version
    open_file_dialog.Display();
    if(open_file_dialog.HasSelected())
    {
        std::string fname = open_file_dialog.GetSelected().string();
        open_file_dialog.ClearSelected();
        std::ifstream ifs(fname);
        std::string err;
        plot.import_json(ifs, &err);
        if (err.size()) {
            plot.func_error = "JSON import failed";
            std::cout << err << "\n";
        }
    }
    save_file_dialog.Display();
    if(save_file_dialog.HasSelected())
    {
        std::string fname = save_file_dialog.GetSelected().string();
        if (fname.size() > 5 && fname.substr(fname.size() - 5) != ".json") {
            fname.append(".json");
        }
        save_file_dialog.ClearSelected();
        std::ofstream ofs(fname);
        plot.export_json(ofs, true);
    }

    // * Handle IO events
    ImGuiIO &io = ImGui::GetIO();
    int mouse_x = static_cast<int>(io.MousePos[0]);
    int mouse_y = static_cast<int>(io.MousePos[1]);
    static int mouse_prev_x = 0, mouse_prev_y = 0;
    if (io.MouseReleased[0]) {
        std::lock_guard<std::mutex> lock(worker_mtx);
        plot.handle_mouse_up(mouse_x, mouse_y);
    }
    if (!io.WantCaptureMouse) {
        if (io.MouseDown[0]) {
            std::lock_guard<std::mutex> lock(worker_mtx);
            plot.handle_mouse_down(mouse_x, mouse_y);
        }
        if (mouse_x != mouse_prev_x || mouse_y != mouse_prev_y) {
            std::lock_guard<std::mutex> lock(worker_mtx);
            plot.handle_mouse_move(mouse_x, mouse_y);
        }
        if (io.MouseWheel) {
            std::lock_guard<std::mutex> lock(worker_mtx);
            plot.handle_mouse_wheel(
                    io.MouseWheel > 0,
                    static_cast<int>(std::fabs(io.MouseWheel) * 120),
                    mouse_x, mouse_y);
        }
        mouse_prev_x = mouse_x;
        mouse_prev_y = mouse_y;
    }

    if (!io.WantCaptureKeyboard || io.KeyCtrl) {
        for (size_t i = 0; i < IM_ARRAYSIZE(io.KeysDown); ++i) {
            if (ImGui::IsKeyDown((int)i)) {
                std::lock_guard<std::mutex> lock(worker_mtx);
                if ((i == 'Q' || i == 'W') && io.KeyCtrl) {
                    glfwSetWindowShouldClose(window, true);
                    return;
                }
                plot.handle_key((int)i,
                        io.KeyCtrl, io.KeyShift, io.KeyAlt);
            }
        }
    }

    // Render dear imgui into screen
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
}  // void main_loop_step()

}  // namespace

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
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Init glew context
    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return false;
    }
    glClearColor(1., 1., 1., 1.); // Clear white
    return true;
}

// Main method
int main(int argc, char ** argv) {
    using namespace nivalis;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) plot.add_func();
        // Load the expression
        plot.funcs[i - 1].expr_str = argv[i];
        plot.reparse_expr(i - 1);
    }
    if (!init_gl()) return 1; // GL initialization failed

    // Setup Dear ImGUI context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    font_sm = io.Fonts->AddFontFromMemoryCompressedTTF(ROBOTO_compressed_data,
            ROBOTO_compressed_size, 16.0f, NULL, GetGlyphRangesGreek());
    font_md = io.Fonts->AddFontFromMemoryCompressedTTF(ROBOTO_compressed_data,
            ROBOTO_compressed_size, 18.0f, NULL);

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    char* glsl_version = NULL;
    ImGui_ImplOpenGL3_Init(glsl_version);
    // Setup Dear ImGui style
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.;
    style.WindowBorderSize = 1.;
    style.FrameBorderSize = 0.;
    style.Colors[ImGuiCol_Border]         = ImVec4(0.8f, 0.8f, 0.8f, 1.f);
    style.Colors[ImGuiCol_BorderShadow]   = ImVec4(0.f, 0.f, 0.f, 0.f);
    style.Colors[ImGuiCol_TitleBg]        = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive]    = ImVec4(0.7f, 0.7f, 0.7f, 1.f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.75, 0.75f, 0.75f, 1.f);
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.99f, 0.99f, 0.99f, 1.f);
    style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.94f, 0.94f, 0.94f, 1.f);

    // Start worker thread
    std::thread thd(draw_worker, std::ref(plot));
    thd.detach();

    // Main GLFW loop (desktop)
    while (!glfwWindowShouldClose(window)) {
        main_loop_step();
    } // Main loop
    // Cleanup
    run_worker_flag = true;
    worker_quit_flag = true;
    worker_cv.notify_one();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
}
