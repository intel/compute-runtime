/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/vec.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace Math {

constexpr uint32_t nextPowerOfTwo(uint32_t value) {
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    ++value;
    return value;
}

constexpr uint64_t nextPowerOfTwo(uint64_t value) {
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    ++value;
    return value;
}

constexpr uint32_t prevPowerOfTwo(uint32_t value) {
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return (value - (value >> 1));
}

constexpr uint64_t prevPowerOfTwo(uint64_t value) {
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return (value - (value >> 1));
}

inline uint32_t getMinLsbSet(uint32_t value) {
    static const uint8_t multiplyDeBruijnBitPosition[32] = {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
    auto invert = -static_cast<int64_t>(value);
    value &= static_cast<uint32_t>(invert);
    return multiplyDeBruijnBitPosition[static_cast<uint32_t>(value * 0x077CB531U) >> 27];
}

constexpr uint32_t log2(uint32_t value) {
    if (value == 0) {
        return 32;
    }
    uint32_t exponent = 0u;
    while (value >>= 1) {
        exponent++;
    }
    return exponent;
}

constexpr uint32_t log2(uint64_t value) {
    if (value == 0) {
        return 64;
    }
    uint32_t exponent = 0;
    while (value >>= 1) {
        exponent++;
    }
    return exponent;
}

constexpr bool isDivisibleByPowerOfTwoDivisor(uint32_t number, uint32_t divisor) {
    return (number & (divisor - 1)) == 0;
}

constexpr size_t computeTotalElementsCount(const Vec3<size_t> &inputVector) {
    size_t minElementCount = 1;
    auto xDim = std::max(minElementCount, inputVector.x);
    auto yDim = std::max(minElementCount, inputVector.y);
    auto zDim = std::max(minElementCount, inputVector.z);
    return xDim * yDim * zDim;
}

template <typename T>
constexpr bool isPow2(T val) {
    return val != 0 && (val & (val - 1)) == 0;
}

template <typename T>
constexpr T ffs(T v) {
    if (v == 0) {
        return std::numeric_limits<T>::max();
    }

    for (T i = 0; i < sizeof(T) * 8; ++i) {
        if (0 != (v & (1ULL << i))) {
            return i;
        }
    }

    std::abort();
}

constexpr size_t divideAndRoundUp(size_t dividend, size_t divisor) {
    return (dividend + divisor - 1) / divisor;
}

constexpr size_t roundUp(size_t value, size_t divisor) {
    return ((value + divisor - 1) / divisor) * divisor;
}

} // namespace Math
