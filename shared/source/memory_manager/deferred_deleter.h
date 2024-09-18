/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/idlist.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace NEO {
class DeferrableDeletion;
class Thread;
class DeferredDeleter {
  public:
    DeferredDeleter();
    virtual ~DeferredDeleter();

    DeferredDeleter(const DeferredDeleter &) = delete;
    DeferredDeleter &operator=(const DeferredDeleter &) = delete;

    MOCKABLE_VIRTUAL void deferDeletion(DeferrableDeletion *deletion);

    MOCKABLE_VIRTUAL void addClient();

    MOCKABLE_VIRTUAL void removeClient();

    MOCKABLE_VIRTUAL void drain(bool blocking, bool hostptrsOnly);

  protected:
    void stop();
    void safeStop();
    void ensureThread();
    MOCKABLE_VIRTUAL void clearQueue(bool hostptrsOnly);
    MOCKABLE_VIRTUAL bool areElementsReleased(bool hostptrsOnly);
    MOCKABLE_VIRTUAL bool shouldStop();

    static void *run(void *);

    std::atomic<bool> doWorkInBackground = false;
    std::atomic<int> elementsToRelease = 0;
    std::atomic<int> hostptrsToRelease = 0;
    std::unique_ptr<Thread> worker;
    int32_t numClients = 0;
    IDList<DeferrableDeletion, true> queue;
    std::mutex queueMutex;
    std::mutex threadMutex;
    std::condition_variable condition;
};
} // namespace NEO
