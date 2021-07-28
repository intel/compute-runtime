/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
