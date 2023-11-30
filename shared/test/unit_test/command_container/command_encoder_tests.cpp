/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "encode_surface_state_args.h"

using namespace NEO;
using CommandEncoderTests = ::testing::Test;

HWTEST_F(CommandEncoderTests, givenMisalignedSizeWhenProgrammingSurfaceStateForBufferThenAlignedSizeIsProgrammed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE renderSurfaceState{};
    MockExecutionEnvironment executionEnvironment{};

    auto misalignedSize = 1u;
    auto alignedSize = 4u;

    NEO::EncodeSurfaceStateArgs args{};
    args.outMemory = &renderSurfaceState;
    args.gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    args.size = misalignedSize;

    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(alignedSize, renderSurfaceState.getWidth());
}

HWTEST_F(CommandEncoderTests, givenImmDataWriteWhenProgrammingMiFlushDwThenSetAllRequiredFields) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    uint64_t gpuAddress = 0x1230000;
    uint64_t immData = 456;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, gpuAddress, immData, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    unsigned int sizeMultiplier = 1;
    if (UnitTestHelper<FamilyType>::additionalMiFlushDwRequired) {
        sizeMultiplier = 2;
        uint64_t gpuAddress = 0x0;
        uint64_t immData = 0;

        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, miFlushDwCmd->getPostSyncOperation());
        EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
        EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
        miFlushDwCmd++;
    }

    EXPECT_EQ(sizeMultiplier * sizeof(MI_FLUSH_DW), linearStream.getUsed());
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushDwCmd->getPostSyncOperation());
    EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
    EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
    EXPECT_EQ(0u, static_cast<uint32_t>(miFlushDwCmd->getNotifyEnable()));
}

HWTEST2_F(CommandEncoderTests, given57bitVaForDestinationAddressWhenProgrammingMiFlushDwThenVerifyAll57bitsAreUsed, IsPVC) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    const uint64_t setGpuAddress = 0xffffffffffffffff;
    const uint64_t verifyGpuAddress = 0xfffffffffffffff8;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, setGpuAddress, 0, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    EXPECT_EQ(verifyGpuAddress, miFlushDwCmd->getDestinationAddress());
}

HWTEST_F(CommandEncoderTests, whenEncodeMemoryPrefetchCalledThenDoNothing) {

    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[MemoryConstants::pageSize] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    GraphicsAllocation allocation(0, AllocationType::UNKNOWN, nullptr, 123, 456, 789, MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);

    EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 2, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

    EXPECT_EQ(0u, linearStream.getUsed());
    EXPECT_EQ(0u, EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(2, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncoderTests, WhenAnyParameterIsProvidedThenRuntimeGenerationLocalIdsIsRequired) {
    uint32_t workDim = 1;
    uint32_t simd = 8;
    size_t lws[3] = {16, 1, 1};
    std::array<uint8_t, 3> walkOrder = {};
    uint32_t requiredWalkOrder = 0u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
}

HWTEST_F(CommandEncoderTests, givenNotify) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    uint64_t gpuAddress = 0x1230000;
    uint64_t immData = 456;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;
    args.notifyEnable = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, gpuAddress, immData, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    unsigned int sizeMultiplier = 1;
    if (UnitTestHelper<FamilyType>::additionalMiFlushDwRequired) {
        sizeMultiplier = 2;
        uint64_t gpuAddress = 0x0;
        uint64_t immData = 0;

        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, miFlushDwCmd->getPostSyncOperation());
        EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
        EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
        miFlushDwCmd++;
    }

    EXPECT_EQ(sizeMultiplier * sizeof(MI_FLUSH_DW), linearStream.getUsed());
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushDwCmd->getPostSyncOperation());
    EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
    EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
    EXPECT_EQ(1u, static_cast<uint32_t>(miFlushDwCmd->getNotifyEnable()));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncoderTests, whenAppendParamsForImageFromBufferThenNothingChanges) {
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto expectedState = surfaceState;

    EXPECT_EQ(0, memcmp(&expectedState, &surfaceState, sizeof(surfaceState)));
    EncodeSurfaceState<FamilyType>::appendParamsForImageFromBuffer(&surfaceState);

    EXPECT_EQ(0, memcmp(&expectedState, &surfaceState, sizeof(surfaceState)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, givenDebugFlagSetWhenProgrammingMiArbThenSetPreparserDisabledValue) {
    DebugManagerStateRestore restore;

    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;

    for (int32_t value : {-1, 0, 1}) {
        debugManager.flags.ForcePreParserEnabledForMiArbCheck.set(value);

        MI_ARB_CHECK buffer[2] = {};
        LinearStream linearStream(buffer, sizeof(buffer));
        MockExecutionEnvironment executionEnvironment{};
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        rootDeviceEnvironment.initGmm();

        EncodeDummyBlitWaArgs waArgs{};
        EncodeMiArbCheck<FamilyType>::programWithWa(linearStream, false, waArgs);

        if (value == 0) {
            EXPECT_TRUE(buffer[0].getPreParserDisable());
        } else {
            EXPECT_FALSE(buffer[0].getPreParserDisable());
        }
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncoderTests, givenPreXeHpPlatformWhenSetupPostSyncMocsThenNothingHappen) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd{};
    MockExecutionEnvironment executionEnvironment{};
    EXPECT_NO_THROW(EncodeDispatchKernel<FamilyType>::setupPostSyncMocs(walkerCmd, *executionEnvironment.rootDeviceEnvironments[0], false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, givenAtLeastXeHpPlatformWhenSetupPostSyncMocsThenCorrect) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.initGmm();
    bool dcFlush = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, rootDeviceEnvironment);

    {
        WALKER_TYPE walkerCmd{};
        EncodeDispatchKernel<FamilyType>::setupPostSyncMocs(walkerCmd, rootDeviceEnvironment, dcFlush);

        auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
        auto expectedMocs = dcFlush ? gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) : gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);

        EXPECT_EQ(expectedMocs, walkerCmd.getPostSync().getMocs());
    }
    {
        DebugManagerStateRestore restorer{};
        auto expectedMocs = 9u;
        debugManager.flags.OverridePostSyncMocs.set(expectedMocs);
        WALKER_TYPE walkerCmd{};
        EncodeDispatchKernel<FamilyType>::setupPostSyncMocs(walkerCmd, rootDeviceEnvironment, dcFlush);
        EXPECT_EQ(expectedMocs, walkerCmd.getPostSync().getMocs());
    }
}

HWTEST2_F(CommandEncoderTests, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenWalkerIsNotChanged, IsAtMostXeHpcCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    WALKER_TYPE walkerCmd{};
    WALKER_TYPE walkerOnStart{};

    uint32_t yOrder = 2u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(WALKER_TYPE))); // no change

    uint32_t linearOrder = 0u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(WALKER_TYPE))); // no change

    uint32_t fakeOrder = 5u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, fakeOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(WALKER_TYPE))); // no change
}

HWTEST_F(CommandEncoderTests, givenDcFlushNotRequiredWhenGettingDcFlushValueThenReturnValueIsFalse) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    constexpr bool requiredFlag = false;
    bool helperValue = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(requiredFlag, rootDeviceEnvironment);
    EXPECT_FALSE(helperValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, givenXeHpPlatformsWhenGettingDefaultSshSizeThenExpectTwoMegabytes) {
    constexpr size_t expectedSize = 2 * MemoryConstants::megaByte;
    EXPECT_EQ(expectedSize, EncodeStates<FamilyType>::getSshHeapSize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncoderTests, givenPreXeHpPlatformsWhenGettingDefaultSshSizeThenExpectSixtyFourKilobytes) {
    constexpr size_t expectedSize = 64 * MemoryConstants::kiloByte;
    EXPECT_EQ(expectedSize, EncodeStates<FamilyType>::getSshHeapSize());
}

HWTEST2_F(CommandEncoderTests, whenUsingDefaultFilteringAndAppendSamplerStateParamsThenDisableLowQualityFilter, IsAtLeastGen12lp) {
    EXPECT_FALSE(debugManager.flags.ForceSamplerLowFilteringPrecision.get());
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getProductHelper();

    auto state = FamilyType::cmdInitSamplerState;

    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    productHelper.adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}

HWTEST2_F(CommandEncoderTests, whenForcingLowQualityFilteringAndAppendSamplerStateParamsThenEnableLowQualityFilter, IsAtLeastGen12lp) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceSamplerLowFilteringPrecision.set(true);
    EXPECT_TRUE(debugManager.flags.ForceSamplerLowFilteringPrecision.get());
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getProductHelper();

    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    auto state = FamilyType::cmdInitSamplerState;

    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    productHelper.adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE, state.getLowQualityFilter());
}