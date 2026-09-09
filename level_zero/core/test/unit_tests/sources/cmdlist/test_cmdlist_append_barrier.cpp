/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/append_operations.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using CommandListAppendBarrier = Test<CommandListFixture>;

HWTEST_F(CommandListAppendBarrier, WhenAppendingBarrierThenPipeControlIsGenerated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    auto result = commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters);
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
    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    auto result = commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters);
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
    result = commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters);
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
        EXPECT_EQ(gpuCrossTileSyncAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(miSemaphore));
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
        EXPECT_EQ(2u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(miSemaphore));
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
            EXPECT_EQ(gpuFinalSyncAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(miSemaphore));
            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
            EXPECT_EQ(2u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(miSemaphore));
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
            EXPECT_EQ(gpuFinalSyncAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(miSemaphore));
            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, miSemaphore->getCompareOperation());
            EXPECT_EQ(4u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(miSemaphore));
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

        CmdListWaitEventParameters waitEventsParameters = {
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };

        auto useSizeBefore = cmdListStream->getUsed();
        auto result = commandList->appendBarrier(eventHandle, 0, nullptr, waitEventsParameters);
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
        auto eventTimeStamp = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolTimeStamp.get(), &eventDesc, device, returnValue));

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
        CmdListWaitEventParameters waitEventsParameters = {
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };
        auto result = commandList->appendBarrier(eventHandle, 0, nullptr, waitEventsParameters);
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
                                               ContextTimestampRegister<FamilyType>::getRegisterOffsetLow(), contextStartAddress,
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
                                               ContextTimestampRegister<FamilyType>::getRegisterOffsetLow(), contextEndAddress,
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

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    auto result = commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters);
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

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    auto result = commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters);
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

    size_t sizeBarrierCommands = sizeof(PIPE_CONTROL) +
                                 sizeof(MI_ATOMIC) +
                                 NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                                 sizeof(MI_BATCH_BUFFER_START);

    size_t expectedSize = sizeBarrierCommands + 2 * sizeof(uint32_t);

    size_t estimatedSize = immediateCommandList->estimateBufferSizeMultiTileBarrier(device->getNEODevice()->getRootDeviceEnvironment());
    size_t usedBeforeSize = cmdStream->getUsed();

    uint64_t startGpuAddress = cmdStream->getGpuBase() + usedBeforeSize;
    uint64_t bbStartGpuAddress = startGpuAddress +
                                 expectedSize;

    uint64_t crossTileSyncGpuAddress = startGpuAddress +
                                       sizeBarrierCommands;

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    returnValue = immediateCommandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters);
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
    EXPECT_EQ(crossTileSyncGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(cmdSemaphoreWait));
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, cmdSemaphoreWait->getCompareOperation());
    EXPECT_EQ(2u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(cmdSemaphoreWait));

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

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    returnValue = immediateCommandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters);
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

using MultiTilePatchPreambleTest = Test<MultiTileCommandListFixture<false, false, false, 1>>;

HWTEST2_F(MultiTilePatchPreambleTest,
          givenMultiTileQueueWithPatchPreambleWhenAppendingCommandListThenExpectMultiTileBarrier, IsAtLeastXeCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    commandQueue->setPatchingPreamble(true);
    commandList->close();
    auto cmdListHandle = commandList->toHandle();

    auto queueStream = &commandQueue->commandStream;
    auto queueStreamGpuBaseAddress = queueStream->getGpuBase();
    auto queueStreamCpuBaseAddress = queueStream->getCpuBase();

    auto sizeBefore = queueStream->getUsed();
    CommandListExecutionInternalOptions internalOptions = {};
    auto result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = queueStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(queueStream->getCpuBase(), sizeBefore),
        (sizeAfter - sizeBefore)));

    auto itorBbStart = find<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBbStart);

    // size of x-tile atomic counters is 2 * sizeof(uint32_t) and they are programmed right after BB_START
    constexpr size_t xTileJumpOffset = 2 * sizeof(uint32_t);

    // first BB_START is to jump over the x-tile atomic counters
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBbStart);
    size_t bbStartOffset = reinterpret_cast<uintptr_t>(cmdBbStart) - reinterpret_cast<uintptr_t>(queueStreamCpuBaseAddress);
    // right after BB_START command
    uint64_t expectedAtomicCounterAddress = queueStreamGpuBaseAddress + bbStartOffset + sizeof(MI_BATCH_BUFFER_START);
    uint64_t expectedBbStartAddress = expectedAtomicCounterAddress + xTileJumpOffset;

    EXPECT_EQ(expectedBbStartAddress, cmdBbStart->getBatchBufferStartAddress());

    // verify x-tile sync: PIPE_CONTROL, MI_ATOMIC, MI_SEMAPHORE_WAIT
    auto itorPipeControl = find<PIPE_CONTROL *>(cmdList.begin(), itorBbStart);
    ASSERT_NE(itorBbStart, itorPipeControl);

    auto itorAtomic = find<MI_ATOMIC *>(itorPipeControl, itorBbStart);
    ASSERT_NE(itorBbStart, itorAtomic);
    auto cmdAtomic = genCmdCast<MI_ATOMIC *>(*itorAtomic);
    auto miAtomicProgrammedAddress = NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*cmdAtomic);
    EXPECT_EQ(expectedAtomicCounterAddress, miAtomicProgrammedAddress);

    auto itorSemaphore = find<MI_SEMAPHORE_WAIT *>(itorAtomic, itorBbStart);
    ASSERT_NE(itorBbStart, itorSemaphore);
    auto cmdSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*itorSemaphore);
    EXPECT_EQ(expectedAtomicCounterAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(cmdSemaphore));
}

HWTEST2_F(MultiTilePatchPreambleTest,
          givenMultiTileQueueWithPatchPreambleWhenAppendingCommandListWithCounterRequiredThenExpectMultiTileBarrierWithPostSync, IsAtLeastXeCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t counterHostGpuAddress = 0;
    uint64_t *hostAddress = nullptr;
    uint64_t counter = 0;
    NEO::GraphicsAllocation *counterHostAllocation = nullptr;
    uint64_t counterDeviceGpuAddress = 0;
    NEO::GraphicsAllocation *counterDeviceAllocation = nullptr;

    commandQueue->getPatchPreambleFullData(counter, hostAddress, counterHostGpuAddress, counterHostAllocation, counterDeviceGpuAddress, counterDeviceAllocation);

    commandQueue->setPatchingPreamble(true);
    commandList->close();
    auto cmdListHandle = commandList->toHandle();

    auto queueStream = &commandQueue->commandStream;
    auto queueStreamGpuBaseAddress = queueStream->getGpuBase();
    auto queueStreamCpuBaseAddress = queueStream->getCpuBase();

    auto sizeBefore = queueStream->getUsed();
    CommandListExecutionInternalOptions internalOptions = {};
    internalOptions.patchPreambleRequiredCounter = counter;
    auto result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = queueStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(queueStream->getCpuBase(), sizeBefore),
        (sizeAfter - sizeBefore)));

    auto bbStartCmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    ASSERT_TRUE(bbStartCmds.size() > 1);

    // size of x-tile atomic counters is 2 * sizeof(uint32_t) and they are programmed right after BB_START
    constexpr size_t xTileJumpOffset = 2 * sizeof(uint32_t);

    // first BB_START is to jump over the x-tile atomic counters
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartCmds[0]);
    size_t bbStartOffset = reinterpret_cast<uintptr_t>(cmdBbStart) - reinterpret_cast<uintptr_t>(queueStreamCpuBaseAddress);
    // right after BB_START command
    uint64_t expectedAtomicCounterAddress = queueStreamGpuBaseAddress + bbStartOffset + sizeof(MI_BATCH_BUFFER_START);
    uint64_t expectedBbStartAddress = expectedAtomicCounterAddress + xTileJumpOffset;
    EXPECT_EQ(expectedBbStartAddress, cmdBbStart->getBatchBufferStartAddress());

    cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartCmds[1]);
    bbStartOffset = reinterpret_cast<uintptr_t>(cmdBbStart) - reinterpret_cast<uintptr_t>(queueStreamCpuBaseAddress);
    expectedAtomicCounterAddress = queueStreamGpuBaseAddress + bbStartOffset + sizeof(MI_BATCH_BUFFER_START);
    expectedBbStartAddress = expectedAtomicCounterAddress + xTileJumpOffset;
    EXPECT_EQ(expectedBbStartAddress, cmdBbStart->getBatchBufferStartAddress());

    // verify x-tile sync: PIPE_CONTROL, MI_ATOMIC, MI_SEMAPHORE_WAIT
    auto pipeControlCmds = findAll<PIPE_CONTROL *>(cmdList.begin(), bbStartCmds[1]);
    ASSERT_NE(0u, pipeControlCmds.size());

    bool foundHostPostSyncWithCounter = false;
    bool foundDevicePostSyncWithCounter = false;

    for (auto &pipeControlCmd : pipeControlCmds) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlCmd);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            auto actualAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl);
            if (counterHostGpuAddress == actualAddress &&
                pipeControl->getImmediateData() == counter) {
                EXPECT_TRUE(pipeControl->getWorkloadPartitionIdOffsetEnable());
                foundHostPostSyncWithCounter = true;
            }
            if (counterDeviceGpuAddress == actualAddress &&
                pipeControl->getImmediateData() == counter) {
                EXPECT_TRUE(pipeControl->getWorkloadPartitionIdOffsetEnable());
                foundDevicePostSyncWithCounter = true;
            }
            if (foundHostPostSyncWithCounter && foundDevicePostSyncWithCounter) {
                break;
            }
        }
    }

    EXPECT_TRUE(foundHostPostSyncWithCounter);
    EXPECT_TRUE(foundDevicePostSyncWithCounter);
}

HWTEST2_F(MultiTilePatchPreambleTest,
          givenMultiTileQueueWithPatchPreambleWhenAppendingCommandListRequiresWaitThenExpectSemaphoreWaitingForAllTiles,
          IsAtLeastXeCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    bool useSemaphore64bCmd = device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport;
    auto postSyncOffset = commandQueue->csr->getImmWritePostSyncWriteOffset();
    size_t partitionCount = commandQueue->partitionCount;

    constexpr uint64_t dummyTagGpuAddress = 0x12345000;
    constexpr uint32_t dummyTagTaskCount = 0x3;
    MockGraphicsAllocation dummyTagAllocation(nullptr, dummyTagGpuAddress, 0x1000);

    commandQueue->saveWaitForPreamble = true;
    commandQueue->setPatchingPreamble(true);
    commandList->close();
    commandList->saveLatestTagAndTaskCount(&dummyTagAllocation, dummyTagTaskCount);

    auto cmdListHandle = commandList->toHandle();

    auto queueStream = &commandQueue->commandStream;

    auto sizeBefore = queueStream->getUsed();
    CommandListExecutionInternalOptions internalOptions = {};
    auto result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = queueStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(queueStream->getCpuBase(), sizeBefore),
        (sizeAfter - sizeBefore)));

    auto semaphoreList = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_TRUE((partitionCount <= semaphoreList.size()));

    bool lriRequired = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, useSemaphore64bCmd);
    auto semAddress = dummyTagGpuAddress;
    for (uint32_t i = 0; i < partitionCount; i++) {
        auto cmdSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreList[i]);
        EXPECT_EQ(semAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(cmdSemaphore));
        if (lriRequired) {
            EXPECT_EQ(0u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(cmdSemaphore));
        } else {
            EXPECT_EQ(dummyTagTaskCount, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(cmdSemaphore));
        }
        semAddress += postSyncOffset;
    }

    if (lriRequired) {
        bool foundLriForSemaphore = false;
        auto lriList = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), semaphoreList[0]);
        ASSERT_TRUE((2u <= lriList.size()));
        MI_LOAD_REGISTER_IMM *cmdLoadImm = nullptr;
        uint32_t i = 0;
        for (; i < lriList.size(); i++) {
            cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lriList[i]);
            if (cmdLoadImm->getRegisterOffset() == 0x2600) {
                EXPECT_EQ(dummyTagTaskCount, cmdLoadImm->getDataDword());
                foundLriForSemaphore = true;
                break;
            }
        }
        ASSERT_TRUE(foundLriForSemaphore);
        ASSERT_TRUE(((i + 1) < lriList.size()));
        cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lriList[i + 1]);
        EXPECT_EQ(0x2604u, cmdLoadImm->getRegisterOffset());
        EXPECT_EQ(0u, cmdLoadImm->getDataDword());
    }
}

struct OutOfOrderImmediateCmdListBarrierFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();

        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

        commandList.reset(CommandList::createImmediate(device->getHwInfo().platform.eProductFamily, device, &queueDesc,
                                                       false, NEO::EngineGroupType::renderCompute, returnValue));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        ASSERT_FALSE(commandList->isInOrderExecutionEnabled());

        ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = 1;
        eventPool.reset(static_cast<EventPool *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

        eventPoolDesc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        timestampEventPool.reset(static_cast<EventPool *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    }

    void tearDown() {
        timestampEventPool.reset(nullptr);
        eventPool.reset(nullptr);
        commandList.reset(nullptr);
        DeviceFixture::tearDown();
    }

    std::unique_ptr<Event> createEvent(EventPool *pool, ze_event_scope_flags_t signalScope) {
        ze_event_desc_t eventDesc{ZE_STRUCTURE_TYPE_EVENT_DESC};
        eventDesc.index = 0;
        eventDesc.wait = 0;
        eventDesc.signal = signalScope;

        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        return std::unique_ptr<Event>(static_cast<Event *>(getHelper<L0GfxCoreHelper>().createEvent(pool, &eventDesc, device, returnValue)));
    }

    L0::ult::CommandList *getWhiteBoxCmdList() { return CommandList::whiteboxCast(commandList.get()); }

    L0::ult::CommandQueue *getQueue() { return static_cast<L0::ult::CommandQueue *>(getWhiteBoxCmdList()->cmdQImmediate); }

    CmdListWaitEventParameters getWaitEventParameters() {
        return CmdListWaitEventParameters{
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };
    }

    std::unique_ptr<L0::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<EventPool> timestampEventPool;
};

using OutOfOrderImmediateCmdListBarrier = Test<OutOfOrderImmediateCmdListBarrierFixture>;

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenSuccessfulHostSynchronizationWhenAppendingBarrierThenNothingIsDispatchedAndHostScopeSignalEventIsCompletedOnHost) {
    auto event = createEvent(eventPool.get(), ZE_EVENT_SCOPE_FLAG_HOST);
    getWhiteBoxCmdList()->dcFlushSupport = true;
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));

    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto usedBefore = cmdStream->getUsed();
    auto taskCountBefore = getQueue()->getTaskCount();

    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters));

    EXPECT_EQ(usedBefore, cmdStream->getUsed());
    EXPECT_EQ(taskCountBefore, getQueue()->getTaskCount());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus(0));
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenEnqueuesAndConsecutiveBarriersWhenAppendingThenOnlyFirstBarrierAfterNewWorkIsDispatched) {
    auto event = createEvent(eventPool.get(), 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));

    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto usedBefore = cmdStream->getUsed();

    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
    EXPECT_GT(cmdStream->getUsed(), usedBefore);

    auto usedAfterFirstBarrier = cmdStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
    EXPECT_EQ(usedAfterFirstBarrier, cmdStream->getUsed());

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    usedBefore = cmdStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters));
    EXPECT_GT(cmdStream->getUsed(), usedBefore);
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenWaitEventsWhenAppendingRedundantBarrierThenBarrierIsNotSkipped) {
    auto event = createEvent(eventPool.get(), 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));

    auto waitEventHandle = event->toHandle();

    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto usedBefore = cmdStream->getUsed();

    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 1, &waitEventHandle, waitEventsParameters));

    EXPECT_GT(cmdStream->getUsed(), usedBefore);
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenTimestampSignalEventWhenAppendingRedundantBarrierThenBarrierIsNotSkipped) {
    auto timestampEvent = createEvent(timestampEventPool.get(), 0);
    ASSERT_TRUE(timestampEvent->isEventTimestampFlagSet());
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));

    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto usedBefore = cmdStream->getUsed();

    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(timestampEvent->toHandle(), 0, nullptr, waitEventsParameters));

    EXPECT_GT(cmdStream->getUsed(), usedBefore);
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenCopyOnlyListAfterHostSynchronizationWhenAppendingConsecutiveBarriersThenEachUpdatesBarrierTag) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t result = ZE_RESULT_SUCCESS;
    commandList.reset(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::copy, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_FALSE(commandList->isInOrderExecutionEnabled());
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));

    auto csr = commandList->getCsr(false);
    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto waitEventsParameters = getWaitEventParameters();
    for (uint32_t barrier = 0; barrier < 2; barrier++) {
        const auto usedBefore = cmdStream->getUsed();
        const auto barrierCountBefore = csr->peekBarrierCount();
        const auto taskCountBefore = getQueue()->getTaskCount();

        ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
        EXPECT_EQ(barrierCountBefore + 1, csr->peekBarrierCount());
        EXPECT_GT(getQueue()->getTaskCount(), taskCountBefore);

        GenCmdList commands;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), usedBefore), cmdStream->getUsed() - usedBefore));
        bool barrierTagUpdateFound = false;
        for (auto it : findAll<MI_FLUSH_DW *>(commands.begin(), commands.end())) {
            auto flush = genCmdCast<MI_FLUSH_DW *>(*it);
            if (flush->getDestinationAddress() == csr->getBarrierCountGpuAddress()) {
                EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, flush->getPostSyncOperation());
                EXPECT_EQ(barrierCountBefore + 1, flush->getImmediateData());
                barrierTagUpdateFound = true;
            }
        }
        EXPECT_TRUE(barrierTagUpdateFound);
    }
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenAggregatedSignalEventWhenAppendingRedundantBarrierThenEventIsSubmitted) {
    auto event = createEvent(eventPool.get(), 0);
    auto allocation = event->getAllocation(device);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->getInOrderExecEventHelper().assignData(1, 0, 1, 1, allocation, allocation, event->getGpuAddress(device), event->getGpuAddress(device),
                                                  static_cast<uint64_t *>(event->getHostAddress()), 1, 0, false, true);
    ASSERT_TRUE(Event::isAggregatedEvent(event.get()));
    ASSERT_FALSE(event->isCounterBased());
    ASSERT_FALSE(event->isEventTimestampFlagSet());
    ASSERT_FALSE(event->isSignalWithUserInterrupt());
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));

    const auto taskCountBefore = getQueue()->getTaskCount();
    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters));
    EXPECT_GT(getQueue()->getTaskCount(), taskCountBefore);
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->queryStatus(0));
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenAsynchronousBarrierWhenAppendingBarrierWithSignalEventThenSignalEventIsSubmitted) {
    auto event = createEvent(eventPool.get(), 0);
    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto waitEventsParameters = getWaitEventParameters();

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));

    auto usedBefore = cmdStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters));
    EXPECT_GT(cmdStream->getUsed(), usedBefore);
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenCounterBasedSignalEventWhenAppendingRedundantBarrierThenInvalidArgumentIsStillReturned) {
    auto event = createEvent(eventPool.get(), 0);
    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    ASSERT_TRUE(event->isCounterBased());
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));

    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters));
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenEmptyListWhenAppendingBarriersAndHostSynchronizingThenNothingIsSubmittedOrWaitedOn) {
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    const auto usedBefore = cmdStream->getUsed();
    const auto taskCountBefore = getQueue()->getTaskCount();
    const auto waitsBefore = csr->waitForCompletionWithTimeoutTaskCountCalled.load();
    const auto clientsBefore = csr->getNumClients();

    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(usedBefore, cmdStream->getUsed());
    EXPECT_EQ(taskCountBefore, getQueue()->getTaskCount());
    EXPECT_EQ(waitsBefore, csr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(clientsBefore, csr->getNumClients());
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenEmptyListWhenAppendingBarrierWithSignalEventThenEventIsCompletedOnHost) {
    auto event = createEvent(eventPool.get(), ZE_EVENT_SCOPE_FLAG_HOST);
    getWhiteBoxCmdList()->dcFlushSupport = true;
    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    const auto usedBefore = cmdStream->getUsed();
    const auto taskCountBefore = getQueue()->getTaskCount();
    auto waitEventsParameters = getWaitEventParameters();

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters));
    EXPECT_EQ(usedBefore, cmdStream->getUsed());
    EXPECT_EQ(taskCountBefore, getQueue()->getTaskCount());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus(0));
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenSuccessfulHostSynchronizeWhenSynchronizingAgainAndAppendingBarrierThenBothAreNoOps) {
    auto event = createEvent(eventPool.get(), 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));

    auto waitsBefore = csr->waitForCompletionWithTimeoutTaskCountCalled.load();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(waitsBefore + 1, csr->waitForCompletionWithTimeoutTaskCountCalled.load());

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(waitsBefore + 1, csr->waitForCompletionWithTimeoutTaskCountCalled.load());

    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto usedBefore = cmdStream->getUsed();
    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
    EXPECT_EQ(usedBefore, cmdStream->getUsed());
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenSubmittedBarrierWhenHostSynchronizingThenBarrierSubmissionIsStillWaitedOn) {
    auto event = createEvent(eventPool.get(), 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));

    auto waitEventsParameters = getWaitEventParameters();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));

    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    auto waitsBefore = csr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(waitsBefore + 1, csr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenUserInterruptSignalEventWhenAppendingRedundantBarrierThenEventIsSubmitted) {
    auto event = createEvent(eventPool.get(), 0);
    event->setSignalWithUserInterrupt(true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));

    const auto taskCountBefore = getQueue()->getTaskCount();
    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters));
    EXPECT_GT(getQueue()->getTaskCount(), taskCountBefore);
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenSubmissionDuringHostSynchronizeWhenSynchronizingAgainThenNewWorkIsStillWaitedOn) {
    auto event = createEvent(eventPool.get(), 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    csr->callBaseWaitForCompletionWithTimeout = false;
    csr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;

    const auto waitedTaskCount = getQueue()->getTaskCount();
    csr->onWaitForCompletionWithTimeout = [&] {
        EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    csr->onWaitForCompletionWithTimeout = nullptr;
    EXPECT_EQ(waitedTaskCount, csr->latestWaitForCompletionWithTimeoutTaskCount.load());
    ASSERT_GT(getQueue()->getTaskCount(), waitedTaskCount);

    csr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::notReady;
    const auto waitsBefore = csr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_NOT_READY, commandList->hostSynchronize(0));
    EXPECT_EQ(waitsBefore + 1, csr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(getQueue()->getTaskCount(), csr->latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenSubmissionDuringPostWaitOperationsWhenAppendingBarrierThenBarrierAndSignalEventAreSubmitted) {
    auto event = createEvent(eventPool.get(), 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    csr->callBaseWaitForCompletionWithTimeout = false;
    csr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;

    getWhiteBoxCmdList()->isTbxMode = true;
    csr->onDownloadAllocations = [&] {
        EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    csr->onDownloadAllocations = nullptr;
    getWhiteBoxCmdList()->isTbxMode = false;

    const auto taskCountBefore = getQueue()->getTaskCount();
    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
    EXPECT_GT(getQueue()->getTaskCount(), taskCountBefore);

    const auto barrierTaskCount = getQueue()->getTaskCount();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters));
    EXPECT_GT(getQueue()->getTaskCount(), barrierTaskCount);
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->queryStatus(0));
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenFailedHostSynchronizeWhenSynchronizingAgainAndAppendingBarrierThenNeitherIsSkipped) {
    auto event = createEvent(eventPool.get(), 0);
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    csr->callBaseWaitForCompletionWithTimeout = false;

    for (auto waitStatus : {NEO::WaitStatus::notReady, NEO::WaitStatus::gpuHang}) {
        ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
        csr->returnWaitForCompletionWithTimeout = waitStatus;
        const auto expectedResult = waitStatus == NEO::WaitStatus::notReady ? ZE_RESULT_NOT_READY : ZE_RESULT_ERROR_DEVICE_LOST;
        EXPECT_EQ(expectedResult, commandList->hostSynchronize(0));

        const auto waitsBefore = csr->waitForCompletionWithTimeoutTaskCountCalled.load();
        EXPECT_EQ(expectedResult, commandList->hostSynchronize(0));
        EXPECT_EQ(waitsBefore + 1, csr->waitForCompletionWithTimeoutTaskCountCalled.load());

        const auto taskCountBefore = getQueue()->getTaskCount();
        auto waitEventsParameters = getWaitEventParameters();
        EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
        EXPECT_GT(getQueue()->getTaskCount(), taskCountBefore);
    }
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenInternalHostSynchronizeWhenSynchronizingWithPostWaitOperationsThenCleanupIsNotSkipped) {
    auto event = createEvent(eventPool.get(), 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));
    auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(commandList.get());
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    const auto clientsBefore = csr->getNumClients();

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->hostSynchronize(0, false));
    EXPECT_EQ(clientsBefore, csr->getNumClients());
    const auto taskCountBefore = getQueue()->getTaskCount();
    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
    EXPECT_EQ(taskCountBefore, getQueue()->getTaskCount());

    const auto waitsBefore = csr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(waitsBefore + 1, csr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(clientsBefore - 1, csr->getNumClients());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(waitsBefore + 1, csr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

HWTEST_F(OutOfOrderImmediateCmdListBarrier, givenSynchronousSubmissionWhenAppendingBarrierAndHostSynchronizingThenBothAreNoOps) {
    getWhiteBoxCmdList()->isSyncModeQueue = true;
    auto event = createEvent(eventPool.get(), 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendEventReset(event->toHandle()));

    const auto taskCountBefore = getQueue()->getTaskCount();
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    const auto waitsBefore = csr->waitForCompletionWithTimeoutTaskCountCalled.load();
    auto waitEventsParameters = getWaitEventParameters();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters));
    EXPECT_EQ(taskCountBefore, getQueue()->getTaskCount());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(waitsBefore, csr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

} // namespace ult
} // namespace L0
