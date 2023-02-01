/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using GfxCoreHelperDg2AndLaterTest = ::testing::Test;

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, GivenUseL1CacheAsTrueWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsSetProperly, IsAtLeastXeHpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &helper = reinterpret_cast<GfxCoreHelperHw<FamilyType> &>(gfxCoreHelper);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = true;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB, surfaceState.getL1CachePolicyL1CacheControl());
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, GivenOverrideL1CacheControlInSurfaceStateForScratchSpaceWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsSetProperly, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.set(1);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &helper = reinterpret_cast<GfxCoreHelperHw<FamilyType> &>(gfxCoreHelper);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = true;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_POLICY_UC, surfaceState.getL1CachePolicyL1CacheControl());
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, GivenUseL1CacheAsFalseWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsNotSet, IsAtLeastXeHpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &helper = reinterpret_cast<GfxCoreHelperHw<FamilyType> &>(gfxCoreHelper);
    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = false;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_NE(RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB, surfaceState.getL1CachePolicyL1CacheControl());
}

using PipeControlHelperTestsDg2AndLater = ::testing::Test;

HWTEST2_F(PipeControlHelperTestsDg2AndLater, WhenAddingPipeControlWAThenCorrectCommandsAreProgrammed, IsAtLeastXeHpgCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    uint8_t buffer[128];
    uint64_t address = 0x1234567887654321;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &hardwareInfo = *mockExecutionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    bool requiresMemorySynchronization = (MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(rootDeviceEnvironment) > 0) ? true : false;

    for (auto ftrLocalMemory : ::testing::Bool()) {
        LinearStream stream(buffer, 128);
        hardwareInfo.featureTable.flags.ftrLocalMemory = ftrLocalMemory;

        MemorySynchronizationCommands<FamilyType>::addBarrierWa(stream, address, rootDeviceEnvironment);

        if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(rootDeviceEnvironment) == false) {
            EXPECT_EQ(0u, stream.getUsed());
            continue;
        }

        GenCmdList cmdList;
        FamilyType::PARSE::parseCommandBuffer(cmdList, stream.getCpuBase(), stream.getUsed());
        EXPECT_EQ(requiresMemorySynchronization ? 2u : 1u, cmdList.size());

        PIPE_CONTROL expectedPipeControl = FamilyType::cmdInitPipeControl;
        expectedPipeControl.setCommandStreamerStallEnable(true);
        UnitTestHelper<FamilyType>::setPipeControlHdcPipelineFlush(expectedPipeControl, true);
        expectedPipeControl.setUnTypedDataPortCacheFlush(true);
        auto it = cmdList.begin();
        auto pPipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        ASSERT_NE(nullptr, pPipeControl);
        EXPECT_TRUE(memcmp(&expectedPipeControl, pPipeControl, sizeof(PIPE_CONTROL)) == 0);

        if (requiresMemorySynchronization) {
            if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(rootDeviceEnvironment)) {
                MI_SEMAPHORE_WAIT expectedMiSemaphoreWait;
                EncodeSempahore<FamilyType>::programMiSemaphoreWait(&expectedMiSemaphoreWait, address,
                                                                    EncodeSempahore<FamilyType>::invalidHardwareTag,
                                                                    MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                                    false);
                auto pMiSemaphoreWait = genCmdCast<MI_SEMAPHORE_WAIT *>(*(++it));
                ASSERT_NE(nullptr, pMiSemaphoreWait);
                EXPECT_TRUE(memcmp(&expectedMiSemaphoreWait, pMiSemaphoreWait, sizeof(MI_SEMAPHORE_WAIT)) == 0);
            }
        }
    }
}

HWTEST2_F(PipeControlHelperTestsDg2AndLater, WhenSettingExtraPipeControlPropertiesThenCorrectValuesAreSet, IsAtLeastXeHpgCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    for (auto ftrLocalMemory : ::testing::Bool()) {
        HardwareInfo hardwareInfo = *defaultHwInfo;
        hardwareInfo.featureTable.flags.ftrLocalMemory = ftrLocalMemory;

        PipeControlArgs args;
        MemorySynchronizationCommands<FamilyType>::setPostSyncExtraProperties(args, hardwareInfo);

        if (ftrLocalMemory) {
            EXPECT_TRUE(args.hdcPipelineFlush);
            EXPECT_TRUE(args.unTypedDataPortCacheFlush);
        } else {
            EXPECT_FALSE(args.hdcPipelineFlush);
            EXPECT_FALSE(args.unTypedDataPortCacheFlush);
        }
    }
}

HWTEST2_F(PipeControlHelperTestsDg2AndLater, whenSettingCacheFlushExtraFieldsThenExpectHdcAndUnTypedDataPortFlushSet, IsAtLeastXeHpgCore) {
    PipeControlArgs args;

    MemorySynchronizationCommands<FamilyType>::setCacheFlushExtraProperties(args);
    EXPECT_TRUE(args.hdcPipelineFlush);
    EXPECT_TRUE(args.unTypedDataPortCacheFlush);
}

HWTEST2_F(PipeControlHelperTestsDg2AndLater, givenRequestedCacheFlushesWhenProgrammingPipeControlThenFlushHdcAndUnTypedDataPortCache, IsAtLeastXeHpgCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    uint32_t buffer[sizeof(PIPE_CONTROL) * 2] = {};
    LinearStream stream(buffer, sizeof(buffer));

    PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
    args.compressionControlSurfaceCcsFlush = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
    EXPECT_TRUE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWTEST2_F(PipeControlHelperTestsDg2AndLater, givenDebugVariableSetWhenProgrammingPipeControlThenFlushHdcAndUnTypedDataPortCache, IsAtLeastXeHpgCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restore;
    DebugManager.flags.FlushAllCaches.set(true);

    uint32_t buffer[sizeof(PIPE_CONTROL) * 2] = {};
    LinearStream stream(buffer, sizeof(buffer));

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
    EXPECT_TRUE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWTEST2_F(PipeControlHelperTestsDg2AndLater, givenDebugDisableCacheFlushWhenProgrammingPipeControlWithCacheFlushThenExpectDebugOverrideFlushHdcAndUnTypedDataPortCache, IsAtLeastXeHpgCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restore;
    DebugManager.flags.DoNotFlushCaches.set(true);

    uint32_t buffer[sizeof(PIPE_CONTROL) * 2] = {};
    LinearStream stream(buffer, sizeof(buffer));

    PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
    args.compressionControlSurfaceCcsFlush = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_FALSE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_FALSE(pipeControl->getUnTypedDataPortCacheFlush());
    EXPECT_FALSE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, givenXeHPGAndLaterPlatformWhenCheckingIfUnTypedDataPortCacheFlushRequiredThenReturnTrue, IsAtLeastXeHpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.unTypedDataPortCacheFlushRequired());
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, givenGfxCoreHelperWhenCheckIsUpdateTaskCountFromWaitSupportedThenReturnsTrue, IsAtLeastXeHpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    EXPECT_TRUE(gfxCoreHelper.isUpdateTaskCountFromWaitSupported());
}

using ProductHelperTestDg2AndLater = ::testing::Test;

HWTEST2_F(ProductHelperTestDg2AndLater, givenDg2AndLaterPlatformWhenAskedIfHeapInLocalMemThenTrueIsReturned, IsAtLeastXeHpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.heapInLocalMem(*defaultHwInfo));
}
