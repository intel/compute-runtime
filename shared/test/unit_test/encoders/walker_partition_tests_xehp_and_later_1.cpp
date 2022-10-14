/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/unit_test/encoders/walker_partition_fixture_xehp_and_later.h"

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenWalkerPartitionWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    testArgs.partitionCount = 16u;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

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

    EXPECT_EQ(expectedCommandUsedSize, computeControlSectionOffset<FamilyType>(testArgs));

    auto optionalBatchBufferEndOffset = expectedCommandUsedSize + sizeof(BatchBufferControlData);

    auto totalProgrammedSize = optionalBatchBufferEndOffset + sizeof(WalkerPartition::BATCH_BUFFER_END<FamilyType>);

    testArgs.tileCount = 4u;
    testArgs.emitBatchBufferEnd = true;
    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                              gpuVirtualAddress,
                                                                              &walker,
                                                                              totalBytesProgrammed,
                                                                              testArgs,
                                                                              *defaultHwInfo);

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
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());

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
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;

    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
    EXPECT_EQ(controlSectionOffset + sizeof(StaticPartitioningControlSection), totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
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
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

    checkForProperCmdBufferAddressOffset = false;
    testArgs.synchronizeBeforeExecution = true;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
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
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
    testArgs.semaphoreProgrammingRequired = true;
    testArgs.crossTileAtomicSynchronization = false;
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
    EXPECT_EQ(controlSectionOffset, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
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
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());
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

    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWithSelfCleanupWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.emitSelfCleanup = true;
    testArgs.staticPartitioning = true;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto finalSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(testArgs);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
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
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
        EXPECT_EQ(2 * testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWithSelfCleanupAndCrossTileSyncDisabledWithFlagWhenConstructCommandBufferIsCalledThenStillProgramTheSync) {
    testArgs.crossTileAtomicSynchronization = false;
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.emitSelfCleanup = true;
    testArgs.staticPartitioning = true;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto finalSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(testArgs);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
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
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
        EXPECT_EQ(2 * testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWithSelfCleanupAndAtomicsForSelfCleanupWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.useAtomicsForSelfCleanup = true;
    testArgs.emitSelfCleanup = true;
    testArgs.staticPartitioning = true;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto finalSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(testArgs);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
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
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
        EXPECT_EQ(2 * testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWithSlefCleanupAndCrossTileSyncDisabledWithFlagWhenUsingAtomicForSelfCleanupAndConstructCommandBufferIsCalledThenStillProgramTheSync) {
    testArgs.crossTileAtomicSynchronization = false;
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.emitSelfCleanup = true;
    testArgs.useAtomicsForSelfCleanup = true;
    testArgs.staticPartitioning = true;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    const auto preWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    const auto finalSyncAddress = cmdBufferGpuAddress + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(testArgs);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
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
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
        EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
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
        EXPECT_EQ(2 * testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenDebugModesForWalkerPartitionWhenConstructCommandBufferIsCalledThenBatchBufferIsBeingProgrammed) {
    testArgs.crossTileAtomicSynchronization = false;
    testArgs.semaphoreProgrammingRequired = true;
    testArgs.tileCount = 4u;
    testArgs.partitionCount = 16u;
    testArgs.emitBatchBufferEnd = true;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

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
                                   sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) * testArgs.partitionCount;

    auto walkerSectionCommands = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) +
                                 sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    EXPECT_EQ(expectedCommandUsedSize, computeControlSectionOffset<FamilyType>(testArgs));

    auto optionalBatchBufferEndOffset = expectedCommandUsedSize + sizeof(BatchBufferControlData);

    auto totalProgrammedSize = optionalBatchBufferEndOffset + sizeof(WalkerPartition::BATCH_BUFFER_END<FamilyType>);

    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                              gpuVirtualAddress,
                                                                              &walker,
                                                                              totalBytesProgrammed,
                                                                              testArgs,
                                                                              *defaultHwInfo);

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
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());

    parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);

    auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    for (uint32_t partitionId = 0u; partitionId < testArgs.partitionCount; partitionId++) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWhenWparidRegisterProgrammingDisabledThenExpectNoMiLoadRegisterMemCommand) {
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.initializeWparidRegister = false;
    testArgs.emitSelfCleanup = false;
    testArgs.crossTileAtomicSynchronization = false;
    testArgs.useAtomicsForSelfCleanup = false;
    testArgs.emitPipeControlStall = false;
    testArgs.staticPartitioning = true;

    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint64_t expectedControlSectionOffset = sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    EXPECT_EQ(expectedControlSectionOffset, controlSectionOffset);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(testArgs);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticWalkerPartitionWhenPipeControlProgrammingDisabledThenExpectNoPipeControlCommand) {
    testArgs.tileCount = 4u;
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.emitSelfCleanup = false;
    testArgs.emitPipeControlStall = false;
    testArgs.crossTileAtomicSynchronization = false;
    testArgs.useAtomicsForSelfCleanup = false;
    testArgs.staticPartitioning = true;

    checkForProperCmdBufferAddressOffset = false;
    uint64_t cmdBufferGpuAddress = 0x8000123000;
    uint64_t postSyncAddress = 0x8000456000;
    testArgs.workPartitionAllocationGpuVa = 0x8000444000;
    auto walker = createWalker<FamilyType>(postSyncAddress);

    uint64_t expectedControlSectionOffset = sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>) +
                                            sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    uint32_t totalBytesProgrammed{};
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<FamilyType>(testArgs);
    EXPECT_EQ(expectedControlSectionOffset, controlSectionOffset);
    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                             cmdBufferGpuAddress,
                                                                             &walker,
                                                                             totalBytesProgrammed,
                                                                             testArgs,
                                                                             *defaultHwInfo);
    const auto expectedBytesProgrammed = WalkerPartition::estimateSpaceRequiredInCommandBuffer<FamilyType>(testArgs);
    EXPECT_EQ(expectedBytesProgrammed, totalBytesProgrammed);

    auto parsedOffset = 0u;
    {
        auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, loadRegisterMem);
        parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);
        const auto expectedRegister = 0x221Cu;
        EXPECT_TRUE(loadRegisterMem->getMmioRemapEnable());
        EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
        EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
    }
    {
        auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, computeWalker);
        parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    }
    EXPECT_EQ(parsedOffset, totalBytesProgrammed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests, givenStaticPartitionIsPreferredAndWalkerWithNonUniformStartWhenDynamicPartitionSelectedThenExpectReconfigureWparidToStatic) {
    WalkerPartition::COMPUTE_WALKER<FamilyType> walker;
    walker = FamilyType::cmdInitGpgpuWalker;
    walker.setThreadGroupIdStartingX(1u);

    checkForProperCmdBufferAddressOffset = false;
    bool preferredStaticPartitioning = true;
    bool staticPartitioning = false;
    auto partitionCount = computePartitionCountAndSetPartitionType<FamilyType>(&walker, 4u, preferredStaticPartitioning, false, &staticPartitioning);
    EXPECT_FALSE(staticPartitioning);
    EXPECT_EQ(1u, partitionCount);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker.getPartitionType());

    testArgs.partitionCount = partitionCount;
    testArgs.staticPartitioning = staticPartitioning;
    testArgs.preferredStaticPartitioning = preferredStaticPartitioning;
    testArgs.workPartitionAllocationGpuVa = 0x800BADA55000;
    testArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo);

    auto expectedCommandUsedSize = sizeof(WalkerPartition::LOAD_REGISTER_IMM<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::LOAD_REGISTER_REG<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>) * 2 +
                                   sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) * 3 +
                                   sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>) +
                                   sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>) +
                                   sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                   sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);

    EXPECT_EQ(expectedCommandUsedSize, computeControlSectionOffset<FamilyType>(testArgs));

    auto walkerSectionCommands = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>) +
                                 sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);
    auto totalProgrammedSize = expectedCommandUsedSize + sizeof(BatchBufferControlData);

    testArgs.tileCount = 2;
    uint64_t gpuVirtualAddress = 0x8000123000;
    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(cmdBuffer,
                                                                              gpuVirtualAddress,
                                                                              &walker,
                                                                              totalBytesProgrammed,
                                                                              testArgs,
                                                                              *defaultHwInfo);

    EXPECT_EQ(totalProgrammedSize, totalBytesProgrammed);

    auto expectedMask = 0xFFFFu;
    auto expectedRegister = 0x21FCu;
    auto loadRegisterImmediate = genCmdCast<WalkerPartition::LOAD_REGISTER_IMM<FamilyType> *>(cmdBufferAddress);
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
    EXPECT_EQ(MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE, miSetPredicate->getPredicateEnableWparid());
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
    EXPECT_EQ(MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_NEVER, miSetPredicate->getPredicateEnableWparid());
    EXPECT_EQ(MI_SET_PREDICATE<FamilyType>::PREDICATE_ENABLE::PREDICATE_ENABLE_PREDICATE_DISABLE, miSetPredicate->getPredicateEnable());
    parsedOffset += sizeof(WalkerPartition::MI_SET_PREDICATE<FamilyType>);

    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());
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
    EXPECT_EQ(miAtomicTileAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
    EXPECT_EQ(2u, miSemaphoreWait->getSemaphoreDataDword());
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto loadRegisterMem = genCmdCast<WalkerPartition::LOAD_REGISTER_MEM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, loadRegisterMem);
    EXPECT_EQ(testArgs.workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
    EXPECT_EQ(wparidCCSOffset, loadRegisterMem->getRegisterAddress());
    parsedOffset += sizeof(WalkerPartition::LOAD_REGISTER_MEM<FamilyType>);

    //final batch buffer start that routes at the end of the batch buffer
    auto batchBufferStartFinal = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_NE(nullptr, batchBufferStartFinal);
    EXPECT_EQ(batchBufferStartFinal->getBatchBufferStartAddress(), gpuVirtualAddress + totalProgrammedSize);
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto computeWalker = genCmdCast<WalkerPartition::COMPUTE_WALKER<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, computeWalker);
    parsedOffset += sizeof(WalkerPartition::COMPUTE_WALKER<FamilyType>);

    batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_FALSE(batchBufferStart->getPredicationEnable());
    EXPECT_EQ(gpuVirtualAddress, batchBufferStart->getBatchBufferStartAddress());
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests,
            givenPostSyncBarrierProgrammingWhenDoNotEmitSelfCleanupThenExpectPostSyncCommandsAndNoCleanupSection) {
    testArgs.tileCount = 4u;
    testArgs.emitSelfCleanup = false;
    testArgs.usePostSync = true;
    testArgs.postSyncGpuAddress = 0xDAAB00;
    testArgs.postSyncImmediateValue = 0x421;

    uint32_t totalBytesProgrammed = 0u;
    uint64_t gpuVirtualAddress = 0xFF0000;

    auto expectedOffsetSectionSize = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(testHardwareInfo, false) +
                                     sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) + sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                     sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto expectedCommandUsedSize = expectedOffsetSectionSize +
                                   sizeof(BarrierControlSection);

    EXPECT_EQ(expectedOffsetSectionSize, computeBarrierControlSectionOffset<FamilyType>(testArgs, testHardwareInfo));
    EXPECT_EQ(expectedCommandUsedSize, estimateBarrierSpaceRequiredInCommandBuffer<FamilyType>(testArgs, testHardwareInfo));

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = false;
    WalkerPartition::constructBarrierCommandBuffer<FamilyType>(cmdBuffer,
                                                               gpuVirtualAddress,
                                                               totalBytesProgrammed,
                                                               testArgs,
                                                               flushArgs,
                                                               testHardwareInfo);

    EXPECT_EQ(expectedCommandUsedSize, totalBytesProgrammed);
    size_t parsedOffset = 0;

    size_t additionalSyncCmdSize = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(testHardwareInfo);

    if (NEO::MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(testHardwareInfo)) {
        constexpr uint64_t zeroGpuAddress = 0;
        constexpr uint64_t zeroImmediateValue = 0;
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(zeroGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
        EXPECT_EQ(zeroImmediateValue, pipeControl->getImmediateData());
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        parsedOffset += additionalSyncCmdSize;
    }

    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
    EXPECT_EQ(testArgs.postSyncGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(testArgs.postSyncImmediateValue, pipeControl->getImmediateData());
    parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
    parsedOffset += additionalSyncCmdSize;

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto crossTileSyncAddress = gpuVirtualAddress + expectedOffsetSectionSize + offsetof(BarrierControlSection, crossTileSyncCount);
    auto miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(crossTileSyncAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(crossTileSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
    EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(gpuVirtualAddress + expectedOffsetSectionSize + sizeof(BarrierControlSection), batchBufferStart->getBatchBufferStartAddress());
    EXPECT_EQ(BATCH_BUFFER_START<FamilyType>::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, batchBufferStart->getSecondLevelBatchBuffer());
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto controlSection = reinterpret_cast<BarrierControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_EQ(0u, controlSection->crossTileSyncCount);
    EXPECT_EQ(0u, controlSection->finalSyncTileCount);
    parsedOffset += sizeof(BarrierControlSection);

    EXPECT_EQ(parsedOffset, expectedCommandUsedSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests,
            givenPostSyncBarrierProgrammingWhenEmitsSelfCleanupThenExpectPostSyncCommandsAndStoreDataImmCommandCleanupSection) {
    testArgs.tileCount = 4u;
    testArgs.emitSelfCleanup = true;
    testArgs.secondaryBatchBuffer = true;
    testArgs.usePostSync = true;
    testArgs.postSyncGpuAddress = 0xDAAB00;
    testArgs.postSyncImmediateValue = 0x421;

    uint32_t totalBytesProgrammed = 0u;
    uint64_t gpuVirtualAddress = 0xFF0000;

    auto expectedOffsetSectionSize = sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>) +
                                     NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(testHardwareInfo, false) +
                                     sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) + sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                     sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto expectedCommandUsedSize = expectedOffsetSectionSize +
                                   sizeof(BarrierControlSection) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) + sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                   sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) + sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    EXPECT_EQ(expectedOffsetSectionSize, computeBarrierControlSectionOffset<FamilyType>(testArgs, testHardwareInfo));
    EXPECT_EQ(expectedCommandUsedSize, estimateBarrierSpaceRequiredInCommandBuffer<FamilyType>(testArgs, testHardwareInfo));

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    WalkerPartition::constructBarrierCommandBuffer<FamilyType>(cmdBuffer,
                                                               gpuVirtualAddress,
                                                               totalBytesProgrammed,
                                                               testArgs,
                                                               flushArgs,
                                                               testHardwareInfo);
    EXPECT_EQ(expectedCommandUsedSize, totalBytesProgrammed);

    size_t parsedOffset = 0;

    uint64_t finalSyncTileCountAddress = gpuVirtualAddress + expectedOffsetSectionSize + offsetof(BarrierControlSection, finalSyncTileCount);
    constexpr uint32_t expectedData = 0u;

    auto finalSyncTileCountFieldStore = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, finalSyncTileCountFieldStore);
    EXPECT_EQ(finalSyncTileCountAddress, finalSyncTileCountFieldStore->getAddress());
    EXPECT_EQ(expectedData, finalSyncTileCountFieldStore->getDataDword0());
    parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);

    size_t additionalSyncCmdSize = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(testHardwareInfo);

    if (NEO::MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(testHardwareInfo)) {
        constexpr uint64_t zeroGpuAddress = 0;
        constexpr uint64_t zeroImmediateValue = 0;
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(zeroGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
        EXPECT_EQ(zeroImmediateValue, pipeControl->getImmediateData());
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        parsedOffset += additionalSyncCmdSize;
    }

    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
    EXPECT_EQ(testArgs.postSyncGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(testArgs.postSyncImmediateValue, pipeControl->getImmediateData());
    parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
    parsedOffset += additionalSyncCmdSize;

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto crossTileSyncAddress = gpuVirtualAddress + expectedOffsetSectionSize + offsetof(BarrierControlSection, crossTileSyncCount);
    auto miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(crossTileSyncAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(crossTileSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
    EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(gpuVirtualAddress + expectedOffsetSectionSize + sizeof(BarrierControlSection), batchBufferStart->getBatchBufferStartAddress());
    EXPECT_EQ(BATCH_BUFFER_START<FamilyType>::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, batchBufferStart->getSecondLevelBatchBuffer());
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto controlSection = reinterpret_cast<BarrierControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_EQ(0u, controlSection->crossTileSyncCount);
    EXPECT_EQ(0u, controlSection->finalSyncTileCount);
    parsedOffset += sizeof(BarrierControlSection);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(finalSyncTileCountAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(finalSyncTileCountAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
    EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto crossTileFieldStore = genCmdCast<WalkerPartition::MI_STORE_DATA_IMM<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, crossTileFieldStore);
    EXPECT_EQ(crossTileSyncAddress, crossTileFieldStore->getAddress());
    EXPECT_EQ(expectedData, crossTileFieldStore->getDataDword0());
    parsedOffset += sizeof(WalkerPartition::MI_STORE_DATA_IMM<FamilyType>);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(finalSyncTileCountAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(finalSyncTileCountAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
    EXPECT_EQ(2u * testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    EXPECT_EQ(parsedOffset, expectedCommandUsedSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerPartitionTests,
            givenPostSyncBarrierProgrammingWhenEmitsSelfCleanupUsingAtomicThenExpectPostSyncCommandsAndMiAtomicCommandCleanupSection) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    testHardwareInfo.featureTable.flags.ftrLocalMemory = true;

    testArgs.tileCount = 4u;
    testArgs.emitSelfCleanup = true;
    testArgs.secondaryBatchBuffer = true;
    testArgs.useAtomicsForSelfCleanup = true;
    testArgs.usePostSync = true;
    testArgs.postSyncGpuAddress = 0xDAAB00;
    testArgs.postSyncImmediateValue = 0x421;

    uint32_t totalBytesProgrammed = 0u;
    uint64_t gpuVirtualAddress = 0xFF0000;

    auto expectedOffsetSectionSize = sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                                     NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(testHardwareInfo, false) +
                                     sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) + sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                     sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto expectedCommandUsedSize = expectedOffsetSectionSize +
                                   sizeof(BarrierControlSection) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) + sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) +
                                   sizeof(WalkerPartition::MI_ATOMIC<FamilyType>) + sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    EXPECT_EQ(expectedOffsetSectionSize, computeBarrierControlSectionOffset<FamilyType>(testArgs, testHardwareInfo));
    EXPECT_EQ(expectedCommandUsedSize, estimateBarrierSpaceRequiredInCommandBuffer<FamilyType>(testArgs, testHardwareInfo));

    PipeControlArgs flushArgs;
    flushArgs.dcFlushEnable = true;
    WalkerPartition::constructBarrierCommandBuffer<FamilyType>(cmdBuffer,
                                                               gpuVirtualAddress,
                                                               totalBytesProgrammed,
                                                               testArgs,
                                                               flushArgs,
                                                               testHardwareInfo);

    EXPECT_EQ(expectedCommandUsedSize, totalBytesProgrammed);

    size_t parsedOffset = 0;

    uint64_t finalSyncTileCountAddress = gpuVirtualAddress + expectedOffsetSectionSize + offsetof(BarrierControlSection, finalSyncTileCount);
    constexpr uint32_t expectedData = 0u;

    auto finalSyncTileCountFieldAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, finalSyncTileCountFieldAtomic);
    auto miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*finalSyncTileCountFieldAtomic);
    EXPECT_EQ(finalSyncTileCountAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(finalSyncTileCountFieldAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, finalSyncTileCountFieldAtomic->getAtomicOpcode());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_1, finalSyncTileCountFieldAtomic->getDwordLength());
    EXPECT_TRUE(finalSyncTileCountFieldAtomic->getInlineData());
    EXPECT_EQ(expectedData, finalSyncTileCountFieldAtomic->getOperand1DataDword0());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    size_t additionalSyncCmdSize = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(testHardwareInfo);

    if (NEO::MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(testHardwareInfo)) {
        constexpr uint64_t zeroGpuAddress = 0;
        constexpr uint64_t zeroImmediateValue = 0;
        auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(zeroGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
        EXPECT_EQ(zeroImmediateValue, pipeControl->getImmediateData());
        parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
        parsedOffset += additionalSyncCmdSize;
    }

    auto pipeControl = genCmdCast<WalkerPartition::PIPE_CONTROL<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
    EXPECT_EQ(testArgs.postSyncGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(testArgs.postSyncImmediateValue, pipeControl->getImmediateData());
    parsedOffset += sizeof(WalkerPartition::PIPE_CONTROL<FamilyType>);
    parsedOffset += additionalSyncCmdSize;

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    auto crossTileSyncAddress = gpuVirtualAddress + expectedOffsetSectionSize + offsetof(BarrierControlSection, crossTileSyncCount);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(crossTileSyncAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    auto miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(crossTileSyncAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
    EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(gpuVirtualAddress + expectedOffsetSectionSize + sizeof(BarrierControlSection), batchBufferStart->getBatchBufferStartAddress());
    EXPECT_EQ(BATCH_BUFFER_START<FamilyType>::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, batchBufferStart->getSecondLevelBatchBuffer());
    parsedOffset += sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);

    auto controlSection = reinterpret_cast<BarrierControlSection *>(ptrOffset(cmdBuffer, parsedOffset));
    EXPECT_EQ(0u, controlSection->crossTileSyncCount);
    EXPECT_EQ(0u, controlSection->finalSyncTileCount);
    parsedOffset += sizeof(BarrierControlSection);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(finalSyncTileCountAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(finalSyncTileCountAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
    EXPECT_EQ(testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    auto crossTileFieldAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, crossTileFieldAtomic);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*crossTileFieldAtomic);
    EXPECT_EQ(crossTileSyncAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(crossTileFieldAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_MOVE, crossTileFieldAtomic->getAtomicOpcode());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_1, crossTileFieldAtomic->getDwordLength());
    EXPECT_TRUE(crossTileFieldAtomic->getInlineData());
    EXPECT_EQ(expectedData, crossTileFieldAtomic->getOperand1DataDword0());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miAtomic);
    miAtomicProgrammedAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
    EXPECT_EQ(finalSyncTileCountAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(miAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    parsedOffset += sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);

    miSemaphoreWait = genCmdCast<WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType> *>(ptrOffset(cmdBuffer, parsedOffset));
    ASSERT_NE(nullptr, miSemaphoreWait);
    EXPECT_EQ(finalSyncTileCountAddress, miSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT<FamilyType>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphoreWait->getCompareOperation());
    EXPECT_EQ(2u * testArgs.tileCount, miSemaphoreWait->getSemaphoreDataDword());
    parsedOffset += sizeof(WalkerPartition::MI_SEMAPHORE_WAIT<FamilyType>);

    EXPECT_EQ(parsedOffset, expectedCommandUsedSize);
}
