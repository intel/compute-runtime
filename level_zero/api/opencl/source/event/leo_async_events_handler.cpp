/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/event/leo_async_events_handler.h"

#include "shared/source/os_interface/os_thread.h"

#include "level_zero/api/opencl/source/event/leo_event.h"

#include <algorithm>
#include <thread>

namespace NEO {
namespace LEO {

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
    openThread();

    event->incRefInternal();
    registerList.push_back(event);
    asyncCond.notify_one();
}

Event *AsyncEventsHandler::processList() {
    Event *sleepCandidate = nullptr;
    pendingList.clear();

    for (auto event : list) {
        event->updateExecutionStatus();
        if (event->peekHasCallbacks()) {
            pendingList.push_back(event);
            if (sleepCandidate == nullptr) {
                sleepCandidate = event;
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
            self->drainAndReleaseEvents(lock);
            break;
        }
        if (self->list.empty()) {
            self->asyncCond.wait(lock);
        }
        lock.unlock();

        sleepCandidate = self->processList();
        if (sleepCandidate) {
            auto result = sleepCandidate->wait(Event::asyncCompletionWaitTimeoutNs);
            if ((result != ZE_RESULT_SUCCESS) && (result != ZE_RESULT_NOT_READY)) {
                sleepCandidate->abortExecutionDueToGpuHang();
            }
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
        thread->join();
        thread.reset(nullptr);
    }
}

void AsyncEventsHandler::openThread() {
    if (!thread.get()) {
        DEBUG_BREAK_IF(allowAsyncProcess);
        allowAsyncProcess = true;
        thread = Thread::createFunc(asyncProcess, reinterpret_cast<void *>(this));
    }
}

void AsyncEventsHandler::transferRegisterList() {
    // An event is registered once per added callback, so the same event can show up here more than
    // once - either several times within this batch or again while it is already being tracked.
    for (auto event : registerList) {
        if (std::find(list.begin(), list.end(), event) == list.end()) {
            list.push_back(event); // adopt the reference registerEvent() took
        } else {
            event->decRefInternal(); // already tracked - hand the reference back
        }
    }
    registerList.clear();
}

void AsyncEventsHandler::drainAndReleaseEvents(std::unique_lock<std::mutex> &lock) {
    DEBUG_BREAK_IF(!lock.owns_lock());

    auto detachedList = std::move(list);
    list.clear();
    UNRECOVERABLE_IF(!registerList.empty())

    lock.unlock();

    for (auto event : detachedList) {
        event->updateExecutionStatus();
        event->decRefInternal();
    }

    lock.lock();
}

} // namespace LEO
} // namespace NEO
