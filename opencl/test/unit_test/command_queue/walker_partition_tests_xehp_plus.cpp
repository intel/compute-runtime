/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_xehp_plus.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "test.h"

using namespace WalkerPartition;

struct WalkerPartitionTests : public ::testing::Test {
    void SetUp() override {
        cmdBufferAddress = cmdBuffer;
    }

    void TearDown() override {
        auto initialCommandBufferPointer = cmdBuffer;
        if (checkForProperCmdBufferAddressOffset) {
            EXPECT_EQ(ptrDiff(cmdBufferAddress, initialCommandBufferPointer), totalBytesProgrammed);
        }
    }

    template <typename GfxFamily>
    auto createWalker(uint64_t postSyncAddress) {
        WalkerPartition::COMPUTE_WALKER<GfxFamily> walker;
        walker = GfxFamily::cmdInitGpgpuWalker;
        walker.setPartitionType(COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_X);
        auto &postSync = walker.getPostSync();
        postSync.setOperation(POSTSYNC_DATA<GfxFamily>::OPERATION::OPERATION_WRITE_TIMESTAMP);
        postSync.setDestinationAddress(postSyncAddress);
        return walker;
    }

    char cmdBuffer[4096u];
    uint32_t totalBytesProgrammed = 0u;
    void *cmdBufferAddress = nullptr;
    bool checkForProperCmdBufferAddressOffset = true;
    bool synchronizeBeforeExecution = false;
    bool nativeCrossTileSync = false;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerPartitionWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    auto partitionCount = 16u;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t gpuVirtualAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_X);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA<FamilyType>::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);
    uint32_t totalBytesProgrammed;

    auto expectedCommandUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                                   sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                                   sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto walkerSectionCommands = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) +
                                 sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    EXPECT_EQ(expectedCommandUsedSize, computeControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, false, false));

    auto optionalBatchBufferEndOffset = expectedCommandUsedSize + sizeof(BatchBufferControlData);

    auto totalProgrammedSize = optionalBatchBufferEndOffset + sizeof(WalkerPartition::BATCH_BUFFER_END<FamilyType>);

    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                              gpuVirtualAddress,
                                                                              &walker,
                                                                              totalBytesProgrammed,
                                                                              partitionCount,
                                                                              4u,
                                                                              true,
                                                                              synchronizeBeforeExecution,
                                                                              false,
                                                                              false,
                                                                              false);

    EXPECT_EQ(totalProgrammedSize, totalBytesProgrammed);
    auto wparidMaskProgrammingLocation = cmdBufferAddress;

    auto expectedMask = 0xFFF0u;
    auto expectedRegister = 0x21FCu;

    auto loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(wparidMaskProgrammingLocation);
    ASSERT_NE(nullptr, loadRegisterImmediate);
    EXPECT_EQ(expectedRegister, loadRegisterImmediate->getRegisterOffset());
    EXPECT_EQ(expectedMask, loadRegisterImmediate->getDataDword());
    auto parsedOffset = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>);

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto miAtomicAddress = gpuVirtualAddress + expectedCommandUsedSize;
    auto miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicAddress, miAtomicProgrammedAddress);
    EXPECT_TRUE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto loadRegisterReg = genCmdCast<WalkerPartition::LOAD_REGISTER_REG<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, loadRegisterReg);
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableDestination());
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableSource());
    EXPECT_EQ(wparidCCSOffset, loadRegisterReg->getDestinationRegisterAddress());
    EXPECT_EQ(generalPurposeRegister4, loadRegisterReg->getSourceRegisterAddress());
    parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>);

    auto miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE);
    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_TRUE(batchBufferStart->getPredicationEnable());
    //address routes to WALKER section which is before control section
    auto address = batchBufferStart->getBatchBufferStartAddress();
    EXPECT_EQ(address, gpuVirtualAddress + expectedCommandUsedSize - walkerSectionCommands);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_NEVER);
    EXPECT_EQ(miSetPredicate->getPredicateEnable(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE::PREDICATE_ENABLE_PREDICATE_DISABLE);

    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());

    parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto miAtomicTileAddress = gpuVirtualAddress + expectedCommandUsedSize + sizeof(uint32_t);
    auto miAtomicTileProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicTileAddress, miAtomicTileProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());

    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreGraphicsAddress(), miAtomicTileAddress);
    EXPECT_EQ(miSemaphoreWait->getCompareOperation(), MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreDataDword(), 4u);

    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    //final batch buffer start that routes at the end of the batch buffer
    auto batchBufferStartFinal = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_NE(nullptr, batchBufferStartFinal);
    EXPECT_EQ(batchBufferStartFinal->getBatchBufferStartAddress(), gpuVirtualAddress + optionalBatchBufferEndOffset);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, computeWalker);
    parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_FALSE(batchBufferStart->getPredicationEnable());
    EXPECT_EQ(gpuVirtualAddress, batchBufferStart->getBatchBufferStartAddress());
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto controlSection = reinterpret_cast<BatchBufferControlData *>(ptrOffset(cmdBuffer, expectedCommandUsedSize));
    EXPECT_EQ(0u, controlSection->partitionCount);
    EXPECT_EQ(0u, controlSection->tileCount);
    parsedOffset += sizeof(BatchBufferControlData);

    auto batchBufferEnd = genCmdCast<WalkerPartition::BATCH_BUFFER_END<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_NE(nullptr, batchBufferEnd);
    EXPECT_EQ(parsedOffset, optionalBatchBufferEndOffset);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    const auto tileCount = 4u;
    const auto partitionCount = tileCount;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    uint64_t workPartitionAllocationAddress = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, false);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             partitionCount,
                                                                             tileCount,
                                                                             synchronizeBeforeExecution,
                                                                             false,
                                                                             nativeCrossTileSync,
                                                                             workPartitionAllocationAddress,
                                                                             false);
    EXPECT_EQ(controlSectionOffset + sizeof(StaticPartitioningControlSection), totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
    }
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    {
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, batchBufferStart);
        parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
        EXPECT_FALSE(batchBufferStart->getPredicationEnable());
        const auto afterControlSectionAddress = cmdBufferGpuAddress + controlSectionOffset + sizeof(StaticPartitioningControlSection);
        EXPECT_EQ(afterControlSectionAddress, batchBufferStart->getBatchBufferStartAddress());
    }
    {
        auto controlSection = reinterpret_cast<StaticPartitioningControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
        parsedOffset += sizeof(StaticPartitioningControlSection);
        StaticPartitioningControlSection expectedControlSection = {};
        EXPECT_EQ(0, std::memcmp(&expectedControlSection, controlSection, sizeof(StaticPartitioningControlSection)));
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionAndPreWalkerSyncWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    const auto tileCount = 4u;
    const auto partitionCount = tileCount;
    checkForProperCmdBufferAddressOffset = false;
    synchronizeBeforeExecution = true;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    uint64_t workPartitionAllocationAddress = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, false);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             partitionCount,
                                                                             tileCount,
                                                                             synchronizeBeforeExecution,
                                                                             false,
                                                                             nativeCrossTileSync,
                                                                             workPartitionAllocationAddress,
                                                                             false);
    EXPECT_EQ(controlSectionOffset + sizeof(StaticPartitioningControlSection), totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(preWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(preWalkerSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
    }
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    {
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, batchBufferStart);
        parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
        EXPECT_FALSE(batchBufferStart->getPredicationEnable());
        const auto afterControlSectionAddress = cmdBufferGpuAddress + controlSectionOffset + sizeof(StaticPartitioningControlSection);
        EXPECT_EQ(afterControlSectionAddress, batchBufferStart->getBatchBufferStartAddress());
    }
    {
        auto controlSection = reinterpret_cast<StaticPartitioningControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
        parsedOffset += sizeof(StaticPartitioningControlSection);
        StaticPartitioningControlSection expectedControlSection = {};
        EXPECT_EQ(0, std::memcmp(&expectedControlSection, controlSection, sizeof(StaticPartitioningControlSection)));
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionAndSynchronizationWithPostSyncsWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.ExperimentalSynchronizeWithSemaphores.set(1);
    DebugManager.flags.ExperimentalForceCrossAtomicSynchronization.set(0);

    const auto tileCount = 4u;
    const auto partitionCount = tileCount;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    uint64_t workPartitionAllocationAddress = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, false);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             partitionCount,
                                                                             tileCount,
                                                                             synchronizeBeforeExecution,
                                                                             false,
                                                                             nativeCrossTileSync,
                                                                             workPartitionAllocationAddress, false);
    EXPECT_EQ(controlSectionOffset + sizeof(StaticPartitioningControlSection), totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
    }
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    {
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        const auto expectedSemaphoreAddress = walker.getPostSync().getDestinationAddress() + 8llu;
        EXPECT_EQ(expectedSemaphoreAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(1u, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        const auto expectedSemaphoreAddress = walker.getPostSync().getDestinationAddress() + 8llu + 16llu;
        EXPECT_EQ(expectedSemaphoreAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(1u, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        const auto expectedSemaphoreAddress = walker.getPostSync().getDestinationAddress() + 8llu + 32llu;
        EXPECT_EQ(expectedSemaphoreAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(1u, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        const auto expectedSemaphoreAddress = walker.getPostSync().getDestinationAddress() + 8llu + 48llu;
        EXPECT_EQ(expectedSemaphoreAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(1u, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, batchBufferStart);
        parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
        EXPECT_FALSE(batchBufferStart->getPredicationEnable());
        const auto afterControlSectionAddress = cmdBufferGpuAddress + controlSectionOffset + sizeof(StaticPartitioningControlSection);
        EXPECT_EQ(afterControlSectionAddress, batchBufferStart->getBatchBufferStartAddress());
    }
    {
        auto controlSection = reinterpret_cast<StaticPartitioningControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
        parsedOffset += sizeof(StaticPartitioningControlSection);
        StaticPartitioningControlSection expectedControlSection = {};
        EXPECT_EQ(0, std::memcmp(&expectedControlSection, controlSection, sizeof(StaticPartitioningControlSection)));
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWithNativeCrossTileSyncWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    const auto tileCount = 4u;
    const auto partitionCount = tileCount;
    nativeCrossTileSync = true;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    uint64_t workPartitionAllocationAddress = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, false);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto finalSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             partitionCount,
                                                                             tileCount,
                                                                             synchronizeBeforeExecution,
                                                                             false,
                                                                             nativeCrossTileSync,
                                                                             workPartitionAllocationAddress,
                                                                             false);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(false,
                                                                                                           partitionCount,
                                                                                                           synchronizeBeforeExecution,
                                                                                                           nativeCrossTileSync,
                                                                                                           true,
                                                                                                           false);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
    }
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    {
        auto storeDataImm = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);
        EXPECT_EQ(finalSyncAddress, storeDataImm->getAddress());
        EXPECT_FALSE(storeDataImm->getStoreQword());
        EXPECT_EQ(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, storeDataImm->getDwordLength());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
    }
    {
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, batchBufferStart);
        parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
        EXPECT_FALSE(batchBufferStart->getPredicationEnable());
        const auto afterControlSectionAddress = cmdBufferGpuAddress + controlSectionOffset + sizeof(StaticPartitioningControlSection);
        EXPECT_EQ(afterControlSectionAddress, batchBufferStart->getBatchBufferStartAddress());
    }
    {
        auto controlSection = reinterpret_cast<StaticPartitioningControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
        parsedOffset += sizeof(StaticPartitioningControlSection);
        StaticPartitioningControlSection expectedControlSection = {};
        EXPECT_EQ(0, std::memcmp(&expectedControlSection, controlSection, sizeof(StaticPartitioningControlSection)));
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(finalSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto storeDataImm = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);
        EXPECT_EQ(preWalkerSyncAddress, storeDataImm->getAddress());
        EXPECT_FALSE(storeDataImm->getStoreQword());
        EXPECT_EQ(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, storeDataImm->getDwordLength());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
    }
    {
        auto storeDataImm = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, storeDataImm->getAddress());
        EXPECT_FALSE(storeDataImm->getStoreQword());
        EXPECT_EQ(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, storeDataImm->getDwordLength());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(finalSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(2 * tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWithNativeCrossTileSyncAndSyncDisabledWithFlagWhenConstructCommandBufferIsCalledThenStillProgramTheSync) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.ExperimentalForceCrossAtomicSynchronization.set(0);

    const auto tileCount = 4u;
    const auto partitionCount = tileCount;
    nativeCrossTileSync = true;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    uint64_t workPartitionAllocationAddress = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, false);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto finalSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             partitionCount,
                                                                             tileCount,
                                                                             synchronizeBeforeExecution,
                                                                             false,
                                                                             nativeCrossTileSync,
                                                                             workPartitionAllocationAddress,
                                                                             false);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(false,
                                                                                                           partitionCount,
                                                                                                           synchronizeBeforeExecution,
                                                                                                           nativeCrossTileSync,
                                                                                                           true,
                                                                                                           false);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
    }
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    {
        auto storeDataImm = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);
        EXPECT_EQ(finalSyncAddress, storeDataImm->getAddress());
        EXPECT_FALSE(storeDataImm->getStoreQword());
        EXPECT_EQ(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, storeDataImm->getDwordLength());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
    }
    {
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, batchBufferStart);
        parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
        EXPECT_FALSE(batchBufferStart->getPredicationEnable());
        const auto afterControlSectionAddress = cmdBufferGpuAddress + controlSectionOffset + sizeof(StaticPartitioningControlSection);
        EXPECT_EQ(afterControlSectionAddress, batchBufferStart->getBatchBufferStartAddress());
    }
    {
        auto controlSection = reinterpret_cast<StaticPartitioningControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
        parsedOffset += sizeof(StaticPartitioningControlSection);
        StaticPartitioningControlSection expectedControlSection = {};
        EXPECT_EQ(0, std::memcmp(&expectedControlSection, controlSection, sizeof(StaticPartitioningControlSection)));
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(finalSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto storeDataImm = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);
        EXPECT_EQ(preWalkerSyncAddress, storeDataImm->getAddress());
        EXPECT_FALSE(storeDataImm->getStoreQword());
        EXPECT_EQ(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, storeDataImm->getDwordLength());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
    }
    {
        auto storeDataImm = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, storeDataImm->getAddress());
        EXPECT_FALSE(storeDataImm->getStoreQword());
        EXPECT_EQ(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, storeDataImm->getDwordLength());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(finalSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(2 * tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWithNativeCrossTileSyncAndAtomicsForNativeWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    const auto tileCount = 4u;
    const auto partitionCount = tileCount;
    bool useAtomicsForNative = true;
    nativeCrossTileSync = true;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    uint64_t workPartitionAllocationAddress = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, useAtomicsForNative);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto finalSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             partitionCount,
                                                                             tileCount,
                                                                             synchronizeBeforeExecution,
                                                                             false,
                                                                             nativeCrossTileSync,
                                                                             workPartitionAllocationAddress,
                                                                             useAtomicsForNative);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(false,
                                                                                                           partitionCount,
                                                                                                           synchronizeBeforeExecution,
                                                                                                           nativeCrossTileSync,
                                                                                                           true,
                                                                                                           useAtomicsForNative);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
    }
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, batchBufferStart);
        parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
        EXPECT_FALSE(batchBufferStart->getPredicationEnable());
        const auto afterControlSectionAddress = cmdBufferGpuAddress + controlSectionOffset + sizeof(StaticPartitioningControlSection);
        EXPECT_EQ(afterControlSectionAddress, batchBufferStart->getBatchBufferStartAddress());
    }
    {
        auto controlSection = reinterpret_cast<StaticPartitioningControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
        parsedOffset += sizeof(StaticPartitioningControlSection);
        StaticPartitioningControlSection expectedControlSection = {};
        EXPECT_EQ(0, std::memcmp(&expectedControlSection, controlSection, sizeof(StaticPartitioningControlSection)));
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(finalSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(preWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(finalSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(2 * tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWithNativeCrossTileSyncAndSyncDisabledWithFlagWhenUsingAtomicForNativeAndConstructCommandBufferIsCalledThenStillProgramTheSync) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.ExperimentalForceCrossAtomicSynchronization.set(0);

    const auto tileCount = 4u;
    const auto partitionCount = tileCount;
    nativeCrossTileSync = true;
    bool useAtomicsForNative = true;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    uint64_t workPartitionAllocationAddress = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, useAtomicsForNative);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto finalSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             partitionCount,
                                                                             tileCount,
                                                                             synchronizeBeforeExecution,
                                                                             false,
                                                                             nativeCrossTileSync,
                                                                             workPartitionAllocationAddress,
                                                                             useAtomicsForNative);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(false,
                                                                                                           partitionCount,
                                                                                                           synchronizeBeforeExecution,
                                                                                                           nativeCrossTileSync,
                                                                                                           true,
                                                                                                           useAtomicsForNative);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(workPartitionAllocationAddress, loadRegisterMem->getMemoryAddress());
    }
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, batchBufferStart);
        parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
        EXPECT_FALSE(batchBufferStart->getPredicationEnable());
        const auto afterControlSectionAddress = cmdBufferGpuAddress + controlSectionOffset + sizeof(StaticPartitioningControlSection);
        EXPECT_EQ(afterControlSectionAddress, batchBufferStart->getBatchBufferStartAddress());
    }
    {
        auto controlSection = reinterpret_cast<StaticPartitioningControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
        parsedOffset += sizeof(StaticPartitioningControlSection);
        StaticPartitioningControlSection expectedControlSection = {};
        EXPECT_EQ(0, std::memcmp(&expectedControlSection, controlSection, sizeof(StaticPartitioningControlSection)));
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(finalSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(preWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(postWalkerSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
        EXPECT_EQ(finalSyncAddress, UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE<FamilyType>::DATA_SIZE_DWORD, miAtomic->getDataSize());
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_FALSE(miAtomic->getCsStall());
    }
    {
        auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphoreWait);
        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        EXPECT_EQ(finalSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
        EXPECT_EQ(2 * tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenDebugModesFOrWalkerPartitionWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ExperimentalForceCrossAtomicSynchronization.set(0);
    DebugManager.flags.ExperimentalSynchronizeWithSemaphores.set(1);

    auto partitionCount = 16u;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t gpuVirtualAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_X);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA<FamilyType>::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);
    uint32_t totalBytesProgrammed;

    auto expectedCommandUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                                   sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                                   sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                                   sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) * partitionCount;

    auto walkerSectionCommands = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) +
                                 sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    EXPECT_EQ(expectedCommandUsedSize, computeControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, false, false));

    auto optionalBatchBufferEndOffset = expectedCommandUsedSize + sizeof(BatchBufferControlData);

    auto totalProgrammedSize = optionalBatchBufferEndOffset + sizeof(WalkerPartition::BATCH_BUFFER_END<FamilyType>);

    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                              gpuVirtualAddress, &walker, totalBytesProgrammed, partitionCount,
                                                                              4u, true, synchronizeBeforeExecution, false, false, false);

    EXPECT_EQ(totalProgrammedSize, totalBytesProgrammed);
    auto wparidMaskProgrammingLocation = cmdBufferAddress;

    auto expectedMask = 0xFFF0u;
    auto expectedRegister = 0x21FCu;

    auto loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(wparidMaskProgrammingLocation);
    ASSERT_NE(nullptr, loadRegisterImmediate);
    EXPECT_EQ(expectedRegister, loadRegisterImmediate->getRegisterOffset());
    EXPECT_EQ(expectedMask, loadRegisterImmediate->getDataDword());
    auto parsedOffset = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>);

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto miAtomicAddress = gpuVirtualAddress + expectedCommandUsedSize;
    auto miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicAddress, miAtomicProgrammedAddress);
    EXPECT_TRUE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto loadRegisterReg = genCmdCast<WalkerPartition::LOAD_REGISTER_REG<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, loadRegisterReg);
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableDestination());
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableSource());
    EXPECT_EQ(wparidCCSOffset, loadRegisterReg->getDestinationRegisterAddress());
    EXPECT_EQ(generalPurposeRegister4, loadRegisterReg->getSourceRegisterAddress());
    parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>);

    auto miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE);
    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_TRUE(batchBufferStart->getPredicationEnable());
    //address routes to WALKER section which is before control section
    auto address = batchBufferStart->getBatchBufferStartAddress();
    EXPECT_EQ(address, gpuVirtualAddress + expectedCommandUsedSize - walkerSectionCommands);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_NEVER);
    EXPECT_EQ(miSetPredicate->getPredicateEnable(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE::PREDICATE_ENABLE_PREDICATE_DISABLE);

    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());

    parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);

    auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    for (uint32_t partitionId = 0u; partitionId < partitionCount; partitionId++) {
        ASSERT_NE(nullptr, miSemaphoreWait);
        EXPECT_EQ(miSemaphoreWait->getSemaphoreGraphicsAddress(), postSyncAddress + 8llu + partitionId * 16llu);
        EXPECT_EQ(miSemaphoreWait->getCompareOperation(), MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(miSemaphoreWait->getSemaphoreDataDword(), 1u);

        parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
        miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    }

    //final batch buffer start that routes at the end of the batch buffer
    auto batchBufferStartFinal = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_NE(nullptr, batchBufferStartFinal);
    EXPECT_EQ(batchBufferStartFinal->getBatchBufferStartAddress(), gpuVirtualAddress + optionalBatchBufferEndOffset);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, computeWalker);
    parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_FALSE(batchBufferStart->getPredicationEnable());
    EXPECT_EQ(gpuVirtualAddress, batchBufferStart->getBatchBufferStartAddress());
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto controlSection = reinterpret_cast<BatchBufferControlData *>(ptrOffset(cmdBuffer, expectedCommandUsedSize));
    EXPECT_EQ(0u, controlSection->partitionCount);
    EXPECT_EQ(0u, controlSection->tileCount);
    parsedOffset += sizeof(BatchBufferControlData);

    auto batchBufferEnd = genCmdCast<WalkerPartition::BATCH_BUFFER_END<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_NE(nullptr, batchBufferEnd);
    EXPECT_EQ(parsedOffset, optionalBatchBufferEndOffset);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramRegisterCommandWhenItIsCalledThenLoadRegisterImmIsSetUnderPointer) {
    uint32_t registerOffset = 120u;
    uint32_t registerValue = 542u;
    auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>);
    void *loadRegisterImmediateAddress = cmdBufferAddress;
    WalkerPartition::programRegisterWithValue<FamilyType>(cmdBufferAddress, registerOffset, totalBytesProgrammed, registerValue);
    auto loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(loadRegisterImmediateAddress);

    ASSERT_NE(nullptr, loadRegisterImmediate);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);
    EXPECT_EQ(registerOffset, loadRegisterImmediate->getRegisterOffset());
    EXPECT_EQ(registerValue, loadRegisterImmediate->getDataDword());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerPartitionWhenConstructCommandBufferIsCalledWithoutBatchBufferEndThenBatchBufferEndIsNotProgrammed) {
    auto partitionCount = 16u;
    checkForProperCmdBufferAddressOffset = false;
    uint64_t gpuVirtualAddress = 0x8000123000;
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_X);

    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                              gpuVirtualAddress,
                                                                              &walker,
                                                                              totalBytesProgrammed,
                                                                              partitionCount,
                                                                              4u, false,
                                                                              synchronizeBeforeExecution,
                                                                              false,
                                                                              false,
                                                                              false);
    auto totalProgrammedSize = computeControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, false, false) +
                               sizeof(BatchBufferControlData);
    EXPECT_EQ(totalProgrammedSize, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenEstimationWhenItIsCalledThenProperSizeIsReturned) {
    auto partitionCount = 16u;
    auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                            sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                            sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                            sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                            sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                            sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                            sizeof(WalkerPartition::BatchBufferControlData) +
                            sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto requiresBatchBufferEnd = false;
    EXPECT_EQ(expectedUsedSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, false, false));

    requiresBatchBufferEnd = true;
    EXPECT_EQ(expectedUsedSize + sizeof(WalkerPartition::BATCH_BUFFER_END<FamilyType>),
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, false, false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenEstimationWhenPartitionCountIs4ThenSizeIsProperlyEstimated) {
    auto partitionCount = 4u;
    auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                            sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                            sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                            sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                            sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                            sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                            sizeof(WalkerPartition::BatchBufferControlData) +
                            sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto requiresBatchBufferEnd = false;
    EXPECT_EQ(expectedUsedSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, false, false));

    requiresBatchBufferEnd = true;
    EXPECT_EQ(expectedUsedSize + sizeof(WalkerPartition::BATCH_BUFFER_END<FamilyType>),
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, false, false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenEstimationAndSynchronizeBeforeExecutionWhenItIsCalledThenProperSizeIsReturned) {
    auto partitionCount = 16u;
    auto requiresBatchBufferEnd = false;
    auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                            sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                            sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                            sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                            sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                            sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                            sizeof(WalkerPartition::BatchBufferControlData) +
                            sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
    auto expectedDelta = sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                         sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    synchronizeBeforeExecution = false;
    EXPECT_EQ(expectedUsedSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, false, false));

    synchronizeBeforeExecution = true;
    EXPECT_EQ(expectedUsedSize + expectedDelta,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, false, false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningEstimationWhenItIsCalledThenProperSizeIsReturned) {
    const auto partitionCount = 16u;
    const auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>) +
                                  sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                                  sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                                  sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                                  sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                  sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) +
                                  sizeof(WalkerPartition::StaticPartitioningControlSection);

    auto requiresBatchBufferEnd = false;
    EXPECT_EQ(expectedUsedSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, true, false));

    requiresBatchBufferEnd = true;
    EXPECT_EQ(expectedUsedSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, true, false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningEstimationAndSynchronizeBeforeExecutionWhenItIsCalledThenProperSizeIsReturned) {
    const auto partitionCount = 16u;
    const auto requiresBatchBufferEnd = false;
    const auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>) +
                                  sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                                  sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                                  sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                                  sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                  sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) +
                                  sizeof(WalkerPartition::StaticPartitioningControlSection);

    synchronizeBeforeExecution = false;
    EXPECT_EQ(expectedUsedSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, true, false));

    synchronizeBeforeExecution = true;
    const auto preExecutionSynchronizationSize = sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) + sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
    EXPECT_EQ(expectedUsedSize + preExecutionSynchronizationSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, false, true, false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenEstimationNativeSectionsWhenItIsCalledThenProperSizeIsReturned) {
    auto partitionCount = 16u;
    auto requiresBatchBufferEnd = false;
    synchronizeBeforeExecution = false;
    nativeCrossTileSync = true;

    auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                            sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                            sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                            sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                            sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                            sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>) +
                            sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                            sizeof(WalkerPartition::BatchBufferControlData) +
                            sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                            sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) * 2 +
                            sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>) * 3;

    EXPECT_EQ(expectedUsedSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, false, false));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenEstimationNativeSectionsWhenAtomicsUsedForNativeThenProperSizeIsReturned) {
    auto partitionCount = 16u;
    auto requiresBatchBufferEnd = false;
    synchronizeBeforeExecution = false;
    nativeCrossTileSync = true;

    auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                            sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                            sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                            sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                            sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                            sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                            sizeof(WalkerPartition::BatchBufferControlData) +
                            sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                            sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) * 2 +
                            sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 3;

    EXPECT_EQ(expectedUsedSize,
              estimateSpaceRequiredInCommandBuffer<FamilyType>(requiresBatchBufferEnd, partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, false, true));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramWparidPredicationMaskWhenItIsCalledWithWrongInputThenFalseIsReturnedAndNothingIsProgrammed) {
    EXPECT_FALSE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 3));
    EXPECT_FALSE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 5));
    EXPECT_FALSE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 17));
    EXPECT_FALSE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 32));
    EXPECT_FALSE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 15));
    EXPECT_FALSE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 11));
    EXPECT_FALSE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 9));
    EXPECT_EQ(0u, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramWparidPredicationMaskWhenItIsCalledWithPartitionCountThenProperMaskIsSet) {
    auto wparidMaskProgrammingLocation = cmdBufferAddress;
    EXPECT_TRUE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 16));
    auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    auto expectedMask = 0xFFF0u;
    auto expectedRegister = 0x21FCu;

    auto loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(wparidMaskProgrammingLocation);
    ASSERT_NE(nullptr, loadRegisterImmediate);
    EXPECT_EQ(expectedRegister, loadRegisterImmediate->getRegisterOffset());
    EXPECT_EQ(expectedMask, loadRegisterImmediate->getDataDword());

    EXPECT_TRUE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 8));
    wparidMaskProgrammingLocation = ptrOffset(wparidMaskProgrammingLocation, sizeof(LOAD_REGISTER_IMM<FamilyType>));
    loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(wparidMaskProgrammingLocation);
    expectedMask = 0xFFF8u;
    EXPECT_EQ(expectedMask, loadRegisterImmediate->getDataDword());

    EXPECT_TRUE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 4));
    wparidMaskProgrammingLocation = ptrOffset(wparidMaskProgrammingLocation, sizeof(LOAD_REGISTER_IMM<FamilyType>));
    loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(wparidMaskProgrammingLocation);
    expectedMask = 0xFFFCu;
    EXPECT_EQ(expectedMask, loadRegisterImmediate->getDataDword());

    EXPECT_TRUE(programWparidMask<FamilyType>(cmdBufferAddress, totalBytesProgrammed, 2));
    wparidMaskProgrammingLocation = ptrOffset(wparidMaskProgrammingLocation, sizeof(LOAD_REGISTER_IMM<FamilyType>));
    loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(wparidMaskProgrammingLocation);
    expectedMask = 0xFFFEu;
    EXPECT_EQ(expectedMask, loadRegisterImmediate->getDataDword());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramPredicationOnWhenItIsProgrammedThenCommandBufferContainsCorrectCommand) {
    auto expectedUsedSize = sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    void *miSetPredicateAddress = cmdBufferAddress;
    programWparidPredication<FamilyType>(cmdBufferAddress, totalBytesProgrammed, true);
    auto miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(miSetPredicateAddress);

    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramPredicationOffWhenItIsProgrammedThenCommandBufferContainsCorrectCommand) {
    auto expectedUsedSize = sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    void *miSetPredicateAddress = cmdBufferAddress;
    programWparidPredication<FamilyType>(cmdBufferAddress, totalBytesProgrammed, false);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);
    auto miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(miSetPredicateAddress);
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_NEVER);
    EXPECT_EQ(miSetPredicate->getPredicateEnable(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE::PREDICATE_ENABLE_PREDICATE_DISABLE);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramWaitForSemaphoreWhenitisProgrammedThenAllFieldsAreSetCorrectly) {
    auto expectedUsedSize = sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);
    uint64_t gpuAddress = 0x6432100llu;
    uint32_t compareData = 1u;

    void *semaphoreWaitAddress = cmdBufferAddress;
    programWaitForSemaphore<FamilyType>(cmdBufferAddress,
                                        totalBytesProgrammed,
                                        gpuAddress,
                                        compareData,
                                        MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    auto semaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(semaphoreWaitAddress);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    ASSERT_NE(nullptr, semaphoreWait);
    EXPECT_EQ(compareData, semaphoreWait->getSemaphoreDataDword());
    EXPECT_EQ(gpuAddress, semaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreWait->getCompareOperation());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::WAIT_MODE::WAIT_MODE_POLLING_MODE, semaphoreWait->getWaitMode());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::MEMORY_TYPE::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS, semaphoreWait->getMemoryType());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL, semaphoreWait->getRegisterPollMode());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenMiAtomicWhenItIsProgrammedThenAllFieldsAreSetCorrectly) {
    auto expectedUsedSize = sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
    uint64_t gpuAddress = 0xFFFFFFDFEEDBAC10llu;

    void *miAtomicAddress = cmdBufferAddress;
    programMiAtomic<FamilyType>(cmdBufferAddress,
                                totalBytesProgrammed, gpuAddress, true, MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(miAtomicAddress);
    ASSERT_NE(nullptr, miAtomic);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    EXPECT_EQ(0u, miAtomic->getDataSize());
    EXPECT_TRUE(miAtomic->getCsStall());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::MEMORY_TYPE::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS, miAtomic->getMemoryType());
    EXPECT_TRUE(miAtomic->getReturnDataControl());
    EXPECT_FALSE(miAtomic->getWorkloadPartitionIdOffsetEnable());
    auto memoryAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);

    //bits 48-63 are zeroed
    EXPECT_EQ((gpuAddress & 0xFFFFFFFFFFFF), memoryAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenMiLoadRegisterRegWhenItIsProgrammedThenCommandIsProperlySet) {
    auto expectedUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>);
    void *loadRegisterRegAddress = cmdBufferAddress;
    WalkerPartition::programMiLoadRegisterReg<FamilyType>(cmdBufferAddress, totalBytesProgrammed, generalPurposeRegister1, wparidCCSOffset);
    auto loadRegisterReg = genCmdCast<WalkerPartition::LOAD_REGISTER_REG<FamilyType> *>(loadRegisterRegAddress);
    ASSERT_NE(nullptr, loadRegisterReg);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableDestination());
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableSource());
    EXPECT_EQ(generalPurposeRegister1, loadRegisterReg->getSourceRegisterAddress());
    EXPECT_EQ(wparidCCSOffset, loadRegisterReg->getDestinationRegisterAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramPipeControlCommandWhenItIsProgrammedThenItIsProperlySet) {
    auto expectedUsedSize = sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
    void *pipeControlCAddress = cmdBufferAddress;
    WalkerPartition::programPipeControlCommand<FamilyType>(cmdBufferAddress, totalBytesProgrammed, true);
    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(pipeControlCAddress);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramPipeControlCommandWhenItIsProgrammedWithDcFlushFalseThenExpectDcFlushFlagFalse) {
    auto expectedUsedSize = sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
    void *pipeControlCAddress = cmdBufferAddress;
    WalkerPartition::programPipeControlCommand<FamilyType>(cmdBufferAddress, totalBytesProgrammed, false);
    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(pipeControlCAddress);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramPipeControlCommandWhenItIsProgrammedWithDebugDoNotFlushThenItIsProperlySetWithoutDcFlush) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DoNotFlushCaches.set(true);
    auto expectedUsedSize = sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
    void *pipeControlCAddress = cmdBufferAddress;
    WalkerPartition::programPipeControlCommand<FamilyType>(cmdBufferAddress, totalBytesProgrammed, true);
    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(pipeControlCAddress);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramBatchBufferStartCommandWhenItIsCalledThenCommandIsProgrammedCorrectly) {
    auto expectedUsedSize = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
    uint64_t gpuAddress = 0xFFFFFFDFEEDBAC10llu;

    void *batchBufferStartAddress = cmdBufferAddress;
    WalkerPartition::programMiBatchBufferStart<FamilyType>(cmdBufferAddress, totalBytesProgrammed, gpuAddress, true, false);
    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(batchBufferStartAddress);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    //bits 48-63 are zeroed
    EXPECT_EQ((gpuAddress & 0xFFFFFFFFFFFF), batchBufferStart->getBatchBufferStartAddress());

    EXPECT_TRUE(batchBufferStart->getPredicationEnable());
    EXPECT_FALSE(batchBufferStart->getEnableCommandCache());
    EXPECT_EQ(BATCH_BUFFER_START<FamilyType>::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, batchBufferStart->getSecondLevelBatchBuffer());
    EXPECT_EQ(BATCH_BUFFER_START<FamilyType>::ADDRESS_SPACE_INDICATOR::ADDRESS_SPACE_INDICATOR_PPGTT, batchBufferStart->getAddressSpaceIndicator());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenProgramComputeWalkerWhenItIsCalledThenWalkerIsProperlyProgrammed) {
    auto expectedUsedSize = sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(7u);
    walker.setThreadGroupIdYDimension(10u);
    walker.setThreadGroupIdZDimension(11u);

    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_X);
    void *walkerCommandAddress = cmdBufferAddress;
    programPartitionedWalker<FamilyType>(cmdBufferAddress, totalBytesProgrammed, &walker, 2u);
    auto walkerCommand = genCmdCast<COMPUTE_WALKER<FamilyType> *>(walkerCommandAddress);

    ASSERT_NE(nullptr, walkerCommand);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);
    EXPECT_TRUE(walkerCommand->getWorkloadPartitionEnable());
    EXPECT_EQ(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_X, walkerCommand->getPartitionType());
    EXPECT_EQ(4u, walkerCommand->getPartitionSize());

    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_Y);
    walkerCommandAddress = cmdBufferAddress;
    programPartitionedWalker<FamilyType>(cmdBufferAddress, totalBytesProgrammed, &walker, 2u);
    walkerCommand = genCmdCast<COMPUTE_WALKER<FamilyType> *>(walkerCommandAddress);

    ASSERT_NE(nullptr, walkerCommand);
    EXPECT_EQ(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_Y, walkerCommand->getPartitionType());
    EXPECT_EQ(5u, walkerCommand->getPartitionSize());

    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_Z);
    walkerCommandAddress = cmdBufferAddress;
    programPartitionedWalker<FamilyType>(cmdBufferAddress, totalBytesProgrammed, &walker, 2u);
    walkerCommand = genCmdCast<COMPUTE_WALKER<FamilyType> *>(walkerCommandAddress);

    ASSERT_NE(nullptr, walkerCommand);
    EXPECT_EQ(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_Z, walkerCommand->getPartitionType());
    EXPECT_EQ(6u, walkerCommand->getPartitionSize());

    //if we program with partition Count == 1 then do not trigger partition stuff
    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
    walkerCommandAddress = cmdBufferAddress;
    programPartitionedWalker<FamilyType>(cmdBufferAddress, totalBytesProgrammed, &walker, 1u);
    walkerCommand = genCmdCast<COMPUTE_WALKER<FamilyType> *>(walkerCommandAddress);

    ASSERT_NE(nullptr, walkerCommand);
    EXPECT_EQ(0u, walkerCommand->getPartitionSize());
    EXPECT_FALSE(walkerCommand->getWorkloadPartitionEnable());
    EXPECT_EQ(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walkerCommand->getPartitionType());
}
HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerWhenComputePartitionCountIsCalledThenDefaultSizeAndTypeIsReturned) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(16u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerWithNonUniformStartWhenComputePartitionCountIsCalledThenPartitionsAreDisabled) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdStartingX(1u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(1u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker.getPartitionType());

    walker.setThreadGroupIdStartingX(0u);
    walker.setThreadGroupIdStartingY(1u);

    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(1u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker.getPartitionType());

    walker.setThreadGroupIdStartingY(0u);
    walker.setThreadGroupIdStartingZ(1u);

    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(1u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerWithDifferentWorkgroupCountsWhenPartitionCountIsObtainedThenHighestDimensionIsPartitioned) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(64u);
    walker.setThreadGroupIdYDimension(64u);
    walker.setThreadGroupIdZDimension(64u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());

    walker.setThreadGroupIdYDimension(65u);
    walker.setPartitionType(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());

    walker.setThreadGroupIdZDimension(66u);
    walker.setPartitionType(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Z, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenDisalbedMinimalPartitionSizeWhenCoomputePartitionSizeThenProperValueIsReturned) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(64u);
    walker.setThreadGroupIdYDimension(64u);
    walker.setThreadGroupIdZDimension(64u);

    DebugManagerStateRestore restorer;
    DebugManager.flags.SetMinimalPartitionSize.set(0);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(16u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());

    walker.setThreadGroupIdYDimension(65u);
    walker.setPartitionType(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(16u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());

    walker.setThreadGroupIdZDimension(66u);
    walker.setPartitionType(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(16u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Z, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerWithDifferentWorkgroupCountsWhenPartitionCountIsObtainedThenPartitionCountIsClampedToHighestDimension) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(15u);
    walker.setThreadGroupIdYDimension(7u);
    walker.setThreadGroupIdZDimension(4u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());
    walker.setThreadGroupIdXDimension(1u);
    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_DISABLED);

    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());

    walker.setThreadGroupIdYDimension(1u);
    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_DISABLED);

    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Z, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerWithPartitionTypeHintWhenPartitionCountIsObtainedThenSuggestedTypeIsUsedForPartition) {
    DebugManagerStateRestore restore{};

    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(8u);
    walker.setThreadGroupIdYDimension(4u);
    walker.setThreadGroupIdZDimension(2u);

    DebugManager.flags.ExperimentalSetWalkerPartitionType.set(-1);
    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());

    DebugManager.flags.ExperimentalSetWalkerPartitionType.set(static_cast<int32_t>(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_X));
    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());

    DebugManager.flags.ExperimentalSetWalkerPartitionType.set(static_cast<int32_t>(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_Y));
    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());

    DebugManager.flags.ExperimentalSetWalkerPartitionType.set(static_cast<int32_t>(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_Z));
    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Z, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenInvalidPartitionTypeIsRequestedWhenPartitionCountIsObtainedThenFail) {
    DebugManagerStateRestore restore{};

    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(8u);
    walker.setThreadGroupIdYDimension(4u);
    walker.setThreadGroupIdZDimension(2u);

    DebugManager.flags.ExperimentalSetWalkerPartitionType.set(0);
    bool staticPartitioning = false;
    EXPECT_ANY_THROW(computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerWithSmallXDimensionSizeWhenPartitionCountIsObtainedThenPartitionCountIsAdujsted) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(32u);
    walker.setThreadGroupIdYDimension(1024u);
    walker.setThreadGroupIdZDimension(1u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerWithBigXDimensionSizeWhenPartitionCountIsObtainedThenPartitionCountIsNotAdjusted) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(16384u);
    walker.setThreadGroupIdYDimension(1u);
    walker.setThreadGroupIdZDimension(1u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(16u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenCustomMinimalPartitionSizeWhenComputePartitionCountThenProperValueIsReturned) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(16384u);
    walker.setThreadGroupIdYDimension(1u);
    walker.setThreadGroupIdZDimension(1u);

    DebugManagerStateRestore restorer;
    DebugManager.flags.SetMinimalPartitionSize.set(4096);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerWithPartitionTypeProgrammedWhenPartitionCountIsObtainedAndItEqualsOneThenPartitionMechanismIsDisabled) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1u);
    walker.setThreadGroupIdYDimension(1u);
    walker.setThreadGroupIdZDimension(1u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(1u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenXDimensionIsNotLargetAnd2DImagesAreUsedWhenPartitionTypeIsObtainedThenSelectXDimension) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(8u);
    walker.setThreadGroupIdYDimension(64u);
    walker.setThreadGroupIdZDimension(16u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, false, &staticPartitioning);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());

    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, false, true, &staticPartitioning);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningAndNonPartitionableWalkerWhenPartitionCountIsObtainedThenAllowPartitioning) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1u);
    walker.setThreadGroupIdYDimension(1u);
    walker.setThreadGroupIdZDimension(1u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningAndPartitionableWalkerWhenPartitionCountIsObtainedThenAllowPartitioning) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1u);
    walker.setThreadGroupIdYDimension(2u);
    walker.setThreadGroupIdZDimension(1u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningAndBigPartitionCountProgrammedInWalkerWhenPartitionCountIsObtainedThenNumberOfPartitionsIsEqualToNumberOfTiles) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1u);
    walker.setThreadGroupIdYDimension(16384u);
    walker.setThreadGroupIdZDimension(1u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningAndAndNonUniformStartProgrammedInWalkerWhenPartitionCountIsObtainedThenDoNotAllowStaticPartitioningAndSetPartitionCountToOne) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1u);
    walker.setThreadGroupIdYDimension(16384u);
    walker.setThreadGroupIdZDimension(1u);
    walker.setThreadGroupIdStartingX(0);
    walker.setThreadGroupIdStartingY(0);
    walker.setThreadGroupIdStartingZ(1);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, true, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(1u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningAndPartitionTypeHintIsUsedWhenPartitionCountIsObtainedThenUseRequestedPartitionType) {
    DebugManagerStateRestore restore{};
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1u);
    walker.setThreadGroupIdYDimension(16384u);
    walker.setThreadGroupIdZDimension(1u);

    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());

    DebugManager.flags.ExperimentalSetWalkerPartitionType.set(static_cast<int32_t>(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_Z));
    staticPartitioning = false;
    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(4u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Z, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningWhenZDimensionIsNotDivisibleByTwoButIsAboveThreasholThenItIsSelected) {
    DebugManagerStateRestore restore{};
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(1u);
    walker.setThreadGroupIdYDimension(16384u);
    walker.setThreadGroupIdZDimension(2u);

    bool staticPartitioning = true;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Z, walker.getPartitionType());

    DebugManager.flags.WalkerPartitionPreferHighestDimension.set(0);

    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningWhenYDimensionIsDivisibleByTwoThenItIsSelected) {
    DebugManagerStateRestore restore{};
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(16384u);
    walker.setThreadGroupIdYDimension(2u);
    walker.setThreadGroupIdZDimension(1u);

    bool staticPartitioning = true;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Y, walker.getPartitionType());

    DebugManager.flags.WalkerPartitionPreferHighestDimension.set(0);

    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitioningWhenZDimensionIsDivisibleByTwoThenItIsSelected) {
    DebugManagerStateRestore restore{};
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdXDimension(512u);
    walker.setThreadGroupIdYDimension(512u);
    walker.setThreadGroupIdZDimension(513u);

    bool staticPartitioning = true;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Z, walker.getPartitionType());

    DebugManager.flags.WalkerPartitionPreferHighestDimension.set(0);

    partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 2u, true, false, &staticPartitioning);
    EXPECT_TRUE(staticPartitioning);
    EXPECT_EQ(2u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_Z, walker.getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenNativeCrossTileSyncWhenDebugForceDisableCrossTileSyncThenNativeOverridesDebugAndAddsOwnCleanupSection) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ExperimentalForceCrossAtomicSynchronization.set(0);

    auto partitionCount = 16u;
    checkForProperCmdBufferAddressOffset = false;
    nativeCrossTileSync = true;
    uint64_t gpuVirtualAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_X);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA<FamilyType>::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);
    uint32_t totalBytesProgrammed = 0u;

    auto expectedCommandUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                                   sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                                   sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                   sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);

    auto walkerSectionCommands = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) +
                                 sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    EXPECT_EQ(expectedCommandUsedSize, computeControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, false));

    auto cleanupSectionOffset = expectedCommandUsedSize + sizeof(BatchBufferControlData);

    auto totalProgrammedSize = cleanupSectionOffset + 3 * sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>) +
                               2 * sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                               2 * sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    uint32_t tileCount = 4u;
    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                              gpuVirtualAddress, &walker, totalBytesProgrammed, partitionCount,
                                                                              tileCount, false, synchronizeBeforeExecution, false, nativeCrossTileSync, false);

    EXPECT_EQ(totalProgrammedSize, totalBytesProgrammed);
    auto wparidMaskProgrammingLocation = cmdBufferAddress;

    auto expectedMask = 0xFFF0u;
    auto expectedRegister = 0x21FCu;

    auto loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(wparidMaskProgrammingLocation);
    ASSERT_NE(nullptr, loadRegisterImmediate);
    EXPECT_EQ(expectedRegister, loadRegisterImmediate->getRegisterOffset());
    EXPECT_EQ(expectedMask, loadRegisterImmediate->getDataDword());
    auto parsedOffset = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>);

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto miAtomicAddress = gpuVirtualAddress + expectedCommandUsedSize;
    auto miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicAddress, miAtomicProgrammedAddress);
    EXPECT_TRUE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto loadRegisterReg = genCmdCast<WalkerPartition::LOAD_REGISTER_REG<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, loadRegisterReg);
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableDestination());
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableSource());
    EXPECT_EQ(wparidCCSOffset, loadRegisterReg->getDestinationRegisterAddress());
    EXPECT_EQ(generalPurposeRegister4, loadRegisterReg->getSourceRegisterAddress());
    parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>);

    auto miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE);
    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_TRUE(batchBufferStart->getPredicationEnable());
    //address routes to WALKER section which is before control section
    auto address = batchBufferStart->getBatchBufferStartAddress();
    EXPECT_EQ(address, gpuVirtualAddress + expectedCommandUsedSize - walkerSectionCommands);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_NEVER);
    EXPECT_EQ(miSetPredicate->getPredicateEnable(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE::PREDICATE_ENABLE_PREDICATE_DISABLE);
    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    uint64_t expectedCleanupGpuVa = gpuVirtualAddress + expectedCommandUsedSize + offsetof(BatchBufferControlData, finalSyncTileCount);
    constexpr uint32_t expectedData = 0u;
    auto finalSyncTileCountFieldStore = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, finalSyncTileCountFieldStore);
    EXPECT_EQ(expectedCleanupGpuVa, finalSyncTileCountFieldStore->getAddress());
    EXPECT_EQ(expectedData, finalSyncTileCountFieldStore->getDataDword0());
    parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);

    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto miAtomicTileAddress = gpuVirtualAddress + expectedCommandUsedSize + sizeof(uint32_t);
    auto miAtomicTileProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicTileAddress, miAtomicTileProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreGraphicsAddress(), miAtomicTileAddress);
    EXPECT_EQ(miSemaphoreWait->getCompareOperation(), MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreDataDword(), tileCount);

    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    //final batch buffer start that routes at the end of the batch buffer
    auto batchBufferStartFinal = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStartFinal);
    EXPECT_EQ(batchBufferStartFinal->getBatchBufferStartAddress(), gpuVirtualAddress + cleanupSectionOffset);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_NE(nullptr, computeWalker);
    parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_FALSE(batchBufferStart->getPredicationEnable());
    EXPECT_EQ(gpuVirtualAddress, batchBufferStart->getBatchBufferStartAddress());
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto controlSection = reinterpret_cast<BatchBufferControlData *>(ptrOffset(cmdBuffer, expectedCommandUsedSize));
    EXPECT_EQ(0u, controlSection->partitionCount);
    EXPECT_EQ(0u, controlSection->tileCount);
    EXPECT_EQ(0u, controlSection->inTileCount);
    EXPECT_EQ(0u, controlSection->finalSyncTileCount);

    parsedOffset += sizeof(BatchBufferControlData);
    EXPECT_EQ(parsedOffset, cleanupSectionOffset);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    miAtomicTileAddress = gpuVirtualAddress + cleanupSectionOffset - sizeof(BatchBufferControlData) +
                          3 * sizeof(uint32_t);
    miAtomicTileProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicTileAddress, miAtomicTileProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreGraphicsAddress(), miAtomicTileAddress);
    EXPECT_EQ(miSemaphoreWait->getCompareOperation(), MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreDataDword(), tileCount);
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    expectedCleanupGpuVa = gpuVirtualAddress + cleanupSectionOffset - sizeof(BatchBufferControlData);
    auto partitionCountFieldStore = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, partitionCountFieldStore);
    EXPECT_EQ(expectedCleanupGpuVa, partitionCountFieldStore->getAddress());
    EXPECT_EQ(expectedData, partitionCountFieldStore->getDataDword0());
    parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);

    expectedCleanupGpuVa += sizeof(BatchBufferControlData::partitionCount);
    auto tileCountFieldStore = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, tileCountFieldStore);
    EXPECT_EQ(expectedCleanupGpuVa, tileCountFieldStore->getAddress());
    EXPECT_EQ(expectedData, tileCountFieldStore->getDataDword0());
    parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);

    expectedCleanupGpuVa += sizeof(BatchBufferControlData::tileCount);
    auto inTileCountFieldStore = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, inTileCountFieldStore);
    EXPECT_EQ(expectedCleanupGpuVa, inTileCountFieldStore->getAddress());
    EXPECT_EQ(expectedData, inTileCountFieldStore->getDataDword0());
    parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    miAtomicTileProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicTileAddress, miAtomicTileProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreGraphicsAddress(), miAtomicTileAddress);
    EXPECT_EQ(miSemaphoreWait->getCompareOperation(), MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreDataDword(), 2 * tileCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenNativeCrossTileSyncAndAtomicsUsedForNativeWhenDebugForceDisableCrossTileSyncThenNativeOverridesDebugAndAddsOwnCleanupSection) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ExperimentalForceCrossAtomicSynchronization.set(0);

    auto partitionCount = 16u;
    checkForProperCmdBufferAddressOffset = false;
    nativeCrossTileSync = true;
    bool useAtomicsForNative = true;
    uint64_t gpuVirtualAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setPartitionType(COMPUTE_WALKER<FamilyType>::PARTITION_TYPE::PARTITION_TYPE_X);
    auto &postSync = walker.getPostSync();
    postSync.setOperation(POSTSYNC_DATA<FamilyType>::OPERATION::OPERATION_WRITE_TIMESTAMP);
    postSync.setDestinationAddress(postSyncAddress);
    uint32_t totalBytesProgrammed = 0u;

    auto expectedCommandUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                                   sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                                   sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto walkerSectionCommands = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) +
                                 sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    EXPECT_EQ(expectedCommandUsedSize, computeControlSectionOffset<FamilyType>(partitionCount, synchronizeBeforeExecution, nativeCrossTileSync, useAtomicsForNative));

    auto cleanupSectionOffset = expectedCommandUsedSize + sizeof(BatchBufferControlData);

    auto totalProgrammedSize = cleanupSectionOffset + 3 * sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                               2 * sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                               2 * sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    uint32_t tileCount = 4u;
    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                              gpuVirtualAddress, &walker, totalBytesProgrammed, partitionCount,
                                                                              tileCount, false, synchronizeBeforeExecution, false, nativeCrossTileSync, useAtomicsForNative);

    EXPECT_EQ(totalProgrammedSize, totalBytesProgrammed);
    auto wparidMaskProgrammingLocation = cmdBufferAddress;

    auto expectedMask = 0xFFF0u;
    auto expectedRegister = 0x21FCu;

    auto loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(wparidMaskProgrammingLocation);
    ASSERT_NE(nullptr, loadRegisterImmediate);
    EXPECT_EQ(expectedRegister, loadRegisterImmediate->getRegisterOffset());
    EXPECT_EQ(expectedMask, loadRegisterImmediate->getDataDword());
    auto parsedOffset = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>);

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto miAtomicAddress = gpuVirtualAddress + expectedCommandUsedSize;
    auto miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicAddress, miAtomicProgrammedAddress);
    EXPECT_TRUE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto loadRegisterReg = genCmdCast<WalkerPartition::LOAD_REGISTER_REG<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, loadRegisterReg);
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableDestination());
    EXPECT_TRUE(loadRegisterReg->getMmioRemapEnableSource());
    EXPECT_EQ(wparidCCSOffset, loadRegisterReg->getDestinationRegisterAddress());
    EXPECT_EQ(generalPurposeRegister4, loadRegisterReg->getSourceRegisterAddress());
    parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>);

    auto miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE);
    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_TRUE(batchBufferStart->getPredicationEnable());
    //address routes to WALKER section which is before control section
    auto address = batchBufferStart->getBatchBufferStartAddress();
    EXPECT_EQ(address, gpuVirtualAddress + expectedCommandUsedSize - walkerSectionCommands);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    miSetPredicate = genCmdCast<WalkerPartition::MI_SET_PREDICATE<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSetPredicate);
    EXPECT_EQ(miSetPredicate->getPredicateEnableWparid(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_NEVER);
    EXPECT_EQ(miSetPredicate->getPredicateEnable(), MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE::PREDICATE_ENABLE_PREDICATE_DISABLE);
    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    uint64_t expectedCleanupGpuVa = gpuVirtualAddress + expectedCommandUsedSize + offsetof(BatchBufferControlData, finalSyncTileCount);
    auto finalSyncTileCountFieldStore = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, finalSyncTileCountFieldStore);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*finalSyncTileCountFieldStore);
    EXPECT_EQ(expectedCleanupGpuVa, miAtomicProgrammedAddress);
    EXPECT_FALSE(finalSyncTileCountFieldStore->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, finalSyncTileCountFieldStore->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControl->getDcFlushEnable());
    parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto miAtomicTileAddress = gpuVirtualAddress + expectedCommandUsedSize + sizeof(uint32_t);
    auto miAtomicTileProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicTileAddress, miAtomicTileProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreGraphicsAddress(), miAtomicTileAddress);
    EXPECT_EQ(miSemaphoreWait->getCompareOperation(), MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreDataDword(), tileCount);

    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    //final batch buffer start that routes at the end of the batch buffer
    auto batchBufferStartFinal = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStartFinal);
    EXPECT_EQ(batchBufferStartFinal->getBatchBufferStartAddress(), gpuVirtualAddress + cleanupSectionOffset);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_NE(nullptr, computeWalker);
    parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_FALSE(batchBufferStart->getPredicationEnable());
    EXPECT_EQ(gpuVirtualAddress, batchBufferStart->getBatchBufferStartAddress());
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto controlSection = reinterpret_cast<BatchBufferControlData *>(ptrOffset(cmdBuffer, expectedCommandUsedSize));
    EXPECT_EQ(0u, controlSection->partitionCount);
    EXPECT_EQ(0u, controlSection->tileCount);
    EXPECT_EQ(0u, controlSection->inTileCount);
    EXPECT_EQ(0u, controlSection->finalSyncTileCount);

    parsedOffset += sizeof(BatchBufferControlData);
    EXPECT_EQ(parsedOffset, cleanupSectionOffset);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    miAtomicTileAddress = gpuVirtualAddress + cleanupSectionOffset - sizeof(BatchBufferControlData) +
                          3 * sizeof(uint32_t);
    miAtomicTileProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicTileAddress, miAtomicTileProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreGraphicsAddress(), miAtomicTileAddress);
    EXPECT_EQ(miSemaphoreWait->getCompareOperation(), MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreDataDword(), tileCount);
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    expectedCleanupGpuVa = gpuVirtualAddress + cleanupSectionOffset - sizeof(BatchBufferControlData);
    auto partitionCountFieldStore = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, partitionCountFieldStore);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*partitionCountFieldStore);
    EXPECT_EQ(expectedCleanupGpuVa, miAtomicProgrammedAddress);
    EXPECT_FALSE(partitionCountFieldStore->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, partitionCountFieldStore->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    expectedCleanupGpuVa += sizeof(BatchBufferControlData::partitionCount);
    auto tileCountFieldStore = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, tileCountFieldStore);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*tileCountFieldStore);
    EXPECT_EQ(expectedCleanupGpuVa, miAtomicProgrammedAddress);
    EXPECT_FALSE(tileCountFieldStore->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, tileCountFieldStore->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    expectedCleanupGpuVa += sizeof(BatchBufferControlData::tileCount);
    auto inTileCountFieldStore = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, inTileCountFieldStore);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*inTileCountFieldStore);
    EXPECT_EQ(expectedCleanupGpuVa, miAtomicProgrammedAddress);
    EXPECT_FALSE(inTileCountFieldStore->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, inTileCountFieldStore->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    miAtomicTileProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(miAtomicTileAddress, miAtomicTileProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreGraphicsAddress(), miAtomicTileAddress);
    EXPECT_EQ(miSemaphoreWait->getCompareOperation(), MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(miSemaphoreWait->getSemaphoreDataDword(), 2 * tileCount);
}
