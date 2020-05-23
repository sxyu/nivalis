#pragma once
#ifndef _COLOR_H_400B07C2_2177_405C_AA65_BECF37F3C9FC
#define _COLOR_H_400B07C2_2177_405C_AA65_BECF37F3C9FC
#include <cstddef>
#include <cstdint>
#include <string>
namespace nivalis {

namespace color {
    // RGB color
    struct color {
        color();
        color(const color& other);
        color operator=(const color& other);
        bool operator==(const color& other) const;
        bool operator!=(const color& other) const;
        // From hex (e.g. 0xffffff)
        color(unsigned clr);
        // From rgba (0-255)
        color(int r, int g, int b, int a = 255);
        // From rgba (0.-1.)
        color(float r, float g, float b, float a = 1.0f);
        // To hex string (ignores alpha)
        std::string to_hex() const;
        float data[4];
        float &r, &g, &b, &a;
    };
    enum _colors {
        // Hex codes of common colors
        WHITE = 0xFFFFFF,
        BLACK = 0x000000,
        DARK_GRAY = 0xa9a9a9,
        GRAY = 0x808080,
        LIGHT_GRAY = 0xd3d3d3,
        DIM_GRAY = 0x696969,
        RED  = 0xFF0000,
        BLUE  = 0x0000FF,
        ROYAL_BLUE = 0x4169e1,
        GREEN = 0x008000,
        ORANGE = 0xffa500,
        PURPLE = 0x800080,
        YELLOW = 0xFFFF00
    };
    const color TRANSPARENT = color(0.f, 0.f, 0.f, 0.f);

    // From hex string e.g. FFFFFF (no alpha)
    color from_hex(const std::string& hex);

    // Get a color corresponding to an integer,
    // from a pre-defined list of colors
    color from_int(size_t color_index);
}  // namespace color

}  // namespace nivalis
#endif // ifndef _COLOR_H_400B07C2_2177_405C_AA65_BECF37F3C9FC
