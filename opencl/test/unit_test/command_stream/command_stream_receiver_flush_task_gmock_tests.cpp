/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/ult_hw_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/built_in_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_os_context.h"
#include "opencl/test/unit_test/mocks/mock_submissions_aggregator.h"
#include "test.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"
#include "test_traits_common.h"

using namespace NEO;

using ::testing::_;
using ::testing::Invoke;

using CommandStreamReceiverFlushTaskGmockTests = UltCommandStreamReceiverTest;

HWTEST2_F(CommandStreamReceiverFlushTaskGmockTests,
          givenCsrInBatchingModeThreeRecordedCommandBufferEnabledBatchBufferFlatteningAndPatchInfoCollectionWhenFlushBatchedSubmissionsIsCalledThenBatchBuffersAndPatchInfoAreCollected, MatchAny) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;

    constexpr uint32_t expectedCallsCount = TestTraits<gfxCoreFamily>::iohInSbaSupported ? 10 : 9;

    EXPECT_CALL(*mockHelper, setPatchInfoData(::testing::_)).Times(expectedCallsCount);
    size_t removePatchInfoDataCount = 4 * UltMemorySynchronizationCommands<FamilyType>::getExpectedPipeControlCount(pDevice->getHardwareInfo());
    EXPECT_CALL(*mockHelper, removePatchInfoData(::testing::_)).Times(static_cast<int>(removePatchInfoDataCount));
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

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(mockCsr);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
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

HWTEST2_F(CommandStreamReceiverFlushTaskGmockTests, givenMockCommandStreamerWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoDataIsCollected, MatchAny) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);

    pDevice->resetCommandStreamReceiver(mockCsr);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    constexpr uint32_t expectedCallsCount = TestTraits<gfxCoreFamily>::iohInSbaSupported ? 4 : 3;

    std::vector<PatchInfoData> patchInfoDataVector;
    EXPECT_CALL(*mockHelper, setPatchInfoData(_))
        .Times(expectedCallsCount)
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

    EXPECT_EQ(expectedCallsCount, patchInfoDataVector.size());

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

HWTEST2_F(CommandStreamReceiverFlushTaskGmockTests, givenMockCsrWhenCollectStateBaseAddresPatchInfoIsCalledThenAppropriateAddressesAreTaken, MatchAny) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);

    constexpr uint32_t expectedCallsCount = TestTraits<gfxCoreFamily>::iohInSbaSupported ? 4 : 3;

    std::vector<PatchInfoData> patchInfoDataVector;
    EXPECT_CALL(*mockHelper, setPatchInfoData(_))
        .Times(expectedCallsCount)
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

    ASSERT_EQ(patchInfoDataVector.size(), expectedCallsCount);
    PatchInfoData dshPatch = patchInfoDataVector[0];
    PatchInfoData gshPatch = patchInfoDataVector[1];
    PatchInfoData sshPatch = patchInfoDataVector[2];

    for (auto &patch : patchInfoDataVector) {
        EXPECT_EQ(patch.targetAllocation, baseAddress);
        EXPECT_EQ(patch.sourceAllocationOffset, 0u);
    }

    //DSH
    EXPECT_EQ(dshPatch.sourceAllocation, dsh.getGraphicsAllocation()->getGpuAddress());
    EXPECT_EQ(dshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::DYNAMICSTATEBASEADDRESS_BYTEOFFSET);

    if constexpr (TestTraits<gfxCoreFamily>::iohInSbaSupported) {
        //IOH
        PatchInfoData iohPatch = patchInfoDataVector[3];

        EXPECT_EQ(iohPatch.sourceAllocation, ioh.getGraphicsAllocation()->getGpuAddress());
        EXPECT_EQ(iohPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::INDIRECTOBJECTBASEADDRESS_BYTEOFFSET);
    }

    //SSH
    EXPECT_EQ(sshPatch.sourceAllocation, ssh.getGraphicsAllocation()->getGpuAddress());
    EXPECT_EQ(sshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::SURFACESTATEBASEADDRESS_BYTEOFFSET);

    //GSH
    EXPECT_EQ(gshPatch.sourceAllocation, generalStateBase);
    EXPECT_EQ(gshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::GENERALSTATEBASEADDRESS_BYTEOFFSET);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskGmockTests, givenPatchInfoCollectionEnabledWhenScratchSpaceIsProgrammedThenPatchInfoIsCollected) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    mockCsr->overwriteFlatBatchBufferHelper(new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment));

    bool stateBaseAddressDirty;
    bool vfeStateDirty;
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(DeviceBitfield(8)));
    mockCsr->setupContext(osContext);
    mockCsr->getScratchSpaceController()->setRequiredScratchSpace(nullptr, 0u, 10u, 0u, 1u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, vfeStateDirty);

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    mockCsr->requiredScratchSize = 0x200000;

    mockCsr->programVFEState(commandStream, flags, 10);
    ASSERT_EQ(1u, mockCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
    EXPECT_EQ(mockCsr->getScratchSpaceController()->getScratchPatchAddress(), mockCsr->getFlatBatchBufferHelper().getPatchInfoCollection().at(0).sourceAllocation);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskGmockTests, givenPatchInfoCollectionDisabledWhenScratchSpaceIsProgrammedThenPatchInfoIsNotCollected) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    mockCsr->overwriteFlatBatchBufferHelper(new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment));

    bool stateBaseAddressDirty;
    bool vfeStateDirty;
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(DeviceBitfield(8)));
    mockCsr->setupContext(osContext);
    mockCsr->getScratchSpaceController()->setRequiredScratchSpace(nullptr, 0u, 10u, 0u, 1u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, vfeStateDirty);

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    mockCsr->requiredScratchSize = 0x200000;

    mockCsr->programVFEState(commandStream, flags, 10);
    EXPECT_EQ(0u, mockCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskGmockTests, givenPatchInfoCollectionEnabledWhenMediaVfeStateIsProgrammedWithEmptyScratchThenPatchInfoIsNotCollected) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    mockCsr->overwriteFlatBatchBufferHelper(new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment));

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    mockCsr->requiredScratchSize = 0x200000;
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(DeviceBitfield(8)));
    mockCsr->setupContext(osContext);

    mockCsr->programVFEState(commandStream, flags, 10);
    EXPECT_EQ(0u, mockCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
}
