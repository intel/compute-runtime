/*
 * Copyright (C) 2019-2026 Intel Corporation
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
    if (debugManager.flags.DirectSubmissionControllerContextGroupTimeout.get() != -1) {
        contextGroupTimeout = std::chrono::microseconds{debugManager.flags.DirectSubmissionControllerContextGroupTimeout.get()};
    }
    if (debugManager.flags.DirectSubmissionControllerBcsTimeoutDivisor.get() != -1) {
        bcsTimeoutDivisor = debugManager.flags.DirectSubmissionControllerBcsTimeoutDivisor.get();
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
    {
        std::lock_guard<std::mutex> structureLock(condVarMutex);
        directSubmissions.emplace(csr, DirectSubmissionState());
    }
    this->overrideDirectSubmissionTimeouts(csr->getProductHelper());

    if (debugManager.flags.DirectSubmissionControllerContextGroupIdleDetection.get() == -1 &&
        !csr->getProductHelper().isDirectSubmissionContextGroupIdleDetectionEnabled()) {
        isCsrsContextGroupIdleDetectionEnabled = false;
    }
}

void DirectSubmissionController::overrideDirectSubmissionTimeouts(const ProductHelper &productHelper) {
    uint64_t timeoutUs = this->timeout.count();
    productHelper.overrideDirectSubmissionTimeouts(timeoutUs);
    this->timeout = std::chrono::microseconds(timeoutUs);
}

void DirectSubmissionController::unregisterDirectSubmission(CommandStreamReceiver *csr) {
    std::lock_guard<std::mutex> lock(directSubmissionsMutex);
    std::lock_guard<std::mutex> structureLock(condVarMutex);
    directSubmissions.erase(csr);
}

void DirectSubmissionController::startThread() {
    directSubmissionControllingThread = Thread::createFunc(controlDirectSubmissionsState, reinterpret_cast<void *>(this));
}

void DirectSubmissionController::stopThread() {
    {
        std::lock_guard<std::mutex> lock(condVarMutex);
        keepControlling.store(false);
        condVar.notify_one();
    }
    if (directSubmissionControllingThread) {
        directSubmissionControllingThread->join();
        directSubmissionControllingThread.reset();
    }
}

void *DirectSubmissionController::controlDirectSubmissionsState(void *self) {
    auto controller = reinterpret_cast<DirectSubmissionController *>(self);

    controller->timeSinceLastCheck = controller->getCpuTimestamp();
    controller->lastHangCheckTime = CommandStreamReceiver::getCurrentTimeInNs();

    while (controller->keepControlling.load()) {
        std::unique_lock<std::mutex> lock(controller->condVarMutex);
        controller->wait(lock);
        controller->handlePagingFenceRequests(lock);
        controller->sleep(lock);
        controller->handlePagingFenceRequests(lock);
        lock.unlock();
        controller->checkNewSubmissions();
    }

    return nullptr;
}

void DirectSubmissionController::notifyNewSubmission(const CommandStreamReceiver *csr) {
    std::lock_guard<std::mutex> lock(condVarMutex);
    auto it = directSubmissions.find(const_cast<CommandStreamReceiver *>(csr));
    if (it != directSubmissions.end()) {
        ++activeSubmissionsCount;
        it->second.isActive = true;
    }
    condVar.notify_one();
}

void DirectSubmissionController::checkNewSubmissions() {
    auto timeoutMode = timeoutElapsed();
    if (timeoutMode == TimeoutElapsedMode::notElapsed) {
        return;
    }

    std::lock_guard<std::mutex> lock(this->directSubmissionsMutex);
    std::optional<TaskCountType> bcsTaskCount{};

    const auto now = getCpuTimestamp();
    if (isCsrsContextGroupIdleDetectionEnabled) {
        updateContextGroupsProgress(now);
    }

    for (auto &[csr, state] : directSubmissions) {
        if (!state.isActive) {
            continue;
        }

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
            if (isCsrIdleDetectionEnabled && hasContextGroupProgressed(csr)) {
                state.idleSince.reset();
                continue;
            }
            bool isCopyEngineIdle = true;
            if (!isBcs && csr->getProductHelper().checkBcsForDirectSubmissionStop()) {
                isCopyEngineIdle = isCopyEngineOnDeviceIdle(csr->getRootDeviceIndex(), bcsTaskCount);
            }
            auto lock = csr->obtainUniqueOwnership();
            if (!isCsrIdleDetectionEnabled || (isDirectSubmissionIdle(csr, lock) && isCopyEngineIdle)) {
                const auto idleTimeout = isBcs ? this->timeout / this->bcsTimeoutDivisor : this->timeout;
                if (!state.idleSince.has_value()) {
                    state.idleSince = getCpuTimestamp();
                } else if (now - state.idleSince.value() >= idleTimeout) {
                    csr->stopDirectSubmission(false, false);
                    state.isActive = false;
                    state.isStopped = true;
                    state.idleSince.reset();
                    --activeSubmissionsCount;
                }
            } else {
                state.idleSince.reset();
            }
            state.taskCount = csr->peekTaskCount();
        } else {
            if (isCsrsContextGroupIdleDetectionEnabled && csr->getOsContext().isPartOfContextGroup()) {
                groupLastActivityTime[csr->getContextGroupId()] = now;
            }

            state.isStopped = false;
            state.idleSince = now;
            state.taskCount = taskCount;
        }
    }
    if (timeoutMode != TimeoutElapsedMode::bcsOnly) {
        this->timeSinceLastCheck = getCpuTimestamp();
    }
}

void DirectSubmissionController::updateContextGroupsProgress(SteadyClock::time_point now) {
    for (auto &[contextGroupId, progress] : this->contextGroupsProgress) {
        progress.currentTaskCountsSum = 0;
    }
    for (auto &[csr, state] : this->directSubmissions) {
        if (!csr->getOsContext().isPartOfContextGroup()) {
            continue;
        }
        const auto taskCount = csr->peekTaskCount();
        this->contextGroupsProgress[csr->getContextGroupId()].currentTaskCountsSum += taskCount;
        if (state.isActive && taskCount != state.taskCount) {
            this->groupLastActivityTime[csr->getContextGroupId()] = now;
        }
    }
    for (auto &[contextGroupId, progress] : this->contextGroupsProgress) {
        progress.hasProgressed = progress.currentTaskCountsSum != progress.taskCountsSum;
        progress.taskCountsSum = progress.currentTaskCountsSum;
    }
}

bool DirectSubmissionController::hasContextGroupProgressed(CommandStreamReceiver *csr) {
    return this->isCsrsContextGroupIdleDetectionEnabled && csr->getOsContext().isPartOfContextGroup() &&
           this->contextGroupsProgress[csr->getContextGroupId()].hasProgressed;
}

bool DirectSubmissionController::isCsrIdleWithoutTagFlush(CommandStreamReceiver *csr) {
    return csr->peekLatestFlushedTaskCount() == csr->peekTaskCount() && !csr->isBusyWithoutHang(this->lastHangCheckTime);
}

// Helper to check if a single CSR is idle (with flush+poll)
bool DirectSubmissionController::isCsrIdleAfterTagFlush(CommandStreamReceiver *csr, std::unique_lock<std::recursive_mutex> &csrLock) {
    if (csr->peekLatestFlushedTaskCount() == csr->peekTaskCount()) {
        return !csr->isBusyWithoutHang(this->lastHangCheckTime);
    }

    csr->flushTagUpdate();
    auto osTime = csr->peekRootDeviceEnvironment().osTime.get();
    uint64_t currCpuTimeInNS;
    osTime->getCpuTime(&currCpuTimeInNS);
    auto timeToWait = currCpuTimeInNS + timeToPollTagUpdateNS;

    // unblock csr during polling
    csrLock.unlock();
    while (currCpuTimeInNS < timeToWait) {
        if (!csr->isBusyWithoutHang(this->lastHangCheckTime)) {
            break;
        }
        osTime->getCpuTime(&currCpuTimeInNS);
    }
    csrLock.lock();
    return !csr->isBusyWithoutHang(this->lastHangCheckTime);
}

bool DirectSubmissionController::isDirectSubmissionIdle(CommandStreamReceiver *csr, std::unique_lock<std::recursive_mutex> &csrLock) {
    const bool contextGroupIdleDetectionEnabled = isCsrsContextGroupIdleDetectionEnabled && csr->getOsContext().isPartOfContextGroup();
    if (contextGroupIdleDetectionEnabled) {
        auto groupLastActivity = groupLastActivityTime.find(csr->getContextGroupId());
        if (groupLastActivity != groupLastActivityTime.end() &&
            (getCpuTimestamp() - groupLastActivity->second) < contextGroupTimeout) {
            return false;
        }
    }

    // Check if THIS CSR is idle
    if (!isCsrIdleAfterTagFlush(csr, csrLock)) {
        return false;
    }

    // If context group optimization is disabled OR CSR is not part of a context group, use original behavior
    if (!contextGroupIdleDetectionEnabled) {
        return true;
    }

    // Release the main CSR lock before checking other CSRs to avoid deadlock
    csrLock.unlock();

    // Check if all OTHER CSRs in the same context group are idle
    const bool otherMembersIdle = areOtherContextGroupMembersIdle(csr);

    // Re-acquire the main CSR lock before returning
    csrLock.lock();

    // If other CSRs weren't idle, return false
    if (!otherMembersIdle) {
        return false;
    }

    // Double-check that the main CSR is still idle after re-acquiring lock
    return isCsrIdleWithoutTagFlush(csr);
}

bool DirectSubmissionController::areOtherContextGroupMembersIdle(CommandStreamReceiver *csr) {
    const auto contextGroupId = csr->getContextGroupId();
    for (auto &[otherCsr, otherState] : this->directSubmissions) {
        if (otherCsr == csr) {
            continue; // Skip self
        }
        if (!otherCsr->getOsContext().isPartOfContextGroup() ||
            otherCsr->getContextGroupId() != contextGroupId) {
            continue;
        }
        if (!otherState.isActive) {
            continue; // Only consider active CSRs in the group.
        }
        auto otherLock = otherCsr->tryObtainUniqueOwnership();
        if (!otherLock.owns_lock()) {
            return false; // Treat contended CSR as active.
        }
        if (!isCsrIdleAfterTagFlush(otherCsr, otherLock)) {
            return false; // Early exit for performance
        }
    }
    return true;
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

void DirectSubmissionController::enqueueWaitForPagingFence(CommandStreamReceiver *csr, uint64_t pagingFenceValue) {
    std::lock_guard lock(condVarMutex);
    pagingFenceRequests.push({csr, pagingFenceValue});
    condVar.notify_one();
}

void DirectSubmissionController::drainPagingFenceQueue() {
    std::lock_guard lock(condVarMutex);

    while (!pagingFenceRequests.empty()) {
        auto request = pagingFenceRequests.front();
        pagingFenceRequests.pop();
        request.csr->unblockPagingFenceSemaphore(request.pagingFenceValue);
    }
}

void DirectSubmissionController::handlePagingFenceRequests(std::unique_lock<std::mutex> &lock) {
    UNRECOVERABLE_IF(!lock.owns_lock())
    while (!pagingFenceRequests.empty()) {
        auto request = pagingFenceRequests.front();
        pagingFenceRequests.pop();
        lock.unlock();
        request.csr->unblockPagingFenceSemaphore(request.pagingFenceValue);
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
