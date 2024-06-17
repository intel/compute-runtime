/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/windows/sys_calls.h"

#include <chrono>
#include <thread>
namespace NEO {
void DirectSubmissionController::sleep() {
    SysCalls::timeBeginPeriod(1u);
    NEO::sleep(std::chrono::microseconds(this->timeout));
    SysCalls::timeEndPeriod(1u);
}
} // namespace NEO