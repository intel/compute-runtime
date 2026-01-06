/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/helpers/sleep.h"

#include <chrono>
namespace NEO {
bool DirectSubmissionController::sleep(std::unique_lock<std::mutex> &lock) {
    return NEO::waitOnConditionWithPredicate(condVar, lock, getSleepValue(), [&] { return !pagingFenceRequests.empty(); });
}

void DirectSubmissionController::overrideDirectSubmissionTimeouts(const ProductHelper &productHelper) {
}

} // namespace NEO