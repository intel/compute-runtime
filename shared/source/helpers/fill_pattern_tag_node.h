/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_type.h"

namespace NEO {

class FillPaternNodeType {
  public:
    using ValueT = uint64_t;

    static constexpr NEO::AllocationType getAllocationType() { return NEO::AllocationType::fillPattern; };

    static constexpr NEO::TagNodeType getTagNodeType() { return NEO::TagNodeType::fillPattern; }

    static constexpr size_t getSinglePacketSize() { return MemoryConstants::cacheLineSize; }

    void initialize(uint64_t initValue) {}
};
} // namespace NEO