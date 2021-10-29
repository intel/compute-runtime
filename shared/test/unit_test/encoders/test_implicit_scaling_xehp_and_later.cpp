/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_interface.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/implicit_scaling_fixture.h"

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenGetSizeWhenDispatchingCmdBufferThenConsumedSizeMatchEstimatedAndCmdBufferHasCorrectCmds) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, false, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, 0u);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(2u, partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<WALKER_TYPE>();
    auto itor = find<WALKER_TYPE *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenWorkgroupOneAndNoPartitionHintWhenDispatchingCmdBufferThenPartitionCountOneAndPartitionTypeDisabled) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, false, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, false, false, false, 0u);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(1u, partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? true : false;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary |= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_FALSE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<WALKER_TYPE>();
    auto itor = find<WALKER_TYPE *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walkerCmd->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenWorkgroupOneAndPartitionHintWhenDispatchingCmdBufferThenPartitionCountOneAndPartitionTypeFromHint) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1);
    DebugManager.flags.ExperimentalSetWalkerPartitionType.set(static_cast<int32_t>(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_X));
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, false, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, 0u);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(1u, partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<WALKER_TYPE>();
    auto itor = find<WALKER_TYPE *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningWhenDispatchingCmdBufferThenCorrectStaticPartitioningCommandsAreProgrammed) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    DebugManager.flags.UseCrossAtomicSynchronization.set(1);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(2u, partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<WALKER_TYPE>();
    auto itor = find<WALKER_TYPE *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());

    GenCmdList loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(loadRegisterMemList.begin(), loadRegisterMemList.end());
    ASSERT_NE(itorLrm, loadRegisterMemList.end());
    auto loadRegisterMem = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itorLrm);
    EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
    EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningPreferredAndPartitionCountIsOneWhenDispatchingCmdBufferThenCorrectStaticPartitioningCommandsAreProgrammed) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    DebugManager.flags.UseCrossAtomicSynchronization.set(1);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList listStartCmd = hwParser.getCommandsList<BATCH_BUFFER_START>();
    bool secondary = listStartCmd.size() == 0 ? false : true;
    for (auto &ptr : listStartCmd) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    GenCmdList walkerList = hwParser.getCommandsList<WALKER_TYPE>();
    auto itor = find<WALKER_TYPE *>(walkerList.begin(), walkerList.end());
    ASSERT_NE(itor, walkerList.end());
    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());

    GenCmdList loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(loadRegisterMemList.begin(), loadRegisterMemList.end());
    ASSERT_NE(itorLrm, loadRegisterMemList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningPreferredWhenForceDisabledWparidRegisterThenExpectNoCommandFound) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    DebugManager.flags.WparidRegisterProgramming.set(0);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(loadRegisterMemList.begin(), loadRegisterMemList.end());
    EXPECT_EQ(itorLrm, loadRegisterMemList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenStaticPartitioningPreferredWhenForceDisabledPipeControlThenExpectNoCommandFound) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManager.flags.UsePipeControlAfterPartitionedWalker.set(0);

    uint64_t workPartitionAllocationAddress = 0x987654;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    auto itorPipeControl = find<PIPE_CONTROL *>(pipeControlList.begin(), pipeControlList.end());
    EXPECT_EQ(itorPipeControl, pipeControlList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests, GivenDynamicPartitioningPreferredWhenForceDisabledPipeControlThenExpectNoCommandFound) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManager.flags.UsePipeControlAfterPartitionedWalker.set(0);

    uint64_t workPartitionAllocationAddress = 0x0;
    uint64_t postSyncAddress = (1ull << 48) | (1ull << 24);

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);

    size_t expectedSize = 0;
    size_t totalBytesProgrammed = 0;

    expectedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, false, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1));

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    auto itorPipeControl = find<PIPE_CONTROL *>(pipeControlList.begin(), pipeControlList.end());
    EXPECT_EQ(itorPipeControl, pipeControlList.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ImplicitScalingTests,
            givenPipeControlIsRequiredWhenApiRequiresCleanupSectionThenDoAddPipeControlCrossTileSyncAndCleanupSection) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), true);

    uint64_t workPartitionAllocationAddress = 0x1000;

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(WALKER_TYPE) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getSize(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, true, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<WALKER_TYPE>();
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
            givenPipeControlIsNotRequiredWhenApiRequiresCleanupSectionThenDoNotAddPipeControlCrossTileSyncAndCleanupSection) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(WALKER_TYPE);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getSize(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, true, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<WALKER_TYPE>();
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
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.UseCrossAtomicSynchronization.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(WALKER_TYPE) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getSize(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, true, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<WALKER_TYPE>();
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
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.UseCrossAtomicSynchronization.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(WALKER_TYPE) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<WALKER_TYPE>();
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
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.SynchronizeWalkerInWparidMode.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(WALKER_TYPE) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getSize(true, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, true, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<WALKER_TYPE>();
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
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.ProgramWalkerPartitionSelfCleanup.set(1);

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint64_t workPartitionAllocationAddress = 0x1000;

    WALKER_TYPE walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32);

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) +
                          sizeof(WALKER_TYPE) +
                          sizeof(MI_STORE_DATA_IMM) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_BATCH_BUFFER_START) +
                          sizeof(WalkerPartition::StaticPartitioningControlSection) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_STORE_DATA_IMM) * 2 +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT);

    size_t estimatedSize = 0;
    size_t totalBytesProgrammed = 0;

    estimatedSize = ImplicitScalingDispatch<FamilyType>::getSize(false, true, twoTile, Vec3<size_t>(0, 0, 0), Vec3<size_t>(32, 1, 1));
    EXPECT_EQ(expectedSize, estimatedSize);

    uint32_t partitionCount = 0;
    ImplicitScalingDispatch<FamilyType>::dispatchCommands(commandStream, walker, twoTile, partitionCount, true, false, false, workPartitionAllocationAddress);
    totalBytesProgrammed = commandStream.getUsed();
    EXPECT_EQ(expectedSize, totalBytesProgrammed);
    EXPECT_EQ(twoTile.count(), partitionCount);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto loadRegisterMemList = hwParser.getCommandsList<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(1u, loadRegisterMemList.size());

    auto computeWalkerList = hwParser.getCommandsList<WALKER_TYPE>();
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
