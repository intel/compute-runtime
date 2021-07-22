/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"

#include <level_zero/ze_api.h>

#include <limits>
#include <sstream>
#include <string>
namespace L0 {

class EuThread {
  public:
    enum class State {
        Running,
        Stopped,
        Unavailable
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

    bool stopThread() {
        if (state == State::Stopped) {
            return false;
        }
        state = State::Stopped;
        return true;
    }

    bool resumeThread() {
        if (state != State::Stopped) {
            return false;
        }
        state = State::Running;
        return true;
    }

    bool isStopped() {
        return state == State::Stopped;
    }

    bool isRunning() {
        return state != State::Stopped;
    }

    std::string toString() {
        std::stringstream threadString;
        threadString << "device index = " << threadId.tileIndex << " slice = " << threadId.slice << " subslice = " << threadId.subslice << " eu = " << threadId.eu
                     << " thread = " << threadId.thread;
        return threadString.str();
    }

    ThreadId getThreadId() {
        return threadId;
    }

  protected:
    ThreadId threadId;
    State state = State::Unavailable;
};

static_assert(sizeof(EuThread::ThreadId) == sizeof(uint64_t));
} // namespace L0
