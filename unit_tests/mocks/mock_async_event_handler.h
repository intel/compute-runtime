/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/event/async_events_handler.h"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <iterator>

using namespace OCLRT;
namespace MockAsyncEventHandlerGlobals {
extern bool destructorCalled;
}

class MockHandler : public AsyncEventsHandler {
  public:
    using AsyncEventsHandler::allowAsyncProcess;
    using AsyncEventsHandler::asyncMtx;
    using AsyncEventsHandler::asyncProcess;
    using AsyncEventsHandler::openThread;
    using AsyncEventsHandler::thread;

    ~MockHandler() override {
        if (!allowThreadCreating) {
            asyncProcess(this); // process once for cleanup
        }
        MockAsyncEventHandlerGlobals::destructorCalled = true;
    }

    MockHandler(bool allowAsync = false) : AsyncEventsHandler() {
        allowThreadCreating = allowAsync;
        transferCounter.store(0);
        MockAsyncEventHandlerGlobals::destructorCalled = false;
    }

    Event *process() {
        std::move(registerList.begin(), registerList.end(), std::back_inserter(list));
        registerList.clear();
        return processList();
    }

    void transferRegisterList() override {
        transferCounter++;
        AsyncEventsHandler::transferRegisterList();
    }

    void openThread() override {
        if (allowThreadCreating) {
            AsyncEventsHandler::openThread();
        }
        openThreadCalled = true;
    }

    bool peekIsListEmpty() { return list.size() == 0; }
    bool peekIsRegisterListEmpty() { return registerList.size() == 0; }
    std::atomic<int> transferCounter;
    bool openThreadCalled = false;
    bool allowThreadCreating = false;
};
