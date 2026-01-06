/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

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

    uint32_t destroyMonitorFenceCalled = 0;
    uint32_t createMonitoredFenceCalled = 0;
    bool createMonitoredFenceCalledFail = false;
};
} // namespace NEO
