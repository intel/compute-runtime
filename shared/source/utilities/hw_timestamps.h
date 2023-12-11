/*
 * Copyright (C) 2018-2023 Intel Corporation
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
        globalStartTS = 0;
        contextStartTS = 0;
        globalEndTS = 0;
        contextEndTS = 0;
        globalCompleteTS = 0;
        contextCompleteTS = 0;
    }

    static constexpr AllocationType getAllocationType() {
        return AllocationType::profilingTagBuffer;
    }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::hwTimeStamps; }

    uint64_t getContextStartValue(uint32_t) const { return contextStartTS; }
    uint64_t getGlobalStartValue(uint32_t) const { return globalStartTS; }
    uint64_t getContextEndValue(uint32_t) const { return contextEndTS; }
    uint64_t getGlobalEndValue(uint32_t) const { return globalEndTS; }

    uint64_t globalStartTS;
    uint64_t contextStartTS;
    uint64_t globalEndTS;
    uint64_t contextEndTS;
    uint64_t globalCompleteTS;
    uint64_t contextCompleteTS;
};

static_assert((6 * sizeof(uint64_t)) == sizeof(HwTimeStamps),
              "This structure is consumed by GPU and has to follow specific restrictions for padding and size");
} // namespace NEO
