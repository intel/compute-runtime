/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/deferred_deleter_helper.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(deferredDeleterHelper, DefferedDeleterIsDisabledWhenCheckIFDeferrDeleterIsEnabledThenCorrectValueReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableDeferredDeleter.set(false);
    EXPECT_FALSE(isDeferredDeleterEnabled());
}
TEST(deferredDeleterHelper, DefferedDeleterIsEnabledWhenCheckIFDeferrDeleterIsEnabledThenCorrectValueReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableDeferredDeleter.set(true);
    EXPECT_TRUE(isDeferredDeleterEnabled());
}