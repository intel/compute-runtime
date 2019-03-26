/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/deferred_deleter.h"

namespace NEO {
class MockDeferredDeleter : public DeferredDeleter {
  public:
    MockDeferredDeleter();

    ~MockDeferredDeleter();

    void deferDeletion(DeferrableDeletion *deletion) override;

    void addClient() override;

    void removeClient() override;

    void drain(bool blocking) override;

    bool areElementsReleased() override;

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

    std::atomic<int> shouldStopCalled;

    std::atomic<int> clearCalled;

    int deferDeletionCalled = 0;

    void forceStop();

    void allowEarlyStopThread();

    void expectDrainBlockingValue(bool value);

    bool expectedDrainValue = false;

    bool expectDrainCalled = false;

    void clearQueue() override;
};
} // namespace NEO
