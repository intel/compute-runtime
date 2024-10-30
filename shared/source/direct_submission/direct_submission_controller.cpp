/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/os_time.h"
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
    if (debugManager.flags.DirectSubmissionControllerBcsTimeoutDivisor.get() != -1) {
        bcsTimeoutDivisor = debugManager.flags.DirectSubmissionControllerBcsTimeoutDivisor.get();
    }

    if (debugManager.flags.DirectSubmissionControllerMaxTimeout.get() != -1) {
        maxTimeout = std::chrono::microseconds{debugManager.flags.DirectSubmissionControllerMaxTimeout.get()};
    }
    isCsrIdleDetectionEnabled = true;
    if (debugManager.flags.DirectSubmissionControllerIdleDetection.get() != -1) {
        isCsrIdleDetectionEnabled = debugManager.flags.DirectSubmissionControllerIdleDetection.get();
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
    directSubmissionControllingThread = Thread::createFunc(controlDirectSubmissionsState, reinterpret_cast<void *>(this));
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
        std::unique_lock<std::mutex> lock(controller->condVarMutex);
        controller->handlePagingFenceRequests(lock, false);

        auto isControllerNotified = controller->sleep(lock);
        if (isControllerNotified) {
            controller->handlePagingFenceRequests(lock, false);
        }
    }

    controller->timeSinceLastCheck = controller->getCpuTimestamp();
    controller->lastHangCheckTime = std::chrono::high_resolution_clock::now();
    while (true) {
        if (!controller->keepControlling.load()) {
            return nullptr;
        }
        std::unique_lock<std::mutex> lock(controller->condVarMutex);
        controller->handlePagingFenceRequests(lock, true);

        auto isControllerNotified = controller->sleep(lock);
        if (isControllerNotified) {
            controller->handlePagingFenceRequests(lock, true);
        }
        lock.unlock();
        controller->checkNewSubmissions();
    }
}

void DirectSubmissionController::checkNewSubmissions() {
    auto timeoutMode = timeoutElapsed();
    if (timeoutMode == TimeoutElapsedMode::notElapsed) {
        return;
    }

    std::lock_guard<std::mutex> lock(this->directSubmissionsMutex);
    bool shouldRecalculateTimeout = false;
    for (auto &directSubmission : this->directSubmissions) {
        auto csr = directSubmission.first;
        auto &state = directSubmission.second;

        if (timeoutMode == TimeoutElapsedMode::bcsOnly && !EngineHelpers::isBcs(csr->getOsContext().getEngineType())) {
            continue;
        }

        auto taskCount = csr->peekTaskCount();
        if (taskCount == state.taskCount) {
            if (state.isStopped) {
                continue;
            }
            auto lock = csr->obtainUniqueOwnership();
            if (!isCsrIdleDetectionEnabled || isDirectSubmissionIdle(csr, lock)) {
                csr->stopDirectSubmission(false);
                state.isStopped = true;
                shouldRecalculateTimeout = true;
                this->lowestThrottleSubmitted = QueueThrottle::HIGH;
            }
            state.taskCount = csr->peekTaskCount();
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

    if (timeoutMode != TimeoutElapsedMode::bcsOnly) {
        this->timeSinceLastCheck = getCpuTimestamp();
    }
}

bool DirectSubmissionController::isDirectSubmissionIdle(CommandStreamReceiver *csr, std::unique_lock<std::recursive_mutex> &csrLock) {
    if (csr->peekLatestFlushedTaskCount() == csr->peekTaskCount()) {
        return !csr->isBusyWithoutHang(lastHangCheckTime);
    }

    csr->flushTagUpdate();

    auto osTime = csr->peekRootDeviceEnvironment().osTime.get();
    uint64_t currCpuTimeInNS;
    osTime->getCpuTime(&currCpuTimeInNS);
    auto timeToWait = currCpuTimeInNS + timeToPollTagUpdateNS;

    // unblock csr during polling
    csrLock.unlock();
    while (currCpuTimeInNS < timeToWait) {
        if (!csr->isBusyWithoutHang(lastHangCheckTime)) {
            break;
        }
        osTime->getCpuTime(&currCpuTimeInNS);
    }
    csrLock.lock();
    return !csr->isBusyWithoutHang(lastHangCheckTime);
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

void DirectSubmissionController::enqueueWaitForPagingFence(CommandStreamReceiver *csr, uint64_t pagingFenceValue) {
    std::lock_guard lock(this->condVarMutex);
    pagingFenceRequests.push({csr, pagingFenceValue});
    condVar.notify_one();
}

void DirectSubmissionController::drainPagingFenceQueue() {
    std::lock_guard lock(this->condVarMutex);

    while (!pagingFenceRequests.empty()) {
        auto request = pagingFenceRequests.front();
        pagingFenceRequests.pop();
        request.csr->unblockPagingFenceSemaphore(request.pagingFenceValue);
    }
}

void DirectSubmissionController::handlePagingFenceRequests(std::unique_lock<std::mutex> &lock, bool checkForNewSubmissions) {
    UNRECOVERABLE_IF(!lock.owns_lock())
    while (!pagingFenceRequests.empty()) {
        auto request = pagingFenceRequests.front();
        pagingFenceRequests.pop();
        lock.unlock();

        request.csr->unblockPagingFenceSemaphore(request.pagingFenceValue);
        if (checkForNewSubmissions) {
            checkNewSubmissions();
        }

        lock.lock();
    }
}

TimeoutElapsedMode DirectSubmissionController::timeoutElapsed() {
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(getCpuTimestamp() - this->timeSinceLastCheck);
    if (diff >= this->timeout) {
        return TimeoutElapsedMode::fullyElapsed;
    } else if (this->bcsTimeoutDivisor > 1 && diff >= (this->timeout / this->bcsTimeoutDivisor)) {
        return TimeoutElapsedMode::bcsOnly;
    }

    return TimeoutElapsedMode::notElapsed;
}
} // namespace NEO
