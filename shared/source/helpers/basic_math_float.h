/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cmath>
#include <cstdint>

namespace Math {

union FloatConversion {
    uint32_t u;
    float f;
};

static const FloatConversion posInfinity = {0x7f800000};
static const FloatConversion negInfinity = {0xff800000};

inline uint16_t float2Half(float f) {
    FloatConversion u;
    u.f = f;

    uint32_t fsign = (u.u >> 16) & 0x8000;
    float x = std::fabs(f);

    // nan
    if (x != x) {
        u.u >>= (24 - 11);
        u.u &= 0x7fff;
        u.u |= 0x0200; // silence the NaN
        return u.u | fsign;
    }

    // overflow
    if (x >= std::ldexp(1.0f, 16)) {
        if (x == posInfinity.f) {
            return 0x7c00 | fsign;
        }

        return 0x7bff | fsign;
    }

    // underflow
    if (x < std::ldexp(1.0f, -24)) {
        return fsign; // The halfway case can return 0x0001 or 0. 0 is even.
    }

    // half denormal
    if (x < std::ldexp(1.0f, -14)) {
        x *= std::ldexp(1.0f, 24);
        return (uint16_t)((int)x | fsign);
    }

    u.u &= 0xFFFFE000U;
    u.u -= 0x38000000U;

    return (u.u >> (24 - 11)) | fsign;
}

} // namespace Math
