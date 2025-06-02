/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/helpers/state_base_address_tests.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "hw_cmds_xe3_core.h"

using namespace NEO;
HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterEncodeMiFlushDWTest, whenMiFlushDwIsProgrammedThenSetFlushCcsAndLlc, IGFX_XE3_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTests, whenEncodeMemoryPrefetchCalledThenDoNothing, IGFX_XE3_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandEncodeStatesTestPvcAndLater, givenCommandContainerWhenNumGrfRequiredIsGreaterThanDefaultThenLargeGrfModeEnabled, IGFX_XE3_CORE);

using CommandEncoderSbaTestXe3 = Test<DeviceFixture>;

using CommandEncodeXe3CoreTest = ::testing::Test;

XE3_CORETEST_F(CommandEncodeXe3CoreTest, whenMiFlushDwIsProgrammedThenSetAndFlushLlcWithoutCcs) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, 0x1230000, 456, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);
    auto expectedFlushCcs = NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]) ? 1u : 0u;
    EXPECT_EQ(expectedFlushCcs, miFlushDwCmd->getFlushCcs());
    EXPECT_EQ(1u, miFlushDwCmd->getFlushLlc());
}

XE3_CORETEST_F(CommandEncodeXe3CoreTest, givenInterfaceDescriptorDataWhenAdjustInterfaceDescriptoDataIsCalledThenArgsDoesntChange) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    INTERFACE_DESCRIPTOR_DATA iddArg;
    DefaultWalkerType walkerCmd{};
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    auto samplerCount = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT>(1u);
    iddArg.setSamplerCount(samplerCount);
    iddArg.setBindingTableEntryCount(2u);
    MockDevice mockDevice;
    uint32_t threadsPerGroup = 1;
    uint32_t threadGroups[] = {walkerCmd.getThreadGroupIdXDimension(), walkerCmd.getThreadGroupIdYDimension(), walkerCmd.getThreadGroupIdZDimension()};
    EncodeDispatchKernel<FamilyType>::encodeThreadGroupDispatch(iddArg, mockDevice, *defaultHwInfo, threadGroups, 0, 1, 0, threadsPerGroup, walkerCmd);
    EXPECT_EQ(2u, iddArg.getBindingTableEntryCount());
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_1_AND_4_SAMPLERS_USED, iddArg.getSamplerCount());
}

XE3_CORETEST_F(CommandEncodeXe3CoreTest, givenDebugVariableSetwhenProgramingStatePrefetchThenSetCorrectFields) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[sizeof(STATE_PREFETCH) * 4] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    constexpr uint64_t gpuVa = 0x100000;
    constexpr size_t numCachelines = 3;

    const GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, gpuVa, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

    auto rootDeviceEnv = mockExecutionEnvironment.rootDeviceEnvironments[0].get();
    auto usage = CacheSettingsHelper::getGmmUsageType(allocation.getAllocationType(), false, rootDeviceEnv->getProductHelper(), rootDeviceEnv->getHardwareInfo());
    uint32_t mocs = rootDeviceEnv->getGmmHelper()->getMOCS(usage);

    static constexpr std::array<uint32_t, 7> expectedSizes = {{
        MemoryConstants::cacheLineSize - 1,
        MemoryConstants::cacheLineSize,
        MemoryConstants::cacheLineSize + 1,
        MemoryConstants::cacheLineSize * numCachelines,
        MemoryConstants::pageSize64k - 1,
        MemoryConstants::pageSize64k,
        (MemoryConstants::pageSize64k * 2) + 1,
    }};

    for (auto expectedSize : expectedSizes) {
        memset(buffer, 0, sizeof(buffer));
        LinearStream linearStream(buffer, sizeof(buffer));

        uint32_t alignedSize = alignUp(expectedSize, MemoryConstants::pageSize64k);
        uint32_t expectedCmdsCount = std::max((alignedSize / static_cast<uint32_t>(MemoryConstants::pageSize64k)), 1u);

        EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(expectedSize, *mockExecutionEnvironment.rootDeviceEnvironments[0]));

        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, expectedSize, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, linearStream.getUsed());

        for (uint32_t i = 0; i < expectedCmdsCount; i++) {
            uint32_t programmedSize = statePrefetchCmd[i].getPrefetchSize() * MemoryConstants::cacheLineSize;

            EXPECT_EQ(statePrefetchCmd[i].getAddress(), gpuVa + (i * MemoryConstants::pageSize64k));
            EXPECT_FALSE(statePrefetchCmd[i].getKernelInstructionPrefetch());
            EXPECT_FALSE(statePrefetchCmd[i].getParserStall());
            EXPECT_EQ(mocs, statePrefetchCmd[i].getMemoryObjectControlState());

            if (programmedSize > expectedSize) {
                // cacheline alignemnt
                EXPECT_TRUE((programmedSize - expectedSize) < MemoryConstants::cacheLineSize);
                expectedSize = 0;
            } else {
                expectedSize -= programmedSize;
            }
        }
        EXPECT_EQ(0u, expectedSize);
    }
}

XE3_CORETEST_F(CommandEncodeXe3CoreTest, givenIsaAllocationWhenProgrammingPrefetchThenSetKernelInstructionPrefetchBit) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    EXPECT_EQ(sizeof(STATE_PREFETCH), EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(4096, *mockExecutionEnvironment.rootDeviceEnvironments[0]));

    AllocationType isaTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal};

    for (auto &isaType : isaTypes) {
        memset(buffer, 0, sizeof(STATE_PREFETCH));
        LinearStream linearStream(buffer, sizeof(buffer));

        const GraphicsAllocation allocation(0, 1u /*num gmms*/, isaType, nullptr,
                                            1234, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(sizeof(STATE_PREFETCH), linearStream.getUsed());

        EXPECT_TRUE(statePrefetchCmd->getKernelInstructionPrefetch());
    }
}

XE3_CORETEST_F(CommandEncodeXe3CoreTest, givenDebugFlagSetWhenProgramPrefetchCalledThenDoPrefetchIfSetToOne) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    DebugManagerStateRestore restore;
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};

    AllocationType isaTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal};

    for (auto &isaType : isaTypes) {
        memset(buffer, 0, sizeof(STATE_PREFETCH));
        LinearStream linearStream(buffer, sizeof(buffer));

        const GraphicsAllocation allocation(0, 1u /*num gmms*/, isaType,
                                            nullptr, 1234, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

        debugManager.flags.EnableMemoryPrefetch.set(0);
        EXPECT_EQ(0u, EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(100, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 100, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(0u, linearStream.getUsed());

        debugManager.flags.EnableMemoryPrefetch.set(1);
        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(sizeof(STATE_PREFETCH), linearStream.getUsed());
        auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);
        EXPECT_TRUE(statePrefetchCmd->getKernelInstructionPrefetch());
    }
}

XE3_CORETEST_F(CommandEncodeXe3CoreTest, givenDebugFlagSetWhenProgrammingPrefetchThenSetParserStall) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    MockExecutionEnvironment mockExecutionEnvironment{};

    DebugManagerStateRestore restore;
    debugManager.flags.ForceCsStallForStatePrefetch.set(1);

    const GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer,
                                        nullptr, 1234, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};

    LinearStream linearStream(buffer, sizeof(buffer));

    EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    EXPECT_TRUE(statePrefetchCmd->getParserStall());
}

XE3_CORETEST_F(CommandEncodeXe3CoreTest, givenOffsetWhenProgrammingStatePrefetchThenSetCorrectGpuVa) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[sizeof(STATE_PREFETCH) * 4] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    constexpr uint64_t gpuVa = 0x100000;
    constexpr uint32_t gpuVaOffset = 0x10000;

    const GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, gpuVa, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

    memset(buffer, 0, sizeof(buffer));
    LinearStream linearStream(buffer, sizeof(buffer));

    uint32_t expectedCmdsCount = 3;
    uint32_t alignedSize = MemoryConstants::pageSize64k * expectedCmdsCount;

    EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, alignedSize, gpuVaOffset, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, linearStream.getUsed());

    for (uint32_t i = 0; i < expectedCmdsCount; i++) {
        uint64_t expectedVa = gpuVa + gpuVaOffset + (i * MemoryConstants::pageSize64k);
        EXPECT_EQ(expectedVa, statePrefetchCmd[i].getAddress());
    }
}

using EncodeKernelXe3CoreTest = Test<CommandEncodeStatesFixture>;

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenNoFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenDoNotGenerateFenceCommands) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenGenerateFenceCommands) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenDefaultSettingForFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenGenerateFenceCommands) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenDefaultSettingForFenceWhenKernelUsesSystemMemoryFlagTrueAndNoHostSignalEventThenNotUseSystemFence) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.postSyncArgs.isUsingSystemAllocation = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenDefaultSettingForFenceWhenEventHostSignalScopeFlagTrueAndNoSystemMemoryThenNotUseSystemFence) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.postSyncArgs.isHostScopeSignalEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenDefaultSettingForFenceWhenKernelUsesSystemMemoryAndHostSignalEventFlagTrueThenUseSystemFence) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.postSyncArgs.isUsingSystemAllocation = true;
    dispatchArgs.postSyncArgs.isHostScopeSignalEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_EQ(postSyncData.getSystemMemoryFenceRequest(), !pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice);
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenDebugFlagSetWhenSetPropertiesAllCalledThenDisablePipelinedThreadArbitrationPolicy) {
    DebugManagerStateRestore restore;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    {
        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);

        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled);
        EXPECT_TRUE(streamProperties.stateComputeMode.isPipelinedEuThreadArbitrationEnabled());
    }

    {
        debugManager.flags.PipelinedEuThreadArbitration.set(0);

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);

        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled);
        EXPECT_FALSE(streamProperties.stateComputeMode.isPipelinedEuThreadArbitrationEnabled());
    }
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenDebugFlagWhenProgrammingStateComputeModeThenEnableVrtFieldIsCorrectlySet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore restore;

    uint8_t buffer[sizeof(STATE_COMPUTE_MODE)]{};
    MockExecutionEnvironment executionEnvironment{};
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    {
        // default
        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_TRUE(stateComputeModeCmd.getEnableVariableRegisterSizeAllocationVrt());
    }

    {
        // enabled
        debugManager.flags.EnableXe3VariableRegisterSizeAllocation.set(1);

        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_TRUE(stateComputeModeCmd.getEnableVariableRegisterSizeAllocationVrt());
    }

    {
        // disabled
        debugManager.flags.EnableXe3VariableRegisterSizeAllocation.set(0);

        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_FALSE(stateComputeModeCmd.getEnableVariableRegisterSizeAllocationVrt());
    }
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenSystolicModeRequiredWhenEncodeIsCalledThenDontReprogramPipelineSelect) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    bool requiresUncachedMocs = false;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    cmdContainer->lastPipelineSelectModeRequiredRef() = false;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.preemptionMode = PreemptionMode::Initial;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
    EXPECT_FALSE(cmdContainer->lastPipelineSelectModeRequiredRef());
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenXe3ThenPipelineSelectIsNotProgrammed) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), dispatchInterface->kernelDescriptor);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    EXPECT_EQ(itor, commands.end());
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenDispatchWalkOrderIsProgrammedCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    DefaultWalkerType walkerCmd{};
    uint32_t yOrder = 2u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[yOrder], HwWalkOrderHelper::yOrderWalk);

    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK, walkerCmd.getDispatchWalkOrder());

    uint32_t linearOrder = 0u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[linearOrder], HwWalkOrderHelper::linearWalk);

    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK, walkerCmd.getDispatchWalkOrder());

    auto currentDispatchWalkOrder = walkerCmd.getDispatchWalkOrder();
    uint32_t fakeOrder = 5u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, fakeOrder, rootDeviceEnvironment);
    EXPECT_EQ(currentDispatchWalkOrder, walkerCmd.getDispatchWalkOrder()); // no change
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenRequiredWorkGroupOrderWhenCallEncodeThreadDataThenDispatchWalkOrderIsProgrammedCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker;

    uint32_t startWorkGroup[3] = {1, 1, 1};
    uint32_t numWorkGroups[3] = {1, 1, 1};
    uint32_t workGroupSizes[3] = {1, 1, 1};

    uint32_t yOrder = 2u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[yOrder], HwWalkOrderHelper::yOrderWalk);
    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, false, false, true, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK, walkerCmd.getDispatchWalkOrder());

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, true, false, true, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK, walkerCmd.getDispatchWalkOrder());

    uint32_t linearOrder = 0u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[linearOrder], HwWalkOrderHelper::linearWalk);
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, false, false, true, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK, walkerCmd.getDispatchWalkOrder());

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, true, false, true, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK, walkerCmd.getDispatchWalkOrder());
}

XE3_CORETEST_F(EncodeKernelXe3CoreTest, givenCommandContainerWhenNumGrfRequiredIsGreaterThanDefaultThenLargeGrfModeEnabled) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore restore;
    debugManager.flags.EnableXe3VariableRegisterSizeAllocation.set(0);

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    StreamProperties streamProperties{};
    streamProperties.initSupport(rootDeviceEnvironment);
    streamProperties.stateComputeMode.setPropertiesAll(false, GrfConfig::largeGrfNumber, 0u, PreemptionMode::Disabled);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, rootDeviceEnvironment);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_COMPUTE_MODE *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*itorCmd);
    EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), cmd->getLargeGrfMode());
}

XE3_CORETEST_F(CommandEncodeXe3CoreTest, givenSurfaceStateWhenAuxParamsForMCSCCSAreSetThenCorrectAuxModeIsSet) {
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;

    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&surfaceState, releaseHelper.get());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), EncodeSurfaceState<FamilyType>::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS);
}

using Xe3SbaTest = SbaTest;

XE3_CORETEST_F(Xe3SbaTest, givenSpecificProductFamilyWhenAppendingSbaThenProgramWtL1CachePolicy) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, sbaCmd.getL1CacheControlCachePolicy());
}

XE3_CORETEST_F(Xe3SbaTest, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceStatelessL1CachingPolicy.set(0u);

    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(0u, sbaCmd.getL1CacheControlCachePolicy());

    debugManager.flags.ForceStatelessL1CachingPolicy.set(1u);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(1u, sbaCmd.getL1CacheControlCachePolicy());
}
