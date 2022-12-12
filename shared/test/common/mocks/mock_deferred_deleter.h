/*
 * Copyright (C) 2018-2023 Intel Corporation
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

    void drain(bool blocking) override;

    void clearQueueTillFirstFailure() override;

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

    std::atomic<int> clearCalledWithBreakTillFailure;

    int deferDeletionCalled = 0;
    bool stopAfter3loopsInRun = false;

    void forceStop();

    void allowEarlyStopThread();

    void expectDrainBlockingValue(bool value);

    void expectClearQueueTillFirstFailure();

    bool expectedDrainValue = false;

    bool expectDrainCalled = false;

    bool expectClearQueueTillFirstFailureCalled = false;

    int clearQueueTillFirstFailureCalled = 0;

    void clearQueue(bool breakOnFailure) override;
};
} // namespace NEO
