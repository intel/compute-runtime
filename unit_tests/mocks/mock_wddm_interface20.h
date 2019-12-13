/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/os_interface/windows/wddm/wddm_interface.h"

namespace NEO {
class WddmMockInterface20 : public WddmInterface20 {
  public:
    using WddmInterface20::WddmInterface20;

    void destroyMonitorFence(D3DKMT_HANDLE fenceHandle) override {
        destroyMonitorFenceCalled++;
        WddmInterface20::destroyMonitorFence(fenceHandle);
    }

    uint32_t destroyMonitorFenceCalled = 0;
};
} // namespace NEO
