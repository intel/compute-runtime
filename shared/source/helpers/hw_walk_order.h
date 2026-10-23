/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace NEO {
namespace HwWalkOrderHelper {
// make sure table below matches Hardware Spec
inline constexpr uint32_t walkOrderPossibilities = 6u;
inline constexpr uint8_t x = 0;
inline constexpr uint8_t y = 1;
inline constexpr uint8_t z = 2;
inline constexpr std::array<uint8_t, 3> linearWalk = {x, y, z};
inline constexpr std::array<uint8_t, 3> yOrderWalk = {y, x, z};
inline constexpr std::array<uint8_t, 3> singleDimWalk = {y, z, x};
inline constexpr std::array<uint8_t, 3> compatibleDimensionOrders[walkOrderPossibilities] = {linearWalk,    // 0 1 2
                                                                                             {x, z, y},     // 0 2 1
                                                                                             yOrderWalk,    // 1 0 2
                                                                                             {z, x, y},     // 1 2 0
                                                                                             singleDimWalk, // 2 0 1
                                                                                             {z, y, x}};    // 2 1 0
inline constexpr uint32_t linearWalkIndex = 0;
inline constexpr uint32_t singleDimWalkIndex = 4;
static_assert(linearWalk == compatibleDimensionOrders[linearWalkIndex], "linearWalkIndex index mismatch");
static_assert(singleDimWalk == compatibleDimensionOrders[singleDimWalkIndex], "singleDimWalk index mismatch");

inline bool isSameOrderOfNonUnitDimensions(const std::array<uint8_t, 3> &walkOrder, const std::array<uint8_t, 3> &otherWalkOrder, const size_t *lws) {
    auto getNonUnitDimensions = [lws](const std::array<uint8_t, 3> &order) {
        std::array<uint8_t, 3> nonUnitDimensions = {};
        size_t nonUnitDimensionsCount = 0u;
        for (auto dimension : order) {
            if (lws[dimension] > 1u) {
                nonUnitDimensions[nonUnitDimensionsCount++] = dimension;
            }
        }
        return std::make_pair(nonUnitDimensions, nonUnitDimensionsCount);
    };
    return getNonUnitDimensions(walkOrder) == getNonUnitDimensions(otherWalkOrder);
}

} // namespace HwWalkOrderHelper
} // namespace NEO
