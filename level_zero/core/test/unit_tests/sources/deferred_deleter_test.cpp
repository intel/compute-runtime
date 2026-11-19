/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/deferred_deleter_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DeferredDeleterHelper, GivenDeferredDeleterHelperWhenCheckIFDeferrDeleterIsEnabledThenFalseIsReturned) {
    EXPECT_FALSE(isDeferredDeleterEnabled());
}

TEST(DeferredDeleterHelper, GivenDeferredDeleterDisabledByDebugFlagWhenCheckIfDeferredDeleterIsEnabledThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeferredDeleter.set(0);
    EXPECT_FALSE(isDeferredDeleterEnabled());
}

TEST(DeferredDeleterHelper, GivenDeferredDeleterEnabledByDebugFlagWhenCheckIfDeferredDeleterIsEnabledThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeferredDeleter.set(1);
    EXPECT_TRUE(isDeferredDeleterEnabled());
}
