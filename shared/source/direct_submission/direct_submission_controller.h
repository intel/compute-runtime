/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace NEO {
class MemoryManager;
class CommandStreamReceiver;
class Thread;

class DirectSubmissionController {
  public:
    DirectSubmissionController();
    virtual ~DirectSubmissionController();

    void registerDirectSubmission(CommandStreamReceiver *csr);
    void unregisterDirectSubmission(CommandStreamReceiver *csr);

    void startControlling();

    static bool isSupported();

  protected:
    struct DirectSubmissionState {
        bool isStopped = true;
        uint32_t taskCount = 0u;
    };

    static void *controlDirectSubmissionsState(void *self);
    void checkNewSubmissions();
    MOCKABLE_VIRTUAL void sleep();

    void adjustTimeout(CommandStreamReceiver *csr);

    uint32_t maxCcsCount = 1u;
    std::array<uint32_t, DeviceBitfield().size()> ccsCount = {};
    std::unordered_map<CommandStreamReceiver *, DirectSubmissionState> directSubmissions;
    std::mutex directSubmissionsMutex;

    std::unique_ptr<Thread> directSubmissionControllingThread;
    std::atomic_bool keepControlling = true;
    std::atomic_bool runControlling = false;

    int timeout = 5000;
    int timeoutDivisor = 4;
};
} // namespace NEO