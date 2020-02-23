/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

namespace NEO {
class WddmMockInterface23 : public WddmInterface23 {
  public:
    using WddmInterface23::WddmInterface23;

    bool createHwQueue(OsContextWin &osContext) override {
        createHwQueueCalled++;
        createHwQueueResult = forceCreateHwQueueFail ? false : WddmInterface23::createHwQueue(osContext);
        return createHwQueueResult;
    }

    void destroyMonitorFence(MonitoredFence &monitorFence) override {
        destroyMonitorFenceCalled++;
        WddmInterface23::destroyMonitorFence(monitorFence);
    }

    uint32_t createHwQueueCalled = 0;
    bool forceCreateHwQueueFail = false;
    bool createHwQueueResult = false;

    uint32_t destroyMonitorFenceCalled = 0;
};
} // namespace NEO
