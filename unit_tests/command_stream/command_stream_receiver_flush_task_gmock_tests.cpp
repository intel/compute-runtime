/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/preamble.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/utilities/linux/debug_env_reader.h"
#include "test.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"

using namespace OCLRT;

using ::testing::_;
using ::testing::Invoke;

using CommandStreamReceiverFlushTaskGmockTests = UltCommandStreamReceiverTest;

HWTEST_F(CommandStreamReceiverFlushTaskGmockTests, givenCsrInBatchingModeThreeRecordedCommandBufferEnabledBatchBufferFlatteningAndPatchInfoCollectionWhenFlushBatchedSubmissionsIsCalledThenBatchBuffersAndPatchInfoAreCollected) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment);
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;

    EXPECT_CALL(*mockHelper, setPatchInfoData(::testing::_)).Times(10);
    EXPECT_CALL(*mockHelper, removePatchInfoData(::testing::_)).Times(4 * mockCsr->getRequiredPipeControlSize() / sizeof(PIPE_CONTROL));
    EXPECT_CALL(*mockHelper, registerCommandChunk(::testing::_)).Times(4);
    EXPECT_CALL(*mockHelper, registerBatchBufferStartAddress(::testing::_, ::testing::_)).Times(3);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto lastBatchBuffer = primaryBatch->next->next;

    auto bbEndLocation = primaryBatch->next->batchBufferEndLocation;
    auto lastBatchBufferAddress = (uint64_t)ptrOffset(lastBatchBuffer->batchBuffer.commandBufferAllocation->getGpuAddress(),
                                                      lastBatchBuffer->batchBuffer.startOffset);

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(lastBatchBufferAddress, batchBufferStart->getBatchBufferStartAddressGraphicsaddress472());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskGmockTests, givenMockCommandStreamerWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoDataIsNotCollected) {

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment);
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(mockCsr);

    DispatchFlags dispatchFlags;
    dispatchFlags.throttle = QueueThrottle::MEDIUM;

    EXPECT_CALL(*mockHelper, setPatchInfoData(_)).Times(0);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);
}

HWTEST_F(CommandStreamReceiverFlushTaskGmockTests, givenMockCommandStreamerWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoDataIsCollected) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment);
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);

    pDevice->resetCommandStreamReceiver(mockCsr);

    DispatchFlags dispatchFlags;
    dispatchFlags.throttle = QueueThrottle::MEDIUM;

    std::vector<PatchInfoData> patchInfoDataVector;
    EXPECT_CALL(*mockHelper, setPatchInfoData(_))
        .Times(4)
        .WillRepeatedly(Invoke([&](const PatchInfoData &data) {
            patchInfoDataVector.push_back(data);
            return true;
        }));

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(4u, patchInfoDataVector.size());

    for (auto &patchInfoData : patchInfoDataVector) {
        uint64_t expectedAddress = 0u;
        switch (patchInfoData.sourceType) {
        case PatchInfoAllocationType::DynamicStateHeap:
            expectedAddress = dsh.getGraphicsAllocation()->getGpuAddress();
            break;
        case PatchInfoAllocationType::SurfaceStateHeap:
            expectedAddress = ssh.getGraphicsAllocation()->getGpuAddress();
            break;
        case PatchInfoAllocationType::IndirectObjectHeap:
            expectedAddress = ioh.getGraphicsAllocation()->getGpuAddress();
            break;
        default:
            expectedAddress = 0u;
        }
        EXPECT_EQ(expectedAddress, patchInfoData.sourceAllocation);
        EXPECT_EQ(0u, patchInfoData.sourceAllocationOffset);
        EXPECT_EQ(PatchInfoAllocationType::Default, patchInfoData.targetType);
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskGmockTests, givenMockCsrWhenCollectStateBaseAddresPatchInfoIsCalledThenAppropriateAddressesAreTaken) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment));
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);

    std::vector<PatchInfoData> patchInfoDataVector;
    EXPECT_CALL(*mockHelper, setPatchInfoData(_))
        .Times(4)
        .WillRepeatedly(Invoke([&](const PatchInfoData &data) {
            patchInfoDataVector.push_back(data);
            return true;
        }));
    EXPECT_CALL(*mockHelper, registerCommandChunk(_))
        .Times(0);

    uint64_t baseAddress = 0xabcdef;
    uint64_t commandOffset = 0xa;
    uint64_t generalStateBase = 0xff;

    mockCsr->collectStateBaseAddresPatchInfo(baseAddress, commandOffset, dsh, ioh, ssh, generalStateBase);

    ASSERT_EQ(patchInfoDataVector.size(), 4u);
    PatchInfoData dshPatch = patchInfoDataVector[0];
    PatchInfoData gshPatch = patchInfoDataVector[1];
    PatchInfoData sshPatch = patchInfoDataVector[2];
    PatchInfoData iohPatch = patchInfoDataVector[3];

    for (auto &patch : patchInfoDataVector) {
        EXPECT_EQ(patch.targetAllocation, baseAddress);
        EXPECT_EQ(patch.sourceAllocationOffset, 0u);
    }

    //DSH
    EXPECT_EQ(dshPatch.sourceAllocation, dsh.getGraphicsAllocation()->getGpuAddress());
    EXPECT_EQ(dshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::DYNAMICSTATEBASEADDRESS_BYTEOFFSET);

    //IOH
    EXPECT_EQ(iohPatch.sourceAllocation, ioh.getGraphicsAllocation()->getGpuAddress());
    EXPECT_EQ(iohPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::INDIRECTOBJECTBASEADDRESS_BYTEOFFSET);

    //SSH
    EXPECT_EQ(sshPatch.sourceAllocation, ssh.getGraphicsAllocation()->getGpuAddress());
    EXPECT_EQ(sshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::SURFACESTATEBASEADDRESS_BYTEOFFSET);

    //GSH
    EXPECT_EQ(gshPatch.sourceAllocation, generalStateBase);
    EXPECT_EQ(gshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::GENERALSTATEBASEADDRESS_BYTEOFFSET);
}
