/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_deferrable_deletion.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(DeferredDeleter, NonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<DeferredDeleter>::value);
    EXPECT_FALSE(std::is_copy_constructible<DeferredDeleter>::value);
}

TEST(DeferredDeleter, NonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<DeferredDeleter>::value);
    EXPECT_FALSE(std::is_copy_assignable<DeferredDeleter>::value);
}

struct DeferredDeleterTest : public ::testing::Test {

    void SetUp() override {
        deleter.reset(new MockDeferredDeleter());
    }

    void TearDown() override {
        EXPECT_TRUE(deleter->isQueueEmpty());
        EXPECT_EQ(0, deleter->getElementsToRelease());
    }

    void waitForAsyncThread() {
        while (!deleter->isWorking()) {
            std::this_thread::yield();
        }
    }
    MockDeferrableDeletion *createDeletion() {
        return new MockDeferrableDeletion();
    }
    std::unique_ptr<MockDeferredDeleter> deleter;
};

TEST_F(DeferredDeleterTest, initialValues) {
    EXPECT_EQ(0, deleter->getClientsNum());
    EXPECT_FALSE(deleter->isWorking());
    EXPECT_FALSE(deleter->isThreadRunning());
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0, deleter->clearCalled);
    EXPECT_EQ(0, deleter->areElementsReleasedCalled);
    EXPECT_EQ(0, deleter->shouldStopCalled);
    EXPECT_EQ(0, deleter->getElementsToRelease());
    EXPECT_TRUE(deleter->isQueueEmpty());
    EXPECT_FALSE(deleter->expectDrainCalled);
    EXPECT_FALSE(deleter->expectedDrainValue);
}

TEST_F(DeferredDeleterTest, clearQueueWithOnePair) {
    auto deletion = createDeletion();
    deleter->DeferredDeleter::deferDeletion(deletion);
    EXPECT_FALSE(deleter->isQueueEmpty());
    deleter->drain();
    EXPECT_TRUE(deleter->isQueueEmpty());
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(1, deleter->clearCalled);
}

TEST_F(DeferredDeleterTest, addTwoClients) {
    deleter->DeferredDeleter::addClient();
    waitForAsyncThread();
    EXPECT_TRUE(deleter->isThreadRunning());
    EXPECT_TRUE(deleter->isWorking());
    EXPECT_EQ(1, deleter->getClientsNum());
    auto threadHandle = deleter->getThreadHandle();
    EXPECT_NE(nullptr, threadHandle);
    deleter->DeferredDeleter::addClient();
    EXPECT_TRUE(deleter->isThreadRunning());
    EXPECT_TRUE(deleter->isWorking());
    EXPECT_EQ(2, deleter->getClientsNum());
    EXPECT_EQ(threadHandle, deleter->getThreadHandle());
    deleter->forceStop();
    EXPECT_FALSE(deleter->isWorking());
    EXPECT_FALSE(deleter->isThreadRunning());
}

TEST_F(DeferredDeleterTest, addAndRemoveTwoClients) {
    deleter->DeferredDeleter::addClient();
    deleter->DeferredDeleter::addClient();
    waitForAsyncThread();
    EXPECT_EQ(2, deleter->getClientsNum());
    deleter->DeferredDeleter::removeClient();
    EXPECT_TRUE(deleter->isThreadRunning());
    EXPECT_TRUE(deleter->isWorking());
    EXPECT_EQ(1, deleter->getClientsNum());
    deleter->allowEarlyStopThread();
    deleter->DeferredDeleter::removeClient();
    EXPECT_FALSE(deleter->isThreadRunning());
    EXPECT_FALSE(deleter->isWorking());
    EXPECT_EQ(0, deleter->getClientsNum());
    deleter->forceStop();
    EXPECT_FALSE(deleter->isThreadRunning());
}

TEST_F(DeferredDeleterTest, drainWhenNotWorking) {
    EXPECT_FALSE(deleter->isWorking());
    deleter->drain();
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(1, deleter->clearCalled);
    EXPECT_EQ(2, deleter->areElementsReleasedCalled);
}

TEST_F(DeferredDeleterTest, drainWhenWorking) {
    deleter->DeferredDeleter::addClient();
    waitForAsyncThread();
    EXPECT_TRUE(deleter->isWorking());
    deleter->drain();
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(2, deleter->areElementsReleasedCalled);
    deleter->forceStop();
}

TEST_F(DeferredDeleterTest, stopWhenThreadNotRunning) {
    auto deletion = createDeletion();
    deleter->DeferredDeleter::deferDeletion(deletion);
    EXPECT_FALSE(deleter->isQueueEmpty());
    EXPECT_FALSE(deleter->isThreadRunning());
    deleter->expectDrainBlockingValue(false);
    deleter->forceStop();
    EXPECT_TRUE(deleter->isQueueEmpty());
    EXPECT_NE(0, deleter->drainCalled);
}

TEST_F(DeferredDeleterTest, stopWhenThreadRunning) {
    deleter->DeferredDeleter::addClient();
    auto deletion = createDeletion();
    deleter->DeferredDeleter::deferDeletion(deletion);
    EXPECT_TRUE(deleter->isThreadRunning());
    deleter->allowEarlyStopThread();
    deleter->expectDrainBlockingValue(false);
    deleter->DeferredDeleter::removeClient();
    EXPECT_FALSE(deleter->isThreadRunning());
    EXPECT_TRUE(deleter->isQueueEmpty());
    EXPECT_NE(0, deleter->drainCalled);
}

TEST_F(DeferredDeleterTest, asyncThreadWaitsForQueueItem) {
    deleter->DeferredDeleter::addClient();

    waitForAsyncThread();

    auto deletion = createDeletion();
    deleter->DeferredDeleter::deferDeletion(deletion);

    EXPECT_TRUE(deleter->isThreadRunning());
    EXPECT_TRUE(deleter->isWorking());
    deleter->allowEarlyStopThread();
    deleter->DeferredDeleter::removeClient();

    EXPECT_TRUE(deleter->isQueueEmpty());
}

TEST_F(DeferredDeleterTest, asyncThreadClearQueueWithoutWaitingForQueueItem) {
    auto deletion = createDeletion();
    deleter->DeferredDeleter::deferDeletion(deletion);

    EXPECT_FALSE(deleter->isQueueEmpty());
    EXPECT_FALSE(deleter->isThreadRunning());

    deleter->DeferredDeleter::addClient();

    waitForAsyncThread();

    EXPECT_TRUE(deleter->isThreadRunning());
    deleter->allowEarlyStopThread();
    deleter->DeferredDeleter::removeClient();
    EXPECT_TRUE(deleter->isQueueEmpty());
}

TEST_F(DeferredDeleterTest, asyncThreadWaitsForQueueItemTwice) {
    deleter->DeferredDeleter::addClient();

    waitForAsyncThread();

    auto deletion = createDeletion();
    deleter->DeferredDeleter::deferDeletion(deletion);

    EXPECT_TRUE(deleter->isThreadRunning());
    EXPECT_TRUE(deleter->isWorking());

    while (deleter->shouldStopCalled == 0)
        ;

    EXPECT_TRUE(deleter->isThreadRunning());
    EXPECT_TRUE(deleter->isWorking());

    auto secondDeletion = createDeletion();
    deleter->DeferredDeleter::deferDeletion(secondDeletion);

    deleter->allowEarlyStopThread();
    deleter->DeferredDeleter::removeClient();

    EXPECT_TRUE(deleter->isQueueEmpty());
    EXPECT_EQ(0, deleter->getElementsToRelease());
}

TEST_F(DeferredDeleterTest, checkIfAllElementsAreReleased) {
    deleter->setElementsToRelease(1);
    EXPECT_EQ(1, deleter->getElementsToRelease());
    EXPECT_FALSE(deleter->baseAreElementsReleased());
    deleter->setElementsToRelease(0);
    EXPECT_EQ(0, deleter->getElementsToRelease());
    EXPECT_TRUE(deleter->baseAreElementsReleased());
}

TEST_F(DeferredDeleterTest, checkIfThreadShouldStop) {
    deleter->setDoWorkInBackgroundValue(false);
    EXPECT_TRUE(deleter->baseShouldStop());
    deleter->setDoWorkInBackgroundValue(true);
    EXPECT_FALSE(deleter->baseShouldStop());
}

TEST_F(DeferredDeleterTest, givenDeferredDeleterWhenBlockingDrainIsCalledThenArElementsReleasedIsCalled) {
    deleter->drain(true);
    EXPECT_NE(0, deleter->areElementsReleasedCalled);
    EXPECT_EQ(1, deleter->drainCalled);
}

TEST_F(DeferredDeleterTest, givenDeferredDeleterWhenNonBlockingDrainIsCalledThenArElementsReleasedIsNotCalled) {
    deleter->drain(false);
    EXPECT_EQ(0, deleter->areElementsReleasedCalled);
    EXPECT_EQ(1, deleter->drainCalled);
}
