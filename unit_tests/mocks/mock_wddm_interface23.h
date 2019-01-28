/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/os_interface/windows/wddm/wddm_interface.h"

namespace OCLRT {
class WddmMockInterface23 : public WddmInterface23 {
  public:
    using WddmInterface23::WddmInterface23;

    bool createHwQueue(PreemptionMode preemptionMode, OsContextWin &osContext) override {
        createHwQueueCalled++;
        createHwQueueResult = forceCreateHwQueueFail ? false : WddmInterface23::createHwQueue(preemptionMode, osContext);
        return createHwQueueResult;
    }

    uint32_t createHwQueueCalled = 0;
    bool forceCreateHwQueueFail = false;
    bool createHwQueueResult = false;
};
} // namespace OCLRT
