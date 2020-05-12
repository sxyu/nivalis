#include "version.hpp"

#ifdef ENABLE_NIVALIS_OPENGL_IMGUI_BACKEND

#include "plotter/plot_gui.hpp"
#include <iostream>
#include <utility>
#include <thread>

#include "plotter/plotter.hpp"
#include "imgui.h"
#include "imstb_textedit.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "util.hpp"

namespace nivalis {
namespace {

struct OpenGLGraphicsAdaptor {
    // Drawing object
    struct DrawObj {
        enum Type {
            LINE, POLYLINE, RECT, RECT_FILL, STRING, CLEAR
        };
        DrawObj() =default;
        DrawObj(int type, float x, float y, color::color c) :
            type(type), x(x), y(y), c(c) {}

        int type;
        float x, y, w, h, t;
        std::vector<std::array<float, 2> > points;
        color::color c;
        std::string s;
    };

    OpenGLGraphicsAdaptor(GLFWwindow* window) : window(window)  {}
    void frame(ImDrawList* new_draw_list, bool reload_previous = false) {
        draw_list = new_draw_list;
        if (reload_previous) {
            for (auto& obj : objs) {
                switch(obj.type) {
                    case DrawObj::Type::LINE:
                        line(obj.x, obj.y, obj.w, obj.h, obj.c, obj.t, false);
                        break;
                    case DrawObj::Type::POLYLINE:
                        polyline(obj.points, obj.c, obj.t, false);
                        break;
                    case DrawObj::Type::RECT:
                    case DrawObj::Type::RECT_FILL:
                        rectangle(obj.x, obj.y, obj.w, obj.h,
                                obj.type == DrawObj::Type::RECT_FILL, obj.c, false);
                        break;
                    case DrawObj::Type::CLEAR:
                        clear(obj.c, false);
                        break;
                    case DrawObj::Type::STRING:
                        string(obj.x, obj.y, obj.s, obj.c, false);
                        break;
                }
            }
        } else {
            objs.clear();
        }
    }

    void line(float ax, float ay, float bx, float by, color::color c, float thickness = 1., bool upd_cache = true) {
        draw_list->AddLine(ImVec2(ax, ay), ImVec2(bx, by),
                ImColor(c.r, c.g, c.b), thickness);
        if (upd_cache) {
            DrawObj obj(DrawObj::Type::LINE, ax, ay, c);
            obj.w = bx; obj.h = by;
            obj.t = thickness;
            objs.push_back(obj);
        }
    }
    void polyline(const std::vector<std::array<float, 2> >& points, color::color c, float thickness = 1., bool upd_cache = true) {
        std::vector<ImVec2> line(points.size());
        for (size_t i = 0; i < line.size(); ++i) {
            line[i].x = (float) points[i][0];
            line[i].y = (float) points[i][1];
        }
        draw_list->AddPolyline(&line[0], (int)line.size(), ImColor(c.r, c.g, c.b), false, thickness);
        if (upd_cache) {
            DrawObj obj(DrawObj::Type::POLYLINE, 0., 0., c);
            obj.points = points;
            obj.t = thickness;
            objs.push_back(obj);
        }
    }
    void rectangle(float x, float y, float w, float h, bool fill, color::color c, bool upd_cache = true) {
        if (fill) {
            draw_list->AddRectFilled(ImVec2(x,y), ImVec2(x+w, y+h),
                    ImColor(c.r, c.g, c.b));
        }
        else {
            draw_list->AddRect(ImVec2(x,y), ImVec2(x+w, y+h),
                    ImColor(c.r, c.g, c.b));
        }
        if (upd_cache) {
            DrawObj obj(fill ? DrawObj::Type::RECT_FILL : DrawObj::Type::RECT, x, y, c);
            obj.w = w; obj.h = h;
            objs.push_back(obj);
        }
    }
    void clear(color::color c, bool upd_cache = true) {
        glClearColor(c.r/255., c.g/255., c.b/255., 1.0f);
        if (upd_cache) {
            DrawObj obj(DrawObj::Type::CLEAR, 0.f, 0.f, c);
            objs.push_back(obj);
        }
    }
    void string(float x, float y, const std::string& s, color::color c, bool upd_cache = true) {
        // String using ImGui API
        draw_list->AddText(ImVec2(x, y),
                ImColor(c.r, c.g, c.b), s.c_str());
        if (upd_cache) {
            DrawObj obj(DrawObj::Type::STRING, x, y, c);
            obj.s = s;
            objs.push_back(obj);
        }
    }

    // Cache
    std::vector<DrawObj> objs;

    GLFWwindow* window;
    ImDrawList* draw_list;
};

// Add scaled default font
ImFont* AddDefaultFont(float pixel_size) {
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig config;
    config.SizePixels = pixel_size;
    config.OversampleH = config.OversampleV = 1;
    config.PixelSnapH = true;
    ImFont *font = io.Fonts->AddFontDefault(&config);
    return font;
}

// Data for each slider
struct SliderData {
    static const int VAR_NAME_MAX = 256;
    char var_name_buf[VAR_NAME_MAX];
    std::string var_name;
    float var_val, lo = 0.0, hi = 1.0;
    uint32_t var_addr;
};

struct OpenGLPlotBackend {
    using GLPlotter = Plotter<OpenGLPlotBackend, OpenGLGraphicsAdaptor>;

    // Screen size
    static const int SCREEN_WIDTH = 1000, SCREEN_HEIGHT = 600;
    // Max buffer size
    static const int EDITOR_BUF_SZ = 2048;
    // Maximum functions supported
    static const int EDITOR_MAX_FUNCS = 128;

    OpenGLPlotBackend(Environment expr_env, const std::string& init_expr)
        :  plot(*this, expr_env, init_expr, SCREEN_WIDTH, SCREEN_HEIGHT) {
            /* Initialize the library */
            if (!glfwInit()) return;

            /* Create a windowed mode window and its OpenGL context */
            window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
                    "Nivalis Plotter", NULL, NULL);
            // Init glfw
            if (!window)
            {
                glfwTerminate();
                return;
            }
            glfwMakeContextCurrent(window);
            glfwSwapInterval(1);

            // Init glew context
            if (glewInit() != GLEW_OK)
            {
                fprintf(stderr, "Failed to initialize OpenGL loader!\n");
                return;
            }

            // Set up initial function color
            update_func_color(0);

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            imgui_io = &io;
            // Setup Platform/Renderer bindings
            ImGui_ImplGlfw_InitForOpenGL(window, true);
            char* glsl_version = NULL;
            ImGui_ImplOpenGL3_Init(glsl_version);
            // Setup Dear ImGui style
            ImGui::StyleColorsLight();

            // Set pointer to this to pass to callbacks
            glfwSetWindowUserPointer(window, this);
            // glfw io callbacks
            imgui_key_callback = glfwSetKeyCallback(
                        window, [](GLFWwindow* window,
                        int key, int scancode, int action, int mods){
                    OpenGLPlotBackend* be = reinterpret_cast<OpenGLPlotBackend*>(
                        glfwGetWindowUserPointer(window));
                    be->make_active();
                    ImGuiIO* io = be->imgui_io;
                    be->imgui_key_callback(window, key, scancode,
                            action, mods);
                    // Prevent ImGui events from coming here
                    GLPlotter& plot = be->plot;
                    if (io->WantCaptureKeyboard) {
                        return;
                    }
                    if (action == GLFW_REPEAT || action == GLFW_PRESS) {
                        plot.handle_key(key, mods & GLFW_MOD_CONTROL,
                                mods & GLFW_MOD_ALT);
                    }
            });

            glfwSetCursorPosCallback(window, [](GLFWwindow* window,
                        double xpos, double ypos) {
                    OpenGLPlotBackend* be = reinterpret_cast<OpenGLPlotBackend*>(
                        glfwGetWindowUserPointer(window));
                    ImGuiIO* io = be->imgui_io;
                    // Prevent ImGui events from coming here
                    if (io->WantCaptureMouse) return;
                    GLPlotter& plot = be->plot;
                    plot.handle_mouse_move(static_cast<int>(xpos), static_cast<int>(ypos));
            });

            imgui_mousebutton_callback =
                glfwSetMouseButtonCallback(window, [](GLFWwindow* window,
                        int button, int action, int mods){
                    OpenGLPlotBackend* be = reinterpret_cast<OpenGLPlotBackend*>(
                        glfwGetWindowUserPointer(window));
                    be->make_active();
                    ImGuiIO* io = be->imgui_io;
                    GLPlotter& plot = be->plot;
                    double xpos, ypos;
                    glfwGetCursorPos(window, &xpos, &ypos);
                    be->imgui_mousebutton_callback(window, button, action, mods);
                    int xposi = static_cast<int>(xpos);
                    int yposi = static_cast<int>(ypos);
                    if (action == GLFW_PRESS) {
                        // Prevent ImGui events from coming here
                        if (io->WantCaptureMouse) return;
                        plot.handle_mouse_down(xposi, yposi);
                    } else { //if (action == GLFW_RELEASE) {
                        plot.handle_mouse_up(xposi, yposi);
                    }
            });

            glfwSetCursorEnterCallback(window, [](GLFWwindow* window,
                        int entered) {
                if (!entered) {
                    OpenGLPlotBackend* be = reinterpret_cast<OpenGLPlotBackend*>(
                    glfwGetWindowUserPointer(window));
                    GLPlotter& plot = be->plot;
                    plot.handle_mouse_up(0, 0);
                }
            });

            imgui_scroll_callback =
                glfwSetScrollCallback(window, [](GLFWwindow* window,
                        double xoffset, double yoffset) {
                    OpenGLPlotBackend* be =
                        reinterpret_cast<OpenGLPlotBackend*>(
                            glfwGetWindowUserPointer(window));
                    be->make_active();
                    GLPlotter& plot = be->plot;
                    ImGuiIO* io = be->imgui_io;
                    be->imgui_scroll_callback(window, xoffset, yoffset);
                    if (io->WantCaptureMouse) return;

                    double xpos, ypos;
                    glfwGetCursorPos(window, &xpos, &ypos);
                    plot.handle_mouse_wheel(
                            yoffset > 0, static_cast<int>(std::fabs(yoffset) * 120),
                            static_cast<int>(xpos),
                            static_cast<int>(ypos));
            });
            ImFont *font_sm = AddDefaultFont(12);
            ImFont *font_md = AddDefaultFont(14);
            char frame_rate_buf[16];

            std::queue<double> fps_hist;
            double fps_sum = 0.0;

            double frame_time;
            bool init = true;
            OpenGLGraphicsAdaptor adaptor(window);

            // Main GLFW loop
            while (!glfwWindowShouldClose(window)) {
                frame_time = glfwGetTime();
                // Set to open popups
                bool open_color_picker = false,
                     open_reference = false;
                glfwPollEvents();
                // Clear
                glClear(GL_COLOR_BUFFER_BIT);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
                float ratio = width / (float) height;
                glOrtho(0, width, height, 0, 0, 1);

                // feed inputs to dear imgui, start new frame
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                // Handle resize
                int wwidth, wheight, pwwidth = -1, pwheight;
                glfwGetWindowSize(window, &wwidth, &wheight);
                if (wwidth != plot.swid || wheight != plot.shigh) {
                    pwwidth = plot.swid; pwheight = plot.shigh;
                    plot.resize(wwidth, wheight);
                }

                ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
                adaptor.frame(draw_list, !require_update);
                if (require_update) {
                    require_update = false;
                    plot.draw(adaptor);
                    make_active();
                } else if (active_counter > 0) {
                    --active_counter;
                } else {
                    // Sleep to throttle FPS
                    std::this_thread::sleep_for(std::chrono::microseconds(
                        1000000 / RESTING_FPS
                    ));
                }

                // Render GUI
                if (init) {
                    ImGui::SetNextWindowPos(ImVec2(10, 10));
                    ImGui::SetNextWindowSize(ImVec2(400, 130));
                }
                ImGui::Begin("Functions", NULL);
                ImGui::PushFont(font_md);

                for (size_t func_idx = 0; func_idx < plot.funcs.size();
                        ++func_idx) {
                    if (focus_idx == func_idx) {
                        focus_idx = -1;
                        ImGui::SetKeyboardFocusHere(0);
                    }
                    const std::string fid = std::to_string(func_idx);
                    ImGui::PushItemWidth(300.);
                    if (ImGui::InputText(("f" + fid).c_str(),
                            editor_strs[func_idx], EDITOR_BUF_SZ,
                            ImGuiInputTextFlags_CallbackHistory,
                            [](ImGuiTextEditCallbackData* data) -> int {
                                // Handle up/down arrow keys in textboxes
                                OpenGLPlotBackend* be = reinterpret_cast<OpenGLPlotBackend*>(
                                        data->UserData);
                                GLPlotter& plot = be->plot;
                                plot.set_curr_func(plot.curr_func +
                                        (data->EventKey == 3 ? -1 : 1));
                                be->update_func_color(plot.curr_func);
                                be->focus_idx = plot.curr_func;
                                return 0;
                        }, this)) {
                        plot.reparse_expr(func_idx);
                    }
                    if (ImGui::IsItemHovered() ||
                            ImGui::IsItemActive()) {
                        if (func_idx != plot.curr_func)
                            plot.set_curr_func(func_idx);
                    }
                    ImGui::SameLine();
                    auto* col = edit_colors[func_idx];
                    if (ImGui::ColorButton(("c##colfun" +fid).c_str(),
                                ImVec4(col[0], col[1], col[2], col[3]))) {
                        open_color_picker = true;
                        curr_edit_color_idx = func_idx;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(("x##delfun-" + fid).c_str())) {
                        for (size_t i = func_idx; i < plot.funcs.size()-1; ++i) {
                            strcpy(editor_strs[i], editor_strs[i+1]);
                            memcpy(edit_colors[i], edit_colors[i+1],
                                    4*sizeof(float));
                        }
                        plot.delete_func(func_idx);
                        --func_idx;
                    }
                }
                if (plot.funcs.size() <= EDITOR_MAX_FUNCS) {
                    std::string tmp = plot.funcs.back().expr_str;
                    util::trim(tmp);
                    if (tmp.size()) {
                        if (ImGui::Button("+ New function")) {
                            plot.set_curr_func(plot.funcs.size());
                            update_func_color(plot.funcs.size()-1);
                            focus_idx = plot.funcs.size() - 1;
                        }
                        ImGui::SameLine();
                    }
                }
                if (ImGui::Button("? Help")) {
                    open_reference = true;
                }
                ImGui::PushFont(font_sm);
                ImGui::TextColored(ImColor(255, 50, 50), "%s", error_text.c_str());
                ImGui::PushFont(font_md);
                ImGui::End(); //  Functions

                if (init) {
                    ImGui::SetNextWindowPos(ImVec2(10,
                            static_cast<float>(plot.shigh - 150)));
                    ImGui::SetNextWindowSize(ImVec2(333, 130));
                }
                if (ImGui::Begin("Sliders", NULL)) {
                    if (~pwwidth && !init) {
                        // Outer window was resized
                        ImVec2 pos = ImGui::GetWindowPos();
                        ImGui::SetWindowPos(ImVec2(pos.x,
                                    (float)wheight - ((float)pwheight - pos.y)));
                    }
                }

                for (size_t i = 0; i < sliders.size(); ++i) {
                    ImGui::PushItemWidth(50.);
                    auto& sl = sliders[i];
                    std::string slid = std::to_string(i);
                    if (ImGui::InputText(("=##vsl-" + slid).c_str(),
                                sl.var_name_buf,
                                SliderData::VAR_NAME_MAX)) {
                        std::string var_name = sl.var_name_buf;
                        util::trim(var_name);
                        if (sl.var_name != var_name) {
                            sliders_vars.erase(sl.var_name);
                            if (var_name == "") {
                                sl.var_addr = -1;
                                sl.var_name.clear();
                            } else if (sliders_vars.count(var_name)) {
                                // Already has slider
                                slider_error = "Duplicate slider for " +
                                    var_name  + "\n";
                                sl.var_addr = -1;
                                sl.var_name.clear();
                            } else if (var_name == "x" || var_name == "y") {
                                // Not allowed to set in slider (reserved)
                                slider_error = var_name  + " is reserved\n";
                                sl.var_addr = -1;
                                sl.var_name.clear();
                            } else {
                                sl.var_addr = plot.env.addr_of(var_name, false);
                                plot.env.vars[sl.var_addr] = sl.var_val;
                                sliders_vars.insert(var_name);
                                sl.var_name = var_name;
                                slider_error.clear();
                                for (size_t t = 0; t < plot.funcs.size(); ++t)
                                    plot.reparse_expr(t);
                                update();
                            }
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::InputFloat(("##vslinput-" + slid).c_str(),
                                &sl.var_val)) {
                        if (~sl.var_addr) {
                            plot.env.vars[sl.var_addr] = sl.var_val;
                            if (sl.var_val > sl.hi) sl.hi = sl.var_val;
                            if (sl.var_val < sl.lo) sl.lo = sl.var_val;
                            update();
                            make_active();
                        }
                    }

                    ImGui::SameLine(0., 10.0);
                    ImGui::InputFloat(("min##vsl-"+slid).c_str(), &sl.lo);
                    ImGui::SameLine();
                    ImGui::InputFloat(("max##vsl-"+slid).c_str(), &sl.hi);

                    ImGui::SameLine();
                    if (ImGui::Button(("x##delvsl-" + slid).c_str())) {
                        sliders_vars.erase(sl.var_name);
                        sliders.erase(sliders.begin() + i);
                        --i;
                        continue;
                    }

                    ImGui::PushItemWidth(318.);
                    if (ImGui::SliderFloat(("##vslslider" + slid).c_str(),
                                &sl.var_val, sl.lo, sl.hi)) {
                        if (~sl.var_addr) {
                            plot.env.vars[sl.var_addr] = sl.var_val;
                            update();
                            make_active();
                        }
                    }
                }

                if (ImGui::Button("+ New slider")) {
                    sliders.emplace_back();
                    auto& sl = sliders.back();
                    std::string var_name = "a";
                    while (var_name[0] < 'z' &&
                           sliders_vars.count(var_name)) {
                        // Try to find next unused var name
                        ++var_name[0];
                    }
                    sl.var_name = var_name;
                    strncpy(sl.var_name_buf, var_name.c_str(), var_name.size()+1);
                    sl.var_addr = plot.env.addr_of(sl.var_name_buf, false);
                    plot.env.vars[sl.var_addr] = sl.lo;
                    for (size_t t = 0; t < plot.funcs.size(); ++t)
                        plot.reparse_expr(t);
                    sliders_vars.insert(var_name);
                }
                ImGui::PushFont(font_sm);
                ImGui::TextColored(ImColor(255, 50, 50), "%s",
                        slider_error.c_str());
                ImGui::PushFont(font_md);

                ImGui::End(); // Sliders

                if (init) {
                    ImGui::SetNextWindowPos(ImVec2(
                                static_cast<float>(plot.swid - 182), 10));
                    ImGui::SetNextWindowSize(ImVec2(175, 105));
                }

                if (ImGui::Begin("View", NULL, ImGuiWindowFlags_NoResize)) {
                    if (~pwwidth && !init) {
                        // Outer window was resized
                        ImVec2 pos = ImGui::GetWindowPos();
                        ImGui::SetWindowPos(ImVec2(
                                    wwidth - ((float) pwwidth - pos.x),
                                    pos.y));
                    }
                }
                ImGui::PushItemWidth(60.);
                ImGui::InputDouble("<x<", &plot.xmin); ImGui::SameLine();
                ImGui::PushItemWidth(60.);
                ImGui::InputDouble("##xmax", &plot.xmax);
                ImGui::PushItemWidth(60.);
                ImGui::InputDouble("<y<", &plot.ymin); ImGui::SameLine();
                ImGui::PushItemWidth(60.);
                ImGui::InputDouble("##ymax", &plot.ymax);
                if (ImGui::Button("Reset view")) plot.reset_view();
                ImGui::End(); // View

                if (marker_text.size()) {
                    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(marker_posx),
                                static_cast<float>(marker_posy)));
                    ImGui::SetNextWindowSize(ImVec2(250, 50));
                    ImGui::Begin("Marker", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoFocusOnAppearing);
                    ImGui::TextUnformatted(marker_text.c_str());
                    ImGui::End();
                }
                if(open_color_picker) ImGui::OpenPopup("Color picker");
                if(open_reference) ImGui::OpenPopup("Reference");
                if (ImGui::BeginPopupModal("Color picker", NULL,
                            ImGuiWindowFlags_AlwaysAutoResize)) {
                    // Color picker dialog
                    auto* sel_col = edit_colors[curr_edit_color_idx];
                    ImGui::ColorPicker3("color", sel_col);
                    if (ImGui::Button("Ok##cpickok", ImVec2(100.f, 0.0f))) {
                        // Update color when ok pressed
                        auto& fcol = plot.funcs[curr_edit_color_idx].line_color;
                        fcol.r = static_cast<uint8_t>(sel_col[0] * 255.);
                        fcol.g = static_cast<uint8_t>(sel_col[1] * 255.);
                        fcol.b = static_cast<uint8_t>(sel_col[2] * 255.);
                        ImGui::CloseCurrentPopup();
                        update();
                    }
                    ImGui::EndPopup();
                }
                ImGui::SetNextWindowSize(ImVec2(600, 400));
                if (ImGui::BeginPopupModal("Reference", NULL,
                            ImGuiWindowFlags_NoResize)) {
                    // Reference popup
                    if (ImGui::Button("Close##refclose", ImVec2(100.f, 0.0f))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::TextUnformatted("GUI: Function editor");
                    ImGui::Indent();
                        ImGui::BulletText("%s", "The function editor is the window initially on top-left with\ntextboxes. You can enter expressions here to draw:");
                        ImGui::Indent();
                            ImGui::BulletText("%s", "Explicit functions: Simply enter an expression with x in the textbox\ne.g. x^2");
                            ImGui::BulletText("%s", "Implicit functions: Enter an equation with x, y in the textbox, e.g. cos(x*y)=0");
                            ImGui::BulletText("%s", "'Polylines' (points and lines)");
                            ImGui::Indent();
                                ImGui::BulletText("%s", "To draw a single point, write (<x-coord>,<y-coord>) e.g. (1, 2).\nCoords can have variables.");
                                ImGui::BulletText("%s", "To draw a series of points connected in order, write\n(<x1>,<y1>)(<x2>,<y2>)... e.g. (1, 1)(2,2)(3,2)");
                            ImGui::Unindent();
                        ImGui::Unindent();
                        ImGui::BulletText("%s", "Press '+ New function' to add more functions.\nPress 'x' to delete a function.\nPress the colored button to the left to change line color.");
                    ImGui::Unindent();

                    ImGui::TextUnformatted("GUI: Keyboard shortcuts");
                    ImGui::Indent();
                        ImGui::BulletText("On background\n");
                        ImGui::Indent();
                            ImGui::BulletText("%s", "Ctrl + E to focus on functions editor");
                            ImGui::BulletText("%s", "-= keys to zoom, arrow keys to move");
                            ImGui::BulletText("%s", "Ctrl/Alt + -= keys to zoom asymmetrically");
                            ImGui::BulletText("%s", "q to close window\n");
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
                        std::string fnmane = pr.first;
                        if (pr.second == -1 ||
                                ! OpCode::is_binary(pr.second)) {
                            ImGui::BulletText("%s(_)", fnmane.c_str());
                        } else {
                            ImGui::BulletText("%s(_, _)", fnmane.c_str());
                        }
                    }

                    ImGui::Unindent();
                    ImGui::Unindent();
                    ImGui::EndPopup();
                }

                // Show the FPS in background
                double fps = 1. / (glfwGetTime() - frame_time);
                fps_hist.push(fps);
                fps_sum += fps;
                std::snprintf(frame_rate_buf, (sizeof frame_rate_buf) /
                        (sizeof frame_rate_buf[0]), "%.1f FPS", fps_sum / fps_hist.size());
                if (fps_hist.size() >= FPS_AVG_SIZE) {
                    fps_sum -= fps_hist.front();
                    fps_hist.pop();
                }
                draw_list->AddText(ImVec2(10, plot.shigh - 20), ImColor(0,0,0), frame_rate_buf);

                // Render dear imgui into screen
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                glfwSwapBuffers(window);
                glfwPollEvents();
                init = false;
            }

            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
    }

    // Set focus
    void focus_editor() {
        focus_idx = plot.curr_func;
    }
    void focus_background() {
        ImGui::SetWindowFocus();
    }

    // Close window
    void close() { glfwSetWindowShouldClose(window, GL_TRUE); }

    // Require the plot to be updated
    void update(bool force = false) {
        require_update = true;
    }

    // Update editor (tb)
    // Assumes func_id is curr_func !
    void update_editor(size_t func_id, const std::string& contents) {
        strncpy(editor_strs[func_id], contents.c_str(), contents.size()+1);
    }

    // Get contents of editor (tb)
    // Assumes func_id is curr_func !
    const char * read_editor(size_t func_id) {
        return editor_strs[func_id];
    }

    // Set error label
    void show_error(const std::string& txt) {
        error_text = txt;
    }

    // Set func name label (not used)
    void set_func_name(const std::string& txt) { }

    // Show marker at position
    void show_marker_at(const PointMarker& ptm, int px, int py) {
        marker_posx = px; marker_posy = py + 20;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(4) <<
            PointMarker::label_repr(ptm.label) << ptm.x << ", " << ptm.y;
        marker_text = ss.str();
        make_active();
    }

    // Hide marker (label)
    void hide_marker() {
        marker_text.clear();
    }

    // Update color of function in function window
    // to match color in plotter
    void update_func_color(size_t func_id) {
        auto* col = edit_colors[func_id];
        auto& fcol = plot.funcs[func_id].line_color;
        col[0] = fcol.r / 255.0f;
        col[1] = fcol.g / 255.0f;
        col[2] = fcol.b / 255.0f;
        col[3] = 1.;
    }

    void make_active() {
        active_counter = ACTIVE_FRAMES;
    }

private:
    GLFWwindow* window;

    GLFWkeyfun imgui_key_callback;
    GLFWmousebuttonfun imgui_mousebutton_callback;
    GLFWscrollfun imgui_scroll_callback;

    ImGuiIO* imgui_io;

    // Error text, marker data
    std::string error_text, marker_text;
    int marker_posx, marker_posy;

    // Color picker data
    float edit_colors[EDITOR_MAX_FUNCS][4];
    size_t curr_edit_color_idx;

    // Editor data
    char editor_strs[EDITOR_MAX_FUNCS][EDITOR_BUF_SZ];

    // Slider data
    std::string slider_error;
    std::vector<SliderData> sliders;
    std::set<std::string> sliders_vars;

    // Set to focus an editor
    size_t focus_idx = 0;

    // Require update?
    bool require_update = false;
    // Active counter: if > 0 updates at full FPS
    int active_counter = 0;

    // FPS restriction when not moving (to reduce CPU usage)
    const int RESTING_FPS = 12;
    // Frames to stay active after update
    const int ACTIVE_FRAMES = 60;
    // Number of frames to average
    const size_t FPS_AVG_SIZE = 5;

    // Templated plotter instance, contains plotter logic
    GLPlotter plot;
};
}  // namespace

PlotGUI::PlotGUI(Environment& env, const std::string& init_expr) {
    OpenGLPlotBackend openglgui(env, init_expr);
}
}  // namespace nivalis
#endif // ifdef ENABLE_NIVALIS_OPENGL_IMGUI_BACKEND
