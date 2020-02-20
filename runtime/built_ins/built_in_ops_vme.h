/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "built_in_ops.h"

namespace NEO {
namespace EBuiltInOps {

using Type = uint32_t;

constexpr Type VmeBlockMotionEstimateIntel{MaxCoreValue + 1};
constexpr Type VmeBlockAdvancedMotionEstimateCheckIntel{MaxCoreValue + 2};
constexpr Type VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel{MaxCoreValue + 3};
} // namespace EBuiltInOps
} // namespace NEO
