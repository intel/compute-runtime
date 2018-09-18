/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/event/hw_timestamps.h"
#include "instrumentation.h"

namespace OCLRT {

struct HwPerfCounter {
    void initialize() {
        HWPerfCounters = {};
        HWTimeStamp.initialize();
    }
    bool canBeReleased() const { return true; }
    HwPerfCounters HWPerfCounters;
    HwTimeStamps HWTimeStamp;
};
} // namespace OCLRT
