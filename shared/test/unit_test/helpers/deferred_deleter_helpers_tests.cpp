/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/deferred_deleter_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

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