#include "point.hpp"

#include <cstring>

namespace nivalis {

point::point() : point(0.f, 0.f) {}
point::point(const point& other): point(other.data[0], other.data[1]) {}
point::point(float x, float y) : data{x,y}, x(data[0]), y(data[1]) {}

point point::point::operator=(const point& other) {
    memcpy(data, other.data, sizeof data);
    return *this;
}

float& point::operator[](size_t i) { return data[i]; }
float point::operator[](size_t i) const{ return data[i]; }

}  // namespace nivalis
