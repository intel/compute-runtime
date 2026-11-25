/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/source/xe_hpc_core/hw_info_xe_hpc_core.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"

#include "per_product_test_definitions.h"

using DirectSubmissionTestXeHpcCore = Test<DirectSubmissionFixture>;

XE_HPC_CORETEST_F(DirectSubmissionTestXeHpcCore, givenXeHpcCoreWhenDispatchDisablePrefetcherIsCalledThenPrefetcherIsNotDisabled) {
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using Dispatcher = BlitterDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(EncodeMiArbCheck<FamilyType>::getCommandSize(), directSubmission.getSizeDisablePrefetcher());

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
    debugManager.flags.DirectSubmissionDisablePrefetcher.set(1);

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

XE_HPC_CORETEST_F(DirectSubmissionTestXeHpcCore, givenRenderDispatcherWhenDispatchingTlbFlushThenRenderPipelineFlagsAreNotProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    constexpr uint32_t renderPipelineFlagsMask = TestTraits<IGFX_XE_HPC_CORE>::pipeControlRenderPipelineFlagsMask;
    constexpr uint32_t flagsDword = 1;

    alignas(8) uint8_t buffer[4 * sizeof(PIPE_CONTROL)] = {};
    LinearStream cmdBuffer(buffer, sizeof(buffer));
    RenderDispatcher<FamilyType>::dispatchTlbFlush(cmdBuffer, 0ull, pDevice->getRootDeviceEnvironment());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer, 0);
    auto pipeControls = findAll<PIPE_CONTROL *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_EQ(1u, pipeControls.size());

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControls[0]);
    EXPECT_TRUE(pipeControl->getTlbInvalidate());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_EQ(0u, pipeControl->getRawData(flagsDword) & renderPipelineFlagsMask);
}
