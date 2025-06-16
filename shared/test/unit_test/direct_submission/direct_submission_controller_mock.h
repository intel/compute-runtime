/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/direct_submission/direct_submission_controller.h"

namespace NEO {
struct DirectSubmissionControllerMock : public DirectSubmissionController {
    using DirectSubmissionController::bcsTimeoutDivisor;
    using DirectSubmissionController::checkNewSubmissions;
    using DirectSubmissionController::condVarMutex;
    using DirectSubmissionController::directSubmissionControllingThread;
    using DirectSubmissionController::directSubmissions;
    using DirectSubmissionController::directSubmissionsMutex;
    using DirectSubmissionController::getSleepValue;
    using DirectSubmissionController::handlePagingFenceRequests;
    using DirectSubmissionController::keepControlling;
    using DirectSubmissionController::lastTerminateCpuTimestamp;
    using DirectSubmissionController::lowestThrottleSubmitted;
    using DirectSubmissionController::maxTimeout;
    using DirectSubmissionController::pagingFenceRequests;
    using DirectSubmissionController::timeout;
    using DirectSubmissionController::timeoutDivisor;
    using DirectSubmissionController::timeSinceLastCheck;

    bool sleep(std::unique_lock<std::mutex> &lock) override {
        this->sleepCalled = true;
        if (callBaseSleepMethod) {
            return DirectSubmissionController::sleep(lock);
        } else {
            auto ret = sleepReturnValue.load();
            sleepReturnValue.store(false);
            return ret;
        }
    }

    SteadyClock::time_point getCpuTimestamp() override {
        return cpuTimestamp;
    }

    TimeoutElapsedMode timeoutElapsed() override {
        if (timeoutElapsedCallBase) {
            return DirectSubmissionController::timeoutElapsed();
        }
        auto ret = timeoutElapsedReturnValue.load();
        return ret;
    }

    SteadyClock::time_point cpuTimestamp{};
    std::atomic<bool> sleepCalled{false};
    std::atomic<bool> sleepReturnValue{false};
    std::atomic<TimeoutElapsedMode> timeoutElapsedReturnValue{TimeoutElapsedMode::notElapsed};
    std::atomic<bool> timeoutElapsedCallBase{false};
    bool callBaseSleepMethod = false;
};
} // namespace NEO