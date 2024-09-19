/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/deferred_deleter.h"

namespace NEO {
class MockDeferredDeleter : public DeferredDeleter {
  public:
    using DeferredDeleter::run;
    MockDeferredDeleter();

    ~MockDeferredDeleter() override;

    void deferDeletion(DeferrableDeletion *deletion) override;

    void addClient() override;

    void removeClient() override;

    void drain(bool blocking, bool hostptrsOnly) override;

    bool areElementsReleased(bool hostptrsOnly) override;

    bool shouldStop() override;

    void drain();

    int getClientsNum();

    int getElementsToRelease();

    bool isWorking();

    bool isThreadRunning();

    bool isQueueEmpty();

    void setElementsToRelease(int elementsNum);

    void setDoWorkInBackgroundValue(bool value);

    bool baseAreElementsReleased();

    bool baseShouldStop();

    Thread *getThreadHandle();

    void runThread();

    int drainCalled = 0;

    int areElementsReleasedCalled = 0;

    bool areElementsReleasedCalledForHostptrs = false;

    std::atomic<int> shouldStopCalled;

    std::atomic<int> clearCalled;

    int deferDeletionCalled = 0;
    bool stopAfter3loopsInRun = false;

    void forceStop();

    void allowEarlyStopThread();

    void expectDrainBlockingValue(bool value);

    bool expectedDrainValue = false;

    bool expectDrainCalled = false;

    void clearQueue(bool hostptrsOnly) override;
};
} // namespace NEO
