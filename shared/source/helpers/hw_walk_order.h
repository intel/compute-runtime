/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <array>
#include <cstdint>

namespace NEO {
namespace HwWalkOrderHelper {
// make sure table below matches Hardware Spec
inline constexpr uint32_t walkOrderPossibilties = 6u;
inline constexpr uint8_t x = 0;
inline constexpr uint8_t y = 1;
inline constexpr uint8_t z = 2;
inline constexpr std::array<uint8_t, 3> linearWalk = {x, y, z};
inline constexpr std::array<uint8_t, 3> yOrderWalk = {y, x, z};
inline constexpr std::array<uint8_t, 3> singleDimWalk = {y, z, x};
inline constexpr std::array<uint8_t, 3> compatibleDimensionOrders[walkOrderPossibilties] = {linearWalk,    // 0 1 2
                                                                                            {x, z, y},     // 0 2 1
                                                                                            yOrderWalk,    // 1 0 2
                                                                                            {z, x, y},     // 1 2 0
                                                                                            singleDimWalk, // 2 0 1
                                                                                            {z, y, x}};    // 2 1 0
inline constexpr uint32_t singleDimWalkIndex = 4;
static_assert(singleDimWalk == compatibleDimensionOrders[singleDimWalkIndex], "singleDimWalk index mismatch");

inline std::pair<uint32_t, uint32_t> getActiveIndices(std::array<uint8_t, 3> walkOrder, uint8_t firstDim, uint8_t secondDim) {
    auto fstIndex = UINT32_MAX;
    auto sndIndex = UINT32_MAX;
    for (auto i = 0u; i < 3u; i++) {
        if (walkOrder[i] == firstDim) {
            fstIndex = i;
        }
        if (walkOrder[i] == secondDim) {
            sndIndex = i;
        }
    }
    UNRECOVERABLE_IF(fstIndex == UINT32_MAX || sndIndex == UINT32_MAX);
    return {fstIndex, sndIndex};
}

} // namespace HwWalkOrderHelper
} // namespace NEO
