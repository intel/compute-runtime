/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
namespace NEO {
struct MonitoredFence;

struct SyncFence {
    virtual ~SyncFence() = default;
    virtual volatile uint64_t *getCpuAddress() = 0;
    virtual uint64_t getGpuAddress() = 0;
    virtual void setFenceValue(uint64_t value) = 0;
    virtual MonitoredFence *getFence() = 0;
};

} // namespace NEO
