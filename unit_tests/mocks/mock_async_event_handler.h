/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/event/async_events_handler.h"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <iterator>

using namespace OCLRT;

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
    }

    MockHandler(bool allowAsync = false) : AsyncEventsHandler() {
        allowThreadCreating = allowAsync;
        transferCounter.store(0);
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
