#pragma once
#ifndef _NANOVG_ADAPTOR_H_508FF868_0044_47FC_A7CF_4C90ABD4DCFB
#define _NANOVG_ADAPTOR_H_508FF868_0044_47FC_A7CF_4C90ABD4DCFB

#include <vector>
#include <array>
#include <string>
#include "nanovg.h"
#include "color.hpp"

namespace nivalis {
// NanoVG-based graphics adaptor for Plotter
struct NanoVGGraphicsAdaptor {
    void init_fonts();
    void line(float ax, float ay, float bx, float by,
            const color::color& c, float thickness = 1.);
    void polyline(const std::vector<std::array<float, 2> >& points,
            const color::color& c, float thickness = 1., bool closed = false);
    void rectangle(float x, float y, float w, float h, bool fill, const color::color& c);
    void triangle(float x1, float y1, float x2, float y2,
                  float x3, float y3, bool fill, const color::color& c);
    void circle(float x, float y, float r, bool fill, const color::color& c);
    // Axis-aligned ellipse
    void ellipse(float x, float y, float rx, float ry,
                 bool fill, const color::color& c);
    void string(float x, float y, const std::string& s, const color::color& c);

    NVGcontext* ctx = nullptr;
    int nvg_font_normal = 0;
};
}  // namespace nivalis
#endif // ifndef _NANOVG_ADAPTOR_H_508FF868_0044_47FC_A7CF_4C90ABD4DCFB
