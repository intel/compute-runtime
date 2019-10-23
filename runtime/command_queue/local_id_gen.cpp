/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/local_id_gen.h"

#include "core/helpers/aligned_memory.h"
#include "core/utilities/cpu_info.h"

#include <array>

namespace NEO {

struct uint16x8_t;
struct uint16x16_t;

// This is the initial value of SIMD for local ID
// computation.  It correlates to the SIMD lane.
// Must be 32byte aligned for AVX2 usage
ALIGNAS(32)
const uint16_t initialLocalID[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

// Lookup table for generating LocalIDs based on the SIMD of the kernel
void (*LocalIDHelper::generateSimd8)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder) = generateLocalIDsSimd<uint16x8_t, 8>;
void (*LocalIDHelper::generateSimd16)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder) = generateLocalIDsSimd<uint16x8_t, 16>;
void (*LocalIDHelper::generateSimd32)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder) = generateLocalIDsSimd<uint16x8_t, 32>;

// Initialize the lookup table based on CPU capabilities
LocalIDHelper::LocalIDHelper() {
    bool supportsAVX2 = CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureAvX2);
    if (supportsAVX2) {
        LocalIDHelper::generateSimd8 = generateLocalIDsSimd<uint16x8_t, 8>;
        LocalIDHelper::generateSimd16 = generateLocalIDsSimd<uint16x16_t, 16>;
        LocalIDHelper::generateSimd32 = generateLocalIDsSimd<uint16x16_t, 32>;
    }
}

LocalIDHelper LocalIDHelper::initializer;

//traditional function to generate local IDs
void generateLocalIDs(void *buffer, uint16_t simd, const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, bool isImageOnlyKernel) {
    auto threadsPerWorkGroup = static_cast<uint16_t>(getThreadsPerWG(simd, localWorkgroupSize[0] * localWorkgroupSize[1] * localWorkgroupSize[2]));
    bool useLayoutForImages = isImageOnlyKernel && isCompatibleWithLayoutForImages(localWorkgroupSize, dimensionsOrder, simd);
    if (useLayoutForImages) {
        generateLocalIDsWithLayoutForImages(buffer, localWorkgroupSize, simd);
    } else if (simd == 32) {
        LocalIDHelper::generateSimd32(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder);
    } else if (simd == 16) {
        LocalIDHelper::generateSimd16(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder);
    } else if (simd == 8) {
        LocalIDHelper::generateSimd8(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder);
    } else {
        generateLocalIDsForSimdOne(buffer, localWorkgroupSize, dimensionsOrder);
    }
}

bool isCompatibleWithLayoutForImages(const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, uint16_t simd) {
    uint8_t xMask = simd == 8u ? 0b1 : 0b11;
    uint8_t yMask = 0b11;
    return dimensionsOrder.at(0) == 0 &&
           dimensionsOrder.at(1) == 1 &&
           (localWorkgroupSize.at(0) & xMask) == 0 &&
           (localWorkgroupSize.at(1) & yMask) == 0 &&
           localWorkgroupSize.at(2) == 1u;
}

inline void generateLocalIDsWithLayoutForImages(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t simd) {
    uint8_t rowWidth = simd == 32u ? 32u : 16u;
    uint8_t xDelta = simd == 8u ? 2u : 4u;                                                    // difference between corresponding values in consecutive X rows
    uint8_t yDelta = (simd == 8u || localWorkgroupSize.at(1) == 4u) ? 4u : rowWidth / xDelta; // difference between corresponding values in consecutive Y rows

    auto buffer = reinterpret_cast<uint16_t *>(b);
    uint16_t offset = 0u;
    auto numGrfs = (localWorkgroupSize.at(0) * localWorkgroupSize.at(1) * localWorkgroupSize.at(2) + (simd - 1)) / simd;
    uint8_t xMask = simd == 8u ? 0b1 : 0b11;
    uint16_t x = 0u;
    uint16_t y = 0u;
    for (auto grfId = 0; grfId < numGrfs; grfId++) {
        auto rowX = buffer + offset;
        auto rowY = buffer + offset + rowWidth;
        auto rowZ = buffer + offset + 2 * rowWidth;
        uint16_t extraX = 0u;
        uint16_t extraY = 0u;

        for (uint8_t i = 0u; i < simd; i++) {
            if (i > 0) {
                extraX++;
                if (extraX == xDelta) {
                    extraX = 0u;
                }
                if ((i & xMask) == 0) {
                    extraY++;
                    if (y + extraY == localWorkgroupSize.at(1)) {
                        extraY = 0;
                        x += xDelta;
                    }
                }
            }
            if (x == localWorkgroupSize.at(0)) {
                x = 0u;
                y += yDelta;
                if (y >= localWorkgroupSize.at(1)) {
                    y = 0u;
                }
            }
            rowX[i] = x + extraX;
            rowY[i] = y + extraY;
            rowZ[i] = 0u;
        }
        x += xDelta;
        offset += 3 * rowWidth;
    }
}

void generateLocalIDsForSimdOne(void *b, const std::array<uint16_t, 3> &localWorkgroupSize,
                                const std::array<uint8_t, 3> &dimensionsOrder) {
    uint32_t xDimNum = dimensionsOrder[0];
    uint32_t yDimNum = dimensionsOrder[1];
    uint32_t zDimNum = dimensionsOrder[2];

    for (int i = 0; i < localWorkgroupSize[zDimNum]; i++) {
        for (int j = 0; j < localWorkgroupSize[yDimNum]; j++) {
            for (int k = 0; k < localWorkgroupSize[xDimNum]; k++) {
                static_cast<uint16_t *>(b)[0] = k;
                static_cast<uint16_t *>(b)[1] = j;
                static_cast<uint16_t *>(b)[2] = i;
                b = ptrOffset(b, sizeof(GRF));
            }
        }
    }
}

} // namespace NEO
