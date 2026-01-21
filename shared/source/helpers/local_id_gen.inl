/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/ptr_math.h"

#include <array>

namespace NEO {

template <typename Vec, int simd, uint32_t activeChannels>
inline void generateLocalIDsSimdImpl(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup,
                                     const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize) {
    static_assert(activeChannels >= 1 && activeChannels <= 3, "activeChannels must be 1, 2, or 3");

    const int passes = simd / Vec::numChannels;
    int pass = 0;

    uint32_t xDimNum = dimensionsOrder[0];
    uint32_t yDimNum = dimensionsOrder[1];
    uint32_t zDimNum = dimensionsOrder[2];

    const Vec vLwsX(localWorkgroupSize[xDimNum]);
    const Vec vLwsY(localWorkgroupSize[yDimNum]);

    auto zero = Vec::zero();
    auto one = Vec::one();

    const auto threadSkipSize = ((simd == 32 || chooseMaxRowSize) ? 32 : 16) * sizeof(uint16_t);
    Vec vSimdX(simd);
    Vec vSimdY = zero;
    Vec vSimdZ = zero;

    Vec xWrap;
    Vec yWrap;
    // We need to convert simd into appropriate delta adders
    do {
        xWrap = vSimdX >= vLwsX;

        // vSimdX -= xWrap ? lwsX : 0;
        vSimdX -= blend(vLwsX, zero, xWrap);

        if constexpr (activeChannels >= 2u) {
            vSimdY += blend(one, zero, xWrap);
            yWrap = vSimdY >= vLwsY;
            vSimdY -= blend(vLwsY, zero, yWrap);
        }

        if constexpr (activeChannels == 3u) {
            vSimdZ += blend(one, zero, yWrap);
        }

    } while (xWrap || yWrap);

    // Loop for each of the passes
    do {
        auto buffer = b;

        Vec x(&initialLocalID[pass * Vec::numChannels]);
        Vec y = zero;
        Vec z = zero;

        // Convert the initial SIMD lanes to localIDs
        do {
            xWrap = x >= vLwsX;
            x -= blend(vLwsX, zero, xWrap);

            if constexpr (activeChannels >= 2u) {
                y += blend(one, zero, xWrap);

                yWrap = y >= vLwsY;
                y -= blend(vLwsY, zero, yWrap);
            }

            if constexpr (activeChannels == 3u) {
                z += blend(one, zero, yWrap);
            }
        } while (xWrap);

        for (size_t i = 0; i < threadsPerWorkGroup; ++i) {
            x.store(ptrOffset(buffer, xDimNum * threadSkipSize));
            if constexpr (activeChannels >= 2u) {
                y.store(ptrOffset(buffer, yDimNum * threadSkipSize));
            }
            if constexpr (activeChannels == 3u) {
                z.store(ptrOffset(buffer, zDimNum * threadSkipSize));
            }

            x += vSimdX;
            if constexpr (activeChannels >= 2u) {
                y += vSimdY;
            }
            if constexpr (activeChannels == 3u) {
                z += vSimdZ;
            }

            xWrap = x >= vLwsX;
            x -= blend(vLwsX, zero, xWrap);

            if constexpr (activeChannels >= 2u) {
                y += blend(one, zero, xWrap);

                yWrap = y >= vLwsY;
                y -= blend(vLwsY, zero, yWrap);
            }

            if constexpr (activeChannels == 3u) {
                z += blend(one, zero, yWrap);
            }

            buffer = ptrOffset(buffer, activeChannels * threadSkipSize);
        }

        // Adjust buffer for next pass
        b = ptrOffset(b, Vec::numChannels * sizeof(uint16_t));

    } while (++pass < passes);
}

template <typename Vec, int simd>
inline void generateLocalIDsSimd(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup,
                                 const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize, uint8_t activeChannels) {
    switch (activeChannels) {
    default:
        UNRECOVERABLE_IF(true);
        [[fallthrough]];
    case 3:
        generateLocalIDsSimdImpl<Vec, simd, 3u>(b, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder, chooseMaxRowSize);
        break;
    case 2:
        generateLocalIDsSimdImpl<Vec, simd, 2u>(b, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder, chooseMaxRowSize);
        break;
    case 1:
        generateLocalIDsSimdImpl<Vec, simd, 1u>(b, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder, chooseMaxRowSize);
        break;
    }
}
} // namespace NEO
