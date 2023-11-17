/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/test/common/mocks/mock_wddm_residency_controller.h"

namespace NEO {
class MockOsContextWin : public OsContextWin {
  public:
    MockOsContextWin(Wddm &wddm, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor)
        : OsContextWin(wddm, rootDeviceIndex, contextId, engineDescriptor),
          mockResidencyController(wddm, contextId) {}

    WddmResidencyController &getResidencyController() override {
        getResidencyControllerCalledTimes++;
        return mockResidencyController;
    };

    uint64_t getResidencyControllerCalledTimes = 0;
    MockWddmResidencyController mockResidencyController;
};

} // namespace NEO