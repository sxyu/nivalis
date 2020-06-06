#pragma once
#ifndef _POINT_H_3317CEA5_C1EF_4749_95FC_21E67612D4ED
#define _POINT_H_3317CEA5_C1EF_4749_95FC_21E67612D4ED
#include <cstddef>
#include <cstdint>

namespace nivalis {

struct point {
    point();
    point(const point& other);
    point operator=(const point& other);
    point(float x, float y);

    float& operator[](size_t i);
    float operator[](size_t i) const;

    float data[2];
    float & x, & y;
};

}  // namespace nivalis

#endif // ifndef _POINT_H_3317CEA5_C1EF_4749_95FC_21E67612D4ED
