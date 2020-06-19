/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/event/hw_timestamps.h"
#include "opencl/source/os_interface/metrics_library.h"

namespace NEO {

struct HwPerfCounter {
    void initialize() {
        query = {};
        report[0] = 0;
    }

    static GraphicsAllocation::AllocationType getAllocationType() {
        return GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER;
    }

    template <typename Type>
    static uint32_t getSize(Type &performanceCounters) {
        return sizeof(query) + performanceCounters.getGpuReportSize();
    }

    bool isCompleted() const { return true; }
    uint32_t getImplicitGpuDependenciesCount() const { return 0; }

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
