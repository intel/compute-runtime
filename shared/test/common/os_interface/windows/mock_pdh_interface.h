/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/pdh_interface.h"

namespace NEO {

class MockPdhInterface : public PdhInterface {
  public:
    uint64_t getCurrentMemoryUsage(uint32_t rootDeviceIndex, bool dedicatedCounter) override {
        getCurrentMemoryUsageCallCount++;
        lastRootDeviceIndex = rootDeviceIndex;
        lastDedicatedCounter = dedicatedCounter;
        return mockReturnValue;
    }

    uint64_t mockReturnValue = 0;
    uint32_t getCurrentMemoryUsageCallCount = 0;
    uint32_t lastRootDeviceIndex = 0;
    bool lastDedicatedCounter = false;
};

} // namespace NEO
