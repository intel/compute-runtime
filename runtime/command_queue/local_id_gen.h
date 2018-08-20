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

#include "runtime/helpers/ptr_math.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace OCLRT {
union GRF {
    float fRegs[8];
    uint32_t dwRegs[8];
    uint16_t wRegs[16];
};

inline size_t getGRFsPerThread(uint32_t simd) {
    return simd == 32 ? 2 : 1;
}

inline size_t getThreadsPerWG(uint32_t simd, size_t lws) {
    auto result = lws + simd - 1;

    // Original logic:
    // result = (lws + simd - 1) / simd;
    // This sequence is meant to avoid an CPU DIV instruction.
    result >>= simd == 32
                   ? 5
                   : simd == 16
                         ? 4
                         : 3;

    return result;
}

inline size_t getPerThreadSizeLocalIDs(uint32_t simd, uint32_t numChannels = 3) {
    auto numGRFSPerThread = getGRFsPerThread(simd);
    auto returnSize = numChannels * numGRFSPerThread * sizeof(GRF);
    returnSize = std::max(returnSize, sizeof(GRF));
    return returnSize;
}

struct LocalIDHelper {
    static void (*generateSimd8)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder);
    static void (*generateSimd16)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder);
    static void (*generateSimd32)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder);

    static LocalIDHelper initializer;

  private:
    LocalIDHelper();
};

extern const uint16_t initialLocalID[];

template <typename Vec, int simd>
void generateLocalIDsSimd(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup,
                          const std::array<uint8_t, 3> &dimensionsOrder);

void generateLocalIDs(void *buffer, uint16_t simd, const std::array<uint16_t, 3> &localWorkgroupSize,
                      const std::array<uint8_t, 3> &dimensionsOrder, bool isImageOnlyKernel);
void generateLocalIDsWithLayoutForImages(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t simd);

bool isCompatibleWithLayoutForImages(const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, uint16_t simd);
} // namespace OCLRT