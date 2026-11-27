/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_deferrable_deletion.h"
#include "shared/test/common/mocks/mock_os_thread.h"

#include "gtest/gtest.h"

namespace NEO {
namespace SysCalls {
extern bool isShutdownInProgressRetVal;
}
} // namespace NEO

using namespace NEO;

namespace {

class DeferredDeleterWithWorker : public DeferredDeleter {
  public:
    using DeferredDeleter::doWorkInBackground;
    using DeferredDeleter::elementsToRelease;
    using DeferredDeleter::isQueueEmpty;
    using DeferredDeleter::numClients;
    using DeferredDeleter::safeStop;
    using DeferredDeleter::worker;
};

} // namespace

TEST(DeferredDeleterWinTest, givenShutdownInProgressAndWorkerThreadThatNeverExitsMainLoopWhenDeleterIsStoppedThenWorkerIsReleasedWithoutWaiting) {
    VariableBackup<bool> isShutdownInProgressBackup{&SysCalls::isShutdownInProgressRetVal, true};

    DeferredDeleterWithWorker deleter;
    deleter.worker = std::make_unique<MockThread>();
    deleter.doWorkInBackground = true;

    deleter.safeStop();

    EXPECT_EQ(nullptr, deleter.worker);
    EXPECT_TRUE(deleter.doWorkInBackground);
}

TEST(DeferredDeleterWinTest, givenShutdownInProgressWhenLastClientIsRemovedThenQueuedDeletionIsNotAppliedAndWorkerIsKept) {
    VariableBackup<bool> isShutdownInProgressBackup{&SysCalls::isShutdownInProgressRetVal, true};

    DeferredDeleterWithWorker deleter;
    deleter.worker = std::make_unique<MockThread>();
    deleter.doWorkInBackground = true;
    deleter.numClients = 1;
    auto deletion = new MockDeferrableDeletion();
    deleter.deferDeletion(deletion);

    deleter.removeClient();

    EXPECT_EQ(0, deletion->applyCalled);
    EXPECT_FALSE(deleter.isQueueEmpty());
    EXPECT_NE(nullptr, deleter.worker);

    deleter.safeStop();
    EXPECT_TRUE(deleter.isQueueEmpty());
}

TEST(DeferredDeleterWinTest, givenShutdownInProgressAndDeletionNotReleasedByWorkerWhenBlockingDrainIsCalledThenItReturnsWithoutWaiting) {
    VariableBackup<bool> isShutdownInProgressBackup{&SysCalls::isShutdownInProgressRetVal, true};

    DeferredDeleterWithWorker deleter;
    deleter.elementsToRelease = 1;

    deleter.drain(true, false);

    EXPECT_EQ(1, deleter.elementsToRelease);
}
