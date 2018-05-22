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

#pragma once
#include "runtime/memory_manager/deferred_deleter.h"

namespace OCLRT {
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
} // namespace OCLRT
