/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/mockable_virtual_defaults.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace NEO {
class Event;
class Thread;

class AsyncEventsHandler {
  public:
    AsyncEventsHandler();
    virtual ~AsyncEventsHandler();
    void registerEvent(Event *event);
    void closeThread();

  protected:
    Event *processList();
    static void *asyncProcess(void *arg);
    void releaseEvents();
    MOCKABLE_VIRTUAL void openThread();
    MOCKABLE_VIRTUAL void transferRegisterList();
    std::vector<Event *> registerList;
    std::vector<Event *> list;
    std::vector<Event *> pendingList;

    std::unique_ptr<Thread> thread;
    std::mutex asyncMtx;
    std::condition_variable asyncCond;
    std::atomic<bool> allowAsyncProcess;
};
} // namespace NEO
