#include "version.hpp"

#ifdef ENABLE_NIVALIS_OPENGL_IMGUI_BACKEND

#include "plotter/plot_gui.hpp"
#include <iostream>

#include "expr.hpp"
#include "parser.hpp"
#include "util.hpp"

#include "plotter/plotter.hpp"
#include "imgui.h"
#include "imstb_textedit.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace nivalis {
namespace {

struct OpenGLGraphicsAdaptor {
    void line(int ax, int ay, int bx, int by, color::color c) {
        glBegin(GL_LINES);
        glColor3ub(c.r, c.g, c.b);
        glVertex2f(ax, ay);
        glVertex2f(bx, by);
        glEnd();
    }
    void rectangle(int x, int y, int w, int h, bool fill, color::color c) {
        if (fill) {
            glColor3ub(c.r, c.g, c.b);
            glRecti(x, y, x+w, y+h);
        }
        else {
            glBegin(GL_LINE_LOOP);
            glColor3ub(c.r, c.g, c.b);
            glVertex2f(x, y);
            glVertex2f(x, y + h);
            glVertex2f(x + w, y + h);
            glVertex2f(x + w, y);
            glEnd();
        }
    }
    void rectangle(bool fill, color::color c) {
        glClearColor(c.r/255., c.g/255., c.b/255., 1.0f);
    }
    void set_pixel(int x, int y, color::color c) {
        glBegin(GL_POINTS);
        glColor3ub(c.r, c.g, c.b);
        glVertex2i(x, y);
        glEnd();
    }
    void string(int x, int y, const std::string& s, color::color c) {
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

struct OpenGLPlotBackend {
    using GLPlotter = Plotter<OpenGLPlotBackend, OpenGLGraphicsAdaptor>;

    static const int SCREEN_WIDTH = 1000, SCREEN_HEIGHT = 600;
    OpenGLPlotBackend(Environment expr_env, const std::string& init_expr)
        : plot(*this, expr_env, init_expr, SCREEN_WIDTH, SCREEN_HEIGHT) {
            /* Initialize the library */
            if (!glfwInit()) return;

            /* Create a windowed mode window and its OpenGL context */
            window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
                    "Nivalis Plotter", NULL, NULL);
            if (!window)
            {
                glfwTerminate();
                return;
            }

            /* Make the window's context current */
            glfwMakeContextCurrent(window);
            glfwSwapInterval(1); // Enable vsync

            if (glewInit() != GLEW_OK)
            {
                fprintf(stderr, "Failed to initialize OpenGL loader!\n");
                return;
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
                        if (key == 261 && (mods & GLFW_MOD_CONTROL))
                            plot.delete_func();
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
                    plot.handle_mouse_move(xpos, ypos);
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
                    if (action == GLFW_PRESS) {
                        // Prevent ImGui events from coming here
                        if (io->WantCaptureMouse) return;
                        plot.handle_mouse_down(xpos, ypos);
                    } else { //if (action == GLFW_RELEASE) {
                        plot.handle_mouse_up(xpos, ypos);
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
                            yoffset > 0, std::abs(yoffset) * 120,
                            xpos, ypos);
            });
            ImFont *font_sm = AddDefaultFont(12);
            ImFont *font_md = AddDefaultFont(14);

            while (!glfwWindowShouldClose(window))
            {
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

                ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
                OpenGLGraphicsAdaptor adaptor(window, draw_list);
                plot.draw(adaptor);

                // render GUI
                ImGui::SetNextWindowSize(ImVec2(220, 100));
                ImGui::Begin("Options", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                ImGui::PushFont(font_md);

                ImGui::PushItemWidth(200.);
                ImGui::TextUnformatted(func_name.c_str());
                if (focus_on_text_input) {
                    focus_on_text_input = false;
                    ImGui::SetKeyboardFocusHere(-1);
                }
                ImGui::InputText("",
                        editor_str, IM_ARRAYSIZE(editor_str),
                        ImGuiInputTextFlags_CallbackHistory |
                        ImGuiInputTextFlags_CallbackAlways,
                        [](ImGuiTextEditCallbackData* data) -> int {
                            OpenGLPlotBackend* be = reinterpret_cast<OpenGLPlotBackend*>(
                                    data->UserData);
                            GLPlotter& plot = be->plot;
                            if (data->EventFlag == 
                                   ImGuiInputTextFlags_CallbackHistory) {
                                plot.set_curr_func(plot.curr_func +
                                        (data->EventKey == 3 ? -1 : 1));
                                data->CursorPos = data->SelectionStart =
                                    data->SelectionEnd = data->BufTextLen =
                                    (int)snprintf(data->Buf, (size_t)data->BufSize, "%s",
                                            be->editor_str_back);
                                    data->BufDirty = true;
                            } else {
                                if (strcmp(be->editor_str_back, be->editor_str)) {
                                    plot.reparse_expr();
                                    strcpy(be->editor_str_back, be->editor_str);
                                }
                            }
                            return 0;
                        }, this);
                // Control
                if (ImGui::Button("Reset view")) plot.reset_view();
                ImGui::SameLine();
                if (ImGui::Button("<")) plot.set_curr_func(plot.curr_func - 1);
                ImGui::SameLine();
                if (ImGui::Button(">")) plot.set_curr_func(plot.curr_func + 1);
                ImGui::SameLine();
                if (ImGui::Button("x")) plot.delete_func();
                ImGui::PushFont(font_sm);
                ImGui::TextColored(ImColor(255, 50, 50), "%s", error_text.c_str());
                ImGui::PushFont(font_md);

                ImGui::End();

                if (marker_text.size()) {
                    ImGui::SetNextWindowPos(ImVec2(marker_posx, marker_posy));
                    ImGui::SetNextWindowSize(ImVec2(250, 50));
                    ImGui::Begin("Marker", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar );
                    ImGui::TextUnformatted(marker_text.c_str());
                    ImGui::End();
                }

                // Render dear imgui into screen
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                glfwSwapBuffers(window);
                glfwPollEvents();
            }
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
    }

    // Set focus
    void focus_editor() { // TODO
        focus_on_text_input = true;
    }
    void focus_background() { // TODO
        ImGui::SetWindowFocus();
    }

    // Close window
    void close() { glfwSetWindowShouldClose(window, GL_TRUE); }

    // Update the view (don't need to do anything of OpenGL)
    void update(bool force = false) {}

    // Update editor (tb)
    // Assumes func_id is curr_func !
    void update_editor(int func_id, const std::string& contents) {
        strncpy(editor_str_back, contents.c_str(), contents.size() + 1);
        strncpy(editor_str, contents.c_str(), contents.size() + 1);
    }

    // Get contents of editor (tb)
    // Assumes func_id is curr_func !
    const char* read_editor(int func_id) {
        return editor_str;
    }

    // Set error label
    void show_error(const std::string& txt) {
        error_text = txt;
    }

    // Set func name label
    void set_func_name(const std::string& txt) {
        func_name = txt;
    }

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

    char editor_str[256], editor_str_back[256];
    std::string func_name, error_text, marker_text;
    int marker_posx, marker_posy;
    bool focus_on_text_input = true;

private:
    // Templated plotter instance, contains plotter logic
    Environment env;
    Parser parser;

    GLFWwindow* window;

    GLFWkeyfun imgui_key_callback;
    GLFWmousebuttonfun imgui_mousebutton_callback;
    GLFWscrollfun imgui_scroll_callback;

    ImGuiIO* imgui_io;

    GLPlotter plot;
};
}  // namespace

PlotGUI::PlotGUI(Environment& env, const std::string& init_expr) {
    OpenGLPlotBackend openglgui(env, init_expr);
}
}  // namespace nivalis
#endif // ifdef ENABLE_NIVALIS_OPENGL_IMGUI_BACKEND
