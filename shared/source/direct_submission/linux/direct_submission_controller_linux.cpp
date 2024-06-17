/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/helpers/sleep.h"

#include <chrono>
#include <thread>
namespace NEO {
void DirectSubmissionController::sleep() {
    NEO::sleep(std::chrono::microseconds(this->timeout));
}
} // namespace NEO