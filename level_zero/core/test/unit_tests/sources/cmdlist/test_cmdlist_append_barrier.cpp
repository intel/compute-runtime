/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using CommandListAppendBarrier = Test<CommandListFixture>;

HWTEST_F(CommandListAppendBarrier, WhenAppendingBarrierThenPipeControlIsGenerated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    auto result = commandList->appendBarrier(nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceBefore),
                                                      usedSpaceAfter - usedSpaceBefore));

    // Find a PC w/ CS stall
    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);
    auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
}

HWTEST_F(CommandListAppendBarrier, GivenEventVsNoEventWhenAppendingBarrierThenCorrectPipeControlsIsAddedToCommandStream) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    commandList->reset();
    auto result = commandList->appendBarrier(event->toHandle(), 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList1, cmdList2;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList1,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor1 = findAll<PIPE_CONTROL *>(cmdList1.begin(), cmdList1.end());
    ASSERT_FALSE(itor1.empty());

    commandList->reset();
    usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    result = commandList->appendBarrier(nullptr, 0, nullptr);
    usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList2,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));
    auto itor2 = findAll<PIPE_CONTROL *>(cmdList2.begin(), cmdList2.end());
    ASSERT_FALSE(itor2.empty());

    auto sizeWithoutEvent = itor2.size();
    auto sizeWithEvent = itor1.size();

    ASSERT_LE(sizeWithoutEvent, sizeWithEvent);
}

using MultiTileCommandListAppendBarrier = Test<MultiTileCommandListFixture<false, false, false>>;

HWTEST2_F(MultiTileCommandListAppendBarrier, WhenAppendingBarrierThenPipeControlIsGenerated, IsWithinXeGfxFamily) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    size_t beforeControlSectionOffset = sizeof(MI_STORE_DATA_IMM) +
                                        sizeof(PIPE_CONTROL) +
                                        sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                                        sizeof(MI_BATCH_BUFFER_START);

    size_t startOffset = beforeControlSectionOffset +
                         (2 * sizeof(uint32_t));

    size_t expectedUseBuffer = startOffset +
                               sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                               sizeof(MI_STORE_DATA_IMM) +
                               sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT);

    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    auto gpuBaseAddress = commandList->commandContainer.getCommandStream()->getGraphicsAllocation()->getGpuAddress() +
                          usedSpaceBefore;

    auto gpuCrossTileSyncAddress = gpuBaseAddress +
                                   beforeControlSectionOffset;

    auto gpuFinalSyncAddress = gpuCrossTileSyncAddress +
                               sizeof(uint32_t);

    auto gpuStartAddress = gpuBaseAddress +
                           startOffset;

    auto result = commandList->appendBarrier(nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);
    size_t usedBuffer = usedSpaceAfter - usedSpaceBefore;
    EXPECT_EQ(expectedUseBuffer, usedBuffer);

    void *cmdBuffer = ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceBefore);
    size_t parsedOffset = 0;

    {
        auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        EXPECT_EQ(gpuFinalSyncAddress, storeDataImm->getAddress());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
        parsedOffset += sizeof(MI_STORE_DATA_IMM);
    }
    {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_FALSE(pipeControl->getDcFlushEnable());
        parsedOffset += sizeof(PIPE_CONTROL);
    }
    {
        auto miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        auto miAtomicProgrammedAddress = NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
        EXPECT_EQ(gpuCrossTileSyncAddress, miAtomicProgrammedAddress);
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        parsedOffset += sizeof(MI_ATOMIC);
    }
    {
        auto miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphore);
        EXPECT_EQ(gpuCrossTileSyncAddress, miSemaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
        EXPECT_EQ(2u, miSemaphore->getSemaphoreDataDword());
        parsedOffset += sizeof(MI_SEMAPHORE_WAIT);
    }
    {
        auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, bbStart);
        EXPECT_EQ(gpuStartAddress, bbStart->getBatchBufferStartAddress());
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
        parsedOffset += sizeof(MI_BATCH_BUFFER_START);
    }
    {
        auto crossField = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, parsedOffset));
        EXPECT_EQ(0u, *crossField);
        parsedOffset += sizeof(uint32_t);
        auto finalField = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, parsedOffset));
        EXPECT_EQ(0u, *finalField);
        parsedOffset += sizeof(uint32_t);
    }
    {
        auto miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        auto miAtomicProgrammedAddress = NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
        EXPECT_EQ(gpuFinalSyncAddress, miAtomicProgrammedAddress);
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        parsedOffset += sizeof(MI_ATOMIC);
    }
    {
        auto miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphore);
        EXPECT_EQ(gpuFinalSyncAddress, miSemaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
        EXPECT_EQ(2u, miSemaphore->getSemaphoreDataDword());
        parsedOffset += sizeof(MI_SEMAPHORE_WAIT);
    }
    {
        auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        EXPECT_EQ(gpuCrossTileSyncAddress, storeDataImm->getAddress());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
        parsedOffset += sizeof(MI_STORE_DATA_IMM);
    }
    {
        auto miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        auto miAtomicProgrammedAddress = NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
        EXPECT_EQ(gpuFinalSyncAddress, miAtomicProgrammedAddress);
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        parsedOffset += sizeof(MI_ATOMIC);
    }
    {
        auto miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphore);
        EXPECT_EQ(gpuFinalSyncAddress, miSemaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
        EXPECT_EQ(4u, miSemaphore->getSemaphoreDataDword());
        parsedOffset += sizeof(MI_SEMAPHORE_WAIT);
    }
    EXPECT_EQ(expectedUseBuffer, parsedOffset);
}

HWTEST2_F(MultiTileCommandListAppendBarrier,
          GivenCurrentCommandBufferExhaustedWhenAppendingMultiTileBarrierThenPipeControlAndCrossTileSyncIsGeneratedInNewBuffer, IsWithinXeGfxFamily) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    LinearStream *cmdListStream = commandList->commandContainer.getCommandStream();

    size_t beforeControlSectionOffset = sizeof(MI_STORE_DATA_IMM) +
                                        sizeof(PIPE_CONTROL) +
                                        sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                                        sizeof(MI_BATCH_BUFFER_START);

    size_t bbStartOffset = beforeControlSectionOffset +
                           (2 * sizeof(uint32_t));

    size_t expectedUseBuffer = bbStartOffset +
                               sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                               sizeof(MI_STORE_DATA_IMM) +
                               sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT);

    auto firstBatchBufferAllocation = cmdListStream->getGraphicsAllocation();
    auto useSize = cmdListStream->getAvailableSpace();
    useSize -= (sizeof(MI_BATCH_BUFFER_END) +
                sizeof(MI_STORE_DATA_IMM) +
                sizeof(PIPE_CONTROL));
    cmdListStream->getSpace(useSize);

    auto result = commandList->appendBarrier(nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondBatchBufferAllocation = cmdListStream->getGraphicsAllocation();
    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);

    auto gpuBaseAddress = secondBatchBufferAllocation->getGpuAddress();

    auto gpuCrossTileSyncAddress = gpuBaseAddress +
                                   beforeControlSectionOffset;

    auto gpuFinalSyncAddress = gpuCrossTileSyncAddress +
                               sizeof(uint32_t);

    auto gpuStartAddress = gpuBaseAddress +
                           bbStartOffset;

    auto usedSpace = cmdListStream->getUsed();
    EXPECT_EQ(expectedUseBuffer, usedSpace);

    void *cmdBuffer = cmdListStream->getCpuBase();
    size_t parsedOffset = 0;

    {
        auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        EXPECT_EQ(gpuFinalSyncAddress, storeDataImm->getAddress());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
        parsedOffset += sizeof(MI_STORE_DATA_IMM);
    }
    {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, pipeControl);
        EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
        EXPECT_FALSE(pipeControl->getDcFlushEnable());
        parsedOffset += sizeof(PIPE_CONTROL);
    }
    {
        auto miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        auto miAtomicProgrammedAddress = NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
        EXPECT_EQ(gpuCrossTileSyncAddress, miAtomicProgrammedAddress);
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        parsedOffset += sizeof(MI_ATOMIC);
    }
    {
        auto miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphore);
        EXPECT_EQ(gpuCrossTileSyncAddress, miSemaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
        EXPECT_EQ(2u, miSemaphore->getSemaphoreDataDword());
        parsedOffset += sizeof(MI_SEMAPHORE_WAIT);
    }
    {
        auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, bbStart);
        EXPECT_EQ(gpuStartAddress, bbStart->getBatchBufferStartAddress());
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
        parsedOffset += sizeof(MI_BATCH_BUFFER_START);
    }
    {
        auto crossField = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, parsedOffset));
        EXPECT_EQ(0u, *crossField);
        parsedOffset += sizeof(uint32_t);
        auto finalField = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, parsedOffset));
        EXPECT_EQ(0u, *finalField);
        parsedOffset += sizeof(uint32_t);
    }
    {
        auto miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        auto miAtomicProgrammedAddress = NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
        EXPECT_EQ(gpuFinalSyncAddress, miAtomicProgrammedAddress);
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        parsedOffset += sizeof(MI_ATOMIC);
    }
    {
        auto miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphore);
        EXPECT_EQ(gpuFinalSyncAddress, miSemaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
        EXPECT_EQ(2u, miSemaphore->getSemaphoreDataDword());
        parsedOffset += sizeof(MI_SEMAPHORE_WAIT);
    }
    {
        auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        EXPECT_EQ(gpuCrossTileSyncAddress, storeDataImm->getAddress());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
        parsedOffset += sizeof(MI_STORE_DATA_IMM);
    }
    {
        auto miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miAtomic);
        auto miAtomicProgrammedAddress = NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);
        EXPECT_EQ(gpuFinalSyncAddress, miAtomicProgrammedAddress);
        EXPECT_FALSE(miAtomic->getReturnDataControl());
        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
        parsedOffset += sizeof(MI_ATOMIC);
    }
    {
        auto miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, miSemaphore);
        EXPECT_EQ(gpuFinalSyncAddress, miSemaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
        EXPECT_EQ(4u, miSemaphore->getSemaphoreDataDword());
        parsedOffset += sizeof(MI_SEMAPHORE_WAIT);
    }
    EXPECT_EQ(expectedUseBuffer, parsedOffset);
}

} // namespace ult
} // namespace L0
