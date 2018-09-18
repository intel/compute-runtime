/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/debug_helpers.h"

namespace OCLRT {
template <typename T>
struct Vec3 {
    Vec3(T x, T y, T z) : x(x), y(y), z(z) {}
    Vec3(const Vec3 &v) : x(v.x), y(v.y), z(v.z) {}
    Vec3(const T *arr) {
        if (arr == nullptr) {
            x = y = z = 0;
        } else {
            x = arr[0];
            y = arr[1];
            z = arr[2];
        }
    }

    Vec3 &operator=(const Vec3 &arr) {
        x = arr.x;
        y = arr.y;
        z = arr.z;
        return *this;
    }

    Vec3<T> &operator=(const T arr[3]) {
        x = arr[0];
        y = arr[1];
        z = arr[2];
        return *this;
    }

    bool operator==(const Vec3<T> &vec) const {
        return ((x == vec.x) && (y == vec.y) && (z == vec.z));
    }

    bool operator!=(const Vec3<T> &vec) const {
        return !operator==(vec);
    }

    unsigned int getSimplifiedDim() const {
        if (z > 1) {
            return 3;
        }
        if (y > 1) {
            return 2;
        }
        if (x >= 1) {
            return 1;
        }
        DEBUG_BREAK_IF((y != 0) || (z != 0));
        return 0;
    }

    T x;
    T y;
    T z;
};
} // namespace OCLRT
