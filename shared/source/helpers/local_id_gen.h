/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/simd_helper.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace NEO {
struct RootDeviceEnvironment;
inline uint32_t getNumGrfsPerLocalIdCoordinate(uint32_t simd, uint32_t grfSize) {
    return (simd == 32 && grfSize == 32) ? 2 : 1;
}

inline uint32_t getThreadsPerWG(uint32_t simd, uint32_t lws) {
    if (isSimd1(simd)) {
        return lws;
    }
    auto result = lws + simd - 1;

    // Original logic:
    // result = (lws + simd - 1) / simd;
    // This sequence is meant to avoid an CPU DIV instruction.
    result >>= simd == 32
                   ? 5
               : simd == 16
                   ? 4
                   : 3; // for SIMD 8

    return result;
}

inline uint32_t getPerThreadSizeLocalIDs(uint32_t simd, uint32_t grfSize, uint32_t numChannels = 3) {
    if (isSimd1(simd)) {
        return grfSize;
    }
    auto numGRFSPerLocalIdCoord = getNumGrfsPerLocalIdCoordinate(simd, grfSize);
    uint32_t returnSize = numGRFSPerLocalIdCoord * grfSize * numChannels;
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
                      const std::array<uint8_t, 3> &dimensionsOrder, bool isImageOnlyKernel, uint32_t grfSize, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment);
void generateLocalIDsWithLayoutForImages(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t simd);

bool isCompatibleWithLayoutForImages(const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, uint16_t simd);

void generateLocalIDsForSimdOne(void *b, const std::array<uint16_t, 3> &localWorkgroupSize,
                                const std::array<uint8_t, 3> &dimensionsOrder, uint32_t grfSize);
} // namespace NEO
