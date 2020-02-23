/*
 * Copyright (C) 2019-2020 Intel Corporation
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

    bool createMonitoredFence(MonitoredFence &monitorFence) override {
        createMonitoredFenceCalled++;
        if (createMonitoredFenceCalledFail) {
            return false;
        }
        return WddmInterface::createMonitoredFence(monitorFence);
    }

    uint32_t destroyMonitorFenceCalled = 0;
    uint32_t createMonitoredFenceCalled = 0;
    bool createMonitoredFenceCalledFail = false;
};
} // namespace NEO
