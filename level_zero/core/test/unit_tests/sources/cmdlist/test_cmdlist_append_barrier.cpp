/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using CommandListAppendBarrier = Test<CommandListFixture>;

HWTEST_F(CommandListAppendBarrier, WhenAppendingBarrierThenPipeControlIsGenerated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    auto result = commandList->appendBarrier(nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), usedSpaceBefore),
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
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    commandList->reset();
    auto result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList1, cmdList2;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList1,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor1 = findAll<PIPE_CONTROL *>(cmdList1.begin(), cmdList1.end());
    ASSERT_FALSE(itor1.empty());

    commandList->reset();
    usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    result = commandList->appendBarrier(nullptr, 0, nullptr, false);
    usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList2,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));
    auto itor2 = findAll<PIPE_CONTROL *>(cmdList2.begin(), cmdList2.end());
    ASSERT_FALSE(itor2.empty());

    auto sizeWithoutEvent = itor2.size();
    auto sizeWithEvent = itor1.size();

    ASSERT_LE(sizeWithoutEvent, sizeWithEvent);
}

template <typename FamilyType>
void validateMultiTileBarrier(void *cmdBuffer, size_t &parsedOffset,
                              uint64_t gpuFinalSyncAddress, uint64_t gpuCrossTileSyncAddress, uint64_t gpuStartAddress,
                              bool validateCleanupSection, bool secondaryBatchBuffer) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    if (validateCleanupSection) {
        auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, storeDataImm);
        EXPECT_EQ(gpuFinalSyncAddress, storeDataImm->getAddress());
        EXPECT_EQ(0u, storeDataImm->getDataDword0());
        parsedOffset += sizeof(MI_STORE_DATA_IMM);
    } else {
        auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(ptrOffset(cmdBuffer, parsedOffset));
        EXPECT_EQ(nullptr, storeDataImm);
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
        parsedOffset += NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();
    }
    {
        auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(cmdBuffer, parsedOffset));
        ASSERT_NE(nullptr, bbStart);
        EXPECT_EQ(gpuStartAddress, bbStart->getBatchBufferStartAddress());
        if (secondaryBatchBuffer) {
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
        } else {
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
        }
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

    if (validateCleanupSection) {
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
            parsedOffset += NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();
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
            parsedOffset += NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();
        }
    }
}

template <bool usePrimaryBuffer>
struct MultiTileCommandListAppendBarrierFixture : public MultiTileCommandListFixture<false, false, false, static_cast<int32_t>(usePrimaryBuffer)> {
    using BaseClass = MultiTileCommandListFixture<false, false, false, static_cast<int32_t>(usePrimaryBuffer)>;

    using BaseClass::commandList;
    using BaseClass::context;
    using BaseClass::device;
    using BaseClass::driverHandle;
    using BaseClass::event;

    void setUp() {
        BaseClass::setUp();
    }

    void tearDown() {
        BaseClass::tearDown();
    }

    template <typename FamilyType>
    void testBodyNonTimestampEventSignal() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
        using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
        using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

        uint64_t eventGpuAddress = event->getCompletionFieldGpuAddress(device);
        ze_event_handle_t eventHandle = event->toHandle();

        EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
        EXPECT_EQ(2u, commandList->partitionCount);

        LinearStream *cmdListStream = commandList->getCmdContainer().getCommandStream();

        size_t beforeControlSectionOffset = sizeof(MI_STORE_DATA_IMM) +
                                            sizeof(PIPE_CONTROL) +
                                            sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                                            sizeof(MI_BATCH_BUFFER_START);

        size_t bbStartOffset = beforeControlSectionOffset +
                               (2 * sizeof(uint32_t));

        size_t multiTileBarrierSize = bbStartOffset +
                                      sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                                      sizeof(MI_STORE_DATA_IMM) +
                                      sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

        size_t postSyncSize = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(device->getNEODevice()->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);

        auto useSizeBefore = cmdListStream->getUsed();
        auto result = commandList->appendBarrier(eventHandle, 0, nullptr, false);
        auto useSizeAfter = cmdListStream->getUsed();
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(2u, event->getPacketsInUse());

        size_t totaSizedBarrierWithNonTimestampEvent = multiTileBarrierSize + postSyncSize;

        EXPECT_EQ(totaSizedBarrierWithNonTimestampEvent, (useSizeAfter - useSizeBefore));

        auto gpuBaseAddress = cmdListStream->getGraphicsAllocation()->getGpuAddress() + useSizeBefore;

        auto gpuCrossTileSyncAddress = gpuBaseAddress +
                                       beforeControlSectionOffset;

        auto gpuFinalSyncAddress = gpuCrossTileSyncAddress +
                                   sizeof(uint32_t);

        auto gpuStartAddress = gpuBaseAddress +
                               bbStartOffset;

        void *cmdBuffer = ptrOffset(cmdListStream->getCpuBase(), useSizeBefore);
        size_t parsedOffset = 0;

        validateMultiTileBarrier<FamilyType>(cmdBuffer, parsedOffset, gpuFinalSyncAddress, gpuCrossTileSyncAddress, gpuStartAddress, true, !usePrimaryBuffer);
        EXPECT_EQ(multiTileBarrierSize, parsedOffset);

        cmdBuffer = ptrOffset(cmdBuffer, parsedOffset);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          cmdBuffer,
                                                          postSyncSize));

        auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(0u, itorPC.size());
        uint32_t postSyncFound = 0;
        for (auto it : itorPC) {
            auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
            if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
                EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
                EXPECT_EQ(eventGpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                postSyncFound++;
            }
        }
        EXPECT_EQ(1u, postSyncFound);
    }

    template <typename FamilyType>
    void testBodyTimestampEventSignal() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
        using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
        using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
        using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

        auto &rootDeviceEnv = device->getNEODevice()->getRootDeviceEnvironment();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        eventPoolDesc.count = 2;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.wait = 0;
        eventDesc.signal = 0;

        ze_result_t returnValue;
        auto eventPoolTimeStamp = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        auto eventTimeStamp = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolTimeStamp.get(), &eventDesc, device));

        uint64_t eventGpuAddress = eventTimeStamp->getGpuAddress(device);
        uint64_t contextStartAddress = eventGpuAddress + event->getContextStartOffset();
        uint64_t globalStartAddress = eventGpuAddress + event->getGlobalStartOffset();
        uint64_t contextEndAddress = eventGpuAddress + event->getContextEndOffset();
        uint64_t globalEndAddress = eventGpuAddress + event->getGlobalEndOffset();

        ze_event_handle_t eventHandle = eventTimeStamp->toHandle();

        EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
        EXPECT_EQ(2u, commandList->partitionCount);

        LinearStream *cmdListStream = commandList->getCmdContainer().getCommandStream();

        size_t beforeControlSectionOffset = sizeof(MI_STORE_DATA_IMM) +
                                            sizeof(PIPE_CONTROL) +
                                            sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                                            sizeof(MI_BATCH_BUFFER_START);

        size_t bbStartOffset = beforeControlSectionOffset +
                               (2 * sizeof(uint32_t));

        size_t multiTileBarrierSize = bbStartOffset +
                                      sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                                      sizeof(MI_STORE_DATA_IMM) +
                                      sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

        size_t timestampRegisters = 2 * (sizeof(MI_LOAD_REGISTER_REG) + sizeof(MI_LOAD_REGISTER_IMM) +
                                         NEO::EncodeMath<FamilyType>::streamCommandSize + sizeof(MI_STORE_REGISTER_MEM));
        if (NEO::UnitTestHelper<FamilyType>::timestampRegisterHighAddress()) {
            timestampRegisters += 2 * sizeof(MI_STORE_REGISTER_MEM);
        }

        size_t postBarrierSynchronization = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleBarrier() +
                                            NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnv);
        size_t stopRegisters = timestampRegisters + postBarrierSynchronization;

        auto useSizeBefore = cmdListStream->getUsed();
        auto result = commandList->appendBarrier(eventHandle, 0, nullptr, false);
        auto useSizeAfter = cmdListStream->getUsed();
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(2u, eventTimeStamp->getPacketsInUse());

        auto unifiedPostSyncLayout = device->getL0GfxCoreHelper().hasUnifiedPostSyncAllocationLayout();

        size_t totaSizedBarrierWithTimestampEvent = multiTileBarrierSize + timestampRegisters + stopRegisters;
        if (!unifiedPostSyncLayout) {
            totaSizedBarrierWithTimestampEvent += 4 * sizeof(MI_LOAD_REGISTER_IMM);
        }

        EXPECT_EQ(totaSizedBarrierWithTimestampEvent, (useSizeAfter - useSizeBefore));

        void *cmdBuffer = ptrOffset(cmdListStream->getCpuBase(), useSizeBefore);
        GenCmdList cmdList;

        auto registersSizeToParse = timestampRegisters;
        if (!unifiedPostSyncLayout) {
            registersSizeToParse += sizeof(MI_LOAD_REGISTER_IMM);
        }

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          cmdBuffer,
                                                          registersSizeToParse));
        auto begin = cmdList.begin();
        validateTimestampRegisters<FamilyType>(cmdList,
                                               begin,
                                               RegisterOffsets::globalTimestampLdw, globalStartAddress,
                                               RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextStartAddress,
                                               true,
                                               true);

        auto barrierOffset = timestampRegisters;
        if (!unifiedPostSyncLayout) {
            barrierOffset += 2 * sizeof(MI_LOAD_REGISTER_IMM);
        }

        auto gpuBaseAddress = cmdListStream->getGraphicsAllocation()->getGpuAddress() + useSizeBefore + barrierOffset;

        auto gpuCrossTileSyncAddress = gpuBaseAddress +
                                       beforeControlSectionOffset;

        auto gpuFinalSyncAddress = gpuCrossTileSyncAddress +
                                   sizeof(uint32_t);

        auto gpuStartAddress = gpuBaseAddress +
                               bbStartOffset;

        cmdBuffer = ptrOffset(cmdBuffer, barrierOffset);
        size_t parsedOffset = 0;

        validateMultiTileBarrier<FamilyType>(cmdBuffer, parsedOffset, gpuFinalSyncAddress, gpuCrossTileSyncAddress, gpuStartAddress, true, !usePrimaryBuffer);
        EXPECT_EQ(multiTileBarrierSize, parsedOffset);

        cmdBuffer = ptrOffset(cmdBuffer, (parsedOffset + postBarrierSynchronization));
        cmdList.clear();
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          cmdBuffer,
                                                          registersSizeToParse));
        begin = cmdList.begin();
        validateTimestampRegisters<FamilyType>(cmdList,
                                               begin,
                                               RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                               RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                               true,
                                               true);
    }
};

using MultiTileCommandListAppendBarrier = Test<MultiTileCommandListAppendBarrierFixture<false>>;

HWTEST2_F(MultiTileCommandListAppendBarrier, WhenAppendingBarrierThenPipeControlIsGenerated, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    size_t beforeControlSectionOffset = sizeof(MI_STORE_DATA_IMM) +
                                        sizeof(PIPE_CONTROL) +
                                        sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                                        sizeof(MI_BATCH_BUFFER_START);

    size_t startOffset = beforeControlSectionOffset +
                         (2 * sizeof(uint32_t));

    size_t expectedUseBuffer = startOffset +
                               sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                               sizeof(MI_STORE_DATA_IMM) +
                               sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    auto gpuBaseAddress = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getGpuAddress() +
                          usedSpaceBefore;

    auto gpuCrossTileSyncAddress = gpuBaseAddress +
                                   beforeControlSectionOffset;

    auto gpuFinalSyncAddress = gpuCrossTileSyncAddress +
                               sizeof(uint32_t);

    auto gpuStartAddress = gpuBaseAddress +
                           startOffset;

    auto result = commandList->appendBarrier(nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);
    size_t usedBuffer = usedSpaceAfter - usedSpaceBefore;
    EXPECT_EQ(expectedUseBuffer, usedBuffer);

    void *cmdBuffer = ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), usedSpaceBefore);
    size_t parsedOffset = 0;

    validateMultiTileBarrier<FamilyType>(cmdBuffer, parsedOffset, gpuFinalSyncAddress, gpuCrossTileSyncAddress, gpuStartAddress, true, true);

    EXPECT_EQ(expectedUseBuffer, parsedOffset);
}

HWTEST2_F(MultiTileCommandListAppendBarrier,
          GivenCurrentCommandBufferExhaustedWhenAppendingMultiTileBarrierThenPipeControlAndCrossTileSyncIsGeneratedInNewBuffer, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    LinearStream *cmdListStream = commandList->getCmdContainer().getCommandStream();

    size_t beforeControlSectionOffset = sizeof(MI_STORE_DATA_IMM) +
                                        sizeof(PIPE_CONTROL) +
                                        sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                                        sizeof(MI_BATCH_BUFFER_START);

    size_t bbStartOffset = beforeControlSectionOffset +
                           (2 * sizeof(uint32_t));

    size_t expectedUseBuffer = bbStartOffset +
                               sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                               sizeof(MI_STORE_DATA_IMM) +
                               sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    auto firstBatchBufferAllocation = cmdListStream->getGraphicsAllocation();
    auto useSize = cmdListStream->getAvailableSpace();
    useSize -= (sizeof(MI_BATCH_BUFFER_END) +
                sizeof(MI_STORE_DATA_IMM) +
                sizeof(PIPE_CONTROL));
    cmdListStream->getSpace(useSize);

    auto result = commandList->appendBarrier(nullptr, 0, nullptr, false);
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

    validateMultiTileBarrier<FamilyType>(cmdBuffer, parsedOffset, gpuFinalSyncAddress, gpuCrossTileSyncAddress, gpuStartAddress, true, true);

    EXPECT_EQ(expectedUseBuffer, parsedOffset);
}

HWTEST2_F(MultiTileCommandListAppendBarrier,
          GivenNonTimestampEventSignalWhenAppendingMultTileBarrierThenExpectMultiTileBarrierAndPostSyncOperation, IsAtLeastXeCore) {
    testBodyNonTimestampEventSignal<FamilyType>();
}

HWTEST2_F(MultiTileCommandListAppendBarrier,
          GivenTimestampEventSignalWhenAppendingMultTileBarrierThenExpectMultiTileBarrierAndTimestampOperations, IsAtLeastXeCore) {
    testBodyTimestampEventSignal<FamilyType>();
}

using MultiTilePrimaryBatchBufferCommandListAppendBarrier = Test<MultiTileCommandListAppendBarrierFixture<true>>;

HWTEST2_F(MultiTilePrimaryBatchBufferCommandListAppendBarrier,
          GivenNonTimestampEventSignalWhenAppendingMultTileBarrierThenExpectMultiTileBarrierAndPostSyncOperation, IsAtLeastXeCore) {
    testBodyNonTimestampEventSignal<FamilyType>();
}

HWTEST2_F(MultiTilePrimaryBatchBufferCommandListAppendBarrier,
          GivenTimestampEventSignalWhenAppendingMultTileBarrierThenExpectMultiTileBarrierAndTimestampOperations, IsAtLeastXeCore) {
    testBodyTimestampEventSignal<FamilyType>();
}

using MultiTileImmediateCommandListAppendBarrier = Test<MultiTileCommandListFixture<true, false, false, 0>>;

HWTEST2_F(MultiTileImmediateCommandListAppendBarrier,
          givenMultiTileImmediateCommandListWhenAppendingBarrierThenExpectCrossTileSyncAndNoCleanupSection, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    auto immediateCommandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    ASSERT_NE(nullptr, immediateCommandList);
    immediateCommandList->cmdListType = ::L0::CommandList::CommandListType::typeImmediate;
    immediateCommandList->cmdQImmediate = queue.get();
    ze_result_t returnValue = immediateCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, immediateCommandList->partitionCount);

    auto cmdStream = immediateCommandList->getCmdContainer().getCommandStream();

    constexpr size_t sizeBarrierCommands = sizeof(PIPE_CONTROL) +
                                           sizeof(MI_ATOMIC) +
                                           NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                                           sizeof(MI_BATCH_BUFFER_START);

    constexpr size_t expectedSize = sizeBarrierCommands +
                                    2 * sizeof(uint32_t);

    size_t estimatedSize = immediateCommandList->estimateBufferSizeMultiTileBarrier(device->getNEODevice()->getRootDeviceEnvironment());
    size_t usedBeforeSize = cmdStream->getUsed();

    uint64_t startGpuAddress = cmdStream->getGpuBase() + usedBeforeSize;
    uint64_t bbStartGpuAddress = startGpuAddress +
                                 expectedSize;

    uint64_t crossTileSyncGpuAddress = startGpuAddress +
                                       sizeBarrierCommands;

    returnValue = immediateCommandList->appendBarrier(nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    size_t usedAfterSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, estimatedSize);
    EXPECT_EQ(expectedSize, (usedAfterSize - usedBeforeSize));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), usedBeforeSize),
        (usedAfterSize - usedBeforeSize)));

    auto itorSdi = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, itorSdi.size());

    auto pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, pipeControlList.size());
    uint32_t postSyncFound = 0;
    for (auto &it : pipeControlList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            postSyncFound++;
        }
    }
    EXPECT_EQ(0u, postSyncFound);

    auto itorAtomic = find<MI_ATOMIC *>(pipeControlList[0], cmdList.end());
    ASSERT_NE(cmdList.end(), itorAtomic);
    auto cmdAtomic = genCmdCast<MI_ATOMIC *>(*itorAtomic);
    auto miAtomicProgrammedAddress = NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*cmdAtomic);
    EXPECT_EQ(crossTileSyncGpuAddress, miAtomicProgrammedAddress);
    EXPECT_FALSE(cmdAtomic->getReturnDataControl());
    EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, cmdAtomic->getAtomicOpcode());

    auto itorSemaphore = find<MI_SEMAPHORE_WAIT *>(itorAtomic, cmdList.end());
    ASSERT_NE(cmdList.end(), itorSemaphore);
    auto cmdSemaphoreWait = genCmdCast<MI_SEMAPHORE_WAIT *>(*itorSemaphore);
    EXPECT_EQ(crossTileSyncGpuAddress, cmdSemaphoreWait->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, cmdSemaphoreWait->getCompareOperation());
    EXPECT_EQ(2u, cmdSemaphoreWait->getSemaphoreDataDword());

    auto itorBbStart = find<MI_BATCH_BUFFER_START *>(itorSemaphore, cmdList.end());
    ASSERT_NE(cmdList.end(), itorBbStart);
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBbStart);
    EXPECT_EQ(bbStartGpuAddress, cmdBbStart->getBatchBufferStartAddress());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, cmdBbStart->getSecondLevelBatchBuffer());

    auto atomicCounter = reinterpret_cast<uint32_t *>(ptrOffset(cmdBbStart, sizeof(MI_BATCH_BUFFER_START)));
    EXPECT_EQ(0u, *atomicCounter);
    atomicCounter++;
    EXPECT_EQ(0u, *atomicCounter);
    atomicCounter++;

    EXPECT_EQ(ptrOffset(cmdStream->getCpuBase(), usedAfterSize), reinterpret_cast<void *>(atomicCounter));

    void *cmdBuffer = ptrOffset(cmdStream->getCpuBase(), usedBeforeSize);
    size_t parsedOffset = 0;

    validateMultiTileBarrier<FamilyType>(cmdBuffer, parsedOffset, 0, crossTileSyncGpuAddress, bbStartGpuAddress, false, false);
    EXPECT_EQ(expectedSize, parsedOffset);
}

HWTEST2_F(MultiTileImmediateCommandListAppendBarrier,
          givenMultiTileImmediateCommandListUsingFlushTaskWhenAppendingBarrierThenExpectNonSecondaryBufferStart, IsAtLeastXeCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    auto immediateCommandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    ASSERT_NE(nullptr, immediateCommandList);
    immediateCommandList->cmdListType = ::L0::CommandList::CommandListType::typeImmediate;
    immediateCommandList->cmdQImmediate = queue.get();
    ze_result_t returnValue = immediateCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, immediateCommandList->partitionCount);

    auto cmdStream = immediateCommandList->getCmdContainer().getCommandStream();

    size_t usedBeforeSize = cmdStream->getUsed();

    returnValue = immediateCommandList->appendBarrier(nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    size_t usedAfterSize = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), usedBeforeSize),
        (usedAfterSize - usedBeforeSize)));

    auto itorBbStart = find<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBbStart);
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBbStart);
    EXPECT_NE(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, cmdBbStart->getSecondLevelBatchBuffer());
}

} // namespace ult
} // namespace L0
