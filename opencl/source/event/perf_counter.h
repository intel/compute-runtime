/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/event/hw_timestamps.h"

namespace NEO {

struct HwPerfCounter {
    void initialize() {
        report[0] = 0;
    }

    static GraphicsAllocation::AllocationType getAllocationType() {
        return GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER;
    }
    bool isCompleted() const { return true; }

    // Gpu report size is not known during compile time.
    // Such information will be provided by metrics library dll.
    // Bellow variable will be allocated dynamically based on information
    // from metrics library. Take look at CommandStreamReceiver::getEventPerfCountAllocator.
    uint8_t report[1] = {};
};
} // namespace NEO
