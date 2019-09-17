/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/memory_manager/graphics_allocation.h"
#include "runtime/event/hw_timestamps.h"

namespace NEO {

struct HwPerfCounter {
    void initialize() {
        report[0] = 0;
    }

    static GraphicsAllocation::AllocationType getAllocationType() {
        return GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER;
    }
    bool canBeReleased() const { return true; }

    // Gpu report size is not known during compile time.
    // Such information will be provided by metrics library dll.
    // Bellow variable will be allocated dynamically based on information
    // from metrics library. Take look at CommandStreamReceiver::getEventPerfCountAllocator.
    uint8_t report[1] = {};
};
} // namespace NEO
