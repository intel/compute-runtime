/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/debug_helpers.h"
#include "runtime/utilities/vec.h"
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdio.h>

#define KB 1024uLL
#define MB (KB * KB)
#define GB (KB * MB)

namespace OCLRT {
namespace Math {

inline uint32_t nextPowerOfTwo(uint32_t value) {
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    ++value;
    return value;
}

inline uint32_t prevPowerOfTwo(uint32_t value) {
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return (value - (value >> 1));
}

inline uint32_t getMinLsbSet(uint32_t value) {
    static const int multiplyDeBruijnBitPosition[32] = {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
    auto invert = -static_cast<int64_t>(value);
    value &= static_cast<uint32_t>(invert);
    return multiplyDeBruijnBitPosition[static_cast<uint32_t>(value * 0x077CB531U) >> 27];
}

inline uint32_t log2(uint32_t value) {
    uint32_t exponent = 0u;
    uint32_t startVal = value;
    if (value == 0) {
        return 32;
    }
    startVal >>= 1;
    startVal &= 0x7fffffffu;
    while ((startVal & 0xffffffffu) && (exponent < 32)) {
        exponent = exponent + 1;
        startVal >>= 1;
        startVal &= 0x7fffffffu;
    }
    return exponent;
}

inline uint64_t log2(uint64_t value) {
    uint64_t exponent = 0;
    uint64_t startVal = value;
    if (value == 0) {
        return 64;
    }
    startVal >>= 1;
    startVal &= 0x7fffffffffffffff;
    while ((startVal & 0xffffffffffffffff) && (exponent < 64)) {
        exponent = exponent + 1;
        startVal >>= 1;
        startVal &= 0x7fffffffffffffff;
    }
    return exponent;
}

union FloatConversion {
    uint32_t u;
    float f;
};

// clang-format off
static const FloatConversion PosInfinity = {0x7f800000};
static const FloatConversion NegInfinity = {0xff800000};
static const FloatConversion Nan         = {0x7fc00000};
// clang-format on

inline uint16_t float2Half(float f) {
    FloatConversion u;
    u.f = f;

    uint32_t fsign = (u.u >> 16) & 0x8000;
    float x = std::fabs(f);

    //Nan
    if (x != x) {
        u.u >>= (24 - 11);
        u.u &= 0x7fff;
        u.u |= 0x0200; //silence the NaN
        return u.u | fsign;
    }

    // overflow
    if (x >= std::ldexp(1.0f, 16)) {
        if (x == PosInfinity.f)
            return 0x7c00 | fsign;

        return 0x7bff | fsign;
    }

    // underflow
    if (x < std::ldexp(1.0f, -24))
        return fsign; // The halfway case can return 0x0001 or 0. 0 is even.

    // half denormal
    if (x < std::ldexp(1.0f, -14)) {
        x *= std::ldexp(1.0f, 24);
        return (uint16_t)((int)x | fsign);
    }

    u.u &= 0xFFFFE000U;
    u.u -= 0x38000000U;

    return (u.u >> (24 - 11)) | fsign;
}

inline bool isDivisableByPowerOfTwoDivisor(uint32_t number, uint32_t divisor) {
    DEBUG_BREAK_IF((divisor & (divisor - 1)) != 0);
    uint32_t mask = 0xffffffff;
    mask = mask - (divisor - 1);
    if ((number & mask) == number)
        return true;
    else
        return false;
}

inline size_t computeTotalElementsCount(const Vec3<size_t> &inputVector) {
    size_t minElementCount = 1;
    auto xDim = std::max(minElementCount, inputVector.x);
    auto yDim = std::max(minElementCount, inputVector.y);
    auto zDim = std::max(minElementCount, inputVector.z);
    return xDim * yDim * zDim;
}

template <typename T>
bool isPow2(T val) {
    if (val != 0) {
        if ((val & (val - 1)) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace Math
} // namespace OCLRT
