/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"

using DirectSubmissionTestXE_HP_CORE = Test<DirectSubmissionFixture>;

XE_HP_CORE_TEST_F(DirectSubmissionTestXE_HP_CORE, givenBlitterUsedWhenDispatchingPrefetchMitigationThenExpectBbStartCmd) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using Dispatcher = BlitterDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(sizeof(MI_BATCH_BUFFER_START), directSubmission.getSizePrefetchMitigation());

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    directSubmission.dispatchPrefetchMitigation();

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);

    uint64_t exptectedJumpAddress = directSubmission.ringCommandStream.getGraphicsAllocation()->getGpuAddress();
    exptectedJumpAddress += sizeof(MI_BATCH_BUFFER_START);
    EXPECT_EQ(exptectedJumpAddress, bbStart->getBatchBufferStartAddress());
}

XE_HP_CORE_TEST_F(DirectSubmissionTestXE_HP_CORE, givenBlitterUsedWhenDispatchingPrefetchDisableTrueThenExpectArbCheckCmd) {
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
    ASSERT_NE(nullptr, arbCheck);

    EXPECT_EQ(1u, arbCheck->getPreFetchDisable());
}
