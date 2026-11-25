/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using GfxCoreHelperXeHpcCoreTest = ::testing::Test;

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, WhenGettingIsCpuImageTransferPreferredThenTrueIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isCpuImageTransferPreferred(*defaultHwInfo));
}

using ProductHelperTestXeHpcCore = Test<DeviceFixture>;

XE_HPC_CORETEST_F(ProductHelperTestXeHpcCore, givenProductHelperWhenCheckTimestampWaitSupportThenReturnTrue) {
    auto &helper = getHelper<ProductHelper>();
    EXPECT_TRUE(helper.isTimestampWaitSupportedForQueues());
}

XE_HPC_CORETEST_F(ProductHelperTestXeHpcCore, givenProductHelperWhenCheckTimestampWaitSupportForEventsThenReturnTrue) {
    auto &helper = getHelper<ProductHelper>();
    EXPECT_TRUE(helper.isTimestampWaitSupportedForEvents());
}

XE_HPC_CORETEST_F(ProductHelperTestXeHpcCore, givenProductHelperWhenCallGetInternalHeapsPreallocatedThenReturnCorrectValue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 1u);

    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(3);
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 3u);
}

XE_HPC_CORETEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenCallCopyThroughLockedPtrEnabledThenReturnTrue) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo, productHelper));
}

XE_HPC_CORETEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenCallGetAmountOfAllocationsToFillThenReturnTrue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);
}

HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, givenGfxCoreHelperWhenAskingForRelaxedOrderingSupportThenReturnFalse, IGFX_XE_HPC_CORE);

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, givenGfxCoreHelperWhenAskingForRelaxedOrderingSupportThenReturnTrue) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    EXPECT_TRUE(gfxCoreHelper.isRelaxedOrderingSupported());
}

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, whenGetDefaultDeviceHierarchyThenReturnFlatHierarchy) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    auto defaultDeviceHierarchy = gfxCoreHelper.getDefaultDeviceHierarchy();
    EXPECT_EQ(DeviceHierarchyMode::flat, defaultDeviceHierarchy);
}

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, givenRenderPipelineFlushesRequestedWhenProgrammingBarriersThenRenderPipelineFlagsAreNotProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    constexpr uint32_t renderPipelineFlagsMask = TestTraits<IGFX_XE_HPC_CORE>::pipeControlRenderPipelineFlagsMask;
    constexpr uint32_t flagsDword = 1;

    DebugManagerStateRestore restorer;
    debugManager.flags.FlushAllCaches.set(-1);
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream stream(buffer, sizeof(buffer));

    PipeControlArgs args;
    args.renderTargetCacheFlushEnable = true;
    args.pipeControlFlushEnable = true;
    args.vfCacheInvalidationEnable = true;
    args.depthCacheFlushEnable = true;
    args.depthStallEnable = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);
    MemorySynchronizationCommands<FamilyType>::addStateCacheFlush(stream, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

    auto barrier = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_TRUE(barrier->getTextureCacheInvalidationEnable());
    EXPECT_EQ(0u, barrier->getRawData(flagsDword) & renderPipelineFlagsMask);

    auto stateCacheFlush = barrier + 1;
    EXPECT_TRUE(stateCacheFlush->getStateCacheInvalidationEnable());
    EXPECT_EQ(0u, stateCacheFlush->getRawData(flagsDword) & renderPipelineFlagsMask);
}

using CommandStreamReceiverXeHpcCoreTest = Test<DeviceFixture>;

XE_HPC_CORETEST_F(CommandStreamReceiverXeHpcCoreTest, givenRenderStateCacheFlushWhenFlushingPipeControlThenRenderPipelineFlagsAreNotProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    constexpr uint32_t renderPipelineFlagsMask = TestTraits<IGFX_XE_HPC_CORE>::pipeControlRenderPipelineFlagsMask;
    constexpr uint32_t flagsDword = 1;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.sendRenderStateCacheFlush());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pipeControls = findAll<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(0u, pipeControls.size());

    bool stateCacheFlushFound = false;
    for (auto &pipeControlIt : pipeControls) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIt);
        EXPECT_EQ(0u, pipeControl->getRawData(flagsDword) & renderPipelineFlagsMask);
        stateCacheFlushFound |= pipeControl->getStateCacheInvalidationEnable() && pipeControl->getTextureCacheInvalidationEnable();
    }
    EXPECT_TRUE(stateCacheFlushFound);
}

using GfxCoreHelperTestsXeHpcCoreWithEnginesCheck = GfxCoreHelperTestWithEnginesCheck;

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCoreWithEnginesCheck, givenWddmWhenGetEnginesCalledThenPowerHintEnginesAreCreated) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    device->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    device->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<MockDriverModelWDDM>());
    auto &productHelper = device->getProductHelper();
    auto &engines = device->getGfxCoreHelper().getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    for (const auto &engine : engines) {
        countEngine(engine.first, engine.second);
    }

    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::powerHint));
    EXPECT_EQ(1u, getEngineCount(productHelper.getDefaultCopyEngine(), EngineUsage::powerHint));
}
