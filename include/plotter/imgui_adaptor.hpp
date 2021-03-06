#pragma once
#ifndef _IMGUI_ADAPTOR_H_0EB7F921_D939_4011_AA3A_9C87F33527DD
#define _IMGUI_ADAPTOR_H_0EB7F921_D939_4011_AA3A_9C87F33527DD

#include <vector>
#include <array>
#include <string>
#include <limits>
#include "imgui.h"
#include "color.hpp"
#include "point.hpp"

namespace nivalis {

// ImGUi drawlist graphics adaptor for Plotter
struct ImGuiDrawListGraphicsAdaptor {
    void line(float ax, float ay, float bx, float by,
            const color::color& c, float thickness = 1.f);
    // Polyline/polygon.
    void polyline(const std::vector<point>& points,
            const color::color& c, float thickness = 1.f, bool closed = false,
            bool fill = false);
    void rectangle(float x, float y, float w, float h, bool fill, const color::color& c,
            float thickness = 1.f);
    void triangle(float x1, float y1, float x2, float y2,
                  float x3, float y3, bool fill, const color::color& c);
    void circle(float x, float y, float r, bool fill, const color::color& c);
    // Axis-aligned ellipse
    void ellipse(float x, float y, float rx, float ry,
                 bool fill, const color::color& c);
    void string(float x, float y,
                const std::string& s, const color::color& c,
                float align_x = 0.0, float align_y = 0.0);
    void image(float x, float y, float w, float h, char* data_rgb, int cols, int rows);
    ImDrawList* draw_list = nullptr;
    int swid, shigh;
};

// Font range with Greek characters
// we need Greek character support for math
// Retrieve list of range (2 int per range, values are inclusive)
const ImWchar* GetGlyphRangesGreek();

}  // namespace nivalis
#endif // ifndef _IMGUI_ADAPTOR_H_0EB7F921_D939_4011_AA3A_9C87F33527DD
