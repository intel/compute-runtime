/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/device_bitfield.h"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace NEO {
class MemoryManager;
class CommandStreamReceiver;
class Thread;

using SteadyClock = std::chrono::steady_clock;

class DirectSubmissionController {
  public:
    static constexpr size_t defaultTimeout = 5'000;
    DirectSubmissionController();
    virtual ~DirectSubmissionController();

    void registerDirectSubmission(CommandStreamReceiver *csr);
    void unregisterDirectSubmission(CommandStreamReceiver *csr);

    void startControlling();

    static bool isSupported();

  protected:
    struct DirectSubmissionState {
        bool isStopped = true;
        TaskCountType taskCount = 0u;
    };

    static void *controlDirectSubmissionsState(void *self);
    void checkNewSubmissions();
    MOCKABLE_VIRTUAL void sleep();
    MOCKABLE_VIRTUAL SteadyClock::time_point getCpuTimestamp();

    void adjustTimeout(CommandStreamReceiver *csr);
    void recalculateTimeout();

    uint32_t maxCcsCount = 1u;
    std::array<uint32_t, DeviceBitfield().size()> ccsCount = {};
    std::unordered_map<CommandStreamReceiver *, DirectSubmissionState> directSubmissions;
    std::mutex directSubmissionsMutex;

    std::unique_ptr<Thread> directSubmissionControllingThread;
    std::atomic_bool keepControlling = true;
    std::atomic_bool runControlling = false;

    SteadyClock::time_point lastTerminateCpuTimestamp{};
    std::chrono::microseconds maxTimeout{200'000};
    std::chrono::microseconds timeout{defaultTimeout};
    int timeoutDivisor = 1;
};
} // namespace NEO