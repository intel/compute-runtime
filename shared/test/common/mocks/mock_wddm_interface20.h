/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

#include <vector>

namespace NEO {
class WddmMockInterface20 : public WddmInterface20 {
  public:
    using WddmInterface::createMonitoredFence;
    using WddmInterface20::WddmInterface20;

    void destroyMonitorFence(MonitoredFence &monitorFence) override {
        destroyMonitorFenceCalled++;
        WddmInterface20::destroyMonitorFence(monitorFence);
    }

    void destroyMonitorFence(D3DKMT_HANDLE fenceHandle) override {
        destroyMonitorFenceCalled++;
        WddmInterface::destroyMonitorFence(fenceHandle);
    }

    bool createMonitoredFence(MonitoredFence &monitorFence) override {
        createMonitoredFenceCalled++;
        if (createMonitoredFenceCalledFail) {
            return false;
        }
        return WddmInterface::createMonitoredFence(monitorFence);
    }

    bool createFenceForDirectSubmission(MonitoredFence &monitorFence, OsContextWin &osContext) override {
        return createMonitoredFence(monitorFence);
    };

    bool waitForSyncObjectsFromGpu(OsContextWin &osContext, uint32_t objectCount, const D3DKMT_HANDLE *objectHandles, const uint64_t *fenceValues) override {
        waitForSyncObjectsFromGpuCalled++;
        waitForSyncObjectsFromGpuOsContext = &osContext;
        waitForSyncObjectsFromGpuObjectCounts.push_back(objectCount);
        waitForSyncObjectsFromGpuHandles.insert(waitForSyncObjectsFromGpuHandles.end(), objectHandles, objectHandles + objectCount);
        waitForSyncObjectsFromGpuFenceValues.insert(waitForSyncObjectsFromGpuFenceValues.end(), fenceValues, fenceValues + objectCount);
        if (callBaseWaitForSyncObjectsFromGpu) {
            return WddmInterface20::waitForSyncObjectsFromGpu(osContext, objectCount, objectHandles, fenceValues);
        }
        return waitForSyncObjectsFromGpuReturnValue;
    }

    uint32_t destroyMonitorFenceCalled = 0;
    uint32_t createMonitoredFenceCalled = 0;
    bool createMonitoredFenceCalledFail = false;

    uint32_t waitForSyncObjectsFromGpuCalled = 0;
    OsContextWin *waitForSyncObjectsFromGpuOsContext = nullptr;
    std::vector<uint32_t> waitForSyncObjectsFromGpuObjectCounts;
    std::vector<D3DKMT_HANDLE> waitForSyncObjectsFromGpuHandles;
    std::vector<uint64_t> waitForSyncObjectsFromGpuFenceValues;
    bool callBaseWaitForSyncObjectsFromGpu = true;
    bool waitForSyncObjectsFromGpuReturnValue = true;
};
} // namespace NEO
