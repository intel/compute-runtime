/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/device_bitfield.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace NEO {
class MemoryManager;
class CommandStreamReceiver;
class Thread;
class ProductHelper;

using SteadyClock = std::chrono::steady_clock;
using HighResolutionClock = std::chrono::high_resolution_clock;

struct WaitForPagingFenceRequest {
    CommandStreamReceiver *csr;
    uint64_t pagingFenceValue;
};

enum class TimeoutElapsedMode {
    notElapsed,
    bcsOnly,
    fullyElapsed
};

class DirectSubmissionController {
  public:
    static constexpr size_t defaultTimeout = 5'000;
    static constexpr size_t timeToPollTagUpdateNS = 20'000;
    DirectSubmissionController();
    virtual ~DirectSubmissionController();

    void registerDirectSubmission(CommandStreamReceiver *csr);
    void unregisterDirectSubmission(CommandStreamReceiver *csr);

    void startThread();
    void startControlling();
    void stopThread();

    static bool isSupported();

    void enqueueWaitForPagingFence(CommandStreamReceiver *csr, uint64_t pagingFenceValue);
    void drainPagingFenceQueue();

  protected:
    struct DirectSubmissionState {
        DirectSubmissionState(DirectSubmissionState &&other) noexcept {
            isStopped = other.isStopped.load();
            taskCount = other.taskCount.load();
        }
        DirectSubmissionState &operator=(const DirectSubmissionState &other) {
            if (this == &other) {
                return *this;
            }
            this->isStopped = other.isStopped.load();
            this->taskCount = other.taskCount.load();
            return *this;
        }

        DirectSubmissionState() = default;
        ~DirectSubmissionState() = default;

        DirectSubmissionState(const DirectSubmissionState &other) = delete;
        DirectSubmissionState &operator=(DirectSubmissionState &&other) = delete;

        std::atomic_bool isStopped{true};
        std::atomic<TaskCountType> taskCount{0};
    };

    static void *controlDirectSubmissionsState(void *self);
    void checkNewSubmissions();
    bool isDirectSubmissionIdle(CommandStreamReceiver *csr, std::unique_lock<std::recursive_mutex> &csrLock);
    MOCKABLE_VIRTUAL bool sleep(std::unique_lock<std::mutex> &lock);
    MOCKABLE_VIRTUAL SteadyClock::time_point getCpuTimestamp();
    MOCKABLE_VIRTUAL void overrideDirectSubmissionTimeouts(const ProductHelper &productHelper);

    void recalculateTimeout();
    void applyTimeoutForAcLineStatusAndThrottle(bool acLineConnected);
    void updateLastSubmittedThrottle(QueueThrottle throttle);
    size_t getTimeoutParamsMapKey(QueueThrottle throttle, bool acLineStatus);

    void handlePagingFenceRequests(std::unique_lock<std::mutex> &lock, bool checkForNewSubmissions);
    MOCKABLE_VIRTUAL TimeoutElapsedMode timeoutElapsed();
    std::chrono::microseconds getSleepValue() const { return std::chrono::microseconds(this->timeout / this->bcsTimeoutDivisor); }

    uint32_t maxCcsCount = 1u;
    std::array<uint32_t, DeviceBitfield().size()> ccsCount = {};
    std::unordered_map<CommandStreamReceiver *, DirectSubmissionState> directSubmissions;
    std::mutex directSubmissionsMutex;

    std::unique_ptr<Thread> directSubmissionControllingThread;
    std::atomic_bool keepControlling = true;
    std::atomic_bool runControlling = false;

    SteadyClock::time_point timeSinceLastCheck{};
    SteadyClock::time_point lastTerminateCpuTimestamp{};
    HighResolutionClock::time_point lastHangCheckTime{};
    std::chrono::microseconds maxTimeout{defaultTimeout};
    std::chrono::microseconds timeout{defaultTimeout};
    int32_t timeoutDivisor = 1;
    int32_t bcsTimeoutDivisor = 1;
    QueueThrottle lowestThrottleSubmitted = QueueThrottle::HIGH;
    bool isCsrIdleDetectionEnabled = false;

    std::condition_variable condVar;
    std::mutex condVarMutex;

    std::queue<WaitForPagingFenceRequest> pagingFenceRequests;
};
} // namespace NEO