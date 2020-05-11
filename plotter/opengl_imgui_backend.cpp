#include "version.hpp"

#ifdef ENABLE_NIVALIS_OPENGL_IMGUI_BACKEND

#include "plotter/plot_gui.hpp"
#include <iostream>

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
    void line(float ax, float ay, float bx, float by, color::color c, float thickness = 1.) {
        draw_list->AddLine(ImVec2(ax, ay), ImVec2(bx, by),
                ImColor(c.r, c.g, c.b), thickness);
    }
    void polyline(const std::vector<std::array<float, 2> >& points, color::color c, float thickness = 1.) {
        std::vector<ImVec2> line(points.size());
        for (size_t i = 0; i < line.size(); ++i) {
            line[i].x = (float) points[i][0];
            line[i].y = (float) points[i][1];
        }
        draw_list->AddPolyline(&line[0], line.size(), ImColor(c.r, c.g, c.b), false, thickness);
    }
    void rectangle(float x, float y, float w, float h, bool fill, color::color c) {
        if (fill) {
            draw_list->AddRectFilled(ImVec2(x,y), ImVec2(x+w, y+h),
                    ImColor(c.r, c.g, c.b));
        }
        else {
            draw_list->AddRect(ImVec2(x,y), ImVec2(x+w, y+h),
                    ImColor(c.r, c.g, c.b));
        }
    }
    void clear(color::color c) {
        glClearColor(c.r/255., c.g/255., c.b/255., 1.0f);
    }
    void set_pixel(float x, float y, color::color c) {
        draw_list->AddRect(ImVec2(x,y), ImVec2(x, y),
                ImColor(c.r, c.g, c.b));
    }
    void string(float x, float y, const std::string& s, color::color c) {
        // String using ImGui API
        draw_list->AddText(ImVec2(x, y),
                ImColor(c.r, c.g, c.b), s.c_str());
    }
    OpenGLGraphicsAdaptor(GLFWwindow* window, ImDrawList* draw_list)
        : window(window), draw_list(draw_list)  {}
    GLFWwindow* window;
    ImDrawList* draw_list;
};

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
            glfwSwapInterval(1); // Enable vsync

            // Init glew context
            if (glewInit() != GLEW_OK)
            {
                fprintf(stderr, "Failed to initialize OpenGL loader!\n");
                return;
            }

            // Set up initial function color
            {
                auto* col = edit_colors[0];
                auto& fcol = plot.funcs[0].line_color;
                col[0] = fcol.r / 255.0f;
                col[1] = fcol.g / 255.0f;
                col[2] = fcol.b / 255.0f;
                col[3] = 1.;
            }

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

            bool init = true;

            while (!glfwWindowShouldClose(window))
            {
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
                int wwidth, wheight;
                glfwGetWindowSize(window, &wwidth, &wheight);
                if (wwidth != plot.swid || wheight != plot.shigh) {
                    plot.resize(wwidth, wheight);
                }

                ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
                OpenGLGraphicsAdaptor adaptor(window, draw_list);
                plot.draw(adaptor);

                // Render GUI
                if (init) {
                    ImGui::SetNextWindowPos(ImVec2(20, 30));
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
                            OpenGLPlotBackend* be = reinterpret_cast<OpenGLPlotBackend*>(
                                    data->UserData);
                            GLPlotter& plot = be->plot;
                            plot.set_curr_func(plot.curr_func +
                                    (data->EventKey == 3 ? -1 : 1));
                            be->focus_idx = plot.curr_func;
                            return 0;
                        }, this)) {
                        plot.reparse_expr(func_idx);
                    }
                    if (ImGui::IsItemHovered() ||
                            ImGui::IsItemActive()) {
                        plot.set_curr_func(func_idx);
                    }
                    ImGui::SameLine();
                    // ImGui::PushID(("cp" + fid).c_str());
                    auto* col = edit_colors[func_idx];
                    if (ImGui::ColorButton("c",
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
                            auto* col = edit_colors[plot.funcs.size()-1];
                            auto& fcol = plot.funcs.back().line_color;
                            col[0] = fcol.r / 255.0f;
                            col[1] = fcol.g / 255.0f;
                            col[2] = fcol.b / 255.0f;
                            col[3] = 1.;
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
                    ImGui::SetNextWindowPos(ImVec2(20,
                            static_cast<float>(SCREEN_HEIGHT - 220)));
                    ImGui::SetNextWindowSize(ImVec2(290, 200));
                }
                ImGui::Begin("Sliders", NULL);

                for (size_t i = 0; i < sliders.size(); ++i) {
                    ImGui::PushItemWidth(50.);
                    auto& sl = sliders[i];
                    std::string slid = std::to_string(i);
                    if (ImGui::InputText(("var##vsl-" + slid).c_str(),
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
                            }
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

                    ImGui::PushItemWidth(275.);
                    if (ImGui::SliderFloat(("##vslslider" + slid).c_str(),
                                &sl.var_val, sl.lo, sl.hi)) {
                        if (~sl.var_addr) {
                            plot.env.vars[sl.var_addr] = sl.var_val;
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
                                static_cast<float>(SCREEN_WIDTH - 275), 30));
                    ImGui::SetNextWindowSize(ImVec2(250, 105));
                }

                ImGui::Begin("View", NULL, ImGuiWindowFlags_NoResize);
                ImGui::PushItemWidth(80.);
                ImGui::InputDouble("xmin", &plot.xmin); ImGui::SameLine();
                ImGui::PushItemWidth(80.);
                ImGui::InputDouble("xmax", &plot.xmax);
                ImGui::PushItemWidth(80.);
                ImGui::InputDouble("ymin", &plot.ymin); ImGui::SameLine();
                ImGui::PushItemWidth(80.);
                ImGui::InputDouble("ymax", &plot.ymax);
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
                    auto* sel_col = edit_colors[curr_edit_color_idx];
                    ImGui::ColorPicker3("color", sel_col);
                    if (ImGui::Button("Ok##cpickok", ImVec2(100.f, 0.0f))) {
                        auto& fcol = plot.funcs[curr_edit_color_idx].line_color;
                        fcol.r = static_cast<uint8_t>(sel_col[0] * 255.);
                        fcol.g = static_cast<uint8_t>(sel_col[1] * 255.);
                        fcol.b = static_cast<uint8_t>(sel_col[2] * 255.);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::SetNextWindowSize(ImVec2(600, 400));
                if (ImGui::BeginPopupModal("Reference", NULL,
                            ImGuiWindowFlags_NoResize)) {
                    if (ImGui::Button("Close##refclose", ImVec2(100.f, 0.0f))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::TextUnformatted("Operators");
                    ImGui::Indent();
                    ImGui::BulletText("%s", "+- */% ^\nWhere ^ is exponentiation (right-assoc)");
                    ImGui::BulletText("%s", "Parentheses: () and [] are equivalent (but match separately)");
                    ImGui::BulletText("%s", "Comparison: < > <= >= == output 0,1\n(= equivalent to == except in assignment/equality statement)");
                    ImGui::Unindent();

                    ImGui::TextUnformatted("Special Forms");
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

                    ImGui::TextUnformatted("Functions");
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

    // Update the view (don't need to do anything of OpenGL)
    void update(bool force = false) {}

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
        marker_text = PointMarker::label_repr(ptm.label) +
            std::to_string(ptm.x) + ", " + std::to_string(ptm.y);
    }

    // Hide marker (label)
    void hide_marker() {
        marker_text.clear();
    }

    char editor_strs[EDITOR_MAX_FUNCS][EDITOR_BUF_SZ];
    size_t focus_idx = 0;

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

    // Slider data
    std::string slider_error;
    std::vector<SliderData> sliders;
    std::set<std::string> sliders_vars;

    // Templated plotter instance, contains plotter logic
    GLPlotter plot;
};
}  // namespace

PlotGUI::PlotGUI(Environment& env, const std::string& init_expr) {
    OpenGLPlotBackend openglgui(env, init_expr);
}
}  // namespace nivalis
#endif // ifdef ENABLE_NIVALIS_OPENGL_IMGUI_BACKEND
