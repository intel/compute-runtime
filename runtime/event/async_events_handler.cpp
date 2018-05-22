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

#include "runtime/event/async_events_handler.h"
#include "runtime/event/event.h"
#include "runtime/os_interface/os_thread.h"
#include <iterator>

namespace OCLRT {
AsyncEventsHandler::AsyncEventsHandler() {
    allowAsyncProcess = false;
    registerList.reserve(64);
    list.reserve(64);
    pendingList.reserve(64);
}

AsyncEventsHandler::~AsyncEventsHandler() {
    closeThread();
}

void AsyncEventsHandler::registerEvent(Event *event) {
    std::unique_lock<std::mutex> lock(asyncMtx);
    //Create on first use
    openThread();

    event->incRefInternal();
    registerList.push_back(event);
    asyncCond.notify_one();
}

Event *AsyncEventsHandler::processList() {
    uint32_t lowestTaskCount = Event::eventNotReady;
    Event *sleepCandidate = nullptr;
    pendingList.clear();

    for (auto event : list) {
        event->updateExecutionStatus();
        if (event->peekHasCallbacks() || (event->isExternallySynchronized() && (event->peekExecutionStatus() > CL_COMPLETE))) {
            pendingList.push_back(event);
            if (event->peekTaskCount() < lowestTaskCount) {
                sleepCandidate = event;
                lowestTaskCount = event->peekTaskCount();
            }
        } else {
            event->decRefInternal();
        }
    }

    list.swap(pendingList);
    return sleepCandidate;
}

void *AsyncEventsHandler::asyncProcess(void *arg) {
    auto self = reinterpret_cast<AsyncEventsHandler *>(arg);
    std::unique_lock<std::mutex> lock(self->asyncMtx, std::defer_lock);
    Event *sleepCandidate = nullptr;

    while (true) {
        lock.lock();
        self->transferRegisterList();
        if (!self->allowAsyncProcess) {
            self->processList();
            self->releaseEvents();
            break;
        }
        if (self->list.empty()) {
            self->asyncCond.wait(lock);
        }
        lock.unlock();

        sleepCandidate = self->processList();
        if (sleepCandidate) {
            sleepCandidate->wait(true, true);
        }
        std::this_thread::yield();
    }
    return nullptr;
}

void AsyncEventsHandler::closeThread() {
    std::unique_lock<std::mutex> lock(asyncMtx);
    if (allowAsyncProcess) {
        allowAsyncProcess = false;
        asyncCond.notify_one();
        lock.unlock();
        thread.get()->join();
        thread.reset(nullptr);
    }
}

void AsyncEventsHandler::openThread() {
    if (!thread.get()) {
        DEBUG_BREAK_IF(allowAsyncProcess);
        allowAsyncProcess = true;
        thread = Thread::create(asyncProcess, reinterpret_cast<void *>(this));
    }
}

void AsyncEventsHandler::transferRegisterList() {
    std::move(registerList.begin(), registerList.end(), std::back_inserter(list));
    registerList.clear();
}

void AsyncEventsHandler::releaseEvents() {
    for (auto event : list) {
        event->decRefInternal();
    }
    list.clear();
    UNRECOVERABLE_IF(!registerList.empty()) // transferred before release
}
} // namespace OCLRT
