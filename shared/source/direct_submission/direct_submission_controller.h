/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace NEO {
class MemoryManager;
class CommandStreamReceiver;

class DirectSubmissionController {
  public:
    DirectSubmissionController();
    virtual ~DirectSubmissionController();

    void registerDirectSubmission(CommandStreamReceiver *csr);
    void unregisterDirectSubmission(CommandStreamReceiver *csr);

  protected:
    struct DirectSubmissionState {
        bool isStopped = false;
        uint32_t taskCount = 0u;
    };

    void controlDirectSubmissionsState();
    void checkNewSubmissions();

    std::unordered_map<CommandStreamReceiver *, DirectSubmissionState> directSubmissions;
    std::mutex directSubmissionsMutex;

    std::thread directSubmissionControllingThread;
    std::atomic_bool keepControlling = true;

    int timeout = 5;
};
} // namespace NEO