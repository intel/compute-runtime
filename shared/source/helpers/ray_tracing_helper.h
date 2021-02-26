/*
 * Copyright (C) 2016-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstdint>
namespace NEO {
class RayTracingHelper : public NonCopyableOrMovableClass {
  public:
    static const uint32_t memoryBackedFifoSizePerDss;

    static size_t getTotalMemoryBackedFifoSize(const Device &device);
    static size_t getMemoryBackedFifoSizeToPatch();
};
} // namespace NEO
