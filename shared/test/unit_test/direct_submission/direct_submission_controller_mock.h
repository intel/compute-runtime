/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/direct_submission/direct_submission_controller.h"

namespace NEO {
struct DirectSubmissionControllerMock : public DirectSubmissionController {
    using DirectSubmissionController::adjustTimeoutOnThrottleAndAcLineStatus;
    using DirectSubmissionController::checkNewSubmissions;
    using DirectSubmissionController::directSubmissionControllingThread;
    using DirectSubmissionController::directSubmissions;
    using DirectSubmissionController::directSubmissionsMutex;
    using DirectSubmissionController::getTimeoutParamsMapKey;
    using DirectSubmissionController::keepControlling;
    using DirectSubmissionController::lastTerminateCpuTimestamp;
    using DirectSubmissionController::lowestThrottleSubmitted;
    using DirectSubmissionController::maxTimeout;
    using DirectSubmissionController::timeout;
    using DirectSubmissionController::timeoutDivisor;
    using DirectSubmissionController::timeoutParamsMap;

    void sleep() override {
        DirectSubmissionController::sleep();
        this->sleepCalled = true;
    }

    SteadyClock::time_point getCpuTimestamp() override {
        return cpuTimestamp;
    }

    SteadyClock::time_point cpuTimestamp{};
    std::atomic<bool> sleepCalled{false};
};
} // namespace NEO