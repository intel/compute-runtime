/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_deferrable_deletion.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DeferredDeleter, WhenDeferredDeleterIsCreatedThenItIsNotMoveableOrCopyable) {
    EXPECT_FALSE(std::is_move_constructible<DeferredDeleter>::value);
    EXPECT_FALSE(std::is_copy_constructible<DeferredDeleter>::value);
}

TEST(DeferredDeleter, WhenDeferredDeleterIsCreatedThenItIsNotAssignable) {
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

    std::unique_ptr<MockDeferredDeleter> deleter;
};

TEST_F(DeferredDeleterTest, WhenTheAllocationsAreReadyThenCallClearQueueTillFirstFailure) {
    auto deletion1 = new MockDeferrableDeletion();
    auto deletion2 = new MockDeferrableDeletion();
    deleter->DeferredDeleter::deferDeletion(deletion1);
    deleter->DeferredDeleter::deferDeletion(deletion2);
    EXPECT_FALSE(deleter->isQueueEmpty());
    deleter->clearQueueTillFirstFailure();
    EXPECT_TRUE(deleter->isQueueEmpty());
    EXPECT_EQ(1, deleter->clearQueueTillFirstFailureCalled);
    EXPECT_EQ(1, deleter->clearCalled);
    EXPECT_EQ(1, deleter->clearCalledWithBreakTillFailure);
}

TEST_F(DeferredDeleterTest, WhenSomeAllocationIsNotReadyThenCallClearQueueTillFirstFailureMultipleTimes) {
    auto deletionReady = new MockDeferrableDeletion();
    auto deletionNotReadyTill3 = new MockDeferrableDeletion();
    deletionNotReadyTill3->setTrialTimes(3);
    deleter->DeferredDeleter::deferDeletion(deletionReady);
    deleter->DeferredDeleter::deferDeletion(deletionNotReadyTill3);
    EXPECT_FALSE(deleter->isQueueEmpty());
    deleter->clearQueueTillFirstFailure();
    EXPECT_FALSE(deleter->isQueueEmpty());
    EXPECT_EQ(1, deleter->clearQueueTillFirstFailureCalled);
    EXPECT_EQ(1, deleter->clearCalled);
    EXPECT_EQ(1, deleter->clearCalledWithBreakTillFailure);
    deleter->clearQueueTillFirstFailure();
    EXPECT_FALSE(deleter->isQueueEmpty());
    deleter->clearQueueTillFirstFailure();
    EXPECT_TRUE(deleter->isQueueEmpty());
}

TEST_F(DeferredDeleterTest, WhenSomeAllocationIsNotReadyThenCallClearQueueTillFirstFailureAndThenDrain) {
    auto deletionReady = new MockDeferrableDeletion();
    auto deletionNotReadyTill3 = new MockDeferrableDeletion();
    deletionNotReadyTill3->setTrialTimes(3);
    deleter->DeferredDeleter::deferDeletion(deletionReady);
    deleter->DeferredDeleter::deferDeletion(deletionNotReadyTill3);
    EXPECT_FALSE(deleter->isQueueEmpty());
    deleter->clearQueueTillFirstFailure();
    EXPECT_FALSE(deleter->isQueueEmpty());
    EXPECT_EQ(1, deleter->clearQueueTillFirstFailureCalled);
    EXPECT_EQ(1, deleter->clearCalled);
    EXPECT_EQ(1, deleter->clearCalledWithBreakTillFailure);
    deleter->drain();
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_TRUE(deleter->isQueueEmpty());
}

TEST_F(DeferredDeleterTest, GivenDeferredDeleterWithEmptyQueueThenCallClearQueueTillFirstFailure) {
    deleter->clearQueueTillFirstFailure();
    EXPECT_EQ(0, deleter->areElementsReleasedCalled);
    EXPECT_EQ(1, deleter->clearQueueTillFirstFailureCalled);
}
