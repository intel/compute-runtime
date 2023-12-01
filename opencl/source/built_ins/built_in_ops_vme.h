/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/builtinops/built_in_ops.h"

namespace NEO {
namespace EBuiltInOps {

using Type = uint32_t;

inline constexpr Type vmeBlockMotionEstimateIntel{maxCoreValue + 1};
inline constexpr Type vmeBlockAdvancedMotionEstimateCheckIntel{maxCoreValue + 2};
inline constexpr Type vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel{maxCoreValue + 3};
} // namespace EBuiltInOps
} // namespace NEO
