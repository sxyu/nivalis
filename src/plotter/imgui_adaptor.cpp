#include "plotter/imgui_adaptor.hpp"

#include "version.hpp"

#include <algorithm>
#include <cmath>
// #include <iostream>

#ifdef NIVALIS_EMSCRIPTEN
#include <GLES3/gl3.h>
#else
#include <GL/glew.h>
#endif

#include "earcut.hpp"
using ECCoord = float;
using ECInt = uint32_t;
using ECPoint = std::array<ECCoord, 2>;

namespace nivalis {
void ImGuiDrawListGraphicsAdaptor::line(float ax, float ay, float bx, float by,
                                        const color::color& c,
                                        float thickness) {
    draw_list->AddLine(ImVec2(ax, ay), ImVec2(bx, by),
                       ImColor(c.r, c.g, c.b, c.a), thickness);
}
void ImGuiDrawListGraphicsAdaptor::polyline(const std::vector<point>& points,
                                            const color::color& c,
                                            float thickness, bool closed,
                                            bool fill) {
    if (fill) {
        std::vector<std::vector<ECPoint>> poly(1);
        for (auto& p : points) poly[0].push_back({p[0], p[1]});
        std::vector<ECInt> indices = mapbox::earcut<ECInt>(poly);
        for (size_t t = 0; t < indices.size(); t += 3) {
            const auto i = indices[t], j = indices[t + 1], k = indices[t + 2];
            draw_list->AddTriangleFilled(ImVec2(poly[0][i][0], poly[0][i][1]),
                                         ImVec2(poly[0][j][0], poly[0][j][1]),
                                         ImVec2(poly[0][k][0], poly[0][k][1]),
                                         ImColor(c.r, c.g, c.b, c.a));
        }
    } else {
        draw_list->AddPolyline<point>(&points[0], points.size(),
                                      ImColor(c.r, c.g, c.b, c.a), closed,
                                      thickness);
    }
}
void ImGuiDrawListGraphicsAdaptor::rectangle(float x, float y, float w, float h,
                                             bool fill, const color::color& c,
                                             float thickness) {
    if (fill) {
        draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                                 ImColor(c.r, c.g, c.b, c.a));
    } else {
        draw_list->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                           ImColor(c.r, c.g, c.b, c.a), 0.0f,
                           ImDrawCornerFlags_All, thickness);
    }
}
void ImGuiDrawListGraphicsAdaptor::triangle(float x1, float y1, float x2,
                                            float y2, float x3, float y3,
                                            bool fill, const color::color& c) {
    if (fill) {
        draw_list->AddTriangleFilled(ImVec2(x1, y1), ImVec2(x2, y2),
                                     ImVec2(x3, y3),
                                     ImColor(c.r, c.g, c.b, c.a));
    } else {
        draw_list->AddTriangle(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3),
                               ImColor(c.r, c.g, c.b, c.a));
    }
}
void ImGuiDrawListGraphicsAdaptor::circle(float x, float y, float r, bool fill,
                                          const color::color& c) {
    if (fill) {
        draw_list->AddCircleFilled(ImVec2(x, y), r, ImColor(c.r, c.g, c.b, c.a),
                                   std::min(r, 250.f));
    } else {
        draw_list->AddCircle(ImVec2(x, y), r, ImColor(c.r, c.g, c.b, c.a),
                             std::min(r, 250.f));
    }
}
void ImGuiDrawListGraphicsAdaptor::ellipse(float x, float y, float rx, float ry,
                                           bool fill, const color::color& c) {
    if (fill) {
        draw_list->AddEllipseFilled(ImVec2(x, y), rx, ry,
                                    ImColor(c.r, c.g, c.b, c.a),
                                    std::min(.5f * (rx + ry), 250.f));
    } else {
        draw_list->AddEllipse(ImVec2(x, y), rx, ry, ImColor(c.r, c.g, c.b, c.a),
                              std::min(.5f * (rx + ry), 250.f));
    }
}
void ImGuiDrawListGraphicsAdaptor::string(float x, float y,
                                          const std::string& s,
                                          const color::color& c, float align_x,
                                          float align_y) {
    // String using ImGui API
    if (align_x > 0. || align_y > 0.) {
        ImVec2 sz = ImGui::CalcTextSize(s.c_str());
        draw_list->AddText(ImVec2(x - align_x * sz.x, y - align_y * sz.y),
                           ImColor(c.r, c.g, c.b, c.a), s.c_str());
    } else {
        draw_list->AddText(ImVec2(x, y), ImColor(c.r, c.g, c.b, c.a),
                           s.c_str());
    }
}

void ImGuiDrawListGraphicsAdaptor::image(float x, float y, float w, float h,
                                         char* data_rgb, int cols, int rows) {
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cols, rows, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data_rgb);
    draw_list->AddImage((void*)(intptr_t)image_texture, ImVec2(x, y),
                        ImVec2(x + w, y + h));
}

const ImWchar* GetGlyphRangesGreek() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin Supplement
        0x0370, 0x03FF,  // Greek/Coptic
        0,
    };
    return &ranges[0];
}
}  // namespace nivalis
