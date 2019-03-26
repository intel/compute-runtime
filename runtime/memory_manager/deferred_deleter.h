/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/utilities/idlist.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <utility>

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

    MOCKABLE_VIRTUAL void drain(bool blocking);

  protected:
    void stop();
    void safeStop();
    void ensureThread();
    MOCKABLE_VIRTUAL void clearQueue();
    MOCKABLE_VIRTUAL bool areElementsReleased();
    MOCKABLE_VIRTUAL bool shouldStop();

    static void *run(void *);

    std::atomic<bool> doWorkInBackground;
    std::atomic<int> elementsToRelease;
    std::unique_ptr<Thread> worker;
    int32_t numClients = 0;
    IDList<DeferrableDeletion, true> queue;
    std::mutex queueMutex;
    std::mutex threadMutex;
    std::condition_variable condition;
};
} // namespace NEO
