/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;

using CommandEncodeStatesTestPvcAndLater = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenOverrideSlmTotalSizeDebugVariableWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly, IsAtLeastXeHpcCore) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;

    uint32_t maxValueToProgram = 0xC;

    for (uint32_t valueToProgram = 0x0; valueToProgram < maxValueToProgram; valueToProgram++) {
        debugManager.flags.OverrideSlmAllocationSize.set(valueToProgram);
        cmdContainer->reset();
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_EQ(valueToProgram, idd.getSharedLocalMemorySize());
    }
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenVariousValuesWhenCallingSetBarrierEnableThenCorrectValuesAreSet, IsAtLeastXeHpcCore) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using BARRIERS = typename INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS;
    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    MockDevice device;
    auto hwInfo = device.getHardwareInfo();

    struct BarrierCountToBarrierNumEnum {
        uint32_t barrierCount;
        uint32_t numBarriersEncoding;
    };
    constexpr BarrierCountToBarrierNumEnum barriers[8] = {{0, 0},
                                                          {1, 1},
                                                          {2, 2},
                                                          {4, 3},
                                                          {8, 4},
                                                          {16, 5},
                                                          {24, 6},
                                                          {32, 7}};
    for (auto &[barrierCount, numBarriersEnum] : barriers) {
        EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, barrierCount, hwInfo);
        EXPECT_EQ(numBarriersEnum, idd.getNumberOfBarriers());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTestPvcAndLater, givenCommandContainerWhenNumGrfRequiredIsGreaterThanDefaultThenLargeGrfModeEnabled) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
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

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenDebugVariableWhenPostSyncIsProgrammedThenL1IsNotFlushed, IsAtLeastXeHpcCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForcePostSyncL1Flush.set(0);
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, pDevice->getMemoryManager());

    auto inOrderExecInfo = InOrderExecInfo::create(deviceTagAllocator.getTag(), nullptr, *pDevice, 1, false);

    dispatchArgs.inOrderExecInfo = inOrderExecInfo.get();

    EncodeDispatchKernel<FamilyType>::template setupPostSyncForInOrderExec<DefaultWalkerType>(walkerCmd, dispatchArgs);

    auto &postSyncData = walkerCmd.getPostSync();
    EXPECT_FALSE(postSyncData.getDataportPipelineFlush());
    EXPECT_FALSE(postSyncData.getDataportSubsliceCacheFlush());
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenDispatchSizeSmallerOrEqualToAvailableThreadCountWhenAdjustInterfaceDescriptorDataIsCalledThenThreadGroupDispatchSizeIsCorrectlySet, IsAtLeastXeHpcCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.gtSystemInfo.EUCount = 2u;

    const uint32_t numGrf = GrfConfig::largeGrfNumber;
    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();
    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);

    for (const auto threadGroupCount : {1u, 2u}) {
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);

        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
    }
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenMultipleTilesAndImplicitScalingWhenAdjustInterfaceDescriptorDataIsCalledThenThreadGroupDispatchSizeIsCorrectlySet, IsAtLeastXeHpcCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWalkerPartition.set(0);
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.gtSystemInfo.EUCount = 32;
    hwInfo.gtSystemInfo.DualSubSliceCount = hwInfo.gtSystemInfo.MaxDualSubSlicesSupported;
    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();
    const uint32_t numGrf = GrfConfig::defaultGrfNumber;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const uint32_t threadGroupCount = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, numGrf) / 32u;
    iddArg.setNumberOfThreadsInGpgpuThreadGroup(64u);

    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    ASSERT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());

    debugManager.flags.EnableWalkerPartition.set(1);
    pDevice->numSubDevices = 2;
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenNumberOfThreadsInThreadGroupWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsCorrectlySet, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadGroupDispatchSizeAlgorithm.set(1u);
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    auto hwInfo = pDevice->getHardwareInfo();

    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();
    const uint32_t threadGroupCount = 512u;
    const uint32_t numGrf = GrfConfig::defaultGrfNumber;
    std::array<std::pair<uint32_t, uint32_t>, 3> testParams = {{{16u, InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8},
                                                                {32u, InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4},
                                                                {64u, InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2}}};

    for (const auto &[numberOfThreadsInThreadGroup, expectedThreadGroupDispatchSize] : testParams) {
        iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInThreadGroup);

        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);

        EXPECT_EQ(expectedThreadGroupDispatchSize, iddArg.getThreadGroupDispatchSize());
    }
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenNumberOfThreadsInThreadGroupAndDimensionsWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsCorrectlySet, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadGroupDispatchSizeAlgorithm.set(1u);
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    auto hwInfo = pDevice->getHardwareInfo();

    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();
    const uint32_t threadGroupCount = 512u;
    const uint32_t numGrf = GrfConfig::defaultGrfNumber;

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(16u);

    {
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
    }
    walkerCmd.setThreadGroupIdYDimension(2);
    walkerCmd.setThreadGroupIdZDimension(1);
    {
        walkerCmd.setThreadGroupIdXDimension(4);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4, iddArg.getThreadGroupDispatchSize());
    }
    {
        walkerCmd.setThreadGroupIdXDimension(2);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());
    }
    {
        walkerCmd.setThreadGroupIdXDimension(1);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
    }
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(2);
    {
        walkerCmd.setThreadGroupIdXDimension(4);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4, iddArg.getThreadGroupDispatchSize());
    }
    {
        walkerCmd.setThreadGroupIdXDimension(2);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());
    }
    {
        walkerCmd.setThreadGroupIdXDimension(1);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
    }
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    {
        walkerCmd.setThreadGroupIdXDimension(4);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
    }
    {
        walkerCmd.setThreadGroupIdXDimension(2);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
    }
    {
        walkerCmd.setThreadGroupIdXDimension(1);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
    }
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdZDimension(2);
    {
        walkerCmd.setThreadGroupIdYDimension(4);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4, iddArg.getThreadGroupDispatchSize());
    }
    {
        walkerCmd.setThreadGroupIdYDimension(2);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());
    }
    {
        walkerCmd.setThreadGroupIdYDimension(1);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
    }
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenDifferentNumGrfWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsCorrectlySet, IsAtLeastXeHpcCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    auto hwInfo = pDevice->getHardwareInfo();

    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const uint32_t numberOfThreadsInThreadGroup = 1u;

    {
        const uint32_t numGrf = GrfConfig::defaultGrfNumber;
        const uint32_t threadGroupCount = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, numGrf);
        iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInThreadGroup);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        ASSERT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
    }

    {
        const uint32_t numGrf = GrfConfig::largeGrfNumber;
        const uint32_t threadGroupCount = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, numGrf);
        iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInThreadGroup);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
        EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
    }
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenVariousDispatchParamtersWhenAlogrithmV2IsUsedThenProperThreadGroupDispatchSizeIsChoosen, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadGroupDispatchSizeAlgorithm.set(2u);
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    auto mutableHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    mutableHwInfo->gtSystemInfo.MaxSubSlicesSupported = 64u;
    mutableHwInfo->gtSystemInfo.ThreadCount = 4096u;
    auto hwInfo = pDevice->getHardwareInfo();

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    uint32_t numGrf = GrfConfig::defaultGrfNumber;
    const uint32_t threadGroupCount = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, numGrf);

    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(256);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(64u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(64);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(512);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(32u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(512);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(8u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(512);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(32u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(510);
    walkerCmd.setThreadGroupIdYDimension(512);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(32u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(509);
    walkerCmd.setThreadGroupIdYDimension(512);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(32u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(508);
    walkerCmd.setThreadGroupIdYDimension(512);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(16u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(508);
    walkerCmd.setThreadGroupIdYDimension(512);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(16u);
    numGrf = GrfConfig::largeGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(508);
    walkerCmd.setThreadGroupIdYDimension(512);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(32u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(510);
    walkerCmd.setThreadGroupIdZDimension(512);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(32u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(509);
    walkerCmd.setThreadGroupIdZDimension(512);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(16u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(508);
    walkerCmd.setThreadGroupIdZDimension(512);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4, iddArg.getThreadGroupDispatchSize());

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(32u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(508);
    walkerCmd.setThreadGroupIdZDimension(512);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenDualSubSliceCountNotEqualToMaxSubsliceCounteWhenTgDispatchSizeIsSelectedThenAlgorithmV1IsUsed, IsAtLeastXeHpcCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    auto mutableHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    mutableHwInfo->gtSystemInfo.MaxSubSlicesSupported = 64u;
    mutableHwInfo->gtSystemInfo.SubSliceCount = 32u;
    mutableHwInfo->gtSystemInfo.ThreadCount = 2048u;
    auto hwInfo = pDevice->getHardwareInfo();

    uint32_t numGrf = GrfConfig::defaultGrfNumber;

    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();

    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);
    numGrf = GrfConfig::defaultGrfNumber;
    walkerCmd.setThreadGroupIdXDimension(256);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, 256u, numGrf, walkerCmd);
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenNumberOfThreadsInThreadGroupAndDebugFlagDisabledWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsDefault, IsAtLeastXeHpcCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    DebugManagerStateRestore restorer;
    debugManager.flags.AdjustThreadGroupDispatchSize.set(0);
    auto hwInfo = pDevice->getHardwareInfo();

    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();
    const uint32_t threadGroupCount = 512u;
    const uint32_t numGrf = GrfConfig::defaultGrfNumber;
    std::array<std::pair<uint32_t, uint32_t>, 3> testParams = {{{16u, InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1},
                                                                {32u, InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1},
                                                                {64u, InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1}}};

    for (const auto &[numberOfThreadsInThreadGroup, expectedThreadGroupDispatchSize] : testParams) {
        iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInThreadGroup);

        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);

        EXPECT_EQ(expectedThreadGroupDispatchSize, iddArg.getThreadGroupDispatchSize());
    }
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenThreadGroupCountZeroWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsSetToDefault, IsAtLeastXeHpcCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;
    DefaultWalkerType walkerCmd{};
    auto hwInfo = pDevice->getHardwareInfo();

    const uint32_t threadGroupCount = 0u;
    const uint32_t numGrf = GrfConfig::defaultGrfNumber;
    InterfaceDescriptorType iddArg = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();
    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);

    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf, walkerCmd);

    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
}
