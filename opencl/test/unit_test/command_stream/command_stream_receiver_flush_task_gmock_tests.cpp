/*
 * Copyright (C) 2018-2025 Intel Corporation
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
#include "shared/source/gen_common/reg_configs_common.h"
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
#include "shared/test/common/helpers/ult_gfx_core_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/built_in_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"
#include "test_traits_common.h"

using namespace NEO;

using CommandStreamReceiverFlushTaskGmockTests = UltCommandStreamReceiverTest;
using CommandStreamReceiverFlushTaskGmockTestsWithMockCsrHw2 = UltCommandStreamReceiverTestWithCsrT<MockCsrHw2>;

HWTEST2_TEMPLATED_F(CommandStreamReceiverFlushTaskGmockTestsWithMockCsrHw2,
                    givenCsrInBatchingModeThreeRecordedCommandBufferEnabledBatchBufferFlatteningAndPatchInfoCollectionWhenFlushBatchedSubmissionsIsCalledThenBatchBuffersAndPatchInfoAreCollected, MatchAny) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::batchedDispatch));
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    debugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);
    bool heaplessStateInitEnabled = mockCsr->getHeaplessStateInitEnabled();
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;

    uint32_t expectedCallsCount = TestTraits<FamilyType::gfxCoreFamily>::iohInSbaSupported ? 10 : 9;
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        --expectedCallsCount;
    }

    if (mockCsr->getHeaplessStateInitEnabled()) {
        expectedCallsCount = 6;
    }

    size_t removePatchInfoDataCount = 4 * UltMemorySynchronizationCommands<FamilyType>::getExpectedPipeControlCount(pDevice->getRootDeviceEnvironment());

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
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
    EXPECT_EQ(lastBatchBufferAddress, batchBufferStart->getBatchBufferStartAddress());
    EXPECT_EQ(heaplessStateInitEnabled ? 2u : 1u, mockCsr->flushCalledCount);
    EXPECT_EQ(expectedCallsCount, mockHelper->setPatchInfoDataCalled);
    EXPECT_EQ(static_cast<unsigned int>(removePatchInfoDataCount), mockHelper->removePatchInfoDataCalled);
    EXPECT_EQ(heaplessStateInitEnabled ? 3u : 4u, mockHelper->registerCommandChunkCalled);
    EXPECT_EQ(heaplessStateInitEnabled ? 2u : 3u, mockHelper->registerBatchBufferStartAddressCalled);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskGmockTestsWithMockCsrHw2, givenMockCommandStreamerWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoDataIsNotCollected) {

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.throttle = QueueThrottle::MEDIUM;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);
    EXPECT_EQ(0u, mockHelper->setPatchInfoDataCalled);
}

HWTEST2_TEMPLATED_F(CommandStreamReceiverFlushTaskGmockTestsWithMockCsrHw2, givenMockCommandStreamerWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoDataIsCollected, MatchAny) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    uint32_t expectedCallsCount = TestTraits<FamilyType::gfxCoreFamily>::iohInSbaSupported ? 4 : 3;
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        --expectedCallsCount;
    }

    if (mockCsr->getHeaplessStateInitEnabled()) {
        expectedCallsCount = 0;
    }

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(expectedCallsCount, mockHelper->patchInfoDataVector.size());

    for (auto &patchInfoData : mockHelper->patchInfoDataVector) {
        uint64_t expectedAddress = 0u;
        switch (patchInfoData.sourceType) {
        case PatchInfoAllocationType::dynamicStateHeap:
            expectedAddress = dsh.getGraphicsAllocation()->getGpuAddress();
            break;
        case PatchInfoAllocationType::surfaceStateHeap:
            expectedAddress = ssh.getGraphicsAllocation()->getGpuAddress();
            break;
        case PatchInfoAllocationType::indirectObjectHeap:
            expectedAddress = ioh.getGraphicsAllocation()->getGpuAddress();
            break;
        default:
            expectedAddress = 0u;
        }
        EXPECT_EQ(expectedAddress, patchInfoData.sourceAllocation);
        EXPECT_EQ(0u, patchInfoData.sourceAllocationOffset);
        EXPECT_EQ(PatchInfoAllocationType::defaultType, patchInfoData.targetType);
    }

    EXPECT_EQ(expectedCallsCount, mockHelper->setPatchInfoDataCalled);
}

HWTEST2_F(CommandStreamReceiverFlushTaskGmockTests, givenMockCsrWhenCollectStateBaseAddresPatchInfoIsCalledThenAppropriateAddressesAreTaken, MatchAny) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    mockCsr->overwriteFlatBatchBufferHelper(mockHelper);

    uint32_t expectedCallsCount = TestTraits<FamilyType::gfxCoreFamily>::iohInSbaSupported ? 4 : 3;
    auto dshPatchIndex = 0u;
    auto gshPatchIndex = 1u;
    auto sshPatchIndex = 2u;
    auto iohPatchIndex = 3u;
    const bool deviceUsesDsh = pDevice->getHardwareInfo().capabilityTable.supportsImages;
    if (!deviceUsesDsh) {
        --expectedCallsCount;
        gshPatchIndex = 0u;
        sshPatchIndex = 1u;
        iohPatchIndex = 2u;
    }

    uint64_t baseAddress = 0xabcdef;
    uint64_t commandOffset = 0xa;
    uint64_t generalStateBase = 0xff;

    mockCsr->collectStateBaseAddresPatchInfo(baseAddress, commandOffset, &dsh, &ioh, &ssh, generalStateBase, deviceUsesDsh);

    ASSERT_EQ(mockHelper->patchInfoDataVector.size(), expectedCallsCount);

    for (auto &patch : mockHelper->patchInfoDataVector) {
        EXPECT_EQ(patch.targetAllocation, baseAddress);
        EXPECT_EQ(patch.sourceAllocationOffset, 0u);
    }

    // DSH
    if (deviceUsesDsh) {
        PatchInfoData dshPatch = mockHelper->patchInfoDataVector[dshPatchIndex];
        EXPECT_EQ(dshPatch.sourceAllocation, dsh.getGraphicsAllocation()->getGpuAddress());
        EXPECT_EQ(dshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::DYNAMICSTATEBASEADDRESS_BYTEOFFSET);
    }

    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::iohInSbaSupported) {
        // IOH
        PatchInfoData iohPatch = mockHelper->patchInfoDataVector[iohPatchIndex];

        EXPECT_EQ(iohPatch.sourceAllocation, ioh.getGraphicsAllocation()->getGpuAddress());
        EXPECT_EQ(iohPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::INDIRECTOBJECTBASEADDRESS_BYTEOFFSET);
    }

    // SSH
    PatchInfoData sshPatch = mockHelper->patchInfoDataVector[sshPatchIndex];
    EXPECT_EQ(sshPatch.sourceAllocation, ssh.getGraphicsAllocation()->getGpuAddress());
    EXPECT_EQ(sshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::SURFACESTATEBASEADDRESS_BYTEOFFSET);

    // GSH
    PatchInfoData gshPatch = mockHelper->patchInfoDataVector[gshPatchIndex];
    EXPECT_EQ(gshPatch.sourceAllocation, generalStateBase);
    EXPECT_EQ(gshPatch.targetAllocationOffset, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::GENERALSTATEBASEADDRESS_BYTEOFFSET);

    EXPECT_EQ(0u, mockHelper->registerCommandChunkCalled);
    EXPECT_EQ(expectedCallsCount, mockHelper->setPatchInfoDataCalled);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandStreamReceiverFlushTaskGmockTests, givenPatchInfoCollectionEnabledWhenScratchSpaceIsProgrammedThenPatchInfoIsCollected) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    mockCsr->overwriteFlatBatchBufferHelper(new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment));

    bool stateBaseAddressDirty;
    bool vfeStateDirty;
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(DeviceBitfield(8)));
    mockCsr->setupContext(osContext);
    mockCsr->getScratchSpaceController()->setRequiredScratchSpace(nullptr, 0u, 10u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, vfeStateDirty);

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    mockCsr->requiredScratchSlot0Size = 0x200000;

    mockCsr->programVFEState(commandStream, flags, 10);
    ASSERT_EQ(1u, mockCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
    EXPECT_EQ(mockCsr->getScratchSpaceController()->getScratchPatchAddress(), mockCsr->getFlatBatchBufferHelper().getPatchInfoCollection().at(0).sourceAllocation);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandStreamReceiverFlushTaskGmockTests, givenPatchInfoCollectionDisabledWhenScratchSpaceIsProgrammedThenPatchInfoIsNotCollected) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    mockCsr->overwriteFlatBatchBufferHelper(new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment));

    bool stateBaseAddressDirty;
    bool vfeStateDirty;
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(DeviceBitfield(8)));
    mockCsr->setupContext(osContext);
    mockCsr->getScratchSpaceController()->setRequiredScratchSpace(nullptr, 0u, 10u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, vfeStateDirty);

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    mockCsr->requiredScratchSlot0Size = 0x200000;

    mockCsr->programVFEState(commandStream, flags, 10);
    EXPECT_EQ(0u, mockCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandStreamReceiverFlushTaskGmockTests, givenPatchInfoCollectionEnabledWhenMediaVfeStateIsProgrammedWithEmptyScratchThenPatchInfoIsNotCollected) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    mockCsr->overwriteFlatBatchBufferHelper(new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment));

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    mockCsr->requiredScratchSlot0Size = 0x200000;
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(DeviceBitfield(8)));
    mockCsr->setupContext(osContext);

    mockCsr->programVFEState(commandStream, flags, 10);
    EXPECT_EQ(0u, mockCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
}
