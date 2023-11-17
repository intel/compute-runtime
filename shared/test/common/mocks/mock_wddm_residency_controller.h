/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/wddm_residency_controller.h"

namespace NEO {
class MockWddmResidencyController : public WddmResidencyController {
  public:
    using WddmResidencyController::lastTrimFenceValue;
    using WddmResidencyController::trimCallbackHandle;
    using WddmResidencyController::trimCandidateList;
    using WddmResidencyController::trimCandidatesCount;
    using WddmResidencyController::trimResidency;
    using WddmResidencyController::trimResidencyToBudget;
    using WddmResidencyController::WddmResidencyController;

    uint32_t acquireLockCallCount = 0u;
    bool forceTrimCandidateListCompaction = false;

    std::unique_lock<SpinLock> acquireLock() override {
        acquireLockCallCount++;
        return WddmResidencyController::acquireLock();
    }

    bool checkTrimCandidateListCompaction() override {
        return forceTrimCandidateListCompaction || WddmResidencyController::checkTrimCandidateListCompaction();
    }
};
} // namespace NEO