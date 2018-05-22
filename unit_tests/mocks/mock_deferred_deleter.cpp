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

#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "runtime/memory_manager/deferrable_deletion.h"
#include "runtime/os_interface/os_thread.h"
#include "gtest/gtest.h"

namespace OCLRT {

MockDeferredDeleter::MockDeferredDeleter() {
    shouldStopCalled = 0;
    clearCalled = 0;
}

void MockDeferredDeleter::deferDeletion(DeferrableDeletion *deletion) {
    deferDeletionCalled++;
    deletion->apply();
    delete deletion;
}

void MockDeferredDeleter::addClient() {
    ++numClients;
}

void MockDeferredDeleter::removeClient() {
    --numClients;
}

void MockDeferredDeleter::drain(bool blocking) {
    if (expectDrainCalled) {
        EXPECT_EQ(expectedDrainValue, blocking);
    }
    DeferredDeleter::drain(blocking);
    drainCalled++;
}

void MockDeferredDeleter::drain() {
    return drain(true);
}

bool MockDeferredDeleter::areElementsReleased() {
    areElementsReleasedCalled++;
    return areElementsReleasedCalled != 1;
}

bool MockDeferredDeleter::shouldStop() {
    shouldStopCalled++;
    return shouldStopCalled > 1;
}

void MockDeferredDeleter::clearQueue() {
    DeferredDeleter::clearQueue();
    clearCalled++;
}

int MockDeferredDeleter::getClientsNum() {
    return numClients;
}

int MockDeferredDeleter::getElementsToRelease() {
    return elementsToRelease;
}

bool MockDeferredDeleter::isWorking() {
    return doWorkInBackground;
}

bool MockDeferredDeleter::isThreadRunning() {
    return worker != nullptr;
}

bool MockDeferredDeleter::isQueueEmpty() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return queue.peekIsEmpty();
}

void MockDeferredDeleter::setElementsToRelease(int elementsNum) {
    elementsToRelease = elementsNum;
}

void MockDeferredDeleter::setDoWorkInBackgroundValue(bool value) {
    doWorkInBackground = value;
}

bool MockDeferredDeleter::baseAreElementsReleased() {
    return DeferredDeleter::areElementsReleased();
}

bool MockDeferredDeleter::baseShouldStop() {
    return DeferredDeleter::shouldStop();
}

Thread *MockDeferredDeleter::getThreadHandle() {
    return worker.get();
}

std::unique_ptr<DeferredDeleter> createDeferredDeleter() {
    return std::unique_ptr<DeferredDeleter>(new MockDeferredDeleter());
}

void MockDeferredDeleter::runThread() {
    worker = Thread::create(run, reinterpret_cast<void *>(this));
}

void MockDeferredDeleter::forceStop() {
    allowEarlyStopThread();
    stop();
}

void MockDeferredDeleter::allowEarlyStopThread() {
    shouldStopCalled = 2;
}

MockDeferredDeleter::~MockDeferredDeleter() {
    allowEarlyStopThread();
    if (expectDrainCalled) {
        EXPECT_NE(0, drainCalled);
    }
}

void MockDeferredDeleter::expectDrainBlockingValue(bool value) {
    expectedDrainValue = value;
    expectDrainCalled = true;
}
} // namespace OCLRT
