/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include <level_zero/ze_api.h>

#include <atomic>
#include <limits>
#include <sstream>

namespace L0 {

class EuThread {
  public:
    enum class State {
        running,
        stopped,
        unavailable
    };

    struct ThreadId {
        union {
            struct {
                uint64_t thread : 4;
                uint64_t eu : 5;
                uint64_t subslice : 10;
                uint64_t slice : 10;
                uint64_t tileIndex : 2;
                uint64_t reserved : 33;
            };
            uint64_t packed;
        };

        ThreadId(uint32_t tile, uint32_t slice, uint32_t subslice, uint32_t eu, uint32_t thread) {
            this->packed = 0;
            this->tileIndex = tile;
            this->slice = slice;
            this->subslice = subslice;
            this->eu = eu;
            this->thread = thread;
        }

        ThreadId(uint32_t tile, ze_device_thread_t thread) {
            this->packed = 0;
            this->tileIndex = tile;
            this->slice = thread.slice;
            this->subslice = thread.subslice;
            this->eu = thread.eu;
            this->thread = thread.thread;
        }

        operator uint64_t() const {
            return packed;
        }
    };

    virtual ~EuThread() = default;

    EuThread(ThreadId threadId) : threadId(threadId) {}

    bool stopThread(uint64_t memHandle) {
        memoryHandle = memHandle;
        if (state == State::stopped) {
            return false;
        }
        state = State::stopped;
        PRINT_DEBUGGER_THREAD_LOG("Stopped thread: %s", toString().c_str());
        return true;
    }

    bool verifyStopped(uint8_t newCounter) {

        PRINT_DEBUGGER_THREAD_LOG("EuThread::verifyStopped() Thread: %s newCounter == %d oldCounter == %d", toString().c_str(), (int32_t)newCounter, (int32_t)systemRoutineCounter);

        if (newCounter == systemRoutineCounter) {
            if (newCounter % 2 != 0) {
                if (state == State::running) {
                    PRINT_DEBUGGER_ERROR_LOG("Thread: %s state RUNNING when thread is stopped. Switching to STOPPED", toString().c_str());
                    DEBUG_BREAK_IF(true);
                }
                state = State::stopped;
                return true;
            }
        }

        if (newCounter == (systemRoutineCounter + 2)) {
            state = State::stopped;
            systemRoutineCounter = newCounter;
            return true;
        } else if (newCounter > systemRoutineCounter + 2) {
            PRINT_DEBUGGER_ERROR_LOG("Thread: %s state out of sync.", toString().c_str());
            DEBUG_BREAK_IF(true);
        }

        if (newCounter % 2 == 0) {
            if (state == State::stopped) {
                PRINT_DEBUGGER_ERROR_LOG("Thread: %s state STOPPED when thread is running. Switching to RUNNING", toString().c_str());
                DEBUG_BREAK_IF(true);
            }
            state = State::running;
            systemRoutineCounter = newCounter;

            return false;
        }

        state = State::stopped;
        systemRoutineCounter = newCounter;
        return true;
    }

    bool resumeThread() {
        if (state != State::stopped) {
            PRINT_DEBUGGER_THREAD_LOG("Resuming already RUNNING thread: %s", toString().c_str());
            return false;
        }
        PRINT_DEBUGGER_THREAD_LOG("Resumed thread: %s", toString().c_str());
        reportedAsStopped = false;
        state = State::running;
        memoryHandle = invalidHandle;
        return true;
    }

    bool isStopped() const {
        return state == State::stopped;
    }

    bool isRunning() const {
        return state != State::stopped;
    }

    static std::string toString(ThreadId threadId) {
        std::stringstream threadString;
        threadString << "device index = " << threadId.tileIndex << " slice = " << threadId.slice << " subslice = " << threadId.subslice << " eu = " << threadId.eu
                     << " thread = " << threadId.thread;
        return threadString.str();
    }

    std::string toString() const {
        return toString(threadId);
    }

    ThreadId getThreadId() const {
        return threadId;
    }

    uint64_t getMemoryHandle() const { return memoryHandle; }

    uint8_t getLastCounter() const {
        return systemRoutineCounter;
    }
    void reportAsStopped() {
        reportedAsStopped = true;
    }
    bool isReportedAsStopped() {
        DEBUG_BREAK_IF(reportedAsStopped && state != State::stopped);
        return reportedAsStopped;
    }
    void setPageFault(bool value) {
        hasPageFault = value;
    }
    bool getPageFault() {
        return hasPageFault;
    }
    void setContextHandle(uint64_t contextHandleParam) {
        contextHandle = contextHandleParam;
    }
    void getContextHandle(uint64_t &contextHandleParam) const {
        DEBUG_BREAK_IF(state != State::stopped);
        contextHandleParam = contextHandle;
    }
    void setLrcHandle(uint64_t lrcHandleParam) {
        lrcHandle = lrcHandleParam;
    }
    void getLrcHandle(uint64_t &lrcHandleParam) const {
        DEBUG_BREAK_IF(state != State::stopped);
        lrcHandleParam = lrcHandle;
    }

  public:
    static constexpr uint64_t invalidHandle = std::numeric_limits<uint64_t>::max();

  protected:
    ThreadId threadId;
    std::atomic<State> state = State::unavailable;
    uint8_t systemRoutineCounter = 0;
    std::atomic<uint64_t> memoryHandle = invalidHandle;
    std::atomic<bool> reportedAsStopped = false;
    bool hasPageFault = false;
    uint64_t contextHandle = 0;
    uint64_t lrcHandle = 0;
};

static_assert(sizeof(EuThread::ThreadId) == sizeof(uint64_t));
} // namespace L0
