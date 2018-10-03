/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <atomic>

namespace OCLRT {

class WddmResidencyController {
  public:
    WddmResidencyController();

    void acquireLock();
    void releaseLock();

    uint64_t getLastTrimFenceValue() { return lastTrimFenceValue; }
    void setLastTrimFenceValue(uint64_t value) { lastTrimFenceValue = value; }

  protected:
    std::atomic<bool> lock;
    uint64_t lastTrimFenceValue;
};
} // namespace OCLRT
