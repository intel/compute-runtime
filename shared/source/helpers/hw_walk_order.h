/*
 * Copyright (C) 2022-2023 Intel Corporation
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
inline constexpr std::array<uint8_t, 3> compatibleDimensionOrders[walkOrderPossibilties] = {linearWalk, // 0 1 2
                                                                                            {x, z, y},  // 0 2 1
                                                                                            yOrderWalk, // 1 0 2
                                                                                            {z, x, y},  // 1 2 0
                                                                                            {y, z, x},  // 2 0 1
                                                                                            {z, y, x}}; // 2 1 0
} // namespace HwWalkOrderHelper
} // namespace NEO
