/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
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

HWTEST_F(CommandEncoderTests, givenDifferentInputParamsWhenCreatingStandaloneInOrderExecInfoThenSetupCorrectly) {
    MockDevice mockDevice;

    uint64_t counterValue = 2;
    uint64_t *hostAddress = &counterValue;
    uint64_t gpuAddress = castToUint64(ptrOffset(&counterValue, 64));

    auto inOrderExecInfo = InOrderExecInfo::createFromExternalAllocation(mockDevice, gpuAddress, hostAddress, counterValue);

    EXPECT_EQ(counterValue, inOrderExecInfo->getCounterValue());
    EXPECT_EQ(hostAddress, inOrderExecInfo->getBaseHostAddress());
    EXPECT_EQ(gpuAddress, inOrderExecInfo->getBaseDeviceAddress());
    EXPECT_EQ(nullptr, inOrderExecInfo->getDeviceCounterAllocation());
    EXPECT_EQ(nullptr, inOrderExecInfo->getHostCounterAllocation());

    inOrderExecInfo->reset();

    EXPECT_EQ(0u, inOrderExecInfo->getCounterValue());
}

HWTEST_F(CommandEncoderTests, givenDifferentInputParamsWhenCreatingInOrderExecInfoThenSetupCorrectly) {
    MockDevice mockDevice;

    auto &memoryManager = *mockDevice.getMemoryManager();
    uint64_t storage1[2] = {1, 1};
    uint64_t storage2[2] = {1, 1};

    {
        auto inOrderExecInfo = InOrderExecInfo::create(mockDevice, 2, false, false, false);

        EXPECT_EQ(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer(), inOrderExecInfo->getBaseHostAddress());
        EXPECT_EQ(nullptr, inOrderExecInfo->getHostCounterAllocation());
        EXPECT_FALSE(inOrderExecInfo->isHostStorageDuplicated());
        EXPECT_FALSE(inOrderExecInfo->isRegularCmdList());
        EXPECT_FALSE(inOrderExecInfo->isAtomicDeviceSignalling());
        EXPECT_EQ(2u, inOrderExecInfo->getNumDevicePartitionsToWait());
        EXPECT_EQ(2u, inOrderExecInfo->getNumHostPartitionsToWait());
        EXPECT_EQ(0u, InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo));
    }

    {
        auto deviceSyncAllocation = new MockGraphicsAllocation(&storage1, sizeof(storage1));

        InOrderExecInfo inOrderExecInfo(deviceSyncAllocation, nullptr, memoryManager, 2, true, true);
        EXPECT_TRUE(inOrderExecInfo.isRegularCmdList());
        EXPECT_TRUE(inOrderExecInfo.isAtomicDeviceSignalling());
        EXPECT_EQ(1u, inOrderExecInfo.getNumDevicePartitionsToWait());
        EXPECT_EQ(2u, inOrderExecInfo.getNumHostPartitionsToWait());
    }

    {
        auto inOrderExecInfo = InOrderExecInfo::create(mockDevice, 2, false, false, true);

        EXPECT_NE(inOrderExecInfo->getDeviceCounterAllocation(), inOrderExecInfo->getHostCounterAllocation());

        EXPECT_NE(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer(), inOrderExecInfo->getBaseHostAddress());
        EXPECT_EQ(inOrderExecInfo->getHostCounterAllocation()->getUnderlyingBuffer(), inOrderExecInfo->getBaseHostAddress());
        EXPECT_TRUE(inOrderExecInfo->isHostStorageDuplicated());
    }

    {
        auto deviceSyncAllocation = new MockGraphicsAllocation(&storage1, sizeof(storage1));
        auto hostSyncAllocation = new MockGraphicsAllocation(&storage2, sizeof(storage2));
        InOrderExecInfo inOrderExecInfo(deviceSyncAllocation, hostSyncAllocation, memoryManager, 1, false, false);
        auto deviceAllocHostAddress = reinterpret_cast<uint64_t *>(inOrderExecInfo.getDeviceCounterAllocation()->getUnderlyingBuffer());
        EXPECT_EQ(0u, inOrderExecInfo.getCounterValue());
        EXPECT_EQ(0u, inOrderExecInfo.getRegularCmdListSubmissionCounter());
        EXPECT_EQ(0u, inOrderExecInfo.getAllocationOffset());
        EXPECT_EQ(0u, *inOrderExecInfo.getBaseHostAddress());
        EXPECT_EQ(0u, *deviceAllocHostAddress);

        inOrderExecInfo.addCounterValue(2);
        inOrderExecInfo.addRegularCmdListSubmissionCounter(3);
        inOrderExecInfo.setAllocationOffset(4);
        *inOrderExecInfo.getBaseHostAddress() = 5;
        *deviceAllocHostAddress = 6;

        EXPECT_EQ(2u, inOrderExecInfo.getCounterValue());
        EXPECT_EQ(3u, inOrderExecInfo.getRegularCmdListSubmissionCounter());
        EXPECT_EQ(4u, inOrderExecInfo.getAllocationOffset());

        inOrderExecInfo.reset();

        EXPECT_EQ(0u, inOrderExecInfo.getCounterValue());
        EXPECT_EQ(0u, inOrderExecInfo.getRegularCmdListSubmissionCounter());
        EXPECT_EQ(0u, inOrderExecInfo.getAllocationOffset());
        EXPECT_EQ(0u, *inOrderExecInfo.getBaseHostAddress());
        EXPECT_EQ(0u, *deviceAllocHostAddress);
    }

    {
        auto deviceSyncAllocation = new MockGraphicsAllocation(&storage1, sizeof(storage1));

        InOrderExecInfo inOrderExecInfo(deviceSyncAllocation, nullptr, memoryManager, 2, true, false);

        EXPECT_EQ(0u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
        inOrderExecInfo.addCounterValue(2);
        EXPECT_EQ(0u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
        inOrderExecInfo.addRegularCmdListSubmissionCounter(1);
        EXPECT_EQ(0u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
        inOrderExecInfo.addRegularCmdListSubmissionCounter(1);
        EXPECT_EQ(2u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
        inOrderExecInfo.addRegularCmdListSubmissionCounter(1);
        EXPECT_EQ(4u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
    }
}

HWTEST_F(CommandEncoderTests, givenInOrderExecInfoWhenPatchingThenSetCorrectValues) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    MockMemoryManager memoryManager(mockExecutionEnvironment);
    uint64_t storage1[2] = {1, 1};

    auto deviceSyncAllocation = new MockGraphicsAllocation(&storage1, sizeof(storage1));

    auto inOrderExecInfo = std::make_shared<InOrderExecInfo>(deviceSyncAllocation, nullptr, memoryManager, 2, true, false);
    inOrderExecInfo->addCounterValue(1);

    {
        auto cmd = FamilyType::cmdInitStoreDataImm;
        cmd.setDataDword0(1);

        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, reinterpret_cast<void *>(&cmd), nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::sdi, false, false);
        patchCmd.patch(2);

        EXPECT_EQ(3u, cmd.getDataDword0());
    }

    {
        auto cmd = FamilyType::cmdInitMiSemaphoreWait;
        cmd.setSemaphoreDataDword(1);

        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, &cmd, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::semaphore, false, false);
        patchCmd.patch(2);
        EXPECT_EQ(1u, cmd.getSemaphoreDataDword());

        inOrderExecInfo->addRegularCmdListSubmissionCounter(3);
        patchCmd.patch(3);
        EXPECT_EQ(3u, cmd.getSemaphoreDataDword());

        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmdInternal(nullptr, &cmd, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::semaphore, false, false);
        patchCmdInternal.patch(3);

        EXPECT_EQ(4u, cmd.getSemaphoreDataDword());
    }

    {
        auto cmd1 = FamilyType::cmdInitLoadRegisterImm;
        auto cmd2 = FamilyType::cmdInitLoadRegisterImm;
        cmd1.setDataDword(1);
        cmd2.setDataDword(1);

        inOrderExecInfo->reset();
        inOrderExecInfo->addCounterValue(1);
        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, &cmd1, &cmd2, 1, InOrderPatchCommandHelpers::PatchCmdType::lri64b, false, false);
        patchCmd.patch(3);
        EXPECT_EQ(1u, cmd1.getDataDword());
        EXPECT_EQ(1u, cmd2.getDataDword());

        inOrderExecInfo->addRegularCmdListSubmissionCounter(3);
        patchCmd.patch(3);
        EXPECT_EQ(3u, cmd1.getDataDword());
        EXPECT_EQ(0u, cmd2.getDataDword());

        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmdInternal(nullptr, &cmd1, &cmd2, 1, InOrderPatchCommandHelpers::PatchCmdType::lri64b, false, false);
        patchCmdInternal.patch(2);

        EXPECT_EQ(3u, cmd1.getDataDword());
        EXPECT_EQ(0u, cmd2.getDataDword());
    }

    InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, nullptr, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::none, false, false);
    EXPECT_ANY_THROW(patchCmd.patch(1));
}

HWTEST_F(CommandEncoderTests, givenInOrderExecInfoWhenPatchingWalkerThenSetCorrectValues) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    MockMemoryManager memoryManager(mockExecutionEnvironment);
    uint64_t storage1[2] = {1, 1};

    auto deviceSyncAllocation = new MockGraphicsAllocation(&storage1, sizeof(storage1));

    auto inOrderExecInfo = std::make_shared<InOrderExecInfo>(deviceSyncAllocation, nullptr, memoryManager, 2, false, false);

    auto cmd = FamilyType::cmdInitGpgpuWalker;

    InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, &cmd, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::walker, false, false);

    if constexpr (FamilyType::walkerPostSyncSupport) {
        patchCmd.patch(2);

        EXPECT_EQ(3u, cmd.getPostSync().getImmediateData());
    } else {
        EXPECT_ANY_THROW(patchCmd.patch(2));
    }
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

    GraphicsAllocation allocation(0, AllocationType::unknown, nullptr, 123, 456, 789, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

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

        EncodeMiArbCheck<FamilyType>::program(linearStream, false);

        if (value == 0) {
            EXPECT_TRUE(buffer[0].getPreParserDisable());
        } else {
            EXPECT_FALSE(buffer[0].getPreParserDisable());
        }
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncoderTests, givenPreXeHpPlatformWhenSetupPostSyncMocsThenNothingHappen) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd{};
    MockExecutionEnvironment executionEnvironment{};
    EXPECT_NO_THROW(EncodeDispatchKernel<FamilyType>::setupPostSyncMocs(walkerCmd, *executionEnvironment.rootDeviceEnvironments[0], false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, givenAtLeastXeHpPlatformWhenSetupPostSyncMocsThenCorrect) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.initGmm();
    bool dcFlush = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, rootDeviceEnvironment);

    {
        DefaultWalkerType walkerCmd{};
        EncodeDispatchKernel<FamilyType>::setupPostSyncMocs(walkerCmd, rootDeviceEnvironment, dcFlush);

        auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
        auto expectedMocs = dcFlush ? gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) : gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);

        EXPECT_EQ(expectedMocs, walkerCmd.getPostSync().getMocs());
    }
    {
        DebugManagerStateRestore restorer{};
        auto expectedMocs = 9u;
        debugManager.flags.OverridePostSyncMocs.set(expectedMocs);
        DefaultWalkerType walkerCmd{};
        EncodeDispatchKernel<FamilyType>::setupPostSyncMocs(walkerCmd, rootDeviceEnvironment, dcFlush);
        EXPECT_EQ(expectedMocs, walkerCmd.getPostSync().getMocs());
    }
}

HWTEST2_F(CommandEncoderTests, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenWalkerIsNotChanged, IsAtMostXeHpcCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    DefaultWalkerType walkerCmd{};
    DefaultWalkerType walkerOnStart{};

    uint32_t yOrder = 2u;
    EncodeDispatchKernel<FamilyType>::template adjustWalkOrder<DefaultWalkerType>(walkerCmd, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change

    uint32_t linearOrder = 0u;
    EncodeDispatchKernel<FamilyType>::template adjustWalkOrder<DefaultWalkerType>(walkerCmd, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change

    uint32_t fakeOrder = 5u;
    EncodeDispatchKernel<FamilyType>::template adjustWalkOrder<DefaultWalkerType>(walkerCmd, fakeOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change
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

HWTEST2_F(CommandEncoderTests, whenAskingForImplicitScalingValuesThenAlwaysReturnStubs, IsAtMostGen12lp) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    MockExecutionEnvironment executionEnvironment{};
    auto rootExecEnv = executionEnvironment.rootDeviceEnvironments[0].get();

    uint8_t buffer[128] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    WalkerType walkerCmd = {};

    DeviceBitfield deviceBitField = 1;
    uint32_t partitionCount = 1;

    Vec3<size_t> vec3 = {1, 1, 1};

    EXPECT_EQ(0u, ImplicitScalingDispatch<FamilyType>::template getSize<WalkerType>(false, false, deviceBitField, vec3, vec3));

    void *ptr = nullptr;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(linearStream, walkerCmd, &ptr, deviceBitField, RequiredPartitionDim::x, partitionCount, false, false, false, false, 0, *defaultHwInfo);
    EXPECT_EQ(0u, linearStream.getUsed());

    EXPECT_TRUE(ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired());

    EXPECT_EQ(0u, ImplicitScalingDispatch<FamilyType>::getBarrierSize(*rootExecEnv, false, false));

    PipeControlArgs pcArgs = {};
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(linearStream, deviceBitField, pcArgs, *rootExecEnv, 0, 0, false, false);
    EXPECT_EQ(0u, linearStream.getUsed());

    EXPECT_EQ(0u, ImplicitScalingDispatch<FamilyType>::getRegisterConfigurationSize());

    ImplicitScalingDispatch<FamilyType>::dispatchRegisterConfiguration(linearStream, 0, 0);
    EXPECT_EQ(0u, linearStream.getUsed());

    EXPECT_EQ(0u, ImplicitScalingDispatch<FamilyType>::getOffsetRegisterSize());

    ImplicitScalingDispatch<FamilyType>::dispatchOffsetRegister(linearStream, 0);
    EXPECT_EQ(0u, linearStream.getUsed());

    EXPECT_EQ(static_cast<uint32_t>(sizeof(uint64_t)), ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset());

    EXPECT_EQ(static_cast<uint32_t>(GfxCoreHelperHw<FamilyType>::getSingleTimestampPacketSizeHw()), ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset());

    EXPECT_FALSE(ImplicitScalingDispatch<FamilyType>::platformSupportsImplicitScaling(*rootExecEnv));
}