/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/debug_helpers.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

template <typename T>
struct Vec3 {
    static_assert(NEO::TypeTraits::isPodV<T>);

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

    T &operator[](uint32_t i) {
        UNRECOVERABLE_IF(i > 2);
        return values[i];
    }

    T operator[](uint32_t i) const {
        UNRECOVERABLE_IF(i > 2);
        return values[i];
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
        return 0;
    }

    union {
        struct {
            T x, y, z;
        };
        T values[3];
    };
};

static_assert(offsetof(Vec3<size_t>, x) == offsetof(Vec3<size_t>, values[0]));
static_assert(offsetof(Vec3<size_t>, y) == offsetof(Vec3<size_t>, values[1]));
static_assert(offsetof(Vec3<size_t>, z) == offsetof(Vec3<size_t>, values[2]));
