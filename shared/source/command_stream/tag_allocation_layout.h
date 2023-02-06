/*
 * Copyright (C) 2022-2023 Intel Corporation
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
inline constexpr uint64_t barrierCountOffset = 3 * MemoryConstants::kiloByte;
} // namespace TagAllocationLayout
} // namespace NEO
