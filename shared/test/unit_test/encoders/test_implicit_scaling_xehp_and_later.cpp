/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_interface.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/implicit_scaling_fixture.h"

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenGetSizeWhenDispatchingCmdBufferThenConsumedSizeMatchEstimatedAndCmdBufferHasCorrectCmds) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, false, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(0, partitionCount);
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(2u, dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<DefaultWalkerType>();
    auto itor = find<DefaultWalkerType *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenWorkgroupOneAndNoPartitionHintWhenDispatchingCmdBufferThenPartitionCountOneAndPartitionTypeDisabled) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(1);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, false, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(0, partitionCount);
    dispatchArgs.useSecondaryBatchBuffer = false;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(1u, dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? true : false;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary |= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_FALSE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<DefaultWalkerType>();
    auto itor = find<DefaultWalkerType *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walkerCmd->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenWorkgroupOneAndPartitionHintWhenDispatchingCmdBufferThenPartitionCountOneAndPartitionTypeFromHint) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(1);
    debugManager.flags.ExperimentalSetWalkerPartitionType.set(static_cast<int32_t>(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X));
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, false, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(0, partitionCount);

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(1u, dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<DefaultWalkerType>();
    auto itor = find<DefaultWalkerType *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningWhenDispatchingCmdBufferThenCorrectStaticPartitioningCommandsAreProgrammed) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    debugManager.flags.UseCrossAtomicSynchronization.set(1);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(2u, dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<DefaultWalkerType>();
    auto itor = find<DefaultWalkerType *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());

    GenCmdList loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(loadRegisterMemList.begin(), loadRegisterMemList.end());
    EXPECT_EQ(itorLrm, loadRegisterMemList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningWhenPartitionRegisterIsRequiredThenCorrectStaticPartitioningCommandsAreProgrammed) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    debugManager.flags.UseCrossAtomicSynchronization.set(1);
    debugManager.flags.WparidRegisterProgramming.set(1);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(2u, dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<DefaultWalkerType>();
    auto itor = find<DefaultWalkerType *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());

    GenCmdList loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(loadRegisterMemList.begin(), loadRegisterMemList.end());
    ASSERT_NE(itorLrm, loadRegisterMemList.end());
    auto loadRegisterMem = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itorLrm);
    EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
    EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningPreferredAndPartitionCountIsOneWhenDispatchingCmdBufferThenCorrectStaticPartitioningCommandsAreProgrammed) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    debugManager.flags.UseCrossAtomicSynchronization.set(1);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(1);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<DefaultWalkerType>();
    auto itor = find<DefaultWalkerType *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());

    GenCmdList loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(loadRegisterMemList.begin(), loadRegisterMemList.end());
    EXPECT_EQ(itorLrm, loadRegisterMemList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningPreferredWhenForceDisabledWparidRegisterThenExpectNoCommandFound) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    debugManager.flags.WparidRegisterProgramming.set(0);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(1);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(loadRegisterMemList.begin(), loadRegisterMemList.end());
    EXPECT_EQ(itorLrm, loadRegisterMemList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningPreferredWhenForceDisabledPipeControlThenExpectNoCommandFound) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    debugManager.flags.UsePipeControlAfterPartitionedWalker.set(0);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(1);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    auto itorPipeControl = find<PIPE_CONTROL *>(pipeControlList.begin(), pipeControlList.end());
    EXPECT_EQ(itorPipeControl, pipeControlList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenDynamicPartitioningPreferredWhenForceDisabledPipeControlThenExpectNoCommandFound) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    debugManager.flags.UsePipeControlAfterPartitionedWalker.set(0);

    uint64_t workPartitionAllocationAddress = 0x0;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, false, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    auto itorPipeControl = find<PIPE_CONTROL *>(pipeControlList.begin(), pipeControlList.end());
    EXPECT_EQ(itorPipeControl, pipeControlList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsRequiredWhenApiRequiresCleanupSectionThenDoAddPipeControlCrossTileSyncAndCleanupSection) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), true);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(DefaultWalkerType) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.apiSelfCleanup = true;
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(0u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(3u, storeDataImmList.size());

    EXPECT_EQ(1u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(3u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsRequiredAndPartitionRegisterProgrammingForcedWhenApiRequiresCleanupSectionThenDoAddPipeControlCrossTileSyncAndCleanupSection) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), true);
    debugManager.flags.WparidRegisterProgramming.set(1);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(DefaultWalkerType) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.apiSelfCleanup = true;
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(3u, storeDataImmList.size());

    EXPECT_EQ(1u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(3u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsNotRequiredAndPartitionRegisterProgrammingForcedWhenApiRequiresCleanupSectionThenDoNotAddPipeControlCrossTileSyncAndCleanupSection) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);
    debugManager.flags.WparidRegisterProgramming.set(1);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(DefaultWalkerType);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.apiSelfCleanup = true;
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(0u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(0u, storeDataImmList.size());

    EXPECT_EQ(0u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(0u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(0u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsNotRequiredAndForcePartitionRegisterProgrammingWhenApiRequiresCleanupSectionThenDoNotAddPipeControlCrossTileSyncAndCleanupSection) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);
    debugManager.flags.WparidRegisterProgramming.set(1);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(DefaultWalkerType);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;
    dispatchArgs.apiSelfCleanup = true;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(0u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(0u, storeDataImmList.size());

    EXPECT_EQ(0u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(0u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(0u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsNotRequiredAndForcedCrossTileSyncWhenApiRequiresCleanupSectionThenDoNotAddPipeControlAndAddCrossTileSyncAndCleanupSection) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.UseCrossAtomicSynchronization.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(DefaultWalkerType) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;
    dispatchArgs.apiSelfCleanup = true;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(0u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(3u, storeDataImmList.size());

    EXPECT_EQ(0u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(3u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsNotRequiredAndForcedCrossTileSyncWhenApiRequiresNoCleanupSectionThenDoNotAddPipeControlAndCleanupSectionAndAddCrossTileSync) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.UseCrossAtomicSynchronization.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(DefaultWalkerType) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(0u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(0u, storeDataImmList.size());

    EXPECT_EQ(0u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(1u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(1u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsNotRequiredAndForcedCrossTileSyncAndPartitionRegisterWhenApiRequiresNoCleanupSectionThenDoNotAddPipeControlAndCleanupSectionAndAddCrossTileSync) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.UseCrossAtomicSynchronization.set(1);
    debugManager.flags.WparidRegisterProgramming.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(DefaultWalkerType) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(0u, storeDataImmList.size());

    EXPECT_EQ(0u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(1u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(1u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsNotRequiredAndForcedCrossTileSyncBeforeExecWhenApiRequiresCleanupSectionThenDoNotAddPipeControlAndAddCrossTileSyncAndCleanupSection) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.SynchronizeWalkerInWparidMode.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(DefaultWalkerType) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;
    dispatchArgs.apiSelfCleanup = true;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(0u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(3u, storeDataImmList.size());

    EXPECT_EQ(0u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(4u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(4u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsNotRequiredAndForcedCleanupSectionWhenApiNotRequiresCleanupSectionThenDoNotAddPipeControlAndCrossTileSyncAndAddCleanupSection) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.ProgramWalkerPartitionSelfCleanup.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(DefaultWalkerType) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(0u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<DefaultWalkerType>();
    EXPECT_EQ(1u, computeWalkerList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(3u, storeDataImmList.size());

    EXPECT_EQ(0u, hwParser.pipeControlList.size());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(3u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenBarrierDispatchWhenApiNotRequiresSelfCleanupThenExpectMinimalCommandBuffer) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    size_t expectedSize = sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        false,
                                                                        false);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = false;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), 0, 0, false, false);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    EXPECT_EQ(1u, hwParser.pipeControlList.size());

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*hwParser.pipeControlList.begin());
    EXPECT_FALSE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(1u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(1u, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenBarrierDispatchWhenApiRequiresSelfCleanupThenExpectDefaultSelfCleanupSection) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    size_t expectedSize = sizeof(MI_STORE_DATA_IMM) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        true,
                                                                        false);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), 0, 0, true, true);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(2u, storeDataImmList.size());

    EXPECT_EQ(1u, hwParser.pipeControlList.size());
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*hwParser.pipeControlList.begin());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(3u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenBarrierDispatchWhenApiRequiresSelfCleanupForcedUseAtomicThenExpectUseAtomicForSelfCleanupSection) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    size_t expectedSize = sizeof(MI_ATOMIC) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_ATOMIC) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    debugManager.flags.UseAtomicsForSelfCleanupSection.set(1);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        true,
                                                                        false);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), 0, 0, true, true);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    EXPECT_EQ(1u, hwParser.pipeControlList.size());
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*hwParser.pipeControlList.begin());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(5u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPostSyncBarrierDispatchWhenApiNotRequiresSelfCleanupThenExpectPostSyncMinimalCommandBuffer) {

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MockExecutionEnvironment mockExecutionEnvironment{};

    size_t expectedSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(pDevice->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        false,
                                                                        true);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = false;
    uint64_t postSyncAddress = 0xFF000A180F0;
    uint64_t postSyncValue = 0xFF00FF;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), postSyncAddress, postSyncValue, false, false);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    size_t expectedPipeControls = 1u;
    size_t expectedSemaphores = 1u;

    bool semaphoreAsAdditionalSync = UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(this->pDevice->getRootDeviceEnvironment());
    if (semaphoreAsAdditionalSync) {
        expectedSemaphores++;
    }

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(this->pDevice->getRootDeviceEnvironment())) {
        expectedPipeControls++;
        if (semaphoreAsAdditionalSync) {
            expectedSemaphores++;
        }
    }
    EXPECT_EQ(expectedPipeControls, hwParser.pipeControlList.size());

    auto pipeControlItor = hwParser.pipeControlList.begin();
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlItor);
    if (expectedPipeControls == 2) {
        constexpr uint64_t zeroGpuAddress = 0;
        constexpr uint64_t zeroImmediateValue = 0;
        EXPECT_EQ(zeroGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
        EXPECT_EQ(zeroImmediateValue, pipeControl->getImmediateData());
        pipeControlItor++;
        pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlItor);
    }
    EXPECT_EQ(postSyncAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(postSyncValue, pipeControl->getImmediateData());
    EXPECT_FALSE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(1u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(expectedSemaphores, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPostSyncBarrierDispatchWhenApiRequiresSelfCleanupThenExpectPostSyncAndDefaultSelfCleanupSection) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    size_t expectedSize = sizeof(MI_STORE_DATA_IMM) +
                          MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(pDevice->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        true,
                                                                        true);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    uint64_t postSyncAddress = 0xFF000A180F0;
    uint64_t postSyncValue = 0xFF00FF;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), postSyncAddress, postSyncValue, true, true);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(2u, storeDataImmList.size());

    size_t expectedPipeControls = 1u;
    size_t expectedSemaphores = 3u;

    bool semaphoreAsAdditionalSync = UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(this->pDevice->getRootDeviceEnvironment());
    if (semaphoreAsAdditionalSync) {
        expectedSemaphores++;
    }
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(this->pDevice->getRootDeviceEnvironment())) {
        expectedPipeControls++;
        if (semaphoreAsAdditionalSync) {
            expectedSemaphores++;
        }
    }
    EXPECT_EQ(expectedPipeControls, hwParser.pipeControlList.size());
    auto pipeControlItor = hwParser.pipeControlList.begin();
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlItor);
    if (expectedPipeControls == 2) {
        constexpr uint64_t zeroGpuAddress = 0;
        constexpr uint64_t zeroImmediateValue = 0;
        EXPECT_EQ(zeroGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
        EXPECT_EQ(zeroImmediateValue, pipeControl->getImmediateData());
        pipeControlItor++;
        pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlItor);
    }
    EXPECT_EQ(postSyncAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(postSyncValue, pipeControl->getImmediateData());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(3u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(expectedSemaphores, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPostSyncBarrierDispatchWhenApiRequiresSelfCleanupForcedUseAtomicThenExpectPostSyncAndUseAtomicForSelfCleanupSection) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.UseAtomicsForSelfCleanupSection.set(1);
    debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    testHardwareInfo.featureTable.flags.ftrLocalMemory = true;

    size_t expectedSize = sizeof(MI_ATOMIC) +
                          MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(pDevice->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_ATOMIC) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        true,
                                                                        true);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    uint64_t postSyncAddress = 0xFF000A180F0;
    uint64_t postSyncValue = 0xFF00FF;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), postSyncAddress, postSyncValue, true, true);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    size_t expectedPipeControls = 1u;
    size_t expectedSemaphores = 3u;

    bool semaphoreAsAdditionalSync = UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(this->pDevice->getRootDeviceEnvironment());
    if (semaphoreAsAdditionalSync) {
        expectedSemaphores++;
    }
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(this->pDevice->getRootDeviceEnvironment())) {
        expectedPipeControls++;
        if (semaphoreAsAdditionalSync) {
            expectedSemaphores++;
        }
    }
    EXPECT_EQ(expectedPipeControls, hwParser.pipeControlList.size());

    auto pipeControlItor = hwParser.pipeControlList.begin();
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlItor);
    if (expectedPipeControls == 2) {
        constexpr uint64_t zeroGpuAddress = 0;
        constexpr uint64_t zeroImmediateValue = 0;
        EXPECT_EQ(zeroGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
        EXPECT_EQ(zeroImmediateValue, pipeControl->getImmediateData());
        pipeControlItor++;
        pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlItor);
    }
    EXPECT_EQ(postSyncAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(postSyncValue, pipeControl->getImmediateData());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(5u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(expectedSemaphores, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenForceNoCleanupWhenBarrierDispatchAndApiRequiresSelfCleanupThenExpectNoSelfCleanupSection) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.ProgramWalkerPartitionSelfCleanup.set(0);

    size_t expectedSize = sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        true,
                                                                        false);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), 0, 0, true, true);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(0u, storeDataImmList.size());

    EXPECT_EQ(1u, hwParser.pipeControlList.size());
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*hwParser.pipeControlList.begin());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(1u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(1u, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenForceSelfCleanupWhenBarrierDispatchAndApiNotRequiresSelfCleanupThenExpectSelfCleanupSection) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.ProgramWalkerPartitionSelfCleanup.set(1);

    size_t expectedSize = sizeof(MI_STORE_DATA_IMM) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        false,
                                                                        false);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), 0, 0, false, true);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(2u, storeDataImmList.size());

    EXPECT_EQ(1u, hwParser.pipeControlList.size());
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*hwParser.pipeControlList.begin());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(3u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenForceAddPipeControlBeforeCleanupCrossTileSyncWhenBarrierDispatchAndApiRequiresSelfCleanupThenExpectSelfCleanupSectionWithExtraPipeControls) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.ProgramStallCommandForSelfCleanup.set(1);

    size_t expectedSize = sizeof(MI_STORE_DATA_IMM) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::BarrierControlSection) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getBarrierSize(this->pDevice->getRootDeviceEnvironment(),
                                                                        true,
                                                                        false);
    EXPECT_EQ(expectedSize, estimatedSize);

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(commandStream, twoTile, flushArgs, this->pDevice->getRootDeviceEnvironment(), 0, 0, true, true);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(2u, storeDataImmList.size());

    EXPECT_EQ(3u, hwParser.pipeControlList.size());
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*hwParser.pipeControlList.begin());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());

    auto miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(3u, miAtomicList.size());

    auto miSemaphoreList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreList.size());

    auto bbStartList = hwParser.getCommandsList<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(1u, bbStartList.size());
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartList.begin());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenStaticPartitioningWhenBlockDispatchFlagIsTrueThenDoNotDispatchAnyCommands) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());

    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    DefaultWalkerType walker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walker.setThreadGroupIdXDimension(32);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    DefaultWalkerType walkerDispatched = walker;

    uint64_t workPartitionAllocationAddress = 0x1000;
    size_t expectedTotalBytesProgrammed = 0;
    void *outWalkerPtr = nullptr;

    uint32_t partitionCount = 0;
    auto dispatchArgs = createDispatchCommandArgs(workPartitionAllocationAddress, partitionCount);
    dispatchArgs.threadGroupCount = 32;
    dispatchArgs.blockDispatchToCommandBuffer = true;
    dispatchArgs.outWalkerPtr = &outWalkerPtr;

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, dispatchArgs);
    size_t totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedTotalBytesProgrammed, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), dispatchArgs.partitionCount);
    EXPECT_EQ(nullptr, outWalkerPtr);

    dispatchArgs.blockDispatchToCommandBuffer = false;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walkerDispatched, twoTile, dispatchArgs);
    ASSERT_NE(nullptr, outWalkerPtr);

    EXPECT_EQ(0, memcmp(&walkerDispatched, outWalkerPtr, sizeof(DefaultWalkerType)));
}
