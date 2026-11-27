/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/gmm_callbacks.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/kmd_notify_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/pool_allocators.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_allocations_list.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_host_function_allocator.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_kmd_notify_helper.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_scratch_space_controller_xehp_and_later.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/heapless_matchers.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/command_stream/command_stream_receiver_tests.h"
#include "shared/test/unit_test/direct_submission/direct_submission_controller_mock.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "gtest/gtest.h"

#include <chrono>
#include <functional>
#include <limits>
#include <vector>

using namespace NEO;

HWTEST_F(CommandStreamReceiverHwTest, givenContextInitializedAndRequiresInstructionCacheFlushWhenFlushImmediateThenInstructionCacheInvalidateEnableIsSentBeforeBbStartJumpingToImmediateBuffer) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    // 1st flush, dispatch all required n-p state commands
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    auto usedAfterFirstFlush = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.registerInstructionCacheFlush();

    // only instruction cache flush is dispatched in dynamic preamble
    auto immediateBufferStartOffset = commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, immediateBufferStartOffset, immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedAfterFirstFlush);

    auto cmdPtrIt = hwParserCsr.cmdList.begin();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdPtrIt);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdPtrIt);
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_TRUE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(commandStreamReceiver.requiresInstructionCacheFlush);

    cmdPtrIt++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdPtrIt);

    auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmdPtrIt);
    ASSERT_NE(nullptr, bbStart);

    EXPECT_EQ(cmdBufferGpuAddress + immediateBufferStartOffset, bbStart->getBatchBufferStartAddress());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenFlushIsBlockingWhenFlushTaskCalledThenExpectMonitorFenceFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    commandStreamReceiver.taskCount = 5;
    flushTaskFlags.blocking = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_EQ(6u, commandStreamReceiver.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenFlushIsDcFlushWhenFlushTaskCalledThenExpectMonitorFenceFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    commandStreamReceiver.taskCount = 11;
    flushTaskFlags.dcFlush = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_EQ(12u, commandStreamReceiver.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenFlushGuardBufferWithPipeControlWhenFlushTaskCalledThenExpectMonitorFenceFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    commandStreamReceiver.taskCount = 17;
    flushTaskFlags.guardCommandBufferWithPipeControl = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_EQ(18u, commandStreamReceiver.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverHwTest, givenUcResourceRequiresTagUpdateWhenFlushTaskThenFlushTagUpdate) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.ucResourceRequiresTagUpdate = true;
    auto offset = commandStream.getUsed();

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(commandStream.getCpuBase(), offset), (commandStream.getUsed() - offset)));

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment())) {
        itorPipeControl++;
    }
    EXPECT_NE(itorPipeControl, cmdList.end());
    auto cmd = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);
    EXPECT_FALSE(cmd->getDcFlushEnable());
    EXPECT_EQ(cmd->getPostSyncOperation(), FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    EXPECT_EQ(cmd->getImmediateData(), commandStreamReceiver.peekTaskCount());

    uint64_t address = cmd->getAddress();
    auto csrAddress = commandStreamReceiver.getUcTagGPUAddress();
    EXPECT_EQ(csrAddress, address);
}

HWTEST_F(CommandStreamReceiverHwTest, givenUcResourceRequiresTagUpdateWhenFlushBcsTaskWithBlitPropertiesThenMiFlushDwProgrammedWithUcTagAddress) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}));
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(mockOsContext);
    commandStreamReceiver.initializeTagAllocation();
    commandStreamReceiver.ucResourceRequiresTagUpdate = true;

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          commandStreamReceiver, commandStreamReceiver.getTagAllocation(), nullptr,
                                                                          commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(),
                                                                          commandStreamReceiver.getTagAllocation()->getGpuAddress(), 0,
                                                                          0, 0, 0, 0, 0, 0, 0);

    BlitPropertiesContainer container;
    container.push_back(blitProperties);
    commandStreamReceiver.flushBcsTask(container, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto ucTagAddress = commandStreamReceiver.getUcTagGPUAddress();
    auto cmdIterator = find<MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    bool ucTagAddressFound = false;
    while (cmdIterator != hwParser.cmdList.end()) {
        auto flush = genCmdCast<MI_FLUSH_DW *>(*cmdIterator);
        if (flush->getDestinationAddress() == ucTagAddress) {
            ucTagAddressFound = true;
            break;
        }
        cmdIterator = find<MI_FLUSH_DW *>(++cmdIterator, hwParser.cmdList.end());
    }
    EXPECT_TRUE(ucTagAddressFound);
    EXPECT_FALSE(commandStreamReceiver.ucResourceRequiresTagUpdate);
}

HWTEST_F(CommandStreamReceiverHwTest, givenUcResourceRequiresTagUpdateWhenFlushBcsTaskWithCommandStreamThenMiFlushDwProgrammedWithUcTagAddress) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}));
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(mockOsContext);
    commandStreamReceiver.initializeTagAllocation();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);

    // first flush to initialize
    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    auto offset = commandStream.getUsed();
    commandStreamReceiver.ucResourceRequiresTagUpdate = true;

    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, offset);

    auto ucTagAddress = commandStreamReceiver.getUcTagGPUAddress();
    auto cmdIterator = find<MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    bool ucTagAddressFound = false;
    while (cmdIterator != hwParser.cmdList.end()) {
        auto flush = genCmdCast<MI_FLUSH_DW *>(*cmdIterator);
        if (flush->getDestinationAddress() == ucTagAddress) {
            ucTagAddressFound = true;
            break;
        }
        cmdIterator = find<MI_FLUSH_DW *>(++cmdIterator, hwParser.cmdList.end());
    }
    EXPECT_TRUE(ucTagAddressFound);
    EXPECT_FALSE(commandStreamReceiver.ucResourceRequiresTagUpdate);
}

HWTEST_F(CommandStreamReceiverHwTest, GivenFlushHeapStorageRequiresRecyclingTagWhenFlushTaskCalledThenExpectMonitorFenceFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    commandStreamReceiver.taskCount = 23;
    commandStreamReceiver.heapStorageRequiresRecyclingTag = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_EQ(24u, commandStreamReceiver.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverHwTest, givenPendingSubmissionRequestsPeekedWhenRestoredThenConsumedRequestsArePendingAgainAndNewRequestsAreKept) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.ucResourceRequiresTagUpdate = true;
    commandStreamReceiver.heapStorageRequiresRecyclingTag = false;
    commandStreamReceiver.isWalkerWithProfilingEnqueued = true;
    const auto pendingSubmissionRequests = commandStreamReceiver.peekPendingSubmissionRequests();

    commandStreamReceiver.ucResourceRequiresTagUpdate = false;
    commandStreamReceiver.heapStorageRequiresRecyclingTag = true;
    commandStreamReceiver.isWalkerWithProfilingEnqueued = false;
    commandStreamReceiver.restorePendingSubmissionRequests(pendingSubmissionRequests);

    EXPECT_TRUE(commandStreamReceiver.ucResourceRequiresTagUpdate);
    EXPECT_TRUE(commandStreamReceiver.heapStorageRequiresRecyclingTag);
    EXPECT_TRUE(commandStreamReceiver.isWalkerWithProfilingEnqueued);
}

HWTEST_F(CommandStreamReceiverHwTest, givenEpilogueStreamAvailableWhenFlushTaskCalledThenDispachEpilogueCommandsIntoEpilogueStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    GraphicsAllocation *commandBuffer = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream epilogueStream(commandBuffer);

    commandStreamReceiver.storeMakeResidentAllocations = true;
    flushTaskFlags.guardCommandBufferWithPipeControl = true;
    flushTaskFlags.optionalEpilogueCmdStream = &epilogueStream;

    commandStream.getSpace(4);

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandBuffer));

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(epilogueStream, 0);
    auto cmdIterator = find<typename FamilyType::PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST_F(CommandStreamReceiverHwTest, givenUcResourceRequiresTagUpdateAndFullTaskStreamWithEpilogueStreamWhenFlushTaskCalledThenTagUpdateIsDispatchedIntoEpilogueStream) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    GraphicsAllocation *commandBuffer = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream epilogueStream(commandBuffer);

    flushTaskFlags.optionalEpilogueCmdStream = &epilogueStream;
    commandStreamReceiver.ucResourceRequiresTagUpdate = true;

    // a regular command list dispatched as the primary batch buffer fills the task stream
    commandStream.getSpace(commandStream.getAvailableSpace());

    EXPECT_NO_THROW(commandStreamReceiver.flushTask(commandStream,
                                                    0,
                                                    &dsh,
                                                    &ioh,
                                                    nullptr,
                                                    taskLevel,
                                                    flushTaskFlags,
                                                    *pDevice));
    EXPECT_EQ(commandStream.getMaxAvailableSpace(), commandStream.getUsed());
    EXPECT_FALSE(commandStreamReceiver.ucResourceRequiresTagUpdate);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(epilogueStream, 0);
    auto ucTagAddress = commandStreamReceiver.getUcTagGPUAddress();
    bool ucTagUpdateFound = false;
    for (auto &cmd : findAll<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end())) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*cmd);
        uint64_t address = pipeControl->getAddress();
        ucTagUpdateFound |= (address == ucTagAddress);
    }
    EXPECT_TRUE(ucTagUpdateFound);

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenSpecialPipelineSelectModeChangedWhenGetCmdSizeForPielineSelectIsCalledThenCorrectSizeIsReturned, IsAtMostXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    csrSizeRequest.systolicPipelineSelectMode = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    size_t size = commandStreamReceiver.getCmdSizeForPipelineSelect();

    size_t expectedSize = sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<FamilyType>::isBarrierPriorToPipelineSelectWaRequired(pDevice->getRootDeviceEnvironment())) {
        expectedSize += sizeof(PIPE_CONTROL);
    }
    EXPECT_EQ(expectedSize, size);
}

HWTEST_F(CommandStreamReceiverHwTest, givenPreambleSentWhenEstimatingFlushTaskSizeThenResultDependsOnAdditionalCmdsSize) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.isPreambleSent = false;
    auto preambleNotSentPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    auto preambleNotSentFlush = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    commandStreamReceiver.isPreambleSent = true;
    auto preambleSentPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    auto preambleSentFlush = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    auto actualDifferenceForPreamble = preambleNotSentPreamble - preambleSentPreamble;
    auto actualDifferenceForFlush = preambleNotSentFlush - preambleSentFlush;

    commandStreamReceiver.isPreambleSent = false;
    auto expectedDifferenceForPreamble = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*pDevice);
    auto expectedDifferenceForFlush = expectedDifferenceForPreamble + PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedDifferenceForPreamble, actualDifferenceForPreamble);
    EXPECT_EQ(expectedDifferenceForFlush, actualDifferenceForFlush);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenStaticPartitionEnabledWhenOnlySinglePartitionUsedThenExpectSinglePipeControlAsBarrier, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char[]> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);

    commandStreamReceiver.staticWorkPartitioningEnabled = true;
    commandStreamReceiver.activePartitions = 1;

    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(sizeof(PIPE_CONTROL), estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

HWTEST2_F(CommandStreamReceiverHwTest, givenStaticPartitionDisabledWhenMultiplePartitionsUsedThenExpectSinglePipeControlAsBarrier, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char[]> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);

    commandStreamReceiver.staticWorkPartitioningEnabled = false;
    commandStreamReceiver.activePartitions = 2;

    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(sizeof(PIPE_CONTROL), estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

HWTEST2_F(CommandStreamReceiverHwTest, givenStaticPartitionEnabledWhenMultiplePartitionsUsedThenExpectImplicitScalingWithoutSelfCleanupBarrier, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char[]> buffer(new char[cmdSize]);
    MockGraphicsAllocation allocation(buffer.get(), cmdSize);
    allocation.gpuAddress = 0xFF000;
    LinearStream cs(buffer.get(), cmdSize);
    cs.replaceGraphicsAllocation(&allocation);

    commandStreamReceiver.staticWorkPartitioningEnabled = true;
    commandStreamReceiver.activePartitions = 2;

    size_t expectedSize = sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          2 * sizeof(uint32_t);
    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(expectedSize, estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    void *cmdBuffer = buffer.get();
    size_t offset = 0;

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(cmdBuffer);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());

    offset += sizeof(PIPE_CONTROL);

    MI_ATOMIC *miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miAtomic);
    offset += sizeof(MI_ATOMIC);

    MI_SEMAPHORE_WAIT *miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miSemaphore);
    offset += NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    MI_BATCH_BUFFER_START *bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, bbStart);
    offset += sizeof(MI_BATCH_BUFFER_START);

    uint32_t *data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    EXPECT_EQ(estimatedCmdSize, offset);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenSingleTileWhenProgrammingPostSyncBarrierThenExpectPipeControlWithCorrectFlags, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.activePartitions = 1;

    size_t estimatedCmdSize = ultCsr.getCmdSizeForStallingPostSyncCommands();

    char commandBuffer[MemoryConstants::pageSize];
    LinearStream commandStream(commandBuffer, MemoryConstants::pageSize);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();

    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, false);
    size_t sizeUsed = commandStream.getUsed();
    ASSERT_EQ(estimatedCmdSize, sizeUsed);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandStream.getCpuBase(),
        sizeUsed));
    auto pipeControlIteratorVector = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, pipeControlIteratorVector.size());
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIteratorVector[0]);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

HWTEST2_F(CommandStreamReceiverHwTest, givenImplicitScalingEnabledWhenProgrammingPostSyncBarrierThenExpectPipeControlWithCorrectFlags, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.activePartitions = 2;
    ultCsr.staticWorkPartitioningEnabled = true;

    size_t barrierWithPostSyncOperationSize = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(pDevice->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);
    size_t expectedSize = barrierWithPostSyncOperationSize +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          2 * sizeof(uint32_t);
    size_t estimatedCmdSize = ultCsr.getCmdSizeForStallingPostSyncCommands();
    EXPECT_EQ(expectedSize, estimatedCmdSize);

    char commandBuffer[MemoryConstants::pageSize];
    MockGraphicsAllocation mockCmdBufferAllocation(commandBuffer, 0x4000, MemoryConstants::pageSize);
    LinearStream commandStream(&mockCmdBufferAllocation);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();

    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, false);

    EXPECT_EQ(estimatedCmdSize, commandStream.getUsed());

    void *cmdBuffer = reinterpret_cast<void *>(commandBuffer);
    size_t offset = 0;

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(cmdBuffer);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());

    offset += barrierWithPostSyncOperationSize;

    MI_ATOMIC *miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miAtomic);
    offset += sizeof(MI_ATOMIC);

    MI_SEMAPHORE_WAIT *miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miSemaphore);
    offset += NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    MI_BATCH_BUFFER_START *bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, bbStart);
    offset += sizeof(MI_BATCH_BUFFER_START);

    uint32_t *data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    EXPECT_EQ(estimatedCmdSize, offset);
}

HWTEST_F(CommandStreamReceiverHwTest, givenForcePipeControlPriorToWalkerWhenAddPipeControlFlushTaskIfNeededThenStallingPcIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForcePipeControlPriorToWalker.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    csr.addPipeControlFlushTaskIfNeeded(commandStream, 0);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             commandStream.getCpuBase(),
                                             commandStream.getUsed());

    auto itorCmd = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itorCmd);

    auto pc = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    EXPECT_TRUE(pc->getCommandStreamerStallEnable());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenGetAcLineConnectedCalledThenCorrectValueIsReturned) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockKmdNotifyHelper = new MockKmdNotifyHelper(&pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr.resetKmdNotifyHelper(mockKmdNotifyHelper);

    EXPECT_EQ(1u, mockKmdNotifyHelper->updateAcLineStatusCalled);

    csr.getAcLineConnected(false);
    EXPECT_EQ(1u, mockKmdNotifyHelper->updateAcLineStatusCalled);

    csr.getAcLineConnected(true);
    EXPECT_EQ(2u, mockKmdNotifyHelper->updateAcLineStatusCalled);

    mockKmdNotifyHelper->acLineConnected.store(false);
    EXPECT_FALSE(csr.getAcLineConnected(false));

    mockKmdNotifyHelper->acLineConnected.store(true);
    EXPECT_TRUE(csr.getAcLineConnected(false));
}

HWTEST_F(CommandStreamReceiverTest, givenBcsCsrWhenInitializeDeviceWithFirstSubmissionIsCalledThenSuccessIsReturned) {
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}));
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(mockOsContext);
    commandStreamReceiver.initializeTagAllocation();

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenInitializeDeviceWithFirstSubmissionIsCalledThenFlushOnlyForTheFirstTime) {
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({defaultHwInfo->capabilityTable.defaultEngineType, EngineUsage::regular}));
    pDevice->setPreemptionMode(PreemptionMode::Disabled);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(mockOsContext);
    commandStreamReceiver.initializeTagAllocation();
    EXPECT_EQ(0u, commandStreamReceiver.taskCount);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);
    EXPECT_EQ(0u, commandStreamReceiver.waitForTaskCountWithKmdNotifyFallbackCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenTbxCsrWhenInitializingThenWaitForCompletion) {
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({defaultHwInfo->capabilityTable.defaultEngineType, EngineUsage::regular}));
    pDevice->setPreemptionMode(PreemptionMode::Disabled);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    commandStreamReceiver.setupContext(mockOsContext);
    commandStreamReceiver.initializeTagAllocation();

    EXPECT_EQ(0u, commandStreamReceiver.taskCount);
    EXPECT_EQ(0u, commandStreamReceiver.waitForCompletionWithTimeoutTaskCountCalled);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);
    EXPECT_EQ(1u, commandStreamReceiver.waitForCompletionWithTimeoutTaskCountCalled);
    EXPECT_TRUE(commandStreamReceiver.latestWaitForCompletionWithTimeoutWaitParams.skipTbxDownload);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);
    EXPECT_EQ(1u, commandStreamReceiver.waitForCompletionWithTimeoutTaskCountCalled);

    MockCsrHw<FamilyType> failingCommandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    failingCommandStreamReceiver.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    failingCommandStreamReceiver.setupContext(mockOsContext);
    failingCommandStreamReceiver.initializeTagAllocation();
    failingCommandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, failingCommandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(0u, failingCommandStreamReceiver.waitForTaskCountWithKmdNotifyFallbackCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenGetUmdPowerHintValueCalledThenCorrectValueIsReturned) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(nullptr, commandStreamReceiver.osContext);
    EXPECT_EQ(0u, commandStreamReceiver.getUmdPowerHintValue());

    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({defaultHwInfo->capabilityTable.defaultEngineType, EngineUsage::regular}));
    commandStreamReceiver.setupContext(mockOsContext);
    mockOsContext.setUmdPowerHintValue(1u);
    EXPECT_EQ(1u, commandStreamReceiver.getUmdPowerHintValue());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenMakeResidentCalledThenUpdateTaskCountIfObjectIsAlwaysResident) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto contextId = csr.getOsContext().getContextId();
    MockGraphicsAllocation graphicsAllocation;

    csr.makeResident(graphicsAllocation);
    auto initialAllocTaskCount = graphicsAllocation.getTaskCount(contextId);
    EXPECT_EQ(initialAllocTaskCount, csr.peekTaskCount() + 1);

    graphicsAllocation.updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, contextId);
    csr.taskCount = 10;

    csr.makeResident(graphicsAllocation);
    auto updatedTaskCount = graphicsAllocation.getTaskCount(contextId);
    EXPECT_EQ(updatedTaskCount, csr.peekTaskCount() + 1);
    EXPECT_NE(updatedTaskCount, initialAllocTaskCount);
}

using CommandStreamReceiverHwHeaplessTest = Test<DeviceFixture>;

HWTEST_F(CommandStreamReceiverHwHeaplessTest, whenHeaplessCommandStreamReceiverFunctionsAreCalledThenExceptionIsThrown) {
    std::unique_ptr<UltCommandStreamReceiver<FamilyType>> csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*pDevice->executionEnvironment, rootDeviceIndex, pDevice->getDeviceBitfield());

    LinearStream commandStream(0, 0);

    EXPECT_ANY_THROW(csr->flushTaskHeapless(commandStream, 0, nullptr, nullptr, nullptr, 0, csr->recordedDispatchFlags, *pDevice));
    EXPECT_ANY_THROW(csr->programHeaplessProlog(*pDevice));
    EXPECT_ANY_THROW(csr->programComputeModeHeapless(*pDevice, commandStream));
    EXPECT_ANY_THROW(csr->getCmdSizeForHeaplessPrologue(*pDevice));
    EXPECT_ANY_THROW(csr->handleAllocationsResidencyForHeaplessProlog(commandStream, *pDevice));
    EXPECT_ANY_THROW(csr->programHeaplessStateProlog(*pDevice, commandStream));
    EXPECT_ANY_THROW(csr->isHeaplessStatePrologRequiredForContext());
    EXPECT_ANY_THROW(csr->handleAllocationsResidencyForFlushTaskHeapless(nullptr, nullptr, nullptr, *pDevice));
    EXPECT_ANY_THROW(csr->getRequiredCmdStreamHeaplessSize(csr->recordedDispatchFlags, *pDevice));
    EXPECT_ANY_THROW(csr->getRequiredCmdStreamHeaplessSizeAligned(csr->recordedDispatchFlags, *pDevice));
    EXPECT_ANY_THROW(csr->flushImmediateTaskHeapless(commandStream, 0, csr->recordedImmediateDispatchFlags, *pDevice));
    EXPECT_ANY_THROW(csr->handleImmediateFlushStatelessAllocationsResidency(0, commandStream, *pDevice));

    EXPECT_FALSE(csr->heaplessPrologProgrammed);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskInHeaplessModeWhenNextDispatchRequiresScratchSpaceThenNoScratchIsAllocated,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.heaplessModeEnabled = true;

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    commandStreamReceiver.setRequiredScratchSizes(0x100, 0);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_EQ(nullptr, frontEndCmd);

    EXPECT_EQ(nullptr, commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation());
}

using CommandStreamReceiverContextGroupTest = ::testing::Test;

HWTEST_F(CommandStreamReceiverContextGroupTest, givenSecondaryCsrWhenGettingInternalAllocationsThenAllocationFromPrimnaryCsrAreReturned) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);
    debugManager.flags.OverrideLatePreemptionStart.set(1);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();

    const auto ccsIndex = 0;
    auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];
    auto secondaryEnginesCount = secondaryEngines.engines.size();
    ASSERT_EQ(5u, secondaryEnginesCount);

    EXPECT_TRUE(secondaryEngines.engines[0].commandStreamReceiver->isInitialized());

    auto primaryCsr = secondaryEngines.engines[0].commandStreamReceiver;
    primaryCsr->createGlobalStatelessHeap();

    for (uint32_t secondaryIndex = 1; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, std::nullopt);
    }

    for (uint32_t i = 0; i < secondaryEngines.highPriorityEnginesTotal; i++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, std::nullopt);
    }

    for (uint32_t secondaryIndex = 0; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {

        if (secondaryIndex > 0) {
            EXPECT_NE(primaryCsr->getTagAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getTagAllocation());
        }

        if (gfxCoreHelper.isFenceAllocationRequired(hwInfo, device->getRootDeviceEnvironment().getHelper<ProductHelper>())) {
            EXPECT_EQ(primaryCsr->getGlobalFenceAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalFenceAllocation());
        }

        EXPECT_EQ(primaryCsr->getPreemptionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPreemptionAllocation());
        EXPECT_EQ(primaryCsr->getGlobalStatelessHeapAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalStatelessHeapAllocation());
        EXPECT_EQ(primaryCsr->getGlobalStatelessHeap(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalStatelessHeap());
        EXPECT_EQ(primaryCsr->getPrimaryScratchSpaceController(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPrimaryScratchSpaceController());
        EXPECT_EQ(primaryCsr->getPreemptionMode(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPreemptionMode());
        EXPECT_EQ(primaryCsr->getWorkPartitionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(primaryCsr->getSkipPreemptionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getSkipPreemptionAllocation());
    }
}

HWTEST_F(CommandStreamReceiverContextGroupTest, givenSecondaryCsrWhenCreatingGlobalStatelessHeapThenHeapIsAllocatedOnlyOnPrimaryCsr) {
    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    const auto engineType = EngineHelpers::mapCcsIndexToEngineType(0);
    auto &secondaryEngines = device->secondaryEngines[engineType];
    ASSERT_EQ(5u, secondaryEngines.engines.size());

    auto primaryCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(secondaryEngines.engines[0].commandStreamReceiver);
    ASSERT_EQ(nullptr, primaryCsr->globalStatelessHeapAllocation);

    auto secondaryCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(secondaryEngines.engines[1].commandStreamReceiver);
    ASSERT_NE(nullptr, secondaryCsr->getPrimaryCsr());

    secondaryCsr->createGlobalStatelessHeap();

    EXPECT_NE(nullptr, primaryCsr->globalStatelessHeapAllocation);
    EXPECT_EQ(nullptr, secondaryCsr->globalStatelessHeapAllocation);
    EXPECT_EQ(primaryCsr->getGlobalStatelessHeap(), secondaryCsr->getGlobalStatelessHeap());
    EXPECT_EQ(primaryCsr->getGlobalStatelessHeapAllocation(), secondaryCsr->getGlobalStatelessHeapAllocation());
}

HWTEST_F(CommandStreamReceiverContextGroupTest, givenSecondaryRootCsrWhenGettingInternalAllocationsThenAllocationFromPrimnaryCsrAreReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);
    debugManager.flags.OverrideLatePreemptionStart.set(1);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    UltDeviceFactory deviceFactory{1, 2};
    auto device = deviceFactory.rootDevices[0];
    const auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();

    const auto ccsIndex = 0;
    auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];
    auto secondaryEnginesCount = secondaryEngines.engines.size();
    ASSERT_EQ(5u, secondaryEnginesCount);

    EXPECT_TRUE(secondaryEngines.engines[0].commandStreamReceiver->isInitialized());

    auto primaryCsr = secondaryEngines.engines[0].commandStreamReceiver;
    primaryCsr->createGlobalStatelessHeap();

    for (uint32_t secondaryIndex = 1; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, std::nullopt);
    }

    for (uint32_t i = 0; i < secondaryEngines.highPriorityEnginesTotal; i++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, std::nullopt);
    }

    for (uint32_t secondaryIndex = 0; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {

        if (secondaryIndex > 0) {
            EXPECT_NE(primaryCsr->getTagAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getTagAllocation());
        }

        if (gfxCoreHelper.isFenceAllocationRequired(hwInfo, device->getRootDeviceEnvironment().getHelper<ProductHelper>())) {
            EXPECT_EQ(primaryCsr->getGlobalFenceAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalFenceAllocation());
        }

        EXPECT_EQ(primaryCsr->getPreemptionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPreemptionAllocation());
        EXPECT_EQ(primaryCsr->getGlobalStatelessHeapAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalStatelessHeapAllocation());
        EXPECT_EQ(primaryCsr->getGlobalStatelessHeap(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalStatelessHeap());
        EXPECT_EQ(primaryCsr->getPrimaryScratchSpaceController(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPrimaryScratchSpaceController());
        EXPECT_EQ(primaryCsr->getPreemptionMode(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPreemptionMode());
        EXPECT_EQ(primaryCsr->getWorkPartitionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(primaryCsr->getSkipPreemptionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getSkipPreemptionAllocation());
    }
}

HWTEST_F(CommandStreamReceiverContextGroupTest, givenContextGroupWhenCreatingEnginesThenSetCorrectMaxOsContextCount) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0b110;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    device->getExecutionEnvironment()->calculateMaxOsContextCount();

    auto &rootDeviceEnvironment = *device->getExecutionEnvironment()->rootDeviceEnvironments[0];
    auto &engineInstances = device->getGfxCoreHelper().getGpgpuEngineInstances(rootDeviceEnvironment);
    const bool lowPriorityContextGroupSupported = rootDeviceEnvironment.isLowPriorityContextGroupSupported();
    uint32_t numContextGroupEngines = 0;

    for (const auto &engine : engineInstances) {
        const bool lowPriorityContextGroupEngine = engine.second == EngineUsage::lowPriority && lowPriorityContextGroupSupported;
        if (engine.second == EngineUsage::regular || engine.second == EngineUsage::highPriority || lowPriorityContextGroupEngine) {
            numContextGroupEngines++;
        }
    }

    auto nonGroupCount = static_cast<uint32_t>(engineInstances.size()) - numContextGroupEngines;

    auto osContextCount = nonGroupCount + (numContextGroupEngines * device->getGfxCoreHelper().getContextGroupContextsCount());

    EXPECT_EQ(osContextCount, MemoryManager::maxOsContextCount);
}
HWTEST_F(CommandStreamReceiverContextGroupTest, givenMultipleSubDevicesAndContextGroupWhenCreatingEnginesThenSetCorrectMaxOsContextCount) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(8);
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.EnableImplicitScaling.set(0);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0b110;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    device->getExecutionEnvironment()->calculateMaxOsContextCount();

    auto &rootDeviceEnvironment = *device->getExecutionEnvironment()->rootDeviceEnvironments[0];
    auto &engineInstances = device->getGfxCoreHelper().getGpgpuEngineInstances(rootDeviceEnvironment);
    const bool lowPriorityContextGroupSupported = rootDeviceEnvironment.isLowPriorityContextGroupSupported();
    uint32_t numContextGroupEngines = 0;

    for (const auto &engine : engineInstances) {
        const bool lowPriorityContextGroupEngine = engine.second == EngineUsage::lowPriority && lowPriorityContextGroupSupported;
        if (engine.second == EngineUsage::regular || engine.second == EngineUsage::highPriority || lowPriorityContextGroupEngine) {
            numContextGroupEngines++;
        }
    }

    auto nonGroupCount = static_cast<uint32_t>(engineInstances.size()) - numContextGroupEngines;

    auto osContextCount = nonGroupCount + (numContextGroupEngines * device->getGfxCoreHelper().getContextGroupContextsCount());

    const uint32_t rootContextGroupsCount = lowPriorityContextGroupSupported ? 2 : 1;
    osContextCount = osContextCount * 2 + rootContextGroupsCount * device->getGfxCoreHelper().getContextGroupContextsCount();

    EXPECT_EQ(osContextCount, MemoryManager::maxOsContextCount);
}

HWTEST_F(CommandStreamReceiverContextGroupTest, givenSecondaryCsrsWhenSameResourcesAreUsedThenResidencyIsProperlyHandled) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    const auto ccsIndex = 0;
    auto &commandStreamReceiver0 = *device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, std::nullopt)->commandStreamReceiver;
    auto &commandStreamReceiver1 = *device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, std::nullopt)->commandStreamReceiver;

    auto csr0ContextId = commandStreamReceiver0.getOsContext().getContextId();
    auto csr1ContextId = commandStreamReceiver1.getOsContext().getContextId();

    auto initialTaskCount0 = commandStreamReceiver0.peekTaskCount();
    auto initialTaskCount1 = commandStreamReceiver1.peekTaskCount();
    MockGraphicsAllocation graphicsAllocation;

    commandStreamReceiver0.makeResident(graphicsAllocation);
    EXPECT_EQ(1u, commandStreamReceiver0.getResidencyAllocations().size());
    EXPECT_EQ(0u, commandStreamReceiver1.getResidencyAllocations().size());

    commandStreamReceiver1.makeResident(graphicsAllocation);
    EXPECT_EQ(1u, commandStreamReceiver0.getResidencyAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getResidencyAllocations().size());

    EXPECT_EQ(initialTaskCount0 + 1u, graphicsAllocation.getResidencyTaskCount(csr0ContextId));
    EXPECT_EQ(initialTaskCount1 + 1u, graphicsAllocation.getResidencyTaskCount(csr1ContextId));

    commandStreamReceiver0.makeNonResident(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation.isResident(csr0ContextId));
    EXPECT_TRUE(graphicsAllocation.isResident(csr1ContextId));

    commandStreamReceiver1.makeNonResident(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation.isResident(csr0ContextId));
    EXPECT_FALSE(graphicsAllocation.isResident(csr1ContextId));
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenEnqueueWaitForPagingFenceCalledThenEnqueueIfPossibleAndReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDirectSubmissionController.set(1);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    auto pagingFenceValue = 10u;
    EXPECT_FALSE(csr.enqueueWaitForPagingFence(pagingFenceValue));

    VariableBackup<decltype(NEO::Thread::createFunc)> funcBackup{&NEO::Thread::createFunc, [](void *(*func)(void *), void *arg) -> std::unique_ptr<Thread> { return nullptr; }};
    // enqueue waitForPagingFence is possible only if controller exists and direct submission is enabled
    auto controller = static_cast<DirectSubmissionControllerMock *>(executionEnvironment->initializeDirectSubmissionController());
    controller->stopThread();
    EXPECT_FALSE(csr.enqueueWaitForPagingFence(pagingFenceValue));

    csr.directSubmissionAvailable = true;
    EXPECT_TRUE(csr.enqueueWaitForPagingFence(pagingFenceValue));

    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);
    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    csr.directSubmissionAvailable = false;
    controller->handlePagingFenceRequests(lock);
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenDrainPagingFenceQueueThenQueueDrained) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDirectSubmissionController.set(1);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(csr);
    csr.directSubmission.reset(directSubmission);

    auto executionEnvironment = pDevice->getExecutionEnvironment();
    auto pagingFenceValue = 10u;
    EXPECT_FALSE(csr.enqueueWaitForPagingFence(pagingFenceValue));

    VariableBackup<decltype(NEO::Thread::createFunc)> funcBackup{&NEO::Thread::createFunc, [](void *(*func)(void *), void *arg) -> std::unique_ptr<Thread> { return nullptr; }};

    csr.drainPagingFenceQueue();
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    auto controller = static_cast<DirectSubmissionControllerMock *>(executionEnvironment->initializeDirectSubmissionController());
    controller->stopThread();
    csr.directSubmissionAvailable = true;
    EXPECT_TRUE(csr.enqueueWaitForPagingFence(pagingFenceValue));
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    csr.directSubmissionAvailable = false;
    csr.drainPagingFenceQueue();
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    csr.directSubmissionAvailable = true;
    csr.drainPagingFenceQueue();
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);
}

HWTEST_F(CommandStreamReceiverHwTest, givenRequiredFlushTaskCountWhenFlushBcsTaskCalledThenSetStallingCommandFlag) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);

    // first flush can carry preamble, no interest in flags here
    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    // regular dispatch here
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    dispatchBcsFlags.flushTaskCount = true;

    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST_F(CommandStreamReceiverHwTest, givenEpilogueStreamAvailableWhenFlushBcsTaskCalledThenDispachEpilogueCommandsIntoEpilogueStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);

    // first flush can carry preamble, no interest in flags here
    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    // regular dispatch here
    GraphicsAllocation *commandBuffer = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream epilogueStream(commandBuffer);

    commandStreamReceiver.storeMakeResidentAllocations = true;
    dispatchBcsFlags.flushTaskCount = true;
    dispatchBcsFlags.optionalEpilogueCmdStream = &epilogueStream;

    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandBuffer));

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(epilogueStream, 0);
    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST_F(CommandStreamReceiverHwTest, givenEpilogueStreamAvailableWhenFlushImmediateTaskCalledThenDispachEpilogueCommandsIntoEpilogueStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heaplessPrologProgrammed = commandStreamReceiver.heaplessPrologProgrammed;

    // first flush can carry preamble, no interest in flags here

    if (heaplessPrologProgrammed) {
        commandStreamReceiver.flushImmediateTaskHeapless(commandStream,
                                                         commandStream.getUsed(),
                                                         immediateFlushTaskFlags,
                                                         *pDevice);
    } else {
        commandStreamReceiver.flushImmediateTask(commandStream,
                                                 commandStream.getUsed(),
                                                 immediateFlushTaskFlags,
                                                 *pDevice);
    }

    // regular dispatch here
    GraphicsAllocation *commandBuffer = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream epilogueStream(commandBuffer);

    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    immediateFlushTaskFlags.requireTaskCountUpdate = true;
    immediateFlushTaskFlags.optionalEpilogueCmdStream = &epilogueStream;

    if (heaplessPrologProgrammed) {
        commandStreamReceiver.flushImmediateTaskHeapless(commandStream,
                                                         commandStream.getUsed(),
                                                         immediateFlushTaskFlags,
                                                         *pDevice);
    } else {
        commandStreamReceiver.flushImmediateTask(commandStream,
                                                 commandStream.getUsed(),
                                                 immediateFlushTaskFlags,
                                                 *pDevice);
    }

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandBuffer));
    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(epilogueStream, 0);
    auto cmdIterator = find<typename FamilyType::PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(commandBuffer);
}

template <typename FamilyType>
void enableSimulationChunkWriteTracking(UltCommandStreamReceiver<FamilyType> &commandStreamReceiver) {
    commandStreamReceiver.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    commandStreamReceiver.chunkCopySupported = true;
    commandStreamReceiver.writeMemoryParams.chunkWrites.clear();
}

bool isChunkWritten(const WriteMemoryParams &writeMemoryParams, const WrittenMemoryChunk &expectedChunk) {
    return std::find(writeMemoryParams.chunkWrites.begin(), writeMemoryParams.chunkWrites.end(), expectedChunk) != writeMemoryParams.chunkWrites.end();
}

bool isAllocationWritten(const WriteMemoryParams &writeMemoryParams, const GraphicsAllocation *allocation) {
    return std::any_of(writeMemoryParams.chunkWrites.begin(), writeMemoryParams.chunkWrites.end(), [allocation](const WrittenMemoryChunk &chunk) {
        return chunk.gfxAllocation == allocation;
    });
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndCsrCommandsWhenFlushBcsTaskCalledThenTaskStreamAndEpilogueSectionsAreUploaded) {
    DebugManagerStateRestore restorer;
    debugManager.flags.FlushTlbBeforeCopy.set(1);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    MockGraphicsAllocation globalFenceAllocation;
    VariableBackup<GraphicsAllocation *> globalFenceAllocationBackup(&commandStreamReceiver.globalFenceAllocation, &globalFenceAllocation);

    GraphicsAllocation *epilogueAllocation = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, epilogueAllocation);
    LinearStream epilogueStream(epilogueAllocation);
    constexpr size_t epilogueStartOffset = 64u;
    epilogueStream.getSpace(epilogueStartOffset);
    constexpr size_t taskStartOffset = 32u;
    commandStream.getSpace(2 * taskStartOffset);

    DispatchBcsFlags dispatchBcsFlags(false, false, false);
    dispatchBcsFlags.flushTaskCount = true;
    dispatchBcsFlags.optionalEpilogueCmdStream = &epilogueStream;
    dispatchBcsFlags.optionalEpilogueCmdStreamStartOffset = epilogueStartOffset;
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    commandStreamReceiver.flushBcsTask(commandStream, taskStartOffset, dispatchBcsFlags, pDevice->getHardwareInfo());

    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, {commandStream.getGraphicsAllocation(), taskStartOffset, commandStream.getUsed() - taskStartOffset}));
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, {epilogueAllocation, epilogueStartOffset, epilogueStream.getUsed() - epilogueStartOffset}));

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(epilogueAllocation);
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndNoCsrCommandsWhenFlushBcsTaskCalledThenTaskStreamIsNotUploadedAsChunk) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);
    commandStreamReceiver.flushBcsTask(commandStream, commandStream.getUsed(), dispatchBcsFlags, pDevice->getHardwareInfo());

    dispatchBcsFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    commandStreamReceiver.flushBcsTask(commandStream, commandStream.getUsed(), dispatchBcsFlags, pDevice->getHardwareInfo());

    ASSERT_EQ(commandStream.getGraphicsAllocation(), commandStreamReceiver.latestFlushedBatchBuffer.commandBufferAllocation);
    EXPECT_TRUE(commandStreamReceiver.writeMemoryParams.chunkWrites.empty());
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndCsrPreambleWhenFlushImmediateTaskCalledThenTaskStreamSectionIsUploaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    commandStreamReceiver.requiresInstructionCacheFlush = true;
    constexpr size_t taskStartOffset = 32u;
    commandStream.getSpace(2 * taskStartOffset);
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    if (commandStreamReceiver.heaplessPrologProgrammed) {
        commandStreamReceiver.flushImmediateTaskHeapless(commandStream, taskStartOffset, immediateFlushTaskFlags, *pDevice);
    } else {
        commandStreamReceiver.flushImmediateTask(commandStream, taskStartOffset, immediateFlushTaskFlags, *pDevice);
    }

    ASSERT_NE(commandStream.getGraphicsAllocation(), commandStreamReceiver.latestFlushedBatchBuffer.commandBufferAllocation);
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, {commandStream.getGraphicsAllocation(), taskStartOffset, commandStream.getUsed() - taskStartOffset}));
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndEpilogueStreamWhenFlushImmediateTaskWithoutCsrPreambleCalledThenOnlyEpilogueSectionIsUploaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heaplessPrologProgrammed = commandStreamReceiver.heaplessPrologProgrammed;
    auto flushImmediate = [&]() {
        if (heaplessPrologProgrammed) {
            commandStreamReceiver.flushImmediateTaskHeapless(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
        } else {
            commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
        }
    };
    flushImmediate();

    GraphicsAllocation *epilogueAllocation = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, epilogueAllocation);
    LinearStream epilogueStream(epilogueAllocation);
    constexpr size_t epilogueStartOffset = 64u;
    epilogueStream.getSpace(epilogueStartOffset);

    commandStreamReceiver.recordFlushedBatchBuffer = true;
    immediateFlushTaskFlags.requireTaskCountUpdate = true;
    immediateFlushTaskFlags.optionalEpilogueCmdStream = &epilogueStream;
    immediateFlushTaskFlags.optionalEpilogueCmdStreamStartOffset = epilogueStartOffset;
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    flushImmediate();

    ASSERT_EQ(commandStream.getGraphicsAllocation(), commandStreamReceiver.latestFlushedBatchBuffer.commandBufferAllocation);
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, {epilogueAllocation, epilogueStartOffset, epilogueStream.getUsed() - epilogueStartOffset}));
    EXPECT_FALSE(isAllocationWritten(commandStreamReceiver.writeMemoryParams, commandStream.getGraphicsAllocation()));

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(epilogueAllocation);
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndCsrPreambleWhenFlushTaskCalledThenTaskStreamAndEpilogueSectionsAreUploaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    GraphicsAllocation *epilogueAllocation = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, epilogueAllocation);
    LinearStream epilogueStream(epilogueAllocation);
    constexpr size_t epilogueStartOffset = 64u;
    epilogueStream.getSpace(epilogueStartOffset);
    constexpr size_t taskStartOffset = 32u;
    commandStream.getSpace(2 * taskStartOffset);

    commandStreamReceiver.recordFlushedBatchBuffer = true;
    commandStreamReceiver.requiresInstructionCacheFlush = true;
    flushTaskFlags.guardCommandBufferWithPipeControl = true;
    flushTaskFlags.optionalEpilogueCmdStream = &epilogueStream;
    flushTaskFlags.optionalEpilogueCmdStreamStartOffset = epilogueStartOffset;
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    commandStreamReceiver.flushTask(commandStream, taskStartOffset, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);

    ASSERT_EQ(commandStreamReceiver.getCS(0).getGraphicsAllocation(), commandStreamReceiver.latestFlushedBatchBuffer.commandBufferAllocation);
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, {commandStream.getGraphicsAllocation(), taskStartOffset, commandStream.getUsed() - taskStartOffset}));
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, {epilogueAllocation, epilogueStartOffset, epilogueStream.getUsed() - epilogueStartOffset}));

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(epilogueAllocation);
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndRequiredEpilogueWhenFlushTaskWithoutCsrPreambleCalledThenCsrEpilogueSectionIsUploaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);

    auto taskStartOffset = commandStream.getUsed();
    commandStream.getSpace(32);
    auto &csrStream = commandStreamReceiver.getCS(0);
    auto csrUsedBeforeFlush = csrStream.getUsed();
    csrStream.getGraphicsAllocation()->setSimulationInitialUploadDone(true);
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    flushTaskFlags.epilogueRequired = true;
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    commandStreamReceiver.flushTask(commandStream, taskStartOffset, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);

    ASSERT_EQ(commandStream.getGraphicsAllocation(), commandStreamReceiver.latestFlushedBatchBuffer.commandBufferAllocation);
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, {csrStream.getGraphicsAllocation(), csrUsedBeforeFlush, csrStream.getUsed() - csrUsedBeforeFlush}));
    EXPECT_FALSE(isAllocationWritten(commandStreamReceiver.writeMemoryParams, commandStream.getGraphicsAllocation()));
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndNoEpilogueWhenFlushTaskWithoutCsrPreambleCalledThenNothingIsUploadedAsChunk) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);

    auto taskStartOffset = commandStream.getUsed();
    commandStream.getSpace(32);
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    commandStreamReceiver.flushTask(commandStream, taskStartOffset, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);

    ASSERT_EQ(commandStream.getGraphicsAllocation(), commandStreamReceiver.latestFlushedBatchBuffer.commandBufferAllocation);
    EXPECT_TRUE(commandStreamReceiver.writeMemoryParams.chunkWrites.empty());
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndBatchedCommandBuffersWhenFlushingBatchedSubmissionsThenChainedBatchBuffersAreUploaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    auto &commandBufferList = commandStreamReceiver.submissionAggregator->peekCmdBufferList();

    commandStream.getSpace(32);
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);
    auto secondTaskStartOffset = commandStream.getUsed();
    commandStream.getSpace(32);
    commandStreamReceiver.flushTask(commandStream, secondTaskStartOffset, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);
    ASSERT_NE(commandBufferList.front().get(), commandBufferList.back().get());
    auto chainedBatchBuffer = commandBufferList.back()->batchBuffer;
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    EXPECT_TRUE(commandStreamReceiver.flushBatchedSubmissions());

    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, {chainedBatchBuffer.commandBufferAllocation, chainedBatchBuffer.startOffset, chainedBatchBuffer.usedSize - chainedBatchBuffer.startOffset}));
}
template <typename FamilyType>
void verifyBatchedCommandBufferPatchesAreUploaded(UltCommandStreamReceiver<FamilyType> &commandStreamReceiver, Device &device, size_t gapBetweenTasks) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    commandStreamReceiver.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    commandStreamReceiver.dcFlushSupport = true;

    std::vector<uint8_t> commandBufferMemory(MemoryConstants::pageSize);
    MockGraphicsAllocation commandBufferAllocation(commandBufferMemory.data(), commandBufferMemory.size());
    commandBufferAllocation.allocationType = AllocationType::commandBuffer;
    commandBufferAllocation.setSimulationInitialUploadDone(true);

    auto pipeControlSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(device.getRootDeviceEnvironment(), PostSyncMode::immediateData);
    auto nooppedPipeControlLocation = commandBufferMemory.data();
    auto batchBufferEndLocation = ptrOffset(nooppedPipeControlLocation, pipeControlSize);
    auto commandBufferEndingSection = alignUp(ptrOffset(batchBufferEndLocation, sizeof(MI_BATCH_BUFFER_START)), MemoryConstants::cacheLineSize);
    auto secondTaskStartOffset = ptrDiff(commandBufferEndingSection, commandBufferMemory.data()) + gapBetweenTasks;
    auto epiloguePipeControlLocation = ptrOffset(commandBufferMemory.data(), secondTaskStartOffset);

    auto firstCommandBuffer = std::make_unique<CommandBuffer>(device);
    firstCommandBuffer->batchBuffer.commandBufferAllocation = &commandBufferAllocation;
    firstCommandBuffer->batchBuffer.usedSize = ptrDiff(commandBufferEndingSection, commandBufferMemory.data());
    firstCommandBuffer->batchBufferEndLocation = batchBufferEndLocation;
    firstCommandBuffer->pipeControlThatMayBeErasedLocation = nooppedPipeControlLocation;
    firstCommandBuffer->epiloguePipeControlLocation = nooppedPipeControlLocation;
    firstCommandBuffer->taskCount = 1u;

    auto secondCommandBuffer = std::make_unique<CommandBuffer>(device);
    secondCommandBuffer->batchBuffer.commandBufferAllocation = &commandBufferAllocation;
    secondCommandBuffer->batchBuffer.startOffset = secondTaskStartOffset;
    secondCommandBuffer->batchBuffer.usedSize = secondTaskStartOffset;
    secondCommandBuffer->batchBufferEndLocation = ptrOffset(epiloguePipeControlLocation, pipeControlSize);
    secondCommandBuffer->pipeControlThatMayBeErasedLocation = epiloguePipeControlLocation;
    secondCommandBuffer->epiloguePipeControlLocation = epiloguePipeControlLocation;
    secondCommandBuffer->taskCount = 2u;

    commandStreamReceiver.submissionAggregator->recordCommandBuffer(std::move(firstCommandBuffer));
    commandStreamReceiver.submissionAggregator->recordCommandBuffer(std::move(secondCommandBuffer));

    auto batchBufferEndPatchSize = gapBetweenTasks == 0u ? ptrDiff(commandBufferEndingSection, batchBufferEndLocation) : sizeof(MI_BATCH_BUFFER_START);
    WrittenMemoryChunk nooppedPipeControlChunk{&commandBufferAllocation, 0u, pipeControlSize};
    WrittenMemoryChunk batchBufferEndChunk{&commandBufferAllocation, pipeControlSize, batchBufferEndPatchSize};
    WrittenMemoryChunk epiloguePipeControlChunk{&commandBufferAllocation, secondTaskStartOffset, pipeControlSize};
    enableSimulationChunkWriteTracking(commandStreamReceiver);

    EXPECT_TRUE(commandStreamReceiver.flushBatchedSubmissions());

    EXPECT_TRUE(commandStreamReceiver.submissionAggregator->peekCmdBufferList().empty());
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, nooppedPipeControlChunk));
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, batchBufferEndChunk));
    EXPECT_TRUE(isChunkWritten(commandStreamReceiver.writeMemoryParams, epiloguePipeControlChunk));
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndContiguousBatchedCommandBuffersWhenFlushingBatchedSubmissionsThenPatchedCommandBufferRangesAreUploaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    verifyBatchedCommandBufferPatchesAreUploaded(commandStreamReceiver, *pDevice, 0u);
}

HWTEST_F(CommandStreamReceiverHwTest, givenSimulationModeAndNonContiguousBatchedCommandBuffersWhenFlushingBatchedSubmissionsThenPatchedCommandBufferRangesAreUploaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    verifyBatchedCommandBufferPatchesAreUploaded(commandStreamReceiver, *pDevice, MemoryConstants::cacheLineSize);
}

HWTEST_F(CommandStreamReceiverHwTest, givenFlushBcsTaskCmdListDispatchWhenCalledThenNoPrologueIsSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);
    dispatchBcsFlags.dispatchOperation = NEO::AppendOperations::cmdList;

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.recordFlushedBatchBuffer = true;
    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.disableFlatRingBuffer);
}

HWTEST_F(CommandStreamReceiverHwTest, givenImmediateFlushTaskCmdListDispatchWhenFlushingBufferThenDisableFlatRingBuffer) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heaplessPrologProgrammed = commandStreamReceiver.heaplessPrologProgrammed;
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;

    if (heaplessPrologProgrammed) {
        commandStreamReceiver.flushImmediateTaskHeapless(commandStream,
                                                         commandStream.getUsed(),
                                                         immediateFlushTaskFlags,
                                                         *pDevice);
    } else {
        commandStreamReceiver.flushImmediateTask(commandStream,
                                                 commandStream.getUsed(),
                                                 immediateFlushTaskFlags,
                                                 *pDevice);
    }

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.disableFlatRingBuffer);
}

HWTEST_F(CommandStreamReceiverHwTest, GivenWaitOnWalkerPostSyncWhenImmediateFlushTaskCalledThenExpectIsWalkerWithProfilingEnqueuedFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    {
        immediateFlushTaskFlags.isWalkerWithProfilingEnqueued = false;
        commandStreamReceiver.flushImmediateTask(commandStream,
                                                 commandStream.getUsed(),
                                                 immediateFlushTaskFlags,
                                                 *pDevice);

        EXPECT_FALSE(commandStreamReceiver.isWalkerWithProfilingEnqueued);
    }

    {
        immediateFlushTaskFlags.isWalkerWithProfilingEnqueued = true;
        commandStreamReceiver.flushImmediateTask(commandStream,
                                                 commandStream.getUsed(),
                                                 immediateFlushTaskFlags,
                                                 *pDevice);

        EXPECT_TRUE(commandStreamReceiver.isWalkerWithProfilingEnqueued);
    }

    {
        immediateFlushTaskFlags.isWalkerWithProfilingEnqueued = false;
        commandStreamReceiver.isWalkerWithProfilingEnqueued = true;
        commandStreamReceiver.flushImmediateTask(commandStream,
                                                 commandStream.getUsed(),
                                                 immediateFlushTaskFlags,
                                                 *pDevice);

        EXPECT_TRUE(commandStreamReceiver.isWalkerWithProfilingEnqueued);
    }

    {
        immediateFlushTaskFlags.isWalkerWithProfilingEnqueued = true;
        commandStreamReceiver.isWalkerWithProfilingEnqueued = true;
        commandStreamReceiver.flushImmediateTask(commandStream,
                                                 commandStream.getUsed(),
                                                 immediateFlushTaskFlags,
                                                 *pDevice);

        EXPECT_TRUE(commandStreamReceiver.isWalkerWithProfilingEnqueued);
    }
}

HWTEST_F(CommandStreamReceiverHwTest, givenVariousCsrModeWhenGettingHardwareModeThenExpectOnlyWhenModeIsHarware) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardware;
    EXPECT_TRUE(ultCsr.isHardwareMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardwareWithAub;
    EXPECT_FALSE(ultCsr.isHardwareMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    EXPECT_FALSE(ultCsr.isHardwareMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    EXPECT_FALSE(ultCsr.isHardwareMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbxWithAub;
    EXPECT_FALSE(ultCsr.isHardwareMode());
}

using CommandStreamReceiverHostFunctionHwTest = Test<CommandStreamReceiverFixture>;

HWTEST_F(CommandStreamReceiverHostFunctionHwTest, givenHostFunctionWhenMakeResidentHostFunctionAllocationIsCalledThenHostAllocationIsResident) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    uint32_t rootDeviceIndex = csr.getRootDeviceIndex();
    uint32_t maxRootDeviceIndex = static_cast<uint32_t>(pDevice->getExecutionEnvironment()->rootDeviceEnvironments.size() - 1);
    MockMemoryManager mockMemoryManager(*pDevice->executionEnvironment);
    MockHostFunctionAllocator hostFunctionAllocator(&mockMemoryManager,
                                                    &csr,
                                                    MemoryConstants::pageSize64k,
                                                    rootDeviceIndex,
                                                    maxRootDeviceIndex,
                                                    csr.getActivePartitions(),
                                                    false);

    csr.ensureHostFunctionWorkerStarted(&hostFunctionAllocator);
    EXPECT_EQ(1u, csr.createHostFunctionWorkerCounter);

    auto *hostFunctionIdAllocation = csr.getHostFunctionStreamer().getHostFunctionIdAllocation();
    ASSERT_NE(nullptr, hostFunctionIdAllocation);

    auto csrContextId = csr.getOsContext().getContextId();
    EXPECT_FALSE(hostFunctionIdAllocation->isResident(csrContextId));

    auto *mockHostFunctionAllocation = hostFunctionAllocator.allocation.get();
    ASSERT_NE(nullptr, mockHostFunctionAllocation);
    EXPECT_FALSE(mockHostFunctionAllocation->isResident(csrContextId));

    csr.makeResident(*mockHostFunctionAllocation);

    EXPECT_TRUE(hostFunctionIdAllocation->isResident(csrContextId));
    EXPECT_TRUE(mockHostFunctionAllocation->isResident(csrContextId));

    csr.makeNonResident(*hostFunctionIdAllocation);
    EXPECT_FALSE(hostFunctionIdAllocation->isResident(csrContextId));
}

HWTEST_F(CommandStreamReceiverHostFunctionHwTest, givenHostFunctionWhenSignalHostFunctionWorkerIsCalledThenCounterIsUpdated) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ASSERT_EQ(0u, csr.createHostFunctionWorkerCounter);
    ASSERT_EQ(0u, csr.createHostFunctionWorkerCounter);

    uint32_t rootDeviceIndex = csr.getRootDeviceIndex();
    uint32_t maxRootDeviceIndex = static_cast<uint32_t>(pDevice->getExecutionEnvironment()->rootDeviceEnvironments.size() - 1);
    MockMemoryManager mockMemoryManager(*pDevice->executionEnvironment);
    MockHostFunctionAllocator hostFunctionAllocator(&mockMemoryManager,
                                                    &csr,
                                                    MemoryConstants::pageSize64k,
                                                    rootDeviceIndex,
                                                    maxRootDeviceIndex,
                                                    csr.getActivePartitions(),
                                                    false);

    csr.ensureHostFunctionWorkerStarted(&hostFunctionAllocator);
    csr.signalHostFunctionWorker(10u);

    ASSERT_EQ(1u, csr.createHostFunctionWorkerCounter);
    EXPECT_EQ(10u, csr.signalHostFunctionWorkerCounter);
}

HWTEST_F(CommandStreamReceiverTest, givenPoolAllocatorEnabledWhenGetCSThenAllocationIsViewFromPool) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &cs = csr.getCS();
    cs.getSpace(cs.getAvailableSpace());

    csr.getCS();
    auto *allocation = cs.getGraphicsAllocation();

    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->isView());
    auto *parent = allocation->getParentAllocation();
    ASSERT_NE(nullptr, parent);
    EXPECT_TRUE(pDevice->getCommandBufferPoolAllocator().isPoolBuffer(parent));
}

HWTEST_F(CommandStreamReceiverTest, givenPoolViewWhenReleaseCommandBufferAllocationThenDeferredInPoolNotReuseList) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &poolAllocator = pDevice->getCommandBufferPoolAllocator();
    auto *poolView = poolAllocator.allocate(MemoryConstants::pageSize64k);

    ASSERT_NE(nullptr, poolView);
    ASSERT_TRUE(poolView->isView());

    csr.releaseCommandBufferAllocation(poolView);

    EXPECT_TRUE(csr.getAllocationsForReuse().peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverTest, givenNonViewAllocationWhenReleaseCommandBufferAllocationThenStoredInReuseList) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto *memManager = csr.getMemoryManager();
    auto *regularAllocation = memManager->allocateGraphicsMemoryWithProperties(
        MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::commandBuffer});

    ASSERT_NE(nullptr, regularAllocation);
    ASSERT_FALSE(regularAllocation->isView());

    csr.releaseCommandBufferAllocation(regularAllocation);

    EXPECT_FALSE(csr.getAllocationsForReuse().peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverTest, givenViewWithNonPoolParentWhenReleaseCommandBufferAllocationThenStoredInReuseList) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto *memManager = csr.getMemoryManager();
    auto *parentAllocation = memManager->allocateGraphicsMemoryWithProperties(
        MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::commandBuffer});

    ASSERT_NE(nullptr, parentAllocation);

    auto *viewAllocation = parentAllocation->createView(0u, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, viewAllocation);
    ASSERT_TRUE(viewAllocation->isView());
    ASSERT_EQ(parentAllocation, viewAllocation->getParentAllocation());
    EXPECT_FALSE(pDevice->getCommandBufferPoolAllocator().isPoolBuffer(parentAllocation));

    csr.releaseCommandBufferAllocation(viewAllocation);

    auto &reuseList = MockAllocationsList::whiteboxCast(csr.getAllocationsForReuse());
    EXPECT_FALSE(reuseList.peekIsEmpty());

    // Remove view from reuse list before teardown to prevent use-after-free -
    // view delegates getTaskCount to parent which would be freed during device destruction
    auto removedAllocation = reuseList.removeOne(*reuseList.peekFront());
    ASSERT_NE(nullptr, removedAllocation);
    removedAllocation.reset();

    memManager->freeGraphicsMemory(parentAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenPoolAllocatorEnabledWhenCommandBufferExhaustedThenOldViewDeferredAndNewAllocatedFromPool) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &cs = csr.getCS();
    cs.getSpace(cs.getAvailableSpace());

    csr.getCS();
    auto *firstAllocation = cs.getGraphicsAllocation();

    ASSERT_NE(nullptr, firstAllocation);
    ASSERT_TRUE(firstAllocation->isView());

    cs.getSpace(cs.getAvailableSpace());

    csr.getCS();
    auto *secondAllocation = cs.getGraphicsAllocation();

    ASSERT_NE(nullptr, secondAllocation);
    EXPECT_TRUE(secondAllocation->isView());
    auto *parent = secondAllocation->getParentAllocation();
    ASSERT_NE(nullptr, parent);
    EXPECT_TRUE(pDevice->getCommandBufferPoolAllocator().isPoolBuffer(parent));
}

HWTEST_F(CommandStreamReceiverTest, givenPrimaryCommandBufferWithTwoViewsFromSameParentWhenAggregatingThenBothViewsAreTracked) {
    auto contextId = commandStreamReceiver->getOsContext().getContextId();

    auto parentAllocation = std::unique_ptr<GraphicsAllocation>(
        memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), 2 * MemoryConstants::pageSize64k, AllocationType::commandBuffer}));
    ASSERT_NE(nullptr, parentAllocation);

    auto firstView = std::unique_ptr<GraphicsAllocation>(parentAllocation->createView(0u, MemoryConstants::pageSize64k));
    auto secondView = std::unique_ptr<GraphicsAllocation>(parentAllocation->createView(MemoryConstants::pageSize64k, MemoryConstants::pageSize64k));
    ASSERT_NE(nullptr, firstView);
    ASSERT_NE(nullptr, secondView);

    SubmissionAggregator aggregator;
    auto primaryCommandBuffer = std::make_unique<CommandBuffer>(*pDevice);
    primaryCommandBuffer->batchBuffer.commandBufferAllocation = firstView.get();
    primaryCommandBuffer->surfaces.push_back(firstView.get());
    primaryCommandBuffer->surfaces.push_back(secondView.get());
    aggregator.recordCommandBuffer(std::move(primaryCommandBuffer));

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0u;
    aggregator.aggregateCommandBuffers(0u, resourcePackage, totalUsedSize, std::numeric_limits<size_t>::max(), contextId);

    bool firstViewTracked = false;
    bool secondViewTracked = false;
    for (auto *allocation : resourcePackage) {
        firstViewTracked |= (allocation == firstView.get());
        secondViewTracked |= (allocation == secondView.get());
    }

    EXPECT_TRUE(firstViewTracked);
    EXPECT_TRUE(secondViewTracked);
    EXPECT_EQ(2u, resourcePackage.size());

    memoryManager->freeGraphicsMemory(parentAllocation.release());
}

HWTEST_F(CommandStreamReceiverTest, givenMergedCommandBuffersWithViewsFromSameParentWhenAggregatingThenSecondaryCommandBufferViewIsTracked) {
    auto contextId = commandStreamReceiver->getOsContext().getContextId();

    auto parentAllocation = std::unique_ptr<GraphicsAllocation>(
        memoryManager->allocateGraphicsMemoryWithProperties(
            MockAllocationProperties{pDevice->getRootDeviceIndex(), 2 * MemoryConstants::pageSize64k, AllocationType::commandBuffer}));
    ASSERT_NE(nullptr, parentAllocation);

    auto firstView = std::unique_ptr<GraphicsAllocation>(parentAllocation->createView(0u, MemoryConstants::pageSize64k));
    auto secondView = std::unique_ptr<GraphicsAllocation>(parentAllocation->createView(MemoryConstants::pageSize64k, MemoryConstants::pageSize64k));
    ASSERT_NE(nullptr, firstView);
    ASSERT_NE(nullptr, secondView);

    SubmissionAggregator aggregator;
    auto primaryCommandBuffer = std::make_unique<CommandBuffer>(*pDevice);
    auto secondaryCommandBuffer = std::make_unique<CommandBuffer>(*pDevice);

    primaryCommandBuffer->batchBuffer.commandBufferAllocation = firstView.get();
    primaryCommandBuffer->surfaces.push_back(firstView.get());
    secondaryCommandBuffer->batchBuffer.commandBufferAllocation = secondView.get();

    aggregator.recordCommandBuffer(std::move(primaryCommandBuffer));
    aggregator.recordCommandBuffer(std::move(secondaryCommandBuffer));

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0u;
    aggregator.aggregateCommandBuffers(0u, resourcePackage, totalUsedSize, std::numeric_limits<size_t>::max(), contextId);

    bool secondViewTracked = false;
    for (auto *allocation : resourcePackage) {
        secondViewTracked |= (allocation == secondView.get());
    }

    EXPECT_TRUE(secondViewTracked);

    memoryManager->freeGraphicsMemory(parentAllocation.release());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenCreateDeferredFreeContextThenFieldsMatchCsrState) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto ctx = csr.createDeferredFreeContext();

    EXPECT_EQ(csr.getTagAddress(), ctx.tagAddress);
    EXPECT_EQ(csr.getUcTagAddress(), ctx.ucTagAddress);
    EXPECT_EQ(csr.getOsContext().getContextId(), ctx.contextId);
    EXPECT_EQ(csr.getActivePartitions(), ctx.partitionCount);
    EXPECT_EQ(csr.getImmWritePostSyncWriteOffset(), ctx.tagOffset);
}

HWTEST_F(CommandStreamReceiverTest, givenPoolViewInGlobalStatelessHeapWhenReleasedThenViewReturnedToLinearStreamPool) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableLinearStreamPoolAllocator.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.createGlobalStatelessHeap();

    auto *allocation = csr.getGlobalStatelessHeapAllocation();
    ASSERT_NE(nullptr, allocation);
    ASSERT_TRUE(allocation->isView());
    auto &poolAllocator = pDevice->getLinearStreamPoolAllocator();
    auto *parent = allocation->getParentAllocation();
    ASSERT_NE(nullptr, parent);
    ASSERT_TRUE(poolAllocator.isPoolBuffer(parent));

    csr.releaseGlobalStatelessHeap();

    EXPECT_EQ(nullptr, csr.getGlobalStatelessHeapAllocation());
    EXPECT_EQ(nullptr, csr.getGlobalStatelessHeap());

    constexpr size_t heapSize = 16 * MemoryConstants::kiloByte;
    ASSERT_EQ(0u, poolAllocator.getDefaultPoolSize() % heapSize);
    std::vector<GraphicsAllocation *> allocations;
    auto maxAllocations = poolAllocator.getDefaultPoolSize() / heapSize;

    for (size_t i = 0; i < maxAllocations; i++) {
        auto *newAllocation = poolAllocator.allocate(heapSize);
        ASSERT_NE(nullptr, newAllocation);
        allocations.push_back(newAllocation);
    }

    EXPECT_EQ(1u, poolAllocator.getPoolsCount());

    for (auto *allocationToFree : allocations) {
        poolAllocator.free(allocationToFree);
    }
}

HWTEST_F(CommandStreamReceiverTest, givenPoolViewInCommandStreamWhenCleanupResourcesThenViewReturnedToPoolAndStreamNulled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &cs = csr.getCS();
    cs.getSpace(cs.getAvailableSpace());

    csr.getCS();
    auto *allocation = cs.getGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);
    ASSERT_TRUE(allocation->isView());
    ASSERT_NE(nullptr, allocation->getParentAllocation());
    ASSERT_TRUE(pDevice->getCommandBufferPoolAllocator().isPoolBuffer(allocation->getParentAllocation()));

    csr.cleanupResources();

    EXPECT_EQ(nullptr, cs.getGraphicsAllocation());
    EXPECT_EQ(nullptr, cs.getCpuBase());

    auto &poolAllocator = pDevice->getCommandBufferPoolAllocator();
    auto *newAlloc = poolAllocator.allocate(MemoryConstants::pageSize64k);
    ASSERT_NE(nullptr, newAlloc);
    poolAllocator.free(newAlloc);
}

HWTEST_F(CommandStreamReceiverTest, givenPoolAllocatorEnabledWhenRequestedSizeExceedsPoolMaxThenFallsBackToMemoryManager) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &cs = csr.getCS();
    cs.getSpace(cs.getAvailableSpace());

    constexpr size_t largeSize = 3 * MemoryConstants::megaByte;
    csr.getCS(largeSize);
    auto *allocation = cs.getGraphicsAllocation();

    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->isView());
    EXPECT_EQ(AllocationType::commandBuffer, allocation->getAllocationType());
}

struct CommandStreamReceiverImmediateSWTagsFixture : public CommandStreamReceiverFixture {
    void setUp() {
        debugManager.flags.EnableSWTags.set(true);
        CommandStreamReceiverFixture::setUp();
    }
    DebugManagerStateRestore restorer;
};
using CommandStreamReceiverImmediateSWTagsTest = Test<CommandStreamReceiverImmediateSWTagsFixture>;

HWTEST_F(CommandStreamReceiverImmediateSWTagsTest, givenEnableSWTagsWhenFlushImmediateTaskThenHeapAddressesAreProgrammedAndHeapsResident) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto tagsManager = pDevice->getRootDeviceEnvironment().tagsManager.get();
    ASSERT_NE(nullptr, tagsManager);
    auto bxmlHeap = tagsManager->getBXMLHeapAllocation();
    auto tagHeap = tagsManager->getSWTagHeapAllocation();

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(bxmlHeap));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(tagHeap));

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto storeDataImmCmds = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

    bool bxmlHeapAddressProgrammed = false;
    bool tagHeapAddressProgrammed = false;
    for (auto &entry : storeDataImmCmds) {
        auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(*entry);
        if (storeDataImm->getAddress() == bxmlHeap->getGpuAddress()) {
            bxmlHeapAddressProgrammed = true;
        }
        if (storeDataImm->getAddress() == tagHeap->getGpuAddress()) {
            tagHeapAddressProgrammed = true;
        }
    }
    EXPECT_TRUE(bxmlHeapAddressProgrammed);
    EXPECT_TRUE(tagHeapAddressProgrammed);
}

HWTEST_F(CommandStreamReceiverImmediateSWTagsTest, givenEnableSWTagsWhenFlushImmediateTaskForCmdListOperationThenHeapAddressesAreNotProgrammedAndHeapsNotResident) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto tagsManager = pDevice->getRootDeviceEnvironment().tagsManager.get();
    ASSERT_NE(nullptr, tagsManager);
    auto bxmlHeap = tagsManager->getBXMLHeapAllocation();
    auto tagHeap = tagsManager->getSWTagHeapAllocation();

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_FALSE(commandStreamReceiver.isMadeResident(bxmlHeap));
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(tagHeap));
}

HWTEST_F(CommandStreamReceiverImmediateSWTagsTest, givenEnableSWTagsWhenFlushBcsTaskThenHeapAddressesAreProgrammedAndHeapsResident) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto tagsManager = pDevice->getRootDeviceEnvironment().tagsManager.get();
    ASSERT_NE(nullptr, tagsManager);
    auto bxmlHeap = tagsManager->getBXMLHeapAllocation();
    auto tagHeap = tagsManager->getSWTagHeapAllocation();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);
    commandStreamReceiver.flushBcsTask(commandStream, commandStream.getUsed(), dispatchBcsFlags, pDevice->getHardwareInfo());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(bxmlHeap));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(tagHeap));

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto storeDataImmCmds = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

    bool bxmlHeapAddressProgrammed = false;
    bool tagHeapAddressProgrammed = false;
    for (auto &entry : storeDataImmCmds) {
        auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(*entry);
        if (storeDataImm->getAddress() == bxmlHeap->getGpuAddress()) {
            bxmlHeapAddressProgrammed = true;
        }
        if (storeDataImm->getAddress() == tagHeap->getGpuAddress()) {
            tagHeapAddressProgrammed = true;
        }
    }
    EXPECT_TRUE(bxmlHeapAddressProgrammed);
    EXPECT_TRUE(tagHeapAddressProgrammed);
}

HWTEST_F(CommandStreamReceiverImmediateSWTagsTest, givenEnableSWTagsWhenFlushBcsTaskForCmdListOperationThenHeapAddressesAreNotProgrammedAndHeapsNotResident) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto tagsManager = pDevice->getRootDeviceEnvironment().tagsManager.get();
    ASSERT_NE(nullptr, tagsManager);
    auto bxmlHeap = tagsManager->getBXMLHeapAllocation();
    auto tagHeap = tagsManager->getSWTagHeapAllocation();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);
    dispatchBcsFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    commandStreamReceiver.flushBcsTask(commandStream, commandStream.getUsed(), dispatchBcsFlags, pDevice->getHardwareInfo());

    EXPECT_FALSE(commandStreamReceiver.isMadeResident(bxmlHeap));
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(tagHeap));
}

TEST_F(CommandStreamReceiverTest, givenBaseCommandStreamReceiverWhenUsingExternalSemaphoreGpuWaitThenItIsNotAllowedAndSubmissionFails) {
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    EXPECT_FALSE(csr.isExternalSemaphoreGpuWaitAllowed());
    EXPECT_FALSE(csr.submitExternalSemaphoreWaits({}));
}

TEST_F(CommandStreamReceiverTest, givenOsContextSupportingExternalSemaphoreGpuWaitWhenSettingUpContextThenExternalSemaphoreGpuWaitIsAllowed) {
    OsContextMock osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.mockExternalSemaphoreGpuWaitSupported = true;

    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.setupContext(osContext);

    EXPECT_TRUE(csr.isExternalSemaphoreGpuWaitAllowed());
}

TEST_F(CommandStreamReceiverTest, givenOsContextNotSupportingExternalSemaphoreGpuWaitWhenSettingUpContextThenExternalSemaphoreGpuWaitIsNotAllowed) {
    OsContextMock osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.mockExternalSemaphoreGpuWaitSupported = false;

    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.setupContext(osContext);

    EXPECT_FALSE(csr.isExternalSemaphoreGpuWaitAllowed());
}

TEST_F(CommandStreamReceiverTest, givenForceExternalSemaphoreHostFunctionWaitAndOsContextSupportingExternalSemaphoreGpuWaitWhenSettingUpContextThenExternalSemaphoreGpuWaitIsNotAllowed) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceExternalSemaphoreHostFunctionWait.set(true);

    OsContextMock osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.mockExternalSemaphoreGpuWaitSupported = true;

    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.setupContext(osContext);

    EXPECT_FALSE(csr.isExternalSemaphoreGpuWaitAllowed());
}

HWTEST_F(CommandStreamReceiverTest, givenSubmittedAndCompletedTaskCountsWhenQueryingOutstandingWorkCountThenDifferenceIsReturned) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ASSERT_NE(nullptr, ultCsr.getTagAddress());

    *ultCsr.getTagAddress() = 4u;
    ultCsr.taskCount = 4u;
    ultCsr.latestFlushedTaskCount = 4u;
    EXPECT_EQ(0u, ultCsr.getOutstandingWorkCount());

    ultCsr.taskCount = 7u;
    ultCsr.latestFlushedTaskCount = 7u;
    EXPECT_EQ(3u, ultCsr.getOutstandingWorkCount());

    *ultCsr.getTagAddress() = 9u;
    EXPECT_EQ(0u, ultCsr.getOutstandingWorkCount());

    *ultCsr.getTagAddress() = ultCsr.peekTaskCount();
}

TEST_F(CommandStreamReceiverTest, givenCsrWithoutTagAllocationWhenQueryingOutstandingWorkCountThenZeroIsReturned) {
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.tagAddress = nullptr;
    csr.taskCount = 5u;

    EXPECT_EQ(0u, csr.getOutstandingWorkCount());
}

HWTEST_F(CommandStreamReceiverTest, givenTagUpdatedOnlyInSimulationWhenQueryingOutstandingWorkCountThenTagIsDownloadedBeforeItIsRead) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    constexpr TagAddressType completedInSimulationTaskCount = 5u;
    uint32_t tagDownloadCount = 0u;
    ultCsr.downloadAllocationImpl = [&](GraphicsAllocation &allocation, uint64_t, size_t) {
        if (&allocation == ultCsr.getTagAllocation()) {
            tagDownloadCount++;
            *ultCsr.getTagAddress() = completedInSimulationTaskCount;
        }
    };

    *ultCsr.getTagAddress() = 0u;
    ultCsr.taskCount = completedInSimulationTaskCount;
    ultCsr.latestFlushedTaskCount = completedInSimulationTaskCount;
    EXPECT_EQ(0u, ultCsr.getOutstandingWorkCount());
    EXPECT_EQ(1u, tagDownloadCount);

    *ultCsr.getTagAddress() = 0u;
    ultCsr.latestFlushedTaskCount = 0u;
    EXPECT_EQ(0u, ultCsr.getOutstandingWorkCount());
    EXPECT_EQ(1u, tagDownloadCount);

    ultCsr.downloadAllocationImpl = nullptr;
    *ultCsr.getTagAddress() = ultCsr.peekTaskCount();
}

HWTEST_F(CommandStreamReceiverTest, givenSubmissionsWithoutTaskCountUpdateWhenQueryingOutstandingWorkCountThenOnlyWorkWithTaskCountUpdateIsCounted) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    constexpr TaskCountType taskCountWithTagUpdate = 3u;
    constexpr TaskCountType submissionsWithoutTagUpdate = 4u;

    *ultCsr.getTagAddress() = 1u;
    ultCsr.latestFlushedTaskCount = taskCountWithTagUpdate;
    ultCsr.taskCount = taskCountWithTagUpdate + submissionsWithoutTagUpdate;

    EXPECT_EQ(taskCountWithTagUpdate - 1u, ultCsr.getOutstandingWorkCount());

    *ultCsr.getTagAddress() = ultCsr.peekTaskCount();
}

HWTEST_F(CommandStreamReceiverTest, givenTagReachedWhenCheckingTagThenTrueIsReturnedWithoutDownloadingAllocations) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    *ultCsr.getTagAddress() = 5u;
    const auto downloadsBefore = ultCsr.downloadAllocationsCalledCount.load();

    EXPECT_TRUE(ultCsr.isTagReached(ultCsr.getTagAddress(), 5u, false));
    EXPECT_FALSE(ultCsr.isTagReached(ultCsr.getTagAddress(), 6u, false));
    EXPECT_EQ(downloadsBefore, ultCsr.downloadAllocationsCalledCount.load());

    EXPECT_TRUE(ultCsr.testTaskCountReady(ultCsr.getTagAddress(), 5u));
    EXPECT_EQ(downloadsBefore + 1, ultCsr.downloadAllocationsCalledCount.load());
    EXPECT_FALSE(ultCsr.testTaskCountReady(ultCsr.getTagAddress(), 6u));
    EXPECT_EQ(downloadsBefore + 1, ultCsr.downloadAllocationsCalledCount.load());

    *ultCsr.getTagAddress() = ultCsr.peekTaskCount();
}

HWTEST_F(CommandStreamReceiverTest, givenLaterSubmissionPendingWhenTestingTaskCountReadyThenAllocationsAreDownloadedOnlyUpToTestedTaskCount) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    constexpr TaskCountType testedTaskCount = 5u;
    constexpr TaskCountType pendingSubmissionsCount = 3u;
    ultCsr.taskCount = testedTaskCount + pendingSubmissionsCount;
    ultCsr.latestFlushedTaskCount = testedTaskCount + pendingSubmissionsCount;
    *ultCsr.getTagAddress() = testedTaskCount;
    const auto downloadsBefore = ultCsr.downloadAllocationsCalledCount.load();

    EXPECT_TRUE(ultCsr.testTaskCountReady(ultCsr.getTagAddress(), testedTaskCount));
    EXPECT_EQ(downloadsBefore + 1, ultCsr.downloadAllocationsCalledCount.load());
    EXPECT_TRUE(ultCsr.latestDownloadAllocationsBlocking);
    EXPECT_EQ(testedTaskCount, ultCsr.latestDownloadAllocationsTaskCount);

    *ultCsr.getTagAddress() = ultCsr.peekTaskCount();
}

HWTEST_F(CommandStreamReceiverHwTest, givenMidThreadPreemptionAllocationOnDemandDebugKeyWhenSettingUpContextThenOnDemandModeIsConfigured) {
    DebugManagerStateRestore restorer;
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    UltCommandStreamReceiver<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.heaplessModeEnabled = false;

    commandStreamReceiver.setupContext(osContext);
    EXPECT_EQ(pDevice->getGfxCoreHelper().isMidThreadPreemptionAllocationOnDemandSupported(), commandStreamReceiver.midThreadPreemptionAllocationOnDemand);

    debugManager.flags.MidThreadPreemptionAllocationOnDemand.set(0);
    commandStreamReceiver.setupContext(osContext);
    EXPECT_FALSE(commandStreamReceiver.midThreadPreemptionAllocationOnDemand);

    debugManager.flags.MidThreadPreemptionAllocationOnDemand.set(1);
    commandStreamReceiver.setupContext(osContext);
    EXPECT_TRUE(commandStreamReceiver.midThreadPreemptionAllocationOnDemand);
}

HWTEST_F(CommandStreamReceiverHwTest, givenMidThreadPreemptionAllocationOnDemandEnabledWhenSettingUpHeaplessOrCopyOrContextGroupContextThenOnDemandModeIsDisabled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MidThreadPreemptionAllocationOnDemand.set(1);

    MockOsContext computeOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    MockOsContext copyOsContext(1, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}));
    MockOsContext contextGroupOsContext(2, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    contextGroupOsContext.setContextGroupCount(8);
    UltCommandStreamReceiver<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    commandStreamReceiver.heaplessModeEnabled = true;
    commandStreamReceiver.setupContext(computeOsContext);
    EXPECT_FALSE(commandStreamReceiver.midThreadPreemptionAllocationOnDemand);

    commandStreamReceiver.heaplessModeEnabled = false;
    commandStreamReceiver.setupContext(copyOsContext);
    EXPECT_FALSE(commandStreamReceiver.midThreadPreemptionAllocationOnDemand);

    commandStreamReceiver.setupContext(contextGroupOsContext);
    EXPECT_FALSE(commandStreamReceiver.midThreadPreemptionAllocationOnDemand);

    commandStreamReceiver.setupContext(computeOsContext);
    EXPECT_TRUE(commandStreamReceiver.midThreadPreemptionAllocationOnDemand);
}

HWTEST_F(CommandStreamReceiverHwTest, givenMidThreadPreemptionAllocationOnDemandWhenInitializingResourcesThenPreemptionAllocationIsCreatedOnlyWithoutOnDemandMode) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    UltCommandStreamReceiver<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(osContext);
    commandStreamReceiver.setDevice(pDevice);

    commandStreamReceiver.midThreadPreemptionAllocationOnDemand = true;
    EXPECT_TRUE(commandStreamReceiver.initializeResources(PreemptionMode::MidThread));
    EXPECT_EQ(nullptr, commandStreamReceiver.getPreemptionAllocation());

    commandStreamReceiver.resourcesInitialized = false;
    commandStreamReceiver.midThreadPreemptionAllocationOnDemand = false;
    EXPECT_TRUE(commandStreamReceiver.initializeResources(PreemptionMode::MidThread));
    EXPECT_NE(nullptr, commandStreamReceiver.getPreemptionAllocation());
}

HWTEST_F(CommandStreamReceiverHwTest, givenMidThreadPreemptionAllocationOnDemandWhenCheckingPreemptionStateThenDeferredStateDependsOnAllocationAndDebugger) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    VariableBackup<bool> onDemandBackup(&commandStreamReceiver.midThreadPreemptionAllocationOnDemand, false);
    VariableBackup<GraphicsAllocation *> preemptionAllocationBackup(&commandStreamReceiver.preemptionAllocation, nullptr);
    VariableBackup<bool> stateSipSentBackup(&commandStreamReceiver.isStateSipSent, false);
    MockGraphicsAllocation preemptionAllocation;

    EXPECT_FALSE(commandStreamReceiver.isMidThreadPreemptionStateDeferred());
    EXPECT_FALSE(commandStreamReceiver.isMidThreadPreemptionStateProgrammingRequired(true));
    EXPECT_TRUE(commandStreamReceiver.isStateSipProgrammingRequired());

    commandStreamReceiver.midThreadPreemptionAllocationOnDemand = true;
    EXPECT_TRUE(commandStreamReceiver.isMidThreadPreemptionStateDeferred());
    EXPECT_TRUE(commandStreamReceiver.isMidThreadPreemptionStateProgrammingRequired(true));
    EXPECT_FALSE(commandStreamReceiver.isMidThreadPreemptionStateProgrammingRequired(false));
    EXPECT_FALSE(commandStreamReceiver.isStateSipProgrammingRequired());

    commandStreamReceiver.preemptionAllocation = &preemptionAllocation;
    EXPECT_FALSE(commandStreamReceiver.isMidThreadPreemptionStateDeferred());
    EXPECT_FALSE(commandStreamReceiver.isMidThreadPreemptionStateProgrammingRequired(true));
    EXPECT_TRUE(commandStreamReceiver.isStateSipProgrammingRequired());

    commandStreamReceiver.isStateSipSent = true;
    EXPECT_FALSE(commandStreamReceiver.isStateSipProgrammingRequired());

    commandStreamReceiver.preemptionAllocation = nullptr;
    commandStreamReceiver.isStateSipSent = false;
    pDevice->getRootDeviceEnvironmentRef().debugger.reset(new MockDebugger);
    EXPECT_FALSE(commandStreamReceiver.isMidThreadPreemptionStateDeferred());
    EXPECT_TRUE(commandStreamReceiver.isStateSipProgrammingRequired());
    pDevice->getRootDeviceEnvironmentRef().debugger.reset();
}

template <typename FamilyType>
struct PreemptionAllocationFailingCommandStreamReceiver : public UltCommandStreamReceiver<FamilyType> {
    using UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver;
    bool createPreemptionAllocation() override {
        return false;
    }
};

HWTEST_F(CommandStreamReceiverHwTest, givenPreemptionAllocationFailureWhenProgrammingMidThreadPreemptionStateThenUnrecoverableIsCalled) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    PreemptionAllocationFailingCommandStreamReceiver<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(osContext);

    EXPECT_THROW(commandStreamReceiver.programMidThreadPreemptionState(commandStream, *pDevice), std::exception);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenMidThreadPreemptionAllocationOnDemandWhenFlushingTasksThenPreemptionStateIsProgrammedOnceOnFirstMidThreadPreemptionKernel, IsWithinXe2HpgCoreAndXe3Core) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    pDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ASSERT_TRUE(commandStreamReceiver.midThreadPreemptionAllocationOnDemand);
    commandStreamReceiver.getMemoryManager()->freeGraphicsMemory(commandStreamReceiver.preemptionAllocation);
    commandStreamReceiver.preemptionAllocation = nullptr;
    commandStreamReceiver.storeMakeResidentAllocations = true;

    flushTaskFlags.preemptionMode = PreemptionMode::ThreadGroup;
    flushTaskFlags.hasMidThreadPreemptionKernel = false;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);

    EXPECT_EQ(nullptr, commandStreamReceiver.getPreemptionAllocation());
    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_EQ(0u, hwParser.getCommandCount<STATE_CONTEXT_DATA_BASE_ADDRESS>());
    EXPECT_EQ(0u, hwParser.getCommandCount<STATE_SIP>());

    auto usedSize = commandStreamReceiver.commandStream.getUsed();
    flushTaskFlags.preemptionMode = PreemptionMode::MidThread;
    flushTaskFlags.hasMidThreadPreemptionKernel = true;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);

    auto preemptionAllocation = commandStreamReceiver.getPreemptionAllocation();
    ASSERT_NE(nullptr, preemptionAllocation);
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(preemptionAllocation));
    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());
    hwParser.tearDown();
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    auto stateContextDataBaseAddressCmds = hwParser.getCommandsList<STATE_CONTEXT_DATA_BASE_ADDRESS>();
    ASSERT_EQ(1u, stateContextDataBaseAddressCmds.size());
    auto stateContextDataBaseAddress = genCmdCast<STATE_CONTEXT_DATA_BASE_ADDRESS *>(*stateContextDataBaseAddressCmds.begin());
    EXPECT_EQ(preemptionAllocation->getGpuAddressToPatch(), stateContextDataBaseAddress->getContextDataBaseAddress());
    EXPECT_EQ(1u, hwParser.getCommandCount<STATE_SIP>());

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);

    EXPECT_EQ(preemptionAllocation, commandStreamReceiver.getPreemptionAllocation());
    hwParser.tearDown();
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    EXPECT_EQ(0u, hwParser.getCommandCount<STATE_CONTEXT_DATA_BASE_ADDRESS>());
    EXPECT_EQ(0u, hwParser.getCommandCount<STATE_SIP>());
}

HWTEST2_F(CommandStreamReceiverHwTest, givenMidThreadPreemptionAllocationOnDemandWhenFlushingImmediateTasksThenPreemptionStateIsProgrammedOnceOnFirstMidThreadPreemptionKernel, IsWithinXe2HpgCoreAndXe3Core) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    pDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ASSERT_TRUE(commandStreamReceiver.midThreadPreemptionAllocationOnDemand);
    commandStreamReceiver.getMemoryManager()->freeGraphicsMemory(commandStreamReceiver.preemptionAllocation);
    commandStreamReceiver.preemptionAllocation = nullptr;
    commandStreamReceiver.storeMakeResidentAllocations = true;

    immediateFlushTaskFlags.hasMidThreadPreemptionKernel = false;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(nullptr, commandStreamReceiver.getPreemptionAllocation());
    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_EQ(0u, hwParser.getCommandCount<STATE_CONTEXT_DATA_BASE_ADDRESS>());
    EXPECT_EQ(0u, hwParser.getCommandCount<STATE_SIP>());

    auto usedSize = commandStreamReceiver.commandStream.getUsed();
    immediateFlushTaskFlags.hasMidThreadPreemptionKernel = true;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    auto preemptionAllocation = commandStreamReceiver.getPreemptionAllocation();
    ASSERT_NE(nullptr, preemptionAllocation);
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(preemptionAllocation));
    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());
    hwParser.tearDown();
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    auto stateContextDataBaseAddressCmds = hwParser.getCommandsList<STATE_CONTEXT_DATA_BASE_ADDRESS>();
    ASSERT_EQ(1u, stateContextDataBaseAddressCmds.size());
    auto stateContextDataBaseAddress = genCmdCast<STATE_CONTEXT_DATA_BASE_ADDRESS *>(*stateContextDataBaseAddressCmds.begin());
    EXPECT_EQ(preemptionAllocation->getGpuAddressToPatch(), stateContextDataBaseAddress->getContextDataBaseAddress());
    EXPECT_EQ(1u, hwParser.getCommandCount<STATE_SIP>());

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(preemptionAllocation, commandStreamReceiver.getPreemptionAllocation());
    hwParser.tearDown();
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    EXPECT_EQ(0u, hwParser.getCommandCount<STATE_CONTEXT_DATA_BASE_ADDRESS>());
    EXPECT_EQ(0u, hwParser.getCommandCount<STATE_SIP>());
}

template <typename FamilyType>
class AuxTableLriWritingPageTableMngr : public MockGmmPageTableMngr {
  public:
    static constexpr uint32_t lowRegisterOffset = 0x12340;
    static constexpr uint32_t highRegisterOffset = 0x12344;

    bool initContextAuxTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType) override {
        GmmCallbacks<FamilyType>::writeL3Address(initialBBHandle, 0x123456000ull, (static_cast<uint64_t>(highRegisterOffset) << 32) | lowRegisterOffset);
        return MockGmmPageTableMngr::initContextAuxTableRegister(initialBBHandle, engineType);
    }

    static uint32_t countLris(LinearStream &stream, size_t startOffset) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(stream, startOffset);
        uint32_t count = 0;
        for (auto &cmd : hwParser.cmdList) {
            auto lri = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(cmd);
            if (lri != nullptr && (lri->getRegisterOffset() == lowRegisterOffset || lri->getRegisterOffset() == highRegisterOffset)) {
                count++;
            }
        }
        return count;
    }
};

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerWhenFlushingBcsTaskStreamThenAuxTableRegistersAreProgrammedOnlyOnceInSubmittedCsrStreamBeforeJumpToTaskStream) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.dispatchMode = DispatchMode::immediateDispatch;
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);
    auto dispatchBcsFlags = DispatchFlagsHelper::createDefaultBcsDispatchFlags();

    auto csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    auto taskStreamStart = commandStream.getUsed();
    auto completionStamp = commandStreamReceiver.flushBcsTask(commandStream, taskStreamStart, dispatchBcsFlags, pDevice->getHardwareInfo());

    EXPECT_LT(completionStamp.taskCount, CompletionStamp::notReady);
    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_EQ(&commandStreamReceiver, pageTableManager->initContextAuxTableRegisterParamsPassed[0].initialBBHandle);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));

    EXPECT_EQ(&commandStreamReceiver.commandStream, commandStreamReceiver.latestFlushedBatchBuffer.stream);
    EXPECT_EQ(csrStreamStart, commandStreamReceiver.latestFlushedBatchBuffer.startOffset);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, csrStreamStart);
    uint32_t lrisBeforeJump = 0;
    MI_BATCH_BUFFER_START *jumpToTaskStream = nullptr;
    for (auto &cmd : hwParser.cmdList) {
        if (auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(cmd); bbStart != nullptr && jumpToTaskStream == nullptr) {
            jumpToTaskStream = bbStart;
        } else if (auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmd); lri != nullptr && jumpToTaskStream == nullptr &&
                                                                       (lri->getRegisterOffset() == AuxTableLriWritingPageTableMngr<FamilyType>::lowRegisterOffset ||
                                                                        lri->getRegisterOffset() == AuxTableLriWritingPageTableMngr<FamilyType>::highRegisterOffset)) {
            lrisBeforeJump++;
        }
    }
    ASSERT_NE(nullptr, jumpToTaskStream);
    EXPECT_EQ(commandStream.getGpuBase() + taskStreamStart, jumpToTaskStream->getBatchBufferStartAddress());
    EXPECT_EQ(2u, lrisBeforeJump);

    csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushBcsTask(commandStream, commandStream.getUsed(), dispatchBcsFlags, pDevice->getHardwareInfo());

    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_EQ(0u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));
}

HWTEST_F(CommandStreamReceiverHwTest, givenNoPageTableManagerWhenFlushingBcsTaskStreamWithCmdListDispatchThenTaskStreamIsSubmittedDirectly) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.dispatchMode = DispatchMode::immediateDispatch;
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    commandStreamReceiver.pageTableManager.reset();
    auto dispatchBcsFlags = DispatchFlagsHelper::createDefaultBcsDispatchFlags();
    dispatchBcsFlags.dispatchOperation = NEO::AppendOperations::cmdList;

    auto taskStreamStart = commandStream.getUsed();
    commandStreamReceiver.flushBcsTask(commandStream, taskStreamStart, dispatchBcsFlags, pDevice->getHardwareInfo());

    EXPECT_EQ(&commandStream, commandStreamReceiver.latestFlushedBatchBuffer.stream);
    EXPECT_EQ(taskStreamStart, commandStreamReceiver.latestFlushedBatchBuffer.startOffset);
}

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerWhenFlushingBcsTaskStreamWithCmdListDispatchThenAuxTableRegistersAreProgrammedInSubmittedCsrStream) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.dispatchMode = DispatchMode::immediateDispatch;
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    auto dispatchBcsFlags = DispatchFlagsHelper::createDefaultBcsDispatchFlags();
    dispatchBcsFlags.dispatchOperation = NEO::AppendOperations::cmdList;

    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);
    auto csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    auto taskStreamStart = commandStream.getUsed();
    commandStreamReceiver.flushBcsTask(commandStream, taskStreamStart, dispatchBcsFlags, pDevice->getHardwareInfo());

    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));
    EXPECT_EQ(&commandStreamReceiver.commandStream, commandStreamReceiver.latestFlushedBatchBuffer.stream);
    EXPECT_EQ(csrStreamStart, commandStreamReceiver.latestFlushedBatchBuffer.startOffset);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, csrStreamStart);
    auto bbStart = hwParser.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    EXPECT_EQ(commandStream.getGpuBase() + taskStreamStart, bbStart->getBatchBufferStartAddress());
}

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerRequiringInitializationWhenGettingRequiredBcsCmdStreamSizeThenAuxTableRegistersAreIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.pageTableManager.reset();
    auto dispatchBcsFlags = DispatchFlagsHelper::createDefaultBcsDispatchFlags();
    auto sizeWithoutPageTableManager = commandStreamReceiver.getRequiredCmdStreamSize(dispatchBcsFlags);

    commandStreamReceiver.pageTableManager.reset(new MockGmmPageTableMngr());
    EXPECT_EQ(sizeWithoutPageTableManager + 2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM), commandStreamReceiver.getRequiredCmdStreamSize(dispatchBcsFlags));

    commandStreamReceiver.pageTableManagerInitialized = true;
    EXPECT_EQ(sizeWithoutPageTableManager, commandStreamReceiver.getRequiredCmdStreamSize(dispatchBcsFlags));
}

HWTEST_F(CommandStreamReceiverHwTest, givenAuxTableRegistersSubmissionFailureWhenFlushingBcsTaskStreamThenRegistersAreProgrammedAgainOnNextFlush) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.dispatchMode = DispatchMode::immediateDispatch;
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);
    auto dispatchBcsFlags = DispatchFlagsHelper::createDefaultBcsDispatchFlags();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    auto completionStamp = commandStreamReceiver.flushBcsTask(commandStream, commandStream.getUsed(), dispatchBcsFlags, pDevice->getHardwareInfo());
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, completionStamp.taskCount);
    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::success;
    auto csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    completionStamp = commandStreamReceiver.flushBcsTask(commandStream, commandStream.getUsed(), dispatchBcsFlags, pDevice->getHardwareInfo());

    EXPECT_LT(completionStamp.taskCount, CompletionStamp::notReady);
    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));
}

HWTEST_F(CommandStreamReceiverHwTest, givenAuxTableRegistersSubmissionFailureWhenFlushingBcsTaskBlitThenRegistersAreProgrammedAgainOnNextFlush) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          commandStreamReceiver, commandStreamReceiver.getTagAllocation(), nullptr,
                                                                          commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(),
                                                                          commandStreamReceiver.getTagAllocation()->getGpuAddress(), 0,
                                                                          0, 0, 0, 0, 0, 0, 0);
    BlitPropertiesContainer container;
    container.push_back(blitProperties);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, commandStreamReceiver.flushBcsTask(container, false, *pDevice));
    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::success;
    auto csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    EXPECT_LT(commandStreamReceiver.flushBcsTask(container, false, *pDevice), CompletionStamp::notReady);

    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));

    csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    EXPECT_LT(commandStreamReceiver.flushBcsTask(container, false, *pDevice), CompletionStamp::notReady);
    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_EQ(0u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));
}

HWTEST_F(CommandStreamReceiverHwTest, givenAuxTableRegistersSubmissionFailureWhenFlushingTaskThenRegistersAreProgrammedAgainOnNextFlush) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.dispatchMode = DispatchMode::immediateDispatch;
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    auto completionStamp = commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, completionStamp.taskCount);
    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::success;
    auto csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    completionStamp = commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);

    EXPECT_LT(completionStamp.taskCount, CompletionStamp::notReady);
    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));
}

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerWhenFlushingTaskInBatchedDispatchThenBatchWithAuxTableRegistersIsSubmittedRightAwayAndRegistersAreProgrammedAgainAfterFailure) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    commandStreamReceiver.useNewResourceImplicitFlush = false;
    commandStreamReceiver.useGpuIdleImplicitFlush = false;
    auto &commandBufferList = commandStreamReceiver.submissionAggregator->peekCmdBufferList();
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);
    EXPECT_TRUE(commandBufferList.empty());
    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());

    commandStreamReceiver.flushReturnValue.reset();
    auto csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);
    EXPECT_TRUE(commandBufferList.empty());
    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));

    commandStream.getSpace(4);
    csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);
    EXPECT_FALSE(commandBufferList.empty());
    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_EQ(0u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));

    EXPECT_TRUE(commandStreamReceiver.flushBatchedSubmissions());
}

HWTEST_F(CommandStreamReceiverHwTest, givenAuxTableRegistersSubmissionFailureWhenFlushingMiFlushDwThenRegistersAreProgrammedAgainOnNextFlush) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushMiFlushDW(false));
    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::success;
    auto csrStreamStart = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushMiFlushDW(false));

    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, csrStreamStart));
}

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerWhenFlushingImmediateTaskThenAuxTableRegistersAreProgrammedOnlyOnceInCsrStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM), commandStreamReceiver.getCmdSizeForPageTableManagerRegisters());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_EQ(&commandStreamReceiver, pageTableManager->initContextAuxTableRegisterParamsPassed[0].initialBBHandle);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForPageTableManagerRegisters());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, 0));

    auto usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_EQ(0u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, usedSize));
}

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerWhenFlushingImmediateCmdListDispatchThenAuxTableRegistersAreProgrammedBeforeJumpingToImmediateStream) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    auto immediateStartOffset = commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, immediateStartOffset, immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, 0));

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto bbStart = hwParser.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    EXPECT_EQ(commandStream.getGpuBase() + immediateStartOffset, bbStart->getBatchBufferStartAddress());
}

HWTEST_F(CommandStreamReceiverHwTest, givenAuxTableRegistersInitializationFailureWhenFlushingImmediateTaskThenInitializationIsRetriedOnNextFlush) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto pageTableManager = new MockGmmPageTableMngr();
    pageTableManager->initContextAuxTableRegisterResult = false;
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());

    pageTableManager->initContextAuxTableRegisterResult = true;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
}

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerRequiringInitializationWhenGettingRequiredCmdStreamSizeThenAuxTableRegistersAreIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.pageTableManager.reset();
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    auto sizeWithoutPageTableManager = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    commandStreamReceiver.pageTableManager.reset(new MockGmmPageTableMngr());
    EXPECT_EQ(sizeWithoutPageTableManager + 2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM), commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice));

    commandStreamReceiver.pageTableManagerInitialized = true;
    EXPECT_EQ(sizeWithoutPageTableManager, commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice));
}

HWTEST_F(CommandStreamReceiverHwTest, givenNoPageTableManagerThenAuxTableRegistersInitializationIsNotRequired) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.pageTableManager.reset();

    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForPageTableManagerRegisters());
}

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerWhenFlushingPipeControlThenAuxTableRegistersAreProgrammedInSubmittedCsrStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    auto usedSize = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushPipeControl(false));

    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, usedSize));
    EXPECT_EQ(usedSize, commandStreamReceiver.latestFlushedBatchBuffer.startOffset);

    usedSize = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushPipeControl(false));

    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_EQ(0u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, usedSize));
}

HWTEST_F(CommandStreamReceiverHwTest, givenPageTableManagerWhenFlushingMiFlushDwThenAuxTableRegistersAreProgrammedInSubmittedCsrStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    auto usedSize = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushMiFlushDW(false));

    EXPECT_EQ(1u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, usedSize));
    EXPECT_EQ(usedSize, commandStreamReceiver.latestFlushedBatchBuffer.startOffset);
}

HWTEST_F(CommandStreamReceiverHwTest, givenAuxTableRegistersSubmissionFailureWhenFlushingPipeControlThenRegistersAreProgrammedAgainOnNextFlush) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushPipeControl(false));
    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::success;
    auto usedSize = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushPipeControl(false));

    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, usedSize));

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushPipeControl(false));

    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(0u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, usedSize));
}

HWTEST_F(CommandStreamReceiverHwTest, givenAuxTableRegistersSubmissionFailureWhenFlushingImmediateTaskThenRegistersAreProgrammedAgainOnNextFlush) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto pageTableManager = new AuxTableLriWritingPageTableMngr<FamilyType>();
    commandStreamReceiver.pageTableManager.reset(pageTableManager);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isPageTableManagerInitializationRequired());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::success;
    auto usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(2u, pageTableManager->initContextAuxTableRegisterCalled);
    EXPECT_FALSE(commandStreamReceiver.isPageTableManagerInitializationRequired());
    EXPECT_EQ(2u, AuxTableLriWritingPageTableMngr<FamilyType>::countLris(commandStreamReceiver.commandStream, usedSize));
}

HWTEST2_F(CommandStreamReceiverHwTest, givenScratchAllocationFailureWhenFlushingTaskThenOutOfDeviceMemoryIsReturnedAndNothingIsDispatched, IsHeapfulRequiredAndAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.heaplessModeEnabled = false;
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    commandStreamReceiver.setRequiredScratchSizes(0x100, 0);
    const auto taskCountBefore = commandStreamReceiver.peekTaskCount();
    const auto taskStreamUsedBefore = commandStream.getUsed();
    const auto csrStreamUsedBefore = commandStreamReceiver.commandStream.getUsed();

    auto completionStamp = commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, completionStamp.taskCount);
    EXPECT_EQ(taskCountBefore, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(taskStreamUsedBefore, commandStream.getUsed());
    EXPECT_EQ(csrStreamUsedBefore, commandStreamReceiver.commandStream.getUsed());
    EXPECT_EQ(nullptr, commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation());

    memoryManager->forceFailureInPrimaryAllocation = false;
    completionStamp = commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *pDevice);
    EXPECT_EQ(taskCountBefore + 1, completionStamp.taskCount);
    EXPECT_NE(nullptr, commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation());
}

HWTEST2_F(CommandStreamReceiverHwTest, givenScratchAllocationFailureWhenFlushingImmediateTaskThenOutOfDeviceMemoryIsReturnedAndNothingIsDispatched, IsHeapfulRequiredAndAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.heaplessModeEnabled = false;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    commandStreamReceiver.setRequiredScratchSizes(0x100, 0);
    const auto taskCountBefore = commandStreamReceiver.peekTaskCount();
    const auto csrStreamUsedBefore = commandStreamReceiver.commandStream.getUsed();

    auto completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, completionStamp.taskCount);
    EXPECT_EQ(taskCountBefore, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(csrStreamUsedBefore, commandStreamReceiver.commandStream.getUsed());
    EXPECT_EQ(nullptr, commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation());

    memoryManager->forceFailureInPrimaryAllocation = false;
    completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    EXPECT_EQ(taskCountBefore + 1, completionStamp.taskCount);
    EXPECT_NE(nullptr, commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation());
}

HWTEST_F(CommandStreamReceiverHwTest, givenNoScratchRequiredWhenReservingScratchSpaceThenNothingIsAllocated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.reserveScratchSpace(0u, 0u));
    EXPECT_EQ(nullptr, commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation());
}

HWTEST_F(CommandStreamReceiverHwTest, givenScratchRequiredWhenReservingScratchSpaceThenScratchOfCsrUsedForFlushIsAllocated) {
    auto &primaryCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    UltCommandStreamReceiver<FamilyType> secondaryCsr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    secondaryCsr.setupContext(*pDevice->getDefaultEngine().osContext);
    secondaryCsr.setPrimaryCsr(&primaryCsr);

    secondaryCsr.heaplessModeEnabled = false;
    EXPECT_TRUE(secondaryCsr.reserveScratchSpace(0u, 0x100u));
    EXPECT_TRUE(secondaryCsr.reserveScratchSpace(0x100u, 0u));
    EXPECT_NE(nullptr, secondaryCsr.getScratchSpaceController()->getScratchSpaceSlot0Allocation());
    EXPECT_EQ(nullptr, primaryCsr.getScratchSpaceController()->getScratchSpaceSlot0Allocation());

    secondaryCsr.heaplessModeEnabled = true;
    EXPECT_TRUE(secondaryCsr.reserveScratchSpace(0x100u, 0u));
    EXPECT_NE(nullptr, primaryCsr.getScratchSpaceController()->getScratchSpaceSlot0Allocation());

    secondaryCsr.setPrimaryCsr(nullptr);
}

HWTEST_F(CommandStreamReceiverHwTest, givenScratchAllocationFailureWhenReservingScratchSpaceThenFalseIsReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    EXPECT_FALSE(commandStreamReceiver.reserveScratchSpace(0x100u, 0u));
    EXPECT_EQ(nullptr, commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation());

    memoryManager->forceFailureInPrimaryAllocation = false;
}
