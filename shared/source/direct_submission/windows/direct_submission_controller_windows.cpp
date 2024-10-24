/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/windows/sys_calls_winmm.h"

#include <chrono>
namespace NEO {
bool DirectSubmissionController::sleep(std::unique_lock<std::mutex> &lock) {
    SysCalls::timeBeginPeriod(1u);
    bool returnValue = NEO::waitOnConditionWithPredicate(condVar, lock, getSleepValue(), [&] { return !pagingFenceRequests.empty(); });
    SysCalls::timeEndPeriod(1u);
    return returnValue;
}
} // namespace NEO