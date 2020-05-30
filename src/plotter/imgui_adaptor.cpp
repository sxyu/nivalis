#include <algorithm>
#include <cmath>
#include <iostream>
#include "plotter/imgui_adaptor.hpp"

namespace nivalis {
void ImGuiDrawListGraphicsAdaptor::line(float ax, float ay, float bx, float by,
        const color::color& c, float thickness) {
    draw_list->AddLine(ImVec2(ax, ay), ImVec2(bx, by),
            ImColor(c.r, c.g, c.b, c.a), thickness);
}
void ImGuiDrawListGraphicsAdaptor::polyline(const std::vector<std::array<float, 2> >& points,
        const color::color& c, float thickness, bool closed) {
    std::vector<ImVec2> line(points.size());
    size_t j = 0;
    for (size_t i = 0; i < points.size(); ++i, ++j) {
        line[j] = ImVec2(points[i][0], points[i][1]);
        if (j >= 2) {
            double ax = (line[j].x - line[j-1].x);
            double ay = (line[j].y - line[j-1].y);
            double bx = (line[j-1].x - line[j-2].x);
            double by = (line[j-1].y - line[j-2].y);
            double theta_a = std::fmod(std::atan2(ay, ax) + 2*M_PI, 2*M_PI);
            double theta_b = std::fmod(std::atan2(by, bx) + 2*M_PI, 2*M_PI);
            double angle_between = std::min(std::fabs(theta_a - theta_b),
                    std::fabs(theta_a + 2*M_PI - theta_b));
            if (std::fabs(angle_between - M_PI) < 1e-1) {
                // Near-colinear, currently ImGui's polyline drawing may will break
                // in this case. We split the line here.
                draw_list->AddPolyline(&line[0], (int)j, ImColor(c.r, c.g, c.b, c.a), closed, thickness);
                line[0] = line[j-1]; line[1] = line[j];
                j = 1;
            }
        }
    }
    line.resize(j);
    draw_list->AddPolyline(&line[0], (int)j, ImColor(c.r, c.g, c.b, c.a), closed, thickness);
}
void ImGuiDrawListGraphicsAdaptor::rectangle(float x, float y, float w, float h, bool fill, const color::color& c) {
    if (fill) {
        draw_list->AddRectFilled(ImVec2(x,y), ImVec2(x+w, y+h),
                ImColor(c.r, c.g, c.b, c.a));
    } else {
        draw_list->AddRect(ImVec2(x,y), ImVec2(x+w, y+h),
                ImColor(c.r, c.g, c.b, c.a));
    }
}
void ImGuiDrawListGraphicsAdaptor::triangle(float x1, float y1, float x2, float y2,
        float x3, float y3, bool fill, const color::color& c) {
    if (fill) {
        draw_list->AddTriangleFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3),
                ImColor(c.r, c.g, c.b, c.a));
    } else {
        draw_list->AddTriangle(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3),
                ImColor(c.r, c.g, c.b, c.a));
    }
}
void ImGuiDrawListGraphicsAdaptor::circle(float x, float y, float r, bool fill, const color::color& c) {
    if (fill) {
        draw_list->AddCircleFilled(ImVec2(x,y), r,
                ImColor(c.r, c.g, c.b, c.a), std::min(r, 250.f));
    } else {
        draw_list->AddCircle(ImVec2(x,y), r,
                ImColor(c.r, c.g, c.b, c.a), std::min( r, 250.f));
    }
}
void ImGuiDrawListGraphicsAdaptor::ellipse(float x, float y, float rx, float ry,
             bool fill, const color::color& c) {
    if (fill) {
        draw_list->AddEllipseFilled(ImVec2(x,y), rx, ry,
                ImColor(c.r, c.g, c.b, c.a), std::min(.5f * (rx + ry), 250.f));
    } else {
        draw_list->AddEllipse(ImVec2(x,y), rx, ry,
                ImColor(c.r, c.g, c.b, c.a), std::min(.5f * (rx + ry), 250.f));
    }
}
void ImGuiDrawListGraphicsAdaptor::string(float x, float y, const std::string& s, const color::color& c) {
    // String using ImGui API
    draw_list->AddText(ImVec2(x, y),
            ImColor(c.r, c.g, c.b, c.a), s.c_str());
}
}  // namespace nivalis
