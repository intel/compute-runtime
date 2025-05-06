/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

namespace NEO {
class WddmMockInterface32 : public WddmInterface32 {
  public:
    using WddmInterface32::WddmInterface32;

    bool createHwQueue(OsContextWin &osContext) override {
        createHwQueueCalled++;
        createHwQueueResult = forceCreateHwQueueFail ? false : WddmInterface23::createHwQueue(osContext);
        return createHwQueueResult;
    }

    void destroyMonitorFence(MonitoredFence &monitorFence) override {
        destroyMonitorFenceCalled++;
        WddmInterface32::destroyMonitorFence(monitorFence);
    }

    bool createSyncObject(MonitoredFence &monitorFence) override {
        createSyncObjectCalled++;
        WddmInterface32::createSyncObject(monitorFence);
        return true;
    }

    uint32_t createHwQueueCalled = 0;
    uint32_t createSyncObjectCalled = 0;
    bool forceCreateHwQueueFail = false;
    bool createHwQueueResult = false;

    uint32_t destroyMonitorFenceCalled = 0;
};
} // namespace NEO
