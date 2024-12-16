/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/utilities/metrics_library.h"

namespace NEO {

class HwPerfCounter : public TagTypeBase {
  public:
    using ValueT = uint8_t;

    void initialize(uint8_t initValue) {
        query = {};
        report[0] = initValue;
    }

    static constexpr AllocationType getAllocationType() {
        return AllocationType::profilingTagBuffer;
    }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::hwPerfCounter; }

    template <typename Type>
    static uint32_t getSize(Type &performanceCounters) {
        return sizeof(query) + performanceCounters.getGpuReportSize();
    }

    // Gpu report size is not known during compile time.
    // Such information will be provided by metrics library dll.
    // Bellow variable will be allocated dynamically based on information
    // from metrics library. Take look at CommandStreamReceiver::getEventPerfCountAllocator.
    union {
        MetricsLibraryApi::QueryHandle_1_0 handle = {};
        uint8_t padding[64];
    } query;

    uint8_t report[1] = {};
};

} // namespace NEO
