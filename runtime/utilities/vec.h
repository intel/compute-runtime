/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
