/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"

using DirectSubmissionTestXeHpcCore = Test<DirectSubmissionFixture>;

XE_HPC_CORETEST_F(DirectSubmissionTestXeHpcCore, givenXeHpcCoreWhenDispatchDisablePrefetcherIsCalledThenPrefetcherIsNotDisabled) {
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using Dispatcher = BlitterDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(sizeof(MI_ARB_CHECK), directSubmission.getSizeDisablePrefetcher());

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    directSubmission.dispatchDisablePrefetcher(true);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_ARB_CHECK *arbCheck = hwParse.getCommand<MI_ARB_CHECK>();
    EXPECT_EQ(nullptr, arbCheck);
}

XE_HPC_CORETEST_F(DirectSubmissionTestXeHpcCore, givenXeHpcCoreAndDisablePrefetcherDebugFlagEnabledWhenDispatchDisablePrefetcherIsCalledThenPrefetcherIsDisabled) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisablePrefetcher.set(1);

    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using Dispatcher = BlitterDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    directSubmission.dispatchDisablePrefetcher(true);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_ARB_CHECK *arbCheck = hwParse.getCommand<MI_ARB_CHECK>();
    ASSERT_NE(nullptr, arbCheck);
}
