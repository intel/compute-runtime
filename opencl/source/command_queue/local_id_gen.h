/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/ptr_math.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace NEO {
inline uint32_t getGRFsPerThread(uint32_t simd) {
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
                         : simd == 8
                               ? 3
                               : 0;

    return result;
}

inline uint32_t getPerThreadSizeLocalIDs(uint32_t simd, uint32_t grfSize, uint32_t numChannels = 3) {
    auto numGRFSPerThread = getGRFsPerThread(simd);
    uint32_t returnSize = numGRFSPerThread * grfSize * (simd == 1 ? 1u : numChannels);
    returnSize = std::max(returnSize, grfSize);
    return returnSize;
}

struct LocalIDHelper {
    static void (*generateSimd8)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);
    static void (*generateSimd16)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);
    static void (*generateSimd32)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);

    static LocalIDHelper initializer;

  private:
    LocalIDHelper();
};

extern const uint16_t initialLocalID[];

template <typename Vec, int simd>
void generateLocalIDsSimd(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup,
                          const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);

void generateLocalIDs(void *buffer, uint16_t simd, const std::array<uint16_t, 3> &localWorkgroupSize,
                      const std::array<uint8_t, 3> &dimensionsOrder, bool isImageOnlyKernel, uint32_t grfSize);
void generateLocalIDsWithLayoutForImages(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t simd);

bool isCompatibleWithLayoutForImages(const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, uint16_t simd);

void generateLocalIDsForSimdOne(void *b, const std::array<uint16_t, 3> &localWorkgroupSize,
                                const std::array<uint8_t, 3> &dimensionsOrder, uint32_t grfSize);
} // namespace NEO
