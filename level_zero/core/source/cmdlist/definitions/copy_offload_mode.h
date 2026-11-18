/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/extendable_enum.h"

#include <cstddef>
#include <cstdint>

namespace L0 {

struct CopyOffloadMode : ExtendableEnum {
  public:
    constexpr CopyOffloadMode(uint32_t val) : ExtendableEnum(val) {}
};

namespace CopyOffloadModes {
static constexpr CopyOffloadMode disabled = 0;
static constexpr CopyOffloadMode dualStream = 1;
} // namespace CopyOffloadModes

namespace DualStreamCopyOffload {
inline constexpr uint32_t copyOffloadReadIndex = 0u;
inline constexpr uint32_t copyOffloadWriteIndex = 1u;
inline constexpr size_t numCopyOffloadQueues = copyOffloadWriteIndex + 1;
} // namespace DualStreamCopyOffload

} // namespace L0
