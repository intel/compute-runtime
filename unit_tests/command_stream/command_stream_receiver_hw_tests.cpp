/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "reg_configs_common.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/preamble.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/command_stream/preemption.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "unit_tests/libult/create_command_stream.h"
#include "test.h"
#include "runtime/utilities/linux/debug_env_reader.h"
#include "runtime/command_queue/gpgpu_walker.h"

#include <memory>

using namespace OCLRT;

HWCMDTEST_F(IGFX_GEN8_CORE, UltCommandStreamReceiverTest, givenPreambleSentAndThreadArbitrationPolicyNotChangedWhenEstimatingPreambleCmdSizeThenReturnItsValue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy;
    auto expectedCmdSize = sizeof(typename FamilyType::PIPE_CONTROL) + sizeof(typename FamilyType::MEDIA_VFE_STATE);
    EXPECT_EQ(expectedCmdSize, commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice));
}

HWCMDTEST_F(IGFX_GEN8_CORE, UltCommandStreamReceiverTest, givenNotSentStateSipWhenFirstTaskIsFlushedThenStateSipCmdIsAddedAndIsStateSipSentSetToTrue) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);

        auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.isPreambleSent = true;

        CommandQueueHw<FamilyType> commandQueue(nullptr, mockDevice.get(), 0);
        auto &commandStream = commandQueue.getCS(4096u);

        DispatchFlags dispatchFlags;
        dispatchFlags.preemptionMode = PreemptionMode::MidThread;

        void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

        std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
        std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

        csr.flushTask(commandStream,
                      0,
                      *heap.get(),
                      *heap.get(),
                      *heap.get(),
                      0,
                      dispatchFlags,
                      *mockDevice);

        EXPECT_TRUE(csr.isStateSipSent);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr.getCS(0));

        auto stateSipItor = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_NE(hwParser.cmdList.end(), stateSipItor);

        alignedFree(buffer);
    }
}

HWTEST_F(UltCommandStreamReceiverTest, givenCsrWhenProgramStateSipIsCalledThenIsStateSipCalledIsSetToTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*pDevice);
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programStateSip(cmdStream, *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isStateSipSent);
}

HWTEST_F(UltCommandStreamReceiverTest, givenSentStateSipFlagSetWhenGetRequiredStateSipCmdSizeIsCalledThenStateSipCmdSizeIsNotIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags;

    commandStreamReceiver.isStateSipSent = false;
    auto sizeWithStateSipIsNotSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    commandStreamReceiver.isStateSipSent = true;
    auto sizeWhenSipIsSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    auto sizeForStateSip = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*pDevice);
    EXPECT_EQ(sizeForStateSip, sizeWithStateSipIsNotSent - sizeWhenSipIsSent);
}

HWTEST_F(UltCommandStreamReceiverTest, givenPreambleSentAndThreadArbitrationPolicyChangedWhenEstimatingPreambleCmdSizeThenResultDependsOnPolicyProgrammingCmdSize) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;

    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy;
    auto policyNotChanged = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy + 1;
    auto policyChanged = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifference = policyChanged - policyNotChanged;
    auto expectedDifference = PreambleHelper<FamilyType>::getThreadArbitrationCommandsSize();
    EXPECT_EQ(expectedDifference, actualDifference);
}

HWTEST_F(UltCommandStreamReceiverTest, givenPreambleSentWhenEstimatingPreambleCmdSizeThenResultDependsOnPolicyProgrammingAndAdditionalCmdsSize) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy;

    commandStreamReceiver.isPreambleSent = false;
    auto preambleNotSent = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.isPreambleSent = true;
    auto preambleSent = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifference = preambleNotSent - preambleSent;
    auto expectedDifference = PreambleHelper<FamilyType>::getThreadArbitrationCommandsSize() + PreambleHelper<FamilyType>::getAdditionalCommandsSize(*pDevice);
    EXPECT_EQ(expectedDifference, actualDifference);
}

HWCMDTEST_F(IGFX_GEN8_CORE, UltCommandStreamReceiverTest, givenMediaVfeStateDirtyEstimatingPreambleCmdSizeThenResultDependsVfeStateProgrammingCmdSize) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.overrideMediaVFEStateDirty(false);
    auto notDirty = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.overrideMediaVFEStateDirty(true);
    auto dirty = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifference = dirty - notDirty;
    auto expectedDifference = sizeof(PIPE_CONTROL) + sizeof(MEDIA_VFE_STATE);
    EXPECT_EQ(expectedDifference, actualDifference);
}

HWTEST_F(UltCommandStreamReceiverTest, givenCommandStreamReceiverInInitialStateWhenHeapsAreAskedForDirtyStatusThenTrueIsReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(0u, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(0u, commandStreamReceiver.peekTaskLevel());

    EXPECT_TRUE(commandStreamReceiver.dshState.updateAndCheck(&dsh));
    EXPECT_TRUE(commandStreamReceiver.iohState.updateAndCheck(&ioh));
    EXPECT_TRUE(commandStreamReceiver.sshState.updateAndCheck(&ssh));
}

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTests;

HWTEST_F(CommandStreamReceiverFlushTests, addsBatchBufferEnd) {
    auto usedPrevious = commandStream.getUsed();

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(commandStream, nullptr);

    EXPECT_EQ(commandStream.getUsed(), usedPrevious + sizeof(typename FamilyType::MI_BATCH_BUFFER_END));

    auto batchBufferEnd = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_END *>(
        ptrOffset(commandStream.getCpuBase(), usedPrevious));
    EXPECT_NE(nullptr, batchBufferEnd);
}

HWTEST_F(CommandStreamReceiverFlushTests, shouldAlignToCacheLineSize) {
    commandStream.getSpace(sizeof(uint32_t));
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(commandStream);

    EXPECT_EQ(0u, commandStream.getUsed() % MemoryConstants::cacheLineSize);
}

typedef Test<DeviceFixture> CommandStreamReceiverHwTest;

HWTEST_F(CommandStreamReceiverHwTest, givenCsrHwWhenTypeIsCheckedThenCsrHwIsReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    auto csr = std::unique_ptr<CommandStreamReceiver>(CommandStreamReceiverHw<FamilyType>::create(hwInfo, *pDevice->executionEnvironment));

    EXPECT_EQ(CommandStreamReceiverType::CSR_HW, csr->getType());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverHwTest, WhenCommandStreamReceiverHwIsCreatedThenDefaultSshSizeIs64KB) {
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    EXPECT_EQ(64 * KB, commandStreamReceiver.defaultSshSize);
}
