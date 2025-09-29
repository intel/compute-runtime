/*
 * Copyright (C) 2019-2025 Intel Corporation
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
    isCsrsContextGroupIdleDetectionEnabled = true;
    if (debugManager.flags.DirectSubmissionControllerContextGroupIdleDetection.get() != -1) {
        isCsrsContextGroupIdleDetectionEnabled = debugManager.flags.DirectSubmissionControllerContextGroupIdleDetection.get();
    }
};

DirectSubmissionController::~DirectSubmissionController() {
    UNRECOVERABLE_IF(directSubmissionControllingThread);
}

void DirectSubmissionController::registerDirectSubmission(CommandStreamReceiver *csr) {
    std::lock_guard<std::mutex> lock(directSubmissionsMutex);
    directSubmissions.insert(std::make_pair(csr, DirectSubmissionState()));
    this->overrideDirectSubmissionTimeouts(csr->getProductHelper());
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
    std::optional<TaskCountType> bcsTaskCount{};
    for (auto &directSubmission : this->directSubmissions) {
        auto csr = directSubmission.first;
        auto &state = directSubmission.second;
        auto isBcs = EngineHelpers::isBcs(csr->getOsContext().getEngineType());
        if (timeoutMode == TimeoutElapsedMode::bcsOnly && !isBcs) {
            continue;
        }
        if (isBcs) {
            bcsTaskCount = state.taskCount;
        }
        auto taskCount = csr->peekTaskCount();
        if (taskCount == state.taskCount) {
            if (state.isStopped) {
                continue;
            }
            bool isCopyEngineIdle = true;
            if (!isBcs && csr->getProductHelper().checkBcsForDirectSubmissionStop()) {
                isCopyEngineIdle = isCopyEngineOnDeviceIdle(csr->getRootDeviceIndex(), bcsTaskCount);
            }
            auto lock = csr->tryObtainUniqueOwnership();
            if (!lock.owns_lock()) {
                continue; // Skip this CSR if contended - avoid deadlock
            }
            if (!isCsrIdleDetectionEnabled || (isDirectSubmissionIdle(csr, lock) && isCopyEngineIdle)) {
                csr->stopDirectSubmission(false, false);
                state.isStopped = true;
                shouldRecalculateTimeout = true;
            }
            state.taskCount = csr->peekTaskCount();
        } else {
            state.isStopped = false;
            state.taskCount = taskCount;
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
    // Helper lambda to check if a single CSR is idle (with flush+poll)
    auto checkCSRIdle = [this](CommandStreamReceiver *checkCsr, std::unique_lock<std::recursive_mutex> &lock) -> bool {
        if (checkCsr->peekLatestFlushedTaskCount() == checkCsr->peekTaskCount()) {
            return !checkCsr->isBusyWithoutHang(lastHangCheckTime);
        }

        checkCsr->flushTagUpdate();
        auto osTime = checkCsr->peekRootDeviceEnvironment().osTime.get();
        uint64_t currCpuTimeInNS;
        osTime->getCpuTime(&currCpuTimeInNS);
        auto timeToWait = currCpuTimeInNS + timeToPollTagUpdateNS;

        // unblock csr during polling
        lock.unlock();
        while (currCpuTimeInNS < timeToWait) {
            if (!checkCsr->isBusyWithoutHang(lastHangCheckTime)) {
                break;
            }
            osTime->getCpuTime(&currCpuTimeInNS);
        }
        lock.lock();
        return !checkCsr->isBusyWithoutHang(lastHangCheckTime);
    };

    // Check if THIS CSR is idle
    if (!checkCSRIdle(csr, csrLock)) {
        return false;
    }

    // If context group optimization is disabled OR CSR is not part of a context group, use original behavior
    if (!isCsrsContextGroupIdleDetectionEnabled || !csr->getOsContext().isPartOfContextGroup()) {
        return true;
    }

    // Release the main CSR lock before checking other CSRs to avoid deadlock
    csrLock.unlock();

    // Check if all OTHER CSRs in the same context group are idle
    auto myKey = ContextGroupKey{csr->getRootDeviceIndex(), csr->getContextGroupId()};
    bool allOthersIdle = true;

    for (auto &entry : directSubmissions) {
        auto *otherCsr = entry.first;
        if (otherCsr == csr)
            continue; // Skip self

        auto otherKey = ContextGroupKey{otherCsr->getRootDeviceIndex(), otherCsr->getContextGroupId()};
        if (otherKey == myKey) {
            auto otherLock = otherCsr->tryObtainUniqueOwnership();
            if (!otherLock.owns_lock()) {
                allOthersIdle = false;
                break; // Treat contended CSR as active - avoid deadlock
            }
            if (!checkCSRIdle(otherCsr, otherLock)) {
                allOthersIdle = false;
                break; // Early exit for performance
            }
        }
    }

    // Re-acquire the main CSR lock before returning
    csrLock.lock();

    // If other CSRs weren't idle, return false
    if (!allOthersIdle) {
        return false;
    }

    // Double-check that the main CSR is still idle after re-acquiring lock
    if (csr->peekLatestFlushedTaskCount() == csr->peekTaskCount()) {
        return !csr->isBusyWithoutHang(lastHangCheckTime);
    }
    return false;
}

bool DirectSubmissionController::isCopyEngineOnDeviceIdle(uint32_t rootDeviceIndex, std::optional<TaskCountType> &bcsTaskCount) {
    CommandStreamReceiver *bcsCsr = nullptr;
    TaskCountType registeredTaskCount = 0;
    for (auto &directSubmission : this->directSubmissions) {
        auto csr = directSubmission.first;
        if (csr->getRootDeviceIndex() == rootDeviceIndex && EngineHelpers::isBcs(csr->getOsContext().getEngineType())) {
            if (!directSubmission.second.isStopped) {
                registeredTaskCount = bcsTaskCount.value_or(directSubmission.second.taskCount);
                bcsCsr = csr;
            }
            break;
        }
    }
    if (bcsCsr == nullptr) {
        return true;
    }

    // Non-blocking lock attempt
    auto lock = bcsCsr->tryObtainUniqueOwnership();
    if (!lock.owns_lock()) {
        // Could not acquire -> conservatively declare "not idle"
        return false;
    }

    return (bcsCsr->peekTaskCount() == registeredTaskCount) && isDirectSubmissionIdle(bcsCsr, lock);
}

SteadyClock::time_point DirectSubmissionController::getCpuTimestamp() {
    return SteadyClock::now();
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
