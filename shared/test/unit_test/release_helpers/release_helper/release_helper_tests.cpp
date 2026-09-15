/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ReleaseHelperPatIndexTests, givenEnableOverrideToPat19ForSystemMemorySetWhenOverridingSystemMemoryPatIndexThenFlagValueIsUsed) {
    DebugManagerStateRestore restore;
    MockReleaseHelper releaseHelper;
    constexpr uint64_t patIndex = 3u;

    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(0);
    EXPECT_EQ(patIndex, releaseHelper.overrideSystemMemoryPatIndex(patIndex));

    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(1);
    EXPECT_EQ(19u, releaseHelper.overrideSystemMemoryPatIndex(patIndex));
}
