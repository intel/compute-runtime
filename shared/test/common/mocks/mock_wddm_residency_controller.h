/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/wddm_residency_controller.h"

namespace NEO {
class MockWddmResidencyController : public WddmResidencyController {
  public:
    using WddmResidencyController::csr;
    using WddmResidencyController::lastTrimFenceValue;
    using WddmResidencyController::lock;
    using WddmResidencyController::trimCallbackHandle;
    using WddmResidencyController::trimResidency;
    using WddmResidencyController::trimResidencyToBudget;
    using WddmResidencyController::WddmResidencyController;

    uint32_t acquireLockCallCount = 0u;

    std::unique_lock<SpinLock> acquireLock() override {
        acquireLockCallCount++;
        return WddmResidencyController::acquireLock();
    }
};
} // namespace NEO