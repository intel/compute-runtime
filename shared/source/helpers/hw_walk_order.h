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
constexpr uint32_t walkOrderPossibilties = 6u;
constexpr uint8_t X = 0;
constexpr uint8_t Y = 1;
constexpr uint8_t Z = 2;
constexpr std::array<uint8_t, 3> compatibleDimensionOrders[walkOrderPossibilties] = {{X, Y, Z},  // 0 1 2
                                                                                     {X, Z, Y},  // 0 2 1
                                                                                     {Y, X, Z},  // 1 0 2
                                                                                     {Z, X, Y},  // 1 2 0
                                                                                     {Y, Z, X},  // 2 0 1
                                                                                     {Z, Y, X}}; // 2 1 0
} // namespace HwWalkOrderHelper
} // namespace NEO
