#include <algorithm>
#include "plotter/imgui_adaptor.hpp"

namespace nivalis {
void ImGuiDrawListGraphicsAdaptor::line(float ax, float ay, float bx, float by,
        const color::color& c, float thickness) {
    draw_list->AddLine(ImVec2(ax, ay), ImVec2(bx, by),
            ImColor(c.r, c.g, c.b, c.a), thickness);
}
void ImGuiDrawListGraphicsAdaptor::polyline(const std::vector<std::array<float, 2> >& points,
        const color::color& c, float thickness) {
    // std::vector<ImVec2> line(points.size());
    for (size_t i = 0; i < points.size()-1; ++i) {
        // line[i].x = points[i][0];
        // line[i].y = points[i][1];
        draw_list->AddLine(ImVec2(points[i][0], points[i][1]),
                           ImVec2(points[i+1][0], points[i+1][1]),
                           ImColor(c.r, c.g, c.b, c.a), thickness);
    }
    // ImGui's polyline is bugged currently, has weird artifacts
    // (even though I am already using code from a PR purporting to fix it)
    // draw_list->AddPolyline(&line[0], (int)line.size(), ImColor(c.r, c.g, c.b, c.a), false, thickness);
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
