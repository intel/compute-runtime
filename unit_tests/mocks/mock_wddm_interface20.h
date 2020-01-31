/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/os_interface/windows/wddm/wddm_interface.h"

namespace NEO {
class WddmMockInterface20 : public WddmInterface20 {
  public:
    using WddmInterface::createMonitoredFence;
    using WddmInterface20::WddmInterface20;

    void destroyMonitorFence(MonitoredFence &monitorFence) override {
        destroyMonitorFenceCalled++;
        WddmInterface20::destroyMonitorFence(monitorFence);
    }

    uint32_t destroyMonitorFenceCalled = 0;
};
} // namespace NEO
