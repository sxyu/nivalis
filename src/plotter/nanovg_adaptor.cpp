#include "plotter/nanovg_adaptor.hpp"
#include "resources/roboto.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace nivalis {

namespace {
NVGcolor conv_color(const color::color& c) {
    return nvgRGBAf(c.r, c.g, c.b, c.a);
}
void set_color_and_thickness(NVGcontext* ctx, const color::color& c, float thickness) {
    nvgStrokeColor(ctx, conv_color(c));
    nvgStrokeWidth(ctx, thickness);
}
void fill_or_stroke(NVGcontext* ctx, const color::color& c, bool fill, float thickness = 1.f) {
    if (fill) {
        nvgFillColor(ctx, conv_color(c));
        nvgFill(ctx);
    } else {
        nvgStrokeWidth(ctx, thickness);
        nvgStrokeColor(ctx, conv_color(c));
        nvgStroke(ctx);
    }
}
}  // namespace

void NanoVGGraphicsAdaptor::init_fonts() {
    nvg_font_normal = nvgCreateFontMem(ctx, "sans", ROBOTO_REGULAR, 0, 0);
}
void NanoVGGraphicsAdaptor::line(float ax, float ay, float bx, float by,
        const color::color& c, float thickness) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, ax, ay);
    nvgLineTo(ctx, bx, by);
    set_color_and_thickness(ctx, c, thickness);
    nvgStroke(ctx);
}
void NanoVGGraphicsAdaptor::polyline(const std::vector<std::array<float, 2> >& points,
        const color::color& c, float thickness, bool closed) {
    nvgBeginPath(ctx);
    if (points.size()) {
        nvgMoveTo(ctx, points[0][0], points[0][1]);
    }
    for (size_t i = 1; i < points.size(); ++i) {
        nvgLineTo(ctx, points[i][0], points[i][1]);
    }
    set_color_and_thickness(ctx, c, thickness);
    nvgStroke(ctx);
}
void NanoVGGraphicsAdaptor::rectangle(float x, float y, float w, float h, bool fill, const color::color& c) {
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    fill_or_stroke(ctx, c, fill);
}
void NanoVGGraphicsAdaptor::triangle(float x1, float y1, float x2, float y2,
        float x3, float y3, bool fill, const color::color& c) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x1, y1);
    nvgLineTo(ctx, x2, y2);
    nvgLineTo(ctx, x3, y3);
    fill_or_stroke(ctx, c, fill);
}
void NanoVGGraphicsAdaptor::circle(float x, float y, float r, bool fill, const color::color& c) {
    nvgBeginPath(ctx);
    nvgCircle(ctx, x, y, r);
    fill_or_stroke(ctx, c, fill);
}
void NanoVGGraphicsAdaptor::ellipse(float x, float y, float rx, float ry,
             bool fill, const color::color& c) {
    nvgBeginPath(ctx);
    nvgEllipse(ctx, x, y, rx, ry);
    fill_or_stroke(ctx, c, fill);
}
void NanoVGGraphicsAdaptor::string(float x, float y, const std::string& s, const color::color& c) {
    // String using NanoVG API
    nvgBeginPath(ctx);
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, 13.0);
    nvgTextAlign(ctx, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, conv_color(c));
    nvgText(ctx, x, y, s.c_str(), nullptr);
}
}  // namespace nivalis
