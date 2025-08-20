/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/sys_calls_winmm.h"

#include <chrono>

namespace NEO {
bool DirectSubmissionController::sleep(std::unique_lock<std::mutex> &lock) {
    SysCalls::timeBeginPeriod(1u);
    bool returnValue = NEO::waitOnConditionWithPredicate(condVar, lock, getSleepValue(), [&] { return !pagingFenceRequests.empty(); });
    SysCalls::timeEndPeriod(1u);
    return returnValue;
}

void DirectSubmissionController::overrideDirectSubmissionTimeouts(const ProductHelper &productHelper) {
    uint64_t timeoutUs = this->timeout.count();
    uint64_t maxTimeoutUs = this->maxTimeout.count();
    productHelper.overrideDirectSubmissionTimeouts(timeoutUs, maxTimeoutUs);
    this->timeout = std::chrono::microseconds(timeoutUs);
    this->maxTimeout = std::chrono::microseconds(maxTimeoutUs);
}

} // namespace NEO