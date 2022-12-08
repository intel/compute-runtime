/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <array>
#include <cstdint>

namespace NEO {
namespace HwWalkOrderHelper {
//make sure table below matches Hardware Spec
inline constexpr uint32_t walkOrderPossibilties = 6u;
inline constexpr uint8_t X = 0;
inline constexpr uint8_t Y = 1;
inline constexpr uint8_t Z = 2;
inline constexpr std::array<uint8_t, 3> linearWalk = {X, Y, Z};
inline constexpr std::array<uint8_t, 3> yOrderWalk = {Y, X, Z};
inline constexpr std::array<uint8_t, 3> compatibleDimensionOrders[walkOrderPossibilties] = {linearWalk, // 0 1 2
                                                                                            {X, Z, Y},  // 0 2 1
                                                                                            yOrderWalk, // 1 0 2
                                                                                            {Z, X, Y},  // 1 2 0
                                                                                            {Y, Z, X},  // 2 0 1
                                                                                            {Z, Y, X}}; // 2 1 0
} // namespace HwWalkOrderHelper
} // namespace NEO
