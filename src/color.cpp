#include "color.hpp"

#include <cstring>
#include <cmath>
#include <sstream>
namespace nivalis {
namespace color {
color::color() : color(0.0f, 0.0f, 0.0f) {}
color::color(const color& other)
    : color(other.r, other.g, other.b, other.a) { }
color color::color::operator=(const color& other) {
    memcpy(data, other.data, sizeof data);
    return *this;
}

bool color::operator==(const color& other) const {
    return r == other.r && g == other.g &&
        b == other.b && a == other.a;
}
bool color::operator!=(const color& other) const {
    return !(*this == other);
}

// From hex
color::color(unsigned clr)
    : color((clr& 0xFF0000) >> 16,
            (clr& 0xFF00) >> 8,
            clr& 0xFF, 255){}

color::color(int r, int g, int b, int a)
    : color(r/255.0f, g/255.0f, b/255.0f, a/255.0f) {}

color::color(float r, float g, float b, float a)
       : data{r,g,b,a},
         r(data[0]), g(data[1]), b(data[2]), a(data[3]) {
}

color from_int(size_t color_index) {
    static const color palette[] = {
        color(RED), color(ROYAL_BLUE), color(GREEN), color(ORANGE),
        color(PURPLE), color(BLACK),
        color{255, 116, 0}, color{255, 44, 137}, color{34, 255, 94},
        color{255, 65, 54}, color{255, 255, 64}, color{0, 116, 217},
        color{27, 133, 255}, color{190, 18, 240}, color{20, 31, 210},
        color{75, 20, 133}, color{255, 219, 127}, color{204, 204, 57},
        color{112, 153, 61}, color{64, 204, 46}, color{112, 255, 1},
        color{170, 170, 170}, color{225, 30, 42}
    };
    return palette[color_index % (sizeof palette / sizeof palette[0])];
}

color from_hex(const std::string& hex) {
    if (hex.empty()) return BLACK;
    if (hex[0] == '#') return from_hex(hex.substr(1));
    return color((unsigned) std::stoul(hex, 0, 16));
}

std::string color::to_hex() const {
    static auto trunc = [](float f) -> int{
        return std::min(std::max((int)std::round(f * 255.), 0), 255);
    };
    int tr = trunc(r), tg = trunc(g), tb = trunc(b);
    int t = (tr << 16) + (tg << 8) + tb;
    std::stringstream strm;
    strm << std::hex << t;
    std::string s;
    for (int i = 0; i < 6 - (int)strm.tellp(); ++i) {
        s.push_back('0');        
    }
    return s + strm.str();
}
}  // namespace color
}  // namespace nivalis
