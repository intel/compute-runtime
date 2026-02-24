/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/defs.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"

namespace NEO {

constexpr uintptr_t windowsMinAddress = 0x200000;

struct MonitoredFence {
    D3DKMT_HANDLE fenceHandle = 0;
    D3DGPU_VIRTUAL_ADDRESS gpuAddress = 0;
    volatile uint64_t *cpuAddress = nullptr;
    uint64_t currentFenceValue = 0;
    uint64_t lastSubmittedFence = 0;
};

struct WddmSyncFence : SyncFence {
    volatile uint64_t *getCpuAddress() override { return fence.cpuAddress; }
    uint64_t getGpuAddress() override { return fence.gpuAddress; }
    void setFenceValue(uint64_t value) override { fence.currentFenceValue = value; }
    MonitoredFence *getFence() override { return &fence; }
    MonitoredFence fence;
};

} // namespace NEO
