/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"

namespace NEO {
namespace TagAllocationLayout {
inline constexpr uint64_t debugPauseStateAddressOffset = MemoryConstants::kiloByte;
inline constexpr uint64_t completionFenceOffset = 2 * MemoryConstants::kiloByte;

} // namespace TagAllocationLayout
} // namespace NEO
