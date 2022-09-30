/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/deferred_deleter_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_deferrable_deletion.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(deferredDeleterHelper, DeferredDeleterIsDisabledWhenCheckIFDeferrDeleterIsEnabledThenCorrectValueReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableDeferredDeleter.set(false);
    EXPECT_FALSE(isDeferredDeleterEnabled());
}
TEST(deferredDeleterHelper, DeferredDeleterIsEnabledWhenCheckIFDeferrDeleterIsEnabledThenCorrectValueReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableDeferredDeleter.set(true);
    EXPECT_TRUE(isDeferredDeleterEnabled());
}

TEST(DeferredDeleter, GivenNoShouldStopWhenRunIsExecutingThenQueueIsProcessedInTheLoop) {
    MockDeferredDeleter deleter;

    deleter.stopAfter3loopsInRun = true;
    auto deletion = new MockDeferrableDeletion();
    deleter.DeferredDeleter::deferDeletion(deletion);

    MockDeferredDeleter::run(&deleter);
    EXPECT_EQ(3, deleter.shouldStopCalled);
}