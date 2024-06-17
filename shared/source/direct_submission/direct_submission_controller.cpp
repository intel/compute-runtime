/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/product_helper.h"

#include <chrono>
#include <thread>

namespace NEO {

DirectSubmissionController::DirectSubmissionController() {
    if (debugManager.flags.DirectSubmissionControllerTimeout.get() != -1) {
        timeout = std::chrono::microseconds{debugManager.flags.DirectSubmissionControllerTimeout.get()};
    }
    if (debugManager.flags.DirectSubmissionControllerDivisor.get() != -1) {
        timeoutDivisor = debugManager.flags.DirectSubmissionControllerDivisor.get();
    }
    if (debugManager.flags.DirectSubmissionControllerMaxTimeout.get() != -1) {
        maxTimeout = std::chrono::microseconds{debugManager.flags.DirectSubmissionControllerMaxTimeout.get()};
    }
};

DirectSubmissionController::~DirectSubmissionController() {
    UNRECOVERABLE_IF(directSubmissionControllingThread);
}

void DirectSubmissionController::registerDirectSubmission(CommandStreamReceiver *csr) {
    std::lock_guard<std::mutex> lock(directSubmissionsMutex);
    directSubmissions.insert(std::make_pair(csr, DirectSubmissionState()));
    this->adjustTimeout(csr);
}

void DirectSubmissionController::setTimeoutParamsForPlatform(const ProductHelper &helper) {
    adjustTimeoutOnThrottleAndAcLineStatus = helper.isAdjustDirectSubmissionTimeoutOnThrottleAndAcLineStatusEnabled();

    if (debugManager.flags.DirectSubmissionControllerAdjustOnThrottleAndAcLineStatus.get() != -1) {
        adjustTimeoutOnThrottleAndAcLineStatus = debugManager.flags.DirectSubmissionControllerAdjustOnThrottleAndAcLineStatus.get();
    }

    if (adjustTimeoutOnThrottleAndAcLineStatus) {
        for (auto throttle : {QueueThrottle::LOW, QueueThrottle::MEDIUM, QueueThrottle::HIGH}) {
            for (auto acLineStatus : {false, true}) {
                auto key = this->getTimeoutParamsMapKey(throttle, acLineStatus);
                auto timeoutParam = std::make_pair(key, helper.getDirectSubmissionControllerTimeoutParams(acLineStatus, throttle));
                this->timeoutParamsMap.insert(timeoutParam);
            }
        }
    }
}

void DirectSubmissionController::applyTimeoutForAcLineStatusAndThrottle(bool acLineConnected) {
    const auto &timeoutParams = this->timeoutParamsMap[this->getTimeoutParamsMapKey(this->lowestThrottleSubmitted, acLineConnected)];
    this->timeout = timeoutParams.timeout;
    this->maxTimeout = timeoutParams.maxTimeout;
    this->timeoutDivisor = timeoutParams.timeoutDivisor;
}

void DirectSubmissionController::updateLastSubmittedThrottle(QueueThrottle throttle) {
    if (throttle < this->lowestThrottleSubmitted) {
        this->lowestThrottleSubmitted = throttle;
    }
}

size_t DirectSubmissionController::getTimeoutParamsMapKey(QueueThrottle throttle, bool acLineStatus) {
    return (static_cast<size_t>(throttle) << 1) + acLineStatus;
}

void DirectSubmissionController::unregisterDirectSubmission(CommandStreamReceiver *csr) {
    std::lock_guard<std::mutex> lock(directSubmissionsMutex);
    directSubmissions.erase(csr);
}

void DirectSubmissionController::startThread() {
    directSubmissionControllingThread = Thread::create(controlDirectSubmissionsState, reinterpret_cast<void *>(this));
}

void DirectSubmissionController::stopThread() {
    runControlling.store(false);
    keepControlling.store(false);
    if (directSubmissionControllingThread) {
        directSubmissionControllingThread->join();
        directSubmissionControllingThread.reset();
    }
}

void DirectSubmissionController::startControlling() {
    this->runControlling.store(true);
}

void *DirectSubmissionController::controlDirectSubmissionsState(void *self) {
    auto controller = reinterpret_cast<DirectSubmissionController *>(self);

    while (!controller->runControlling.load()) {
        if (!controller->keepControlling.load()) {
            return nullptr;
        }

        controller->sleep();
    }

    while (true) {
        if (!controller->keepControlling.load()) {
            return nullptr;
        }

        controller->sleep();
        controller->checkNewSubmissions();
    }
}

void DirectSubmissionController::checkNewSubmissions() {
    std::lock_guard<std::mutex> lock(this->directSubmissionsMutex);
    bool shouldRecalculateTimeout = false;
    for (auto &directSubmission : this->directSubmissions) {
        auto csr = directSubmission.first;
        auto &state = directSubmission.second;

        auto taskCount = csr->peekTaskCount();
        if (taskCount == state.taskCount) {
            if (state.isStopped) {
                continue;
            } else {
                auto lock = csr->obtainUniqueOwnership();
                csr->stopDirectSubmission(false);
                state.isStopped = true;
                shouldRecalculateTimeout = true;
                this->lowestThrottleSubmitted = QueueThrottle::HIGH;
            }
        } else {
            state.isStopped = false;
            state.taskCount = taskCount;
            if (this->adjustTimeoutOnThrottleAndAcLineStatus) {
                this->updateLastSubmittedThrottle(csr->getLastDirectSubmissionThrottle());
                this->applyTimeoutForAcLineStatusAndThrottle(csr->getAcLineConnected(true));
            }
        }
    }
    if (shouldRecalculateTimeout) {
        this->recalculateTimeout();
    }
}

SteadyClock::time_point DirectSubmissionController::getCpuTimestamp() {
    return SteadyClock::now();
}

void DirectSubmissionController::adjustTimeout(CommandStreamReceiver *csr) {
    if (EngineHelpers::isCcs(csr->getOsContext().getEngineType())) {
        for (size_t subDeviceIndex = 0u; subDeviceIndex < csr->getOsContext().getDeviceBitfield().size(); ++subDeviceIndex) {
            if (csr->getOsContext().getDeviceBitfield().test(subDeviceIndex)) {
                ++this->ccsCount[subDeviceIndex];
            }
        }
        auto curentMaxCcsCount = std::max_element(this->ccsCount.begin(), this->ccsCount.end());
        if (*curentMaxCcsCount > this->maxCcsCount) {
            this->maxCcsCount = *curentMaxCcsCount;
            this->timeout /= this->timeoutDivisor;
        }
    }
}

void DirectSubmissionController::recalculateTimeout() {
    const auto now = this->getCpuTimestamp();
    const auto timeSinceLastTerminate = std::chrono::duration_cast<std::chrono::microseconds>(now - this->lastTerminateCpuTimestamp);
    DEBUG_BREAK_IF(timeSinceLastTerminate.count() < 0);
    if (timeSinceLastTerminate.count() > this->timeout.count() &&
        timeSinceLastTerminate.count() <= this->maxTimeout.count()) {
        const auto newTimeout = std::chrono::duration_cast<std::chrono::microseconds>(timeSinceLastTerminate * 1.5);
        this->timeout = newTimeout.count() < this->maxTimeout.count() ? newTimeout : this->maxTimeout;
    }
    this->lastTerminateCpuTimestamp = now;
}

} // namespace NEO
