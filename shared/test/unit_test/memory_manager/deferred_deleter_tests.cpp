/*
 * Copyright (C) 2022-2026 Intel Corporation
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

TEST(DeferredDeleter, givenDeferredDeleterWhenBlockingDrainIsCalledThenArElementsReleasedIsCalled) {
    auto deleter = std::make_unique<MockDeferredDeleter>();
    deleter->drain(true, false);
    EXPECT_NE(0, deleter->areElementsReleasedCalled);
    EXPECT_FALSE(deleter->areElementsReleasedCalledForHostptrs);
    EXPECT_EQ(1, deleter->drainCalled);
}

TEST(DeferredDeleter, givenDeferredDeleterWhenBlockingDrainOnlyForHostptrsIsCalledThenArElementsReleasedIsCalledWithHostptrsOnly) {
    auto deleter = std::make_unique<MockDeferredDeleter>();
    deleter->drain(true, true);
    EXPECT_NE(0, deleter->areElementsReleasedCalled);
    EXPECT_TRUE(deleter->areElementsReleasedCalledForHostptrs);
    EXPECT_EQ(1, deleter->drainCalled);
}

TEST(DeferredDeleter, givenHostptrAndNonHostptrDeletionsQueuedWhenClearingHostptrsOnlyThenNonHostptrDeletionStaysInQueueWhileHostptrIsApplied) {
    struct HostptrDeletion : public MockDeferrableDeletion {
        HostptrDeletion(MockDeferredDeleter &deleter, bool &queueEmptyDuringApply) : deleter(deleter), queueEmptyDuringApply(queueEmptyDuringApply) {
            this->externalHostptr = true;
        }
        bool apply() override {
            this->queueEmptyDuringApply = this->deleter.isQueueEmpty();
            return MockDeferrableDeletion::apply();
        }
        MockDeferredDeleter &deleter;
        bool &queueEmptyDuringApply;
    };

    auto deleter = std::make_unique<MockDeferredDeleter>();
    bool queueEmptyDuringApply = true;
    auto nonHostptrDeletion = new MockDeferrableDeletion();
    deleter->DeferredDeleter::deferDeletion(nonHostptrDeletion);
    deleter->DeferredDeleter::deferDeletion(new HostptrDeletion(*deleter, queueEmptyDuringApply));

    deleter->clearQueue(true);

    EXPECT_FALSE(queueEmptyDuringApply);
    EXPECT_FALSE(deleter->isQueueEmpty());
    EXPECT_EQ(0, nonHostptrDeletion->applyCalled);
    EXPECT_EQ(1, deleter->getElementsToRelease());
}

TEST(DeferredDeleter, givenDeferredDeleterWhenNonBlockingDrainIsCalledThenArElementsReleasedIsNotCalled) {
    auto deleter = std::make_unique<MockDeferredDeleter>();
    deleter->drain(false, false);
    EXPECT_EQ(0, deleter->areElementsReleasedCalled);
    EXPECT_EQ(1, deleter->drainCalled);
}

TEST(DeferredDeleter, givenNoClientsWhenDeferDeletionIfHasClientsIsCalledThenDeletionIsNotTakenAndAfterClientIsAddedItIsDeferred) {
    auto deleter = std::make_unique<MockDeferredDeleter>();
    std::unique_ptr<DeferrableDeletion> deletion = std::make_unique<MockDeferrableDeletion>();

    EXPECT_FALSE(deleter->deferDeletionIfHasClients(deletion));
    EXPECT_NE(nullptr, deletion);
    EXPECT_EQ(0, deleter->deferDeletionCalled);

    deleter->addClient();
    EXPECT_TRUE(deleter->deferDeletionIfHasClients(deletion));
    EXPECT_EQ(nullptr, deletion);
    EXPECT_EQ(1, deleter->deferDeletionCalled);
}
