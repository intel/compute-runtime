/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

using ReleaseHelperPipeControlTests = ::testing::Test;

TEST_F(ReleaseHelperPipeControlTests, givenOnlyBaseWaRequiredWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredThenOnlyBasicIsRequired) {
    MockReleaseHelper releaseHelper{};
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsBaseWARequiredResult = true;
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredResult = false;

    EXPECT_EQ((std::pair<bool, bool>{true, false}), releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, false));
}

TEST_F(ReleaseHelperPipeControlTests, givenOnlyExtendedWaRequiredWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredThenOnlyExtendedIsRequired) {
    MockReleaseHelper releaseHelper{};
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsBaseWARequiredResult = false;
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredResult = true;

    EXPECT_EQ((std::pair<bool, bool>{false, true}), releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, false));
}

TEST_F(ReleaseHelperPipeControlTests, givenDebugFlagSetTo1WhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredThenExtendedIsForcedToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);

    MockReleaseHelper releaseHelper{};
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsBaseWARequiredResult = false;
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredResult = false;

    EXPECT_EQ((std::pair<bool, bool>{false, true}), releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, false));
}

TEST_F(ReleaseHelperPipeControlTests, givenDebugFlagSetTo0WhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredThenExtendedIsForcedToFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    MockReleaseHelper releaseHelper{};
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsBaseWARequiredResult = true;
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredResult = true;

    EXPECT_EQ((std::pair<bool, bool>{true, false}), releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, false));
}
