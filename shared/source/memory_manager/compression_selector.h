/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
class CompressionSelector {
  public:
    static bool preferCompressedAllocation(const AllocationProperties &properties, const HardwareInfo &hwInfo);
};

} // namespace NEO
