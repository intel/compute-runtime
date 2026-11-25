/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/mockable_virtual_defaults.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace NEO {
class DeferrableDeletion;
class Thread;
class DeferredDeleter : NEO::NonCopyableAndNonMovableClass {
  public:
    DeferredDeleter();
    virtual ~DeferredDeleter();

    MOCKABLE_VIRTUAL void deferDeletion(DeferrableDeletion *deletion);

    MOCKABLE_VIRTUAL void addClient();

    MOCKABLE_VIRTUAL void removeClient();

    bool deferDeletionIfHasClients(std::unique_ptr<DeferrableDeletion> &deletion);

    MOCKABLE_VIRTUAL void drain(bool blocking, bool hostptrsOnly);

    MOCKABLE_VIRTUAL bool areElementsReleased(bool hostptrsOnly);

  protected:
    void stop();
    void safeStop();
    void ensureThread();
    MOCKABLE_VIRTUAL void clearQueue(bool hostptrsOnly);
    MOCKABLE_VIRTUAL bool shouldStop();
    bool isQueueEmpty();

    static void *run(void *);

    std::atomic<bool> exitedMainLoop = false;
    std::atomic<bool> doWorkInBackground = false;
    std::atomic<int> elementsToRelease = 0;
    std::atomic<int> hostptrsToRelease = 0;
    std::unique_ptr<Thread> worker;
    int32_t numClients = 0;
    std::vector<std::unique_ptr<DeferrableDeletion>> queue;
    std::mutex deletionsMutex;
    std::mutex queueMutex;
    std::mutex threadMutex;
    std::condition_variable condition;
};

static_assert(NEO::NonCopyableAndNonMovable<DeferredDeleter>);

} // namespace NEO
