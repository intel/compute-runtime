/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_type.h"

namespace NEO {

class ImageImplicitArgsNodeType {
  public:
    using ValueT = uint64_t;

    static constexpr NEO::AllocationType getAllocationType() { return NEO::AllocationType::gpuTimestampDeviceBuffer; };

    static constexpr NEO::TagNodeType getTagNodeType() { return NEO::TagNodeType::imageImplicitArgs; }

    static constexpr size_t getSinglePacketSize() { return MemoryConstants::cacheLineSize; }

    void initialize(uint64_t initValue) {}
};
} // namespace NEO
