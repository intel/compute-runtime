/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using GfxCoreHelperDg2AndLaterTest = ::testing::Test;

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, GivenUseL1CacheAsTrueWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsSetProperly, IsAtLeastXeCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &helper = reinterpret_cast<GfxCoreHelperHw<FamilyType> &>(gfxCoreHelper);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = true;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB, surfaceState.getL1CacheControlCachePolicy());
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, GivenOverrideL1CacheControlInSurfaceStateForScratchSpaceWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsSetProperly, IsAtLeastXeCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.set(1);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &helper = reinterpret_cast<GfxCoreHelperHw<FamilyType> &>(gfxCoreHelper);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = true;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_CONTROL_UC, surfaceState.getL1CacheControlCachePolicy());
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, GivenUseL1CacheAsFalseWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsNotSet, IsAtLeastXeCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &helper = reinterpret_cast<GfxCoreHelperHw<FamilyType> &>(gfxCoreHelper);
    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = false;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_NE(RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB, surfaceState.getL1CacheControlCachePolicy());
}

using GfxCoreHelperWithLargeGrf = ::testing::Test;
HWTEST2_F(GfxCoreHelperWithLargeGrf, givenLargeGrfAndSimdSmallerThan32WhenCalculatingMaxWorkGroupSizeThenReturnHalfOfDeviceDefault, IsXeCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto defaultMaxGroupSize = 42u;

    NEO::KernelDescriptor kernelDescriptor{};

    kernelDescriptor.kernelAttributes.simdSize = 16;
    kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
    EXPECT_EQ((defaultMaxGroupSize >> 1), gfxCoreHelper.calculateMaxWorkGroupSize(kernelDescriptor, defaultMaxGroupSize, rootDeviceEnvironment));

    kernelDescriptor.kernelAttributes.simdSize = 32;
    kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
    EXPECT_EQ(defaultMaxGroupSize, gfxCoreHelper.calculateMaxWorkGroupSize(kernelDescriptor, defaultMaxGroupSize, rootDeviceEnvironment));

    kernelDescriptor.kernelAttributes.simdSize = 16;
    kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
    EXPECT_EQ(defaultMaxGroupSize, gfxCoreHelper.calculateMaxWorkGroupSize(kernelDescriptor, defaultMaxGroupSize, rootDeviceEnvironment));
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, givenGfxCoreHelperWhenCallCalculateMaxWorkGroupSizeThenMethodAdjustMaxWorkGroupSizeIsCalled, IsXeCore) {
    static bool isCalledAdjustMaxWorkGroupSize = false;
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        uint32_t adjustMaxWorkGroupSize(const uint32_t grfCount, const uint32_t simd, const uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const override {
            isCalledAdjustMaxWorkGroupSize = true;
            return defaultMaxGroupSize;
        }
    };
    MockGfxCoreHelper gfxCoreHelper;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    NEO::KernelDescriptor kernelDescriptor{};
    auto defaultMaxGroupSize = 42u;
    gfxCoreHelper.calculateMaxWorkGroupSize(kernelDescriptor, defaultMaxGroupSize, rootDeviceEnvironment);
    EXPECT_TRUE(isCalledAdjustMaxWorkGroupSize);
}

using PipeControlHelperTestsDg2AndLater = ::testing::Test;

HWTEST2_F(PipeControlHelperTestsDg2AndLater, WhenSettingExtraPipeControlPropertiesThenCorrectValuesAreSet, IsAtLeastXeCore) {
    PipeControlArgs args{};
    MemorySynchronizationCommands<FamilyType>::setPostSyncExtraProperties(args);

    EXPECT_TRUE(args.hdcPipelineFlush);
    EXPECT_TRUE(args.unTypedDataPortCacheFlush);
}

HWTEST2_F(PipeControlHelperTestsDg2AndLater, whenSettingCacheFlushExtraFieldsThenExpectHdcAndUnTypedDataPortFlushSet, IsAtLeastXeCore) {
    PipeControlArgs args{};

    MemorySynchronizationCommands<FamilyType>::setCacheFlushExtraProperties(args);
    EXPECT_TRUE(args.hdcPipelineFlush);
    EXPECT_TRUE(args.unTypedDataPortCacheFlush);
}

HWTEST2_F(PipeControlHelperTestsDg2AndLater, givenRequestedCacheFlushesWhenProgrammingPipeControlThenFlushHdcAndUnTypedDataPortCache, IsAtLeastXeCore) {
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

HWTEST2_F(PipeControlHelperTestsDg2AndLater, givenDebugVariableSetWhenProgrammingPipeControlThenFlushHdcAndUnTypedDataPortCache, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restore;
    debugManager.flags.FlushAllCaches.set(true);

    uint32_t buffer[sizeof(PIPE_CONTROL) * 2] = {};
    LinearStream stream(buffer, sizeof(buffer));

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
    EXPECT_TRUE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWTEST2_F(PipeControlHelperTestsDg2AndLater, givenDebugDisableCacheFlushWhenProgrammingPipeControlWithCacheFlushThenExpectDebugOverrideFlushHdcAndUnTypedDataPortCache, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restore;
    debugManager.flags.DoNotFlushCaches.set(true);

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

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, givenGfxCoreHelperWhenCheckIsUpdateTaskCountFromWaitSupportedThenReturnsTrue, IsAtLeastXeCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    EXPECT_TRUE(gfxCoreHelper.isUpdateTaskCountFromWaitSupported());
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, givenGfxCoreHelperWhenCheckMakeResidentBeforeLockNeededThenReturnsTrue, IsAtLeastXeCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    EXPECT_TRUE(gfxCoreHelper.makeResidentBeforeLockNeeded(false));
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, givenGfxCoreHelperWhenFlagSetAndCallGetAmountOfAllocationsToFillThenReturnCorrectValue, IsXeCore) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);

    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);

    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);
}

using ProductHelperTestDg2AndLater = ::testing::Test;

HWTEST2_F(ProductHelperTestDg2AndLater, givenDg2AndLaterPlatformWhenAskedIfHeapInLocalMemThenTrueIsReturned, IsAtLeastXeCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.heapInLocalMem(*defaultHwInfo));
}

HWTEST2_F(GfxCoreHelperDg2AndLaterTest, givenPlatformSupportsHdcUntypedCacheFlushWhenPropertyBlocksCacheFlushThenExpectNoCacheFlushSet, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    constexpr size_t bufferSize = 512u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    PipeControlArgs args;
    args.blockSettingPostSyncProperties = true;

    constexpr uint64_t gpuAddress = 0xABC000;
    constexpr uint64_t immediateValue = 0;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(cmdStream,
                                                                               PostSyncMode::timestamp,
                                                                               gpuAddress,
                                                                               immediateValue,
                                                                               rootDeviceEnvironment,
                                                                               args);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    hwParser.findHardwareCommands<FamilyType>();
    ASSERT_NE(0u, hwParser.pipeControlList.size());
    bool timestampPostSyncFound = false;

    for (const auto it : hwParser.pipeControlList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*cmd));
            EXPECT_FALSE(cmd->getUnTypedDataPortCacheFlush());
            timestampPostSyncFound = true;
        }
    }
    EXPECT_TRUE(timestampPostSyncFound);
}
