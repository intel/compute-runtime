/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/event/async_events_handler.h"

#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/os_thread.h"
#include "opencl/source/event/event.h"

#include <iterator>

namespace NEO {
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
    uint32_t lowestTaskCount = CompletionStamp::levelNotReady;
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
} // namespace NEO
