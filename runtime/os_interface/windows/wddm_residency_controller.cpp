/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "wddm_residency_controller.h"

namespace OCLRT {

WddmResidencyController::WddmResidencyController() : lock(false), lastTrimFenceValue(0u) {}

void WddmResidencyController::acquireLock() {
    bool previousLockValue = false;
    while (!lock.compare_exchange_weak(previousLockValue, true))
        previousLockValue = false;
}

void WddmResidencyController::releaseLock() {
    lock = false;
}
} // namespace OCLRT
