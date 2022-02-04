/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

#include <cinttypes>

namespace NEO {

class HwTimeStamps : public TagTypeBase {
  public:
    void initialize() {
        GlobalStartTS = 0;
        ContextStartTS = 0;
        GlobalEndTS = 0;
        ContextEndTS = 0;
        GlobalCompleteTS = 0;
        ContextCompleteTS = 0;
    }

    static constexpr AllocationType getAllocationType() {
        return AllocationType::PROFILING_TAG_BUFFER;
    }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::HwTimeStamps; }

    uint64_t getContextStartValue(uint32_t) const { return ContextStartTS; }
    uint64_t getGlobalStartValue(uint32_t) const { return GlobalStartTS; }
    uint64_t getContextEndValue(uint32_t) const { return ContextEndTS; }
    uint64_t getGlobalEndValue(uint32_t) const { return GlobalEndTS; }

    uint64_t GlobalStartTS;
    uint64_t ContextStartTS;
    uint64_t GlobalEndTS;
    uint64_t ContextEndTS;
    uint64_t GlobalCompleteTS;
    uint64_t ContextCompleteTS;
};

static_assert((6 * sizeof(uint64_t)) == sizeof(HwTimeStamps),
              "This structure is consumed by GPU and has to follow specific restrictions for padding and size");
} // namespace NEO
