#include "plotter/plotter.hpp"
namespace nivalis {
namespace color {
color::color() = default;
color::color(unsigned clr)
        : r(static_cast<uint8_t>((clr& 0xFF0000) >> 16)),
          g(static_cast<uint8_t>((clr& 0xFF00) >> 8)),
          b(static_cast<uint8_t>(clr& 0xFF)) { }

color::color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

const color from_int(size_t color_index) {
    static const color palette[] = {

        RED, ROYAL_BLUE, GREEN, ORANGE, PURPLE, BLACK,
        color{255, 220, 0}, color{201, 13, 177}, color{34, 255, 94},
        color{255, 65, 54}, color{255, 255, 64}, color{0, 116, 217},
        color{27, 133, 255}, color{190, 18, 240}, color{20, 31, 210},
        color{75, 20, 133}, color{255, 219, 127}, color{204, 204, 57},
        color{112, 153, 61}, color{64, 204, 46}, color{112, 255, 1},
        color{170, 170, 170}, color{225, 30, 42}
    };
    return palette[color_index % (sizeof palette / sizeof palette[0])];
}
}  // namespace color
}  // namespace nivalis
