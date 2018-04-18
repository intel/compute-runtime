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
#include "config.h"
#include "CL/cl.h"

// It's max SSH size per kernel (MAX_BINDING_TABLE_INDEX * 64)
const uint32_t SAMPLER_OBJECT_ID_SHIFT = 253 * 64;

// Sampler Patch Token Enums
enum SAMPLER_PATCH_ENUM {
    CLK_DEFAULT_SAMPLER = 0x00,
    CLK_ADDRESS_NONE = 0x00,
    CLK_ADDRESS_CLAMP = 0x01,
    CLK_ADDRESS_CLAMP_TO_EDGE = 0x02,
    CLK_ADDRESS_REPEAT = 0x03,
    CLK_ADDRESS_MIRRORED_REPEAT = 0x04,
    CLK_ADDRESS_MIRRORED_REPEAT_101 = 0x05,
    CLK_NORMALIZED_COORDS_FALSE = 0x00,
    CLK_NORMALIZED_COORDS_TRUE = 0x08,
    CLK_FILTER_NEAREST = 0x00,
    CLK_FILTER_LINEAR = 0x00,
};

inline SAMPLER_PATCH_ENUM GetAddrModeEnum(cl_addressing_mode addressingMode) {
    switch (addressingMode) {
    case CL_ADDRESS_REPEAT:
        return CLK_ADDRESS_REPEAT;
    case CL_ADDRESS_CLAMP_TO_EDGE:
        return CLK_ADDRESS_CLAMP_TO_EDGE;
    case CL_ADDRESS_CLAMP:
        return CLK_ADDRESS_CLAMP;
    case CL_ADDRESS_NONE:
        return CLK_ADDRESS_NONE;
    case CL_ADDRESS_MIRRORED_REPEAT:
        return CLK_ADDRESS_MIRRORED_REPEAT;
    }
    return CLK_ADDRESS_NONE;
}

inline SAMPLER_PATCH_ENUM GetNormCoordsEnum(cl_bool normalizedCoords) {
    if (normalizedCoords == CL_TRUE) {
        return CLK_NORMALIZED_COORDS_TRUE;
    } else {
        return CLK_NORMALIZED_COORDS_FALSE;
    }
}
