/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/event/hw_timestamps.h"
#include "runtime/memory_manager/graphics_allocation.h"

#include "instrumentation.h"

namespace OCLRT {

struct HwPerfCounter {
    void initialize() {
        HWPerfCounters = {};
        HWTimeStamp.initialize();
    }
    static GraphicsAllocation::AllocationType getAllocationType() {
        return GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER;
    }
    bool canBeReleased() const { return true; }
    HwPerfCounters HWPerfCounters;
    HwTimeStamps HWTimeStamp;
};
} // namespace OCLRT
