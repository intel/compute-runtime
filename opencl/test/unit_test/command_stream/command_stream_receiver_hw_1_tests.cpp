/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/mocks/mock_command_encoder.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/cl_blit_properties.h"
#include "opencl/test/unit_test/command_stream/command_stream_receiver_hw_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

HWCMDTEST_F(IGFX_GEN12LP_CORE, UltCommandStreamReceiverTest, givenPreambleSentAndThreadArbitrationPolicyNotChangedWhenEstimatingPreambleCmdSizeThenReturnItsValue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    auto expectedCmdSize = sizeof(typename FamilyType::PIPE_CONTROL) + sizeof(typename FamilyType::MEDIA_VFE_STATE);
    EXPECT_EQ(expectedCmdSize, commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, UltCommandStreamReceiverTest, givenNotSentStateSipWhenFirstTaskIsFlushedThenStateSipCmdIsAddedAndIsStateSipSentSetToTrue) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);

        auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.isPreambleSent = true;

        CommandQueueHw<FamilyType> commandQueue(nullptr, mockDevice.get(), 0, false);
        auto &commandStream = commandQueue.getCS(4096u);

        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.preemptionMode = PreemptionMode::MidThread;

        MockGraphicsAllocation allocation(nullptr, 0);
        IndirectHeap heap(&allocation);

        csr.flushTask(commandStream,
                      0,
                      &heap,
                      &heap,
                      &heap,
                      0,
                      dispatchFlags,
                      mockDevice->getDevice());

        EXPECT_TRUE(csr.isStateSipSent);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr.getCS(0));

        auto stateSipItor = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_NE(hwParser.cmdList.end(), stateSipItor);
    }
}

HWTEST_F(UltCommandStreamReceiverTest, givenCsrWhenProgramStateSipIsCalledThenIsStateSipCalledIsSetToTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*pDevice, commandStreamReceiver.isRcs());
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programStateSip(cmdStream, *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isStateSipSent);
}

HWTEST_F(UltCommandStreamReceiverTest, givenSentStateSipFlagSetWhenGetRequiredStateSipCmdSizeIsCalledThenStateSipCmdSizeIsNotIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    commandStreamReceiver.isStateSipSent = false;
    auto sizeWithStateSipIsNotSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    commandStreamReceiver.isStateSipSent = true;
    auto sizeWhenSipIsSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    auto sizeForStateSip = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*pDevice, commandStreamReceiver.isRcs());
    EXPECT_EQ(sizeForStateSip, sizeWithStateSipIsNotSent - sizeWhenSipIsSent);
}

HWTEST_F(UltCommandStreamReceiverTest, whenGetCmdSizeForPerDssBackedBufferIsCalledThenCorrectResultIsReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.usePerDssBackedBuffer = false;
    commandStreamReceiver.isPerDssBackedBufferSent = true;
    auto basicSize = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    {
        dispatchFlags.usePerDssBackedBuffer = true;
        commandStreamReceiver.isPerDssBackedBufferSent = true;
        auto newSize = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);
        EXPECT_EQ(basicSize, newSize);
    }
    {
        dispatchFlags.usePerDssBackedBuffer = true;
        commandStreamReceiver.isPerDssBackedBufferSent = false;
        auto newSize = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);
        EXPECT_EQ(basicSize, newSize - commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo()));
    }
}

HWTEST_F(UltCommandStreamReceiverTest, givenPreambleSentAndThreadArbitrationPolicyChangedWhenEstimatingFlushTaskSizeThenResultDependsOnPolicyProgrammingCmdSize) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;

    auto policyNotChangedPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    auto policyNotChangedFlush = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.isDirty = true;
    commandStreamReceiver.streamProperties.stateComputeMode.isCoherencyRequired.isDirty = true;
    auto policyChangedPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    auto policyChangedFlush = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    auto actualDifferenceForPreamble = policyChangedPreamble - policyNotChangedPreamble;
    auto actualDifferenceForFlush = policyChangedFlush - policyNotChangedFlush;
    auto expectedDifference = EncodeComputeMode<FamilyType>::getCmdSizeForComputeMode(pDevice->getRootDeviceEnvironment(), false, commandStreamReceiver.isRcs());
    EXPECT_EQ(0u, actualDifferenceForPreamble);
    EXPECT_EQ(expectedDifference, actualDifferenceForFlush);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, UltCommandStreamReceiverTest, givenMediaVfeStateDirtyEstimatingPreambleCmdSizeThenResultDependsVfeStateProgrammingCmdSize) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.setMediaVFEStateDirty(false);
    auto notDirty = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.setMediaVFEStateDirty(true);
    auto dirty = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifference = dirty - notDirty;
    auto expectedDifference = sizeof(PIPE_CONTROL) + sizeof(MEDIA_VFE_STATE);
    EXPECT_EQ(expectedDifference, actualDifference);
}

HWTEST_F(UltCommandStreamReceiverTest, givenCommandStreamReceiverInInitialStateWhenHeapsAreAskedForDirtyStatusThenTrueIsReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_EQ(0u, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(0u, commandStreamReceiver.peekTaskLevel());

    EXPECT_TRUE(commandStreamReceiver.dshState.updateAndCheck(&dsh));
    EXPECT_TRUE(commandStreamReceiver.iohState.updateAndCheck(&ioh));
    EXPECT_TRUE(commandStreamReceiver.sshState.updateAndCheck(&ssh));
}

HWTEST_F(UltCommandStreamReceiverTest, givenPreambleSentAndForceSemaphoreDelayBetweenWaitsFlagWhenEstimatingPreambleCmdSizeThenResultIsExpected) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DebugManagerStateRestore debugManagerStateRestore;

    debugManager.flags.ForceSemaphoreDelayBetweenWaits.set(-1);
    commandStreamReceiver.isPreambleSent = false;

    auto preambleNotSentAndSemaphoreDelayNotReprogrammed = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    debugManager.flags.ForceSemaphoreDelayBetweenWaits.set(0);
    commandStreamReceiver.isPreambleSent = false;

    auto preambleNotSentAndSemaphoreDelayReprogrammed = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.isPreambleSent = true;
    auto preambleSent = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifferenceWhenSemaphoreDelayNotReprogrammed = preambleNotSentAndSemaphoreDelayNotReprogrammed - preambleSent;
    auto expectedDifference = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*pDevice);

    EXPECT_EQ(expectedDifference, actualDifferenceWhenSemaphoreDelayNotReprogrammed);

    auto actualDifferenceWhenSemaphoreDelayReprogrammed = preambleNotSentAndSemaphoreDelayReprogrammed - preambleSent;
    expectedDifference = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*pDevice) + PreambleHelper<FamilyType>::getSemaphoreDelayCommandSize();

    EXPECT_EQ(expectedDifference, actualDifferenceWhenSemaphoreDelayReprogrammed);
}

HWTEST_F(UltCommandStreamReceiverTest, givenNoBlitterOverrideWhenBlitterNotSupportedThenExpectFalseReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = false;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_FALSE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_BCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenNoBlitterOverrideWhenBlitterSupportedThenExpectTrueReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = true;
    properties.submitOnInit = true;
    bool startOnInit = false;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_BCS, startOnInit, startInContext));
    EXPECT_TRUE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenBlitterOverrideEnableWhenBlitterNotSupportedThenExpectTrueReturned) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideBlitterSupport.set(1);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = false;
    bool startOnInit = false;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_BCS, startOnInit, startInContext));
    EXPECT_TRUE(startOnInit);
    EXPECT_TRUE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenBlitterOverrideEnableAndNoStartWhenBlitterNotSupportedThenExpectTrueReturnedStartOnInitSetToTrue) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideBlitterSupport.set(2);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = true;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_BCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_TRUE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenBlitterOverrideDisableWhenBlitterSupportedThenExpectFalseReturned) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideBlitterSupport.set(0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = true;
    properties.submitOnInit = false;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_FALSE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_BCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenNoRenderOverrideWhenRenderNotSupportedThenExpectFalseReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = false;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_FALSE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_RCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenNoRenderOverrideWhenRenderSupportedThenExpectTrueReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = true;
    properties.submitOnInit = true;
    bool startOnInit = false;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_RCS, startOnInit, startInContext));
    EXPECT_TRUE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenRenderOverrideEnableWhenRenderNotSupportedThenExpectTrueReturned) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideRenderSupport.set(1);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = false;
    bool startOnInit = false;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_RCS, startOnInit, startInContext));
    EXPECT_TRUE(startOnInit);
    EXPECT_TRUE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenRenderOverrideEnableAndNoStartWhenRenderNotSupportedThenExpectTrueReturnedAndStartOnInitSetFalse) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideRenderSupport.set(2);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = true;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_RCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_TRUE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenRenderOverrideDisableWhenRenderSupportedThenExpectFalseReturned) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideRenderSupport.set(0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = true;
    properties.submitOnInit = false;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_FALSE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_RCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenNoComputeOverrideWhenComputeNotSupportedThenExpectFalseReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = false;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_FALSE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_CCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenNoComputeOverrideWhenComputeSupportedThenExpectTrueReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = true;
    properties.submitOnInit = true;
    bool startOnInit = false;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_CCS, startOnInit, startInContext));
    EXPECT_TRUE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenComputeOverrideEnableWhenComputeNotSupportedThenExpectTrueReturned) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideComputeSupport.set(1);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = false;
    bool startOnInit = false;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_CCS, startOnInit, startInContext));
    EXPECT_TRUE(startOnInit);
    EXPECT_TRUE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenComputeOverrideEnableAndNoStartWhenComputeNotSupportedThenExpectTrueReturnedAndStartOnInitSetToFalse) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideComputeSupport.set(2);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = false;
    properties.submitOnInit = true;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_TRUE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_CCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_TRUE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenComputeOverrideDisableWhenComputeSupportedThenExpectFalseReturned) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.DirectSubmissionOverrideComputeSupport.set(0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto osContext = static_cast<MockOsContext *>(commandStreamReceiver.osContext);

    DirectSubmissionProperties properties;
    properties.engineSupported = true;
    properties.submitOnInit = false;
    bool startOnInit = true;
    bool startInContext = false;
    EXPECT_FALSE(osContext->checkDirectSubmissionSupportsEngine(properties, aub_stream::ENGINE_CCS, startOnInit, startInContext));
    EXPECT_FALSE(startOnInit);
    EXPECT_FALSE(startInContext);
}

HWTEST_F(UltCommandStreamReceiverTest, givenSinglePartitionWhenCallingWaitKmdNotifyThenExpectImplicitBusyLoopWaitCalled) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.callBaseWaitForCompletionWithTimeout = false;
    commandStreamReceiver.returnWaitForCompletionWithTimeout = NEO::WaitStatus::notReady;

    commandStreamReceiver.waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(2u, commandStreamReceiver.waitForCompletionWithTimeoutTaskCountCalled);
}

HWTEST_F(UltCommandStreamReceiverTest, givenMultiplePartitionsWhenCallingWaitKmdNotifyThenExpectExplicitBusyLoopWaitCalled) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.callBaseWaitForCompletionWithTimeout = false;
    commandStreamReceiver.returnWaitForCompletionWithTimeout = NEO::WaitStatus::notReady;

    commandStreamReceiver.waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);
    EXPECT_EQ(2u, commandStreamReceiver.waitForCompletionWithTimeoutTaskCountCalled);
}

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTests;

HWTEST_F(CommandStreamReceiverFlushTests, WhenAddingBatchBufferEndThenBatchBufferEndIsAppendedCorrectly) {
    auto usedPrevious = commandStream.getUsed();

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(commandStream, nullptr);

    EXPECT_EQ(commandStream.getUsed(), usedPrevious + sizeof(typename FamilyType::MI_BATCH_BUFFER_END));

    auto batchBufferEnd = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_END *>(
        ptrOffset(commandStream.getCpuBase(), usedPrevious));
    EXPECT_NE(nullptr, batchBufferEnd);
}
typedef Test<ClDeviceFixture> CommandStreamReceiverHwTest;

HWTEST_F(CommandStreamReceiverHwTest, givenCsrHwWhenTypeIsCheckedThenCsrHwIsReturned) {
    auto csr = std::unique_ptr<CommandStreamReceiver>(CommandStreamReceiverHw<FamilyType>::create(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    EXPECT_EQ(CommandStreamReceiverType::hardware, csr->getType());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandStreamReceiverHwTest, WhenCommandStreamReceiverHwIsCreatedThenDefaultSshSizeIs64KB) {
    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    EXPECT_EQ(64 * MemoryConstants::kiloByte, commandStreamReceiver.defaultSshSize);
}

HWTEST_F(CommandStreamReceiverHwTest, WhenScratchSpaceIsNotRequiredThenScratchAllocationIsNotCreated) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = commandStreamReceiver->getScratchSpaceController();

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    scratchController->setRequiredScratchSpace(reinterpret_cast<void *>(0x2000), 0u, 0u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_FALSE(cfeStateDirty);
    EXPECT_FALSE(stateBaseAddressDirty);
    EXPECT_EQ(nullptr, scratchController->getScratchSpaceSlot0Allocation());
    EXPECT_EQ(nullptr, scratchController->getScratchSpaceSlot1Allocation());
}

HWTEST_F(CommandStreamReceiverHwTest, WhenScratchSpaceIsRequiredThenCorrectAddressIsReturned) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver->setupContext(*pDevice->getDefaultEngine().osContext);
    auto scratchController = commandStreamReceiver->getScratchSpaceController();

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    std::unique_ptr<void, std::function<decltype(alignedFree)>> surfaceHeap(alignedMalloc(0x1000, 0x1000), alignedFree);
    scratchController->setRequiredScratchSpace(surfaceHeap.get(), 0u, 0x1000u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);

    uint64_t expectedScratchAddress = 0xAAABBBCCCDDD000ull;
    auto scratchAllocation = scratchController->getScratchSpaceSlot0Allocation();
    auto gmmHelper = pDevice->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(expectedScratchAddress);
    scratchAllocation->setCpuPtrAndGpuAddress(scratchAllocation->getUnderlyingBuffer(), canonizedGpuAddress);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateGshAddressForScratchSpace((scratchAllocation->getGpuAddress() - MemoryConstants::pageSize), scratchController->calculateNewGSH()));
}

HWTEST_F(CommandStreamReceiverHwTest, WhenScratchSpaceIsNotRequiredThenGshAddressZeroIsReturned) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = commandStreamReceiver->getScratchSpaceController();

    EXPECT_EQ(nullptr, scratchController->getScratchSpaceSlot0Allocation());
    EXPECT_EQ(0u, scratchController->calculateNewGSH());
}

HWTEST_F(CommandStreamReceiverHwTest, givenDefaultPlatformCapabilityWhenNoDebugKeysSetThenExpectDefaultPlatformSettings) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->postInitFlagsSetup();

    if (commandStreamReceiver->checkPlatformSupportsNewResourceImplicitFlush()) {
        EXPECT_TRUE(commandStreamReceiver->useNewResourceImplicitFlush);
    } else {
        EXPECT_FALSE(commandStreamReceiver->useNewResourceImplicitFlush);
    }
}

HWTEST_F(CommandStreamReceiverHwTest, givenDefaultGpuIdleImplicitFlushWhenNoDebugKeysSetThenExpectDefaultPlatformSettings) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->postInitFlagsSetup();

    if (commandStreamReceiver->checkPlatformSupportsGpuIdleImplicitFlush()) {
        EXPECT_TRUE(commandStreamReceiver->useGpuIdleImplicitFlush);
    } else {
        EXPECT_FALSE(commandStreamReceiver->useGpuIdleImplicitFlush);
    }
}

HWTEST_F(CommandStreamReceiverHwTest, WhenForceDisableNewResourceImplicitFlushThenExpectFlagSetFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PerformImplicitFlushForNewResource.set(0);

    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->postInitFlagsSetup();
    EXPECT_FALSE(commandStreamReceiver->useNewResourceImplicitFlush);
}

HWTEST_F(CommandStreamReceiverHwTest, WhenForceEnableNewResourceImplicitFlushThenExpectFlagSetTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.PerformImplicitFlushForNewResource.set(1);

    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->postInitFlagsSetup();
    EXPECT_TRUE(commandStreamReceiver->useNewResourceImplicitFlush);
}

HWTEST_F(CommandStreamReceiverHwTest, WhenForceDisableGpuIdleImplicitFlushThenExpectFlagSetFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.PerformImplicitFlushForIdleGpu.set(0);

    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->postInitFlagsSetup();
    EXPECT_FALSE(commandStreamReceiver->useGpuIdleImplicitFlush);
}

HWTEST_F(CommandStreamReceiverHwTest, WhenForceEnableGpuIdleImplicitFlushThenExpectFlagSetTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.PerformImplicitFlushForIdleGpu.set(1);

    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->postInitFlagsSetup();
    EXPECT_TRUE(commandStreamReceiver->useGpuIdleImplicitFlush);
}

HWTEST2_F(CommandStreamReceiverHwTest, whenProgramVFEStateIsCalledThenCorrectComputeOverdispatchDisableValueIsProgrammed, IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto pHwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    const auto &productHelper = pDevice->getProductHelper();

    uint8_t memory[1 * MemoryConstants::kiloByte]{};
    auto mockCsr = std::make_unique<MockCsrHw2<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(),
                                                            pDevice->getDeviceBitfield());
    MockOsContext osContext{0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, DeviceBitfield(0))};
    mockCsr->setupContext(osContext);

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto revision : revisions) {
        pHwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(revision, *pHwInfo);

        {
            mockCsr->getStreamProperties().frontEndState = {};
            mockCsr->getStreamProperties().frontEndState.initSupport(rootDeviceEnvironment);
            auto flags = DispatchFlagsHelper::createDefaultDispatchFlags();
            flags.additionalKernelExecInfo = AdditionalKernelExecInfo::disableOverdispatch;

            LinearStream commandStream{&memory, sizeof(memory)};
            mockCsr->mediaVfeStateDirty = true;
            mockCsr->programVFEState(commandStream, flags, 10);
            auto cfeState = reinterpret_cast<CFE_STATE *>(&memory);

            auto expectedDisableOverdispatch = productHelper.isDisableOverdispatchAvailable(*pHwInfo);
            EXPECT_EQ(expectedDisableOverdispatch, cfeState->getComputeOverdispatchDisable());
        }
        {
            auto flags = DispatchFlagsHelper::createDefaultDispatchFlags();
            flags.additionalKernelExecInfo = AdditionalKernelExecInfo::notSet;
            LinearStream commandStream{&memory, sizeof(memory)};
            mockCsr->mediaVfeStateDirty = true;
            mockCsr->programVFEState(commandStream, flags, 10);
            auto cfeState = reinterpret_cast<CFE_STATE *>(&memory);

            EXPECT_FALSE(cfeState->getComputeOverdispatchDisable());
        }
    }
}

HWTEST_F(BcsTests, WhenGetNumberOfBlitsForCopyPerRowIsCalledThenCorrectValuesAreReturned) {
    auto &rootDeviceEnvironment = pClDevice->getRootDeviceEnvironment();
    auto maxWidthToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(rootDeviceEnvironment));
    auto maxHeightToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment, false));
    {
        Vec3<size_t> copySize = {(maxWidthToCopy * maxHeightToCopy - 1), 1, 1};
        size_t expectednBlitsCopyPerRow = 2;
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment, false);
        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
    }
    {
        Vec3<size_t> copySize = {(maxWidthToCopy * maxHeightToCopy), 1, 1};
        size_t expectednBlitsCopyPerRow = 1;
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment, false);
        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
    }
    {
        Vec3<size_t> copySize = {(maxWidthToCopy * maxHeightToCopy + 1), 1, 1};
        size_t expectednBlitsCopyPerRow = 2;
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment, false);
        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
    }
    {
        Vec3<size_t> copySize = {(maxWidthToCopy * maxHeightToCopy + maxWidthToCopy), 1, 1};
        size_t expectednBlitsCopyPerRow = 2;
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment, false);
        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
    }
    {
        Vec3<size_t> copySize = {(maxWidthToCopy * maxHeightToCopy + maxWidthToCopy + 1), 1, 1};
        size_t expectednBlitsCopyPerRow = 3;
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment, false);
        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
    }
    {
        Vec3<size_t> copySize = {(maxWidthToCopy * maxHeightToCopy + 2 * maxWidthToCopy), 1, 1};
        size_t expectednBlitsCopyPerRow = 2;
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment, false);
        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
        EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(copySize, rootDeviceEnvironment, false));
    }
}

HWTEST_F(BcsTests, whenAskingForCmdSizeForMiFlushDwWithMemoryWriteThenReturnCorrectValue) {
    debugManager.flags.ForceDummyBlitWa.set(0);
    EncodeDummyBlitWaArgs waArgs{true, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t waSize = MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs);
    size_t totalSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    constexpr size_t miFlushDwSize = sizeof(typename FamilyType::MI_FLUSH_DW);

    size_t additionalSize = UnitTestHelper<FamilyType>::additionalMiFlushDwRequired ? miFlushDwSize : 0;

    EXPECT_EQ(additionalSize, waSize);
    EXPECT_EQ(miFlushDwSize + additionalSize, totalSize);
}

HWTEST_F(BcsTests, givenBlitPropertiesContainerWhenEstimatingCommandsSizeThenCalculateForAllAttachedProperties) {
    const auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    const uint32_t numberOfBlts = 3;
    const size_t bltSize = (3 * max2DBlitSize);
    const uint32_t numberOfBlitOperations = 4;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};

    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize();

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    auto expectedBlitInstructionsSize = cmdsSizePerBlit * numberOfBlts;

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedBlitInstructionsSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    waArgs.isWaRequired = true;
    auto baseSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) + sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    auto expectedAlignedSize = baseSize + (2 * MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));

    MockGraphicsAllocation bufferMockAllocation(0, 1u, AllocationType::buffer, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation hostMockAllocation(0, 1u, AllocationType::externalHostPtr, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::system64KBPages, MemoryManager::maxOsContextCount);

    BlitPropertiesContainer blitPropertiesContainer;
    for (uint32_t i = 0; i < numberOfBlitOperations; i++) {
        BlitProperties blitProperties;
        blitProperties.blitDirection = BlitterConstants::BlitDirection::bufferToHostPtr;
        blitProperties.auxTranslationDirection = AuxTranslationDirection::none;
        blitProperties.copySize = {bltSize, 1, 1};
        blitProperties.dstAllocation = &hostMockAllocation;
        blitProperties.srcAllocation = &bufferMockAllocation;

        blitPropertiesContainer.push_back(blitProperties);

        expectedAlignedSize += expectedBlitInstructionsSize;

        bool deviceToHostPostSyncFenceRequired = rootDeviceEnvironment.getProductHelper().isDeviceToHostCopySignalingFenceRequired() &&
                                                 !blitProperties.dstAllocation->isAllocatedInLocalMemoryPool() &&
                                                 blitProperties.srcAllocation->isAllocatedInLocalMemoryPool();
        if (deviceToHostPostSyncFenceRequired) {
            expectedAlignedSize += MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
        }
    }

    expectedAlignedSize = alignUp(expectedAlignedSize, MemoryConstants::cacheLineSize);

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        blitPropertiesContainer, false, false, false, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
}

HWTEST_F(BcsTests, givenBlitPropertiesContainerWhenDirectsubmissionEnabledEstimatingCommandsSizeThenCalculateForAllAttachedProperties) {
    const auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    const uint32_t numberOfBlts = 3;
    const size_t bltSize = (3 * max2DBlitSize);
    const uint32_t numberOfBlitOperations = 4;

    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize();

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    auto expectedBlitInstructionsSize = cmdsSizePerBlit * numberOfBlts;

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedBlitInstructionsSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    waArgs.isWaRequired = true;
    auto baseSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) + sizeof(typename FamilyType::MI_BATCH_BUFFER_START);
    auto expectedAlignedSize = baseSize + (2 * MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));

    MockGraphicsAllocation bufferMockAllocation(0, 1u, AllocationType::buffer, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation hostMockAllocation(0, 1u, AllocationType::externalHostPtr, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::system64KBPages, MemoryManager::maxOsContextCount);

    BlitPropertiesContainer blitPropertiesContainer;
    for (uint32_t i = 0; i < numberOfBlitOperations; i++) {
        BlitProperties blitProperties;
        blitProperties.blitDirection = BlitterConstants::BlitDirection::bufferToHostPtr;
        blitProperties.auxTranslationDirection = AuxTranslationDirection::none;
        blitProperties.copySize = {bltSize, 1, 1};
        blitProperties.dstAllocation = &hostMockAllocation;
        blitProperties.srcAllocation = &bufferMockAllocation;
        blitPropertiesContainer.push_back(blitProperties);

        expectedAlignedSize += expectedBlitInstructionsSize;
    }

    expectedAlignedSize = alignUp(expectedAlignedSize, MemoryConstants::cacheLineSize);

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        blitPropertiesContainer, false, false, true, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
}

HWTEST_F(BcsTests, givenBlitPropertiesContainerWhenEstimatingCommandsSizeForWriteReadBufferRectThenCalculateForAllAttachedProperties) {
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    const Vec3<size_t> bltSize = {(3 * max2DBlitSize), 4, 2};
    const size_t numberOfBlts = 3 * bltSize.y * bltSize.z;
    const size_t numberOfBlitOperations = 4 * bltSize.y * bltSize.z;
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize();

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    auto expectedBlitInstructionsSize = cmdsSizePerBlit * numberOfBlts;

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedBlitInstructionsSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    waArgs.isWaRequired = true;
    auto baseSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) + sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    auto expectedAlignedSize = baseSize + (2 * MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));

    MockGraphicsAllocation bufferMockAllocation(0, 1u, AllocationType::buffer, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation hostMockAllocation(0, 1u, AllocationType::externalHostPtr, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::system64KBPages, MemoryManager::maxOsContextCount);

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    BlitPropertiesContainer blitPropertiesContainer;
    for (uint32_t i = 0; i < numberOfBlitOperations; i++) {
        BlitProperties blitProperties;
        blitProperties.blitDirection = BlitterConstants::BlitDirection::bufferToHostPtr;
        blitProperties.auxTranslationDirection = AuxTranslationDirection::none;
        blitProperties.copySize = bltSize;
        blitProperties.dstAllocation = &hostMockAllocation;
        blitProperties.srcAllocation = &bufferMockAllocation;
        blitPropertiesContainer.push_back(blitProperties);

        expectedAlignedSize += expectedBlitInstructionsSize;

        bool deviceToHostPostSyncFenceRequired = rootDeviceEnvironment.getProductHelper().isDeviceToHostCopySignalingFenceRequired() &&
                                                 !blitProperties.dstAllocation->isAllocatedInLocalMemoryPool() &&
                                                 blitProperties.srcAllocation->isAllocatedInLocalMemoryPool();
        if (deviceToHostPostSyncFenceRequired) {
            expectedAlignedSize += MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
        }
    }

    expectedAlignedSize = alignUp(expectedAlignedSize, MemoryConstants::cacheLineSize);

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        blitPropertiesContainer, false, false, false, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
}

HWTEST_F(BcsTests, givenBlitPropertiesContainerWhenDirectSubmissionEnabledEstimatingCommandsSizeForWriteReadBufferRectThenCalculateForAllAttachedProperties) {
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    const Vec3<size_t> bltSize = {(3 * max2DBlitSize), 4, 2};
    const size_t numberOfBlts = 3 * bltSize.y * bltSize.z;
    const size_t numberOfBlitOperations = 4 * bltSize.y * bltSize.z;
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize();

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    auto expectedBlitInstructionsSize = cmdsSizePerBlit * numberOfBlts;

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedBlitInstructionsSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    waArgs.isWaRequired = true;
    auto baseSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) + sizeof(typename FamilyType::MI_BATCH_BUFFER_START);
    auto expectedAlignedSize = baseSize + (2 * MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));

    MockGraphicsAllocation bufferMockAllocation(0, 1u, AllocationType::buffer, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation hostMockAllocation(0, 1u, AllocationType::externalHostPtr, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::system64KBPages, MemoryManager::maxOsContextCount);

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    BlitPropertiesContainer blitPropertiesContainer;
    for (uint32_t i = 0; i < numberOfBlitOperations; i++) {
        BlitProperties blitProperties;
        blitProperties.blitDirection = BlitterConstants::BlitDirection::bufferToHostPtr;
        blitProperties.auxTranslationDirection = AuxTranslationDirection::none;
        blitProperties.copySize = bltSize;
        blitProperties.dstAllocation = &hostMockAllocation;
        blitProperties.srcAllocation = &bufferMockAllocation;
        blitPropertiesContainer.push_back(blitProperties);

        expectedAlignedSize += expectedBlitInstructionsSize;

        bool deviceToHostPostSyncFenceRequired = rootDeviceEnvironment.getProductHelper().isDeviceToHostCopySignalingFenceRequired() &&
                                                 !blitProperties.dstAllocation->isAllocatedInLocalMemoryPool() &&
                                                 blitProperties.srcAllocation->isAllocatedInLocalMemoryPool();
        if (deviceToHostPostSyncFenceRequired) {
            expectedAlignedSize += MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
        }
    }

    expectedAlignedSize = alignUp(expectedAlignedSize, MemoryConstants::cacheLineSize);

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        blitPropertiesContainer, false, false, true, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
}

HWTEST_F(BcsTests, givenTimestampPacketWriteRequestWhenEstimatingSizeForCommandsThenAddMiFlushDw) {
    EncodeDummyBlitWaArgs waArgs{true, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t dummyBlitWaSize = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs);
    waArgs.isWaRequired = false;
    size_t expectedBaseSize = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize();

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        expectedBaseSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedBaseSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    auto expectedSizeWithTimestampPacketWrite = expectedBaseSize + EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) + dummyBlitWaSize;
    auto expectedSizeWithoutTimestampPacketWrite = expectedBaseSize;

    auto estimatedSizeWithTimestampPacketWrite = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        {1, 1, 1}, csrDependencies, true, false, false, pClDevice->getRootDeviceEnvironment(), false, false);
    auto estimatedSizeWithoutTimestampPacketWrite = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        {1, 1, 1}, csrDependencies, false, false, false, pClDevice->getRootDeviceEnvironment(), false, false);

    EXPECT_EQ(expectedSizeWithTimestampPacketWrite, estimatedSizeWithTimestampPacketWrite);
    EXPECT_EQ(expectedSizeWithoutTimestampPacketWrite, estimatedSizeWithoutTimestampPacketWrite);
}

HWTEST_F(BcsTests, givenTimestampPacketWriteRequestWhenEstimatingSizeForCommandsWithProfilingThenAddMiFlushDw) {
    EncodeDummyBlitWaArgs waArgs{true, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t dummyBlitWaSize = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs);
    waArgs.isWaRequired = false;
    size_t expectedBaseSize = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize() + dummyBlitWaSize;

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        expectedBaseSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedBaseSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    expectedBaseSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);

    auto expectedSizeWithTimestampPacketWriteAndProfiling = expectedBaseSize + BlitCommandsHelper<FamilyType>::getProfilingMmioCmdsSize();

    auto estimatedSizeWithTimestampPacketWrite = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        {1, 1, 1}, csrDependencies, true, false, false, pClDevice->getRootDeviceEnvironment(), false, false);
    auto estimatedSizeWithTimestampPacketWriteAndProfiling = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        {1, 1, 1}, csrDependencies, true, true, false, pClDevice->getRootDeviceEnvironment(), false, false);

    EXPECT_EQ(expectedSizeWithTimestampPacketWriteAndProfiling, estimatedSizeWithTimestampPacketWriteAndProfiling);
    EXPECT_EQ(expectedBaseSize, estimatedSizeWithTimestampPacketWrite);
}

HWTEST_F(BcsTests, givenBltSizeAndCsrDependenciesWhenEstimatingCommandSizeThenAddAllRequiredCommands) {
    uint32_t numberOfBlts = 1;
    size_t numberNodesPerContainer = 5;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    MockTimestampPacketContainer timestamp0(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    csrDependencies.timestampPacketContainer.push_back(&timestamp0);
    csrDependencies.timestampPacketContainer.push_back(&timestamp1);

    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize();

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    size_t expectedSize = (cmdsSizePerBlit * numberOfBlts) +
                          TimestampPacketHelper::getRequiredCmdStreamSize<FamilyType>(csrDependencies, false);

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    auto estimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        {1, 1, 1}, csrDependencies, false, false, false, pClDevice->getRootDeviceEnvironment(), false, false);

    EXPECT_EQ(expectedSize, estimatedSize);
}

HWTEST_F(BcsTests, givenBltSizeWithCsrDependenciesAndRelaxedOrderingWhenEstimatingCommandSizeThenAddAllRequiredCommands) {
    uint32_t numberOfBlts = 1;
    size_t numberNodesPerContainer = 5;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    MockTimestampPacketContainer timestamp0(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    csrDependencies.timestampPacketContainer.push_back(&timestamp0);
    csrDependencies.timestampPacketContainer.push_back(&timestamp1);

    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize();

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    size_t expectedSize = (cmdsSizePerBlit * numberOfBlts) +
                          TimestampPacketHelper::getRequiredCmdStreamSize<FamilyType>(csrDependencies, true);

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    auto estimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        {1, 1, 1}, csrDependencies, false, false, false, pClDevice->getRootDeviceEnvironment(), false, true);

    EXPECT_EQ(expectedSize, estimatedSize);
}

HWTEST_F(BcsTests, givenImageAndBufferWhenEstimateBlitCommandSizeThenReturnCorrectCommandSize) {

    for (auto isImage : {false, true}) {
        EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
        auto expectedSize = EncodeMiArbCheck<FamilyType>::getCommandSize();
        expectedSize += isImage ? sizeof(typename FamilyType::XY_BLOCK_COPY_BLT) : sizeof(typename FamilyType::XY_COPY_BLT);

        if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
            expectedSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
        }
        if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
            expectedSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
        }

        auto estimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
            {1, 1, 1}, csrDependencies, false, false, isImage, pClDevice->getRootDeviceEnvironment(), false, false);

        EXPECT_EQ(expectedSize, estimatedSize);
    }
}

HWTEST_F(BcsTests, givenImageAndBufferBlitDirectionsWhenIsImageOperationIsCalledThenReturnCorrectValue) {
    BlitProperties blitProperties{};
    std::pair<bool, BlitterConstants::BlitDirection> params[] = {{false, BlitterConstants::BlitDirection::hostPtrToBuffer},
                                                                 {false, BlitterConstants::BlitDirection::bufferToHostPtr},
                                                                 {false, BlitterConstants::BlitDirection::bufferToBuffer},
                                                                 {true, BlitterConstants::BlitDirection::hostPtrToImage},
                                                                 {true, BlitterConstants::BlitDirection::imageToHostPtr},
                                                                 {true, BlitterConstants::BlitDirection::imageToImage}};

    for (auto [isImageDirection, blitDirection] : params) {
        blitProperties.blitDirection = blitDirection;
        EXPECT_EQ(isImageDirection, blitProperties.isImageOperation());
    }
}

struct RelaxedOrderingBcsTests : public BcsTests {
    void SetUp() override {
        ultHwConfigBackup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;
        ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = true;

        debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);
        debugManager.flags.UpdateTaskCountFromWait.set(2);
        BcsTests::SetUp();
    }

    BlitProperties generateBlitProperties(CommandStreamReceiver &csr, Buffer *clBuffer) {
        auto bufferGpuVa = ptrOffset(clBuffer->getGraphicsAllocation(pDevice->getRootDeviceIndex())->getGpuAddress(), clBuffer->getOffset());
        auto properties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                          csr, clBuffer->getGraphicsAllocation(pDevice->getRootDeviceIndex()), nullptr, hostPtr,
                                                                          bufferGpuVa, 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);
        EXPECT_EQ(1u, properties.srcAllocation->hostPtrTaskCountAssignment.load());
        properties.srcAllocation->hostPtrTaskCountAssignment--;

        return properties;
    }

    std::unique_ptr<VariableBackup<UltHwConfig>> ultHwConfigBackup;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
};

HWTEST2_F(RelaxedOrderingBcsTests, givenDependenciesWhenFlushingThenProgramCorrectCommands, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;

    cl_int retVal = CL_SUCCESS;
    auto clBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    int client1, client2;
    csr.registerClient(&client1);
    csr.registerClient(&client2);
    csr.recordFlushedBatchBuffer = true;

    csr.blitterDirectSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>>(csr);

    auto blitProperties = generateBlitProperties(csr, clBuffer.get());

    MockTimestampPacketContainer timestamp(*csr.getTimestampPacketAllocator(), 1);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestamp);

    // First submission with global state
    flushBcsTask(&csr, blitProperties, false, *pDevice);

    if (csr.heaplessStateInitialized) {
        EXPECT_FALSE(csr.latestFlushedBatchBuffer.hasStallingCmds);
        EXPECT_TRUE(csr.latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    } else {
        EXPECT_TRUE(csr.latestFlushedBatchBuffer.hasStallingCmds);
        EXPECT_FALSE(csr.latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    }

    auto cmdsOffset = csr.commandStream.getUsed();

    flushBcsTask(&csr, blitProperties, false, *pDevice);
    EXPECT_FALSE(csr.latestFlushedBatchBuffer.hasStallingCmds);
    EXPECT_TRUE(csr.latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);

    auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(ptrOffset(csr.commandStream.getCpuBase(), cmdsOffset));
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR0);

    lrrCmd++;
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR4 + 4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR0 + 4);

    auto eventNode = timestamp.peekNodes()[0];
    auto compareAddress = eventNode->getGpuAddress() + eventNode->getContextEndOffset();
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyConditionalDataMemBbStart<FamilyType>(++lrrCmd, 0, compareAddress, 1, CompareOperation::equal, true, false, true));
    csr.blitterDirectSubmission.reset();
}

HWTEST2_F(RelaxedOrderingBcsTests, givenDependenciesWhenFlushingThenProgramProgramRelaxedOrderingOnlyIfAllowed, IsAtLeastXeHpcCore) {
    cl_int retVal = CL_SUCCESS;
    auto clBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    int client1, client2;
    csr.registerClient(&client1);
    csr.registerClient(&client1);
    csr.recordFlushedBatchBuffer = true;

    csr.blitterDirectSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>>(csr);

    auto blitProperties = generateBlitProperties(csr, clBuffer.get());

    MockTimestampPacketContainer timestamp(*csr.getTimestampPacketAllocator(), 1);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestamp);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    EXPECT_FALSE(csr.bcsRelaxedOrderingAllowed(blitPropertiesContainer, true));
    EXPECT_TRUE(csr.bcsRelaxedOrderingAllowed(blitPropertiesContainer, false));

    debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(-1);
    EXPECT_FALSE(csr.bcsRelaxedOrderingAllowed(blitPropertiesContainer, false));

    debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);
    blitPropertiesContainer.push_back(blitProperties);
    EXPECT_FALSE(csr.bcsRelaxedOrderingAllowed(blitPropertiesContainer, false));

    blitPropertiesContainer.push_back(blitProperties);
    csr.unregisterClient(&client1);
    csr.unregisterClient(&client2);
    EXPECT_FALSE(csr.bcsRelaxedOrderingAllowed(blitPropertiesContainer, false));

    csr.registerClient(&client1);
    csr.registerClient(&client2);
    csr.blitterDirectSubmission.reset();
    EXPECT_FALSE(csr.bcsRelaxedOrderingAllowed(blitPropertiesContainer, false));
}

HWTEST2_F(RelaxedOrderingBcsTests, givenTagUpdateWhenFlushingThenDisableRelaxedOrdering, IsAtLeastXeHpcCore) {
    cl_int retVal = CL_SUCCESS;
    auto clBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    int client1, client2;
    csr.registerClient(&client1);
    csr.registerClient(&client2);
    csr.recordFlushedBatchBuffer = true;

    csr.blitterDirectSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>>(csr);

    auto blitProperties = generateBlitProperties(csr, clBuffer.get());

    MockTimestampPacketContainer timestamp(*csr.getTimestampPacketAllocator(), 1);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestamp);

    // First submission with global state
    flushBcsTask(&csr, blitProperties, false, *pDevice);

    if (csr.heaplessStateInitialized) {
        EXPECT_FALSE(csr.latestFlushedBatchBuffer.hasStallingCmds);
        EXPECT_TRUE(csr.latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    } else {
        EXPECT_TRUE(csr.latestFlushedBatchBuffer.hasStallingCmds);
        EXPECT_FALSE(csr.latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    }

    debugManager.flags.UpdateTaskCountFromWait.set(0);

    // blocking
    flushBcsTask(&csr, blitProperties, true, *pDevice);
    EXPECT_TRUE(csr.latestFlushedBatchBuffer.hasStallingCmds);
    EXPECT_FALSE(csr.latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);

    // Disabled UpdateTaskCountFromWait
    debugManager.flags.UpdateTaskCountFromWait.set(0);
    blitProperties = generateBlitProperties(csr, clBuffer.get());
    flushBcsTask(&csr, blitProperties, false, *pDevice);
    EXPECT_TRUE(csr.latestFlushedBatchBuffer.hasStallingCmds);
    EXPECT_FALSE(csr.latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    csr.blitterDirectSubmission.reset();
}

HWTEST_F(BcsTests, givenBltSizeWithLeftoverWhenDispatchedThenProgramAllRequiredCommands) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironment(), true);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    static_cast<OsAgnosticMemoryManager *>(csr.getMemoryManager())->turnOnFakingBigAllocations();

    uint32_t bltLeftover = 17;
    size_t bltSize = static_cast<size_t>((2 * max2DBlitSize) + bltLeftover);
    uint32_t numberOfBlts = 3;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, static_cast<size_t>(bltSize), nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    uint32_t newTaskCount = 19;
    csr.taskCount = newTaskCount - 1;
    uint32_t expectedResursiveLockCount = csr.resourcesInitialized ? 1u : 0u;
    if (csr.heaplessStateInitialized) {
        expectedResursiveLockCount++;
    }

    EXPECT_EQ(expectedResursiveLockCount, csr.recursiveLockCounter.load());
    auto bufferGpuVa = ptrOffset(buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex())->getGpuAddress(), buffer->getOffset());
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                          csr, buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex()), nullptr, hostPtr,
                                                                          bufferGpuVa, 0,
                                                                          0, 0, {bltSize, 1, 1}, 0, 0, 0, 0);
    if (csr.getClearColorAllocation()) {
        expectedResursiveLockCount++;
    }

    EXPECT_EQ(expectedResursiveLockCount, csr.recursiveLockCounter.load());
    bool areResourcesInitialized = csr.resourcesInitialized;
    flushBcsTask(&csr, blitProperties, true, *pDevice);
    EXPECT_EQ(newTaskCount, csr.taskCount);
    EXPECT_EQ(newTaskCount, csr.latestFlushedTaskCount);
    EXPECT_EQ(newTaskCount, csr.latestSentTaskCount);
    EXPECT_EQ(newTaskCount, csr.latestSentTaskCountValueDuringFlush);
    expectedResursiveLockCount++;
    if (areResourcesInitialized != csr.resourcesInitialized) {
        expectedResursiveLockCount++;
    }
    EXPECT_EQ(expectedResursiveLockCount, csr.recursiveLockCounter.load());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream);
    auto &cmdList = hwParser.cmdList;

    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

    for (uint32_t i = 0; i < numberOfBlts; i++) {
        auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*(cmdIterator++));
        EXPECT_NE(nullptr, bltCmd);

        uint32_t expectedWidth = static_cast<uint32_t>(BlitterConstants::maxBlitWidth);
        uint32_t expectedHeight = static_cast<uint32_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironment(), true));
        if (i == (numberOfBlts - 1)) {
            expectedWidth = bltLeftover;
            expectedHeight = 1;
        }
        EXPECT_EQ(expectedWidth, bltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(expectedHeight, bltCmd->getDestinationY2CoordinateBottom());
        EXPECT_EQ(expectedWidth, bltCmd->getDestinationPitch());
        EXPECT_EQ(expectedWidth, bltCmd->getSourcePitch());

        if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
            auto miFlush = genCmdCast<typename FamilyType::MI_FLUSH_DW *>(*(cmdIterator++));
            EXPECT_NE(nullptr, miFlush);
            EncodeDummyBlitWaArgs waArgs{true, &(pDevice->getRootDeviceEnvironmentRef())};
            if (EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) - BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs) == 2 * sizeof(typename FamilyType::MI_FLUSH_DW)) {
                miFlush = genCmdCast<typename FamilyType::MI_FLUSH_DW *>(*(cmdIterator++));
                EXPECT_NE(nullptr, miFlush);
            }
        } else {
            UnitTestHelper<FamilyType>::verifyDummyBlitWa(&(pDevice->getRootDeviceEnvironmentRef()), cmdIterator);
        }
        auto miArbCheckCmd = genCmdCast<typename FamilyType::MI_ARB_CHECK *>(*(cmdIterator++));
        EXPECT_NE(nullptr, miArbCheckCmd);
        EXPECT_TRUE(memcmp(&FamilyType::cmdInitArbCheck, miArbCheckCmd, sizeof(typename FamilyType::MI_ARB_CHECK)) == 0);
    }

    if (UnitTestHelper<FamilyType>::isAdditionalSynchronizationRequired()) {
        if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(pDevice->getRootDeviceEnvironment())) {
            auto miSemaphoreWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(cmdIterator++));
            EXPECT_NE(nullptr, miSemaphoreWaitCmd);
            EXPECT_TRUE(UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*miSemaphoreWaitCmd));
        } else if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()) > 0) {
            cmdIterator++;
        }
    }
    auto releaseHelper = pDevice->getReleaseHelper();
    if (releaseHelper && releaseHelper->isDummyBlitWaRequired()) {
        UnitTestHelper<FamilyType>::verifyDummyBlitWa(&(pDevice->getRootDeviceEnvironmentRef()), cmdIterator);
    }
    auto miFlushCmd = genCmdCast<MI_FLUSH_DW *>(*(cmdIterator++));

    if (UnitTestHelper<FamilyType>::additionalMiFlushDwRequired) {
        uint64_t gpuAddress = 0x0;
        uint64_t immData = 0;

        EXPECT_NE(nullptr, miFlushCmd);
        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, miFlushCmd->getPostSyncOperation());
        EXPECT_EQ(gpuAddress, miFlushCmd->getDestinationAddress());
        EXPECT_EQ(immData, miFlushCmd->getImmediateData());

        miFlushCmd = genCmdCast<MI_FLUSH_DW *>(*(cmdIterator++));
    }

    EXPECT_NE(cmdIterator, cmdList.end());
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushCmd->getPostSyncOperation());
    EXPECT_EQ(csr.getTagAllocation()->getGpuAddress(), miFlushCmd->getDestinationAddress());
    EXPECT_EQ(newTaskCount, miFlushCmd->getImmediateData());

    if (UnitTestHelper<FamilyType>::isAdditionalSynchronizationRequired()) {
        if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(pDevice->getRootDeviceEnvironment())) {
            auto miSemaphoreWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(cmdIterator++));
            EXPECT_NE(nullptr, miSemaphoreWaitCmd);
            EXPECT_TRUE(UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*miSemaphoreWaitCmd));
        } else if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()) > 0) {
            cmdIterator++;
        }
    }

    EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_BATCH_BUFFER_END *>(*(cmdIterator++)));

    // padding
    while (cmdIterator != cmdList.end()) {
        EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_NOOP *>(*(cmdIterator++)));
    }
}
HWTEST_F(BcsTests, givenCommandTypeWhenObtainBlitDirectionIsCalledThenReturnCorrectBlitDirection) {

    std::array<std::pair<uint32_t, BlitterConstants::BlitDirection>, 10> testParams = {{{CL_COMMAND_WRITE_BUFFER, BlitterConstants::BlitDirection::hostPtrToBuffer},
                                                                                        {CL_COMMAND_WRITE_BUFFER_RECT, BlitterConstants::BlitDirection::hostPtrToBuffer},
                                                                                        {CL_COMMAND_READ_BUFFER, BlitterConstants::BlitDirection::bufferToHostPtr},
                                                                                        {CL_COMMAND_READ_BUFFER_RECT, BlitterConstants::BlitDirection::bufferToHostPtr},
                                                                                        {CL_COMMAND_COPY_BUFFER_RECT, BlitterConstants::BlitDirection::bufferToBuffer},
                                                                                        {CL_COMMAND_SVM_MEMCPY, BlitterConstants::BlitDirection::bufferToBuffer},
                                                                                        {CL_COMMAND_WRITE_IMAGE, BlitterConstants::BlitDirection::hostPtrToImage},
                                                                                        {CL_COMMAND_READ_IMAGE, BlitterConstants::BlitDirection::imageToHostPtr},
                                                                                        {CL_COMMAND_COPY_BUFFER, BlitterConstants::BlitDirection::bufferToBuffer},
                                                                                        {CL_COMMAND_COPY_IMAGE, BlitterConstants::BlitDirection::imageToImage}}};

    for (const auto &[commandType, expectedBlitDirection] : testParams) {
        auto blitDirection = ClBlitProperties::obtainBlitDirection(commandType);
        EXPECT_EQ(expectedBlitDirection, blitDirection);
    }
}

HWTEST_F(BcsTests, givenWrongCommandTypeWhenObtainBlitDirectionIsCalledThenExpectThrow) {
    uint32_t wrongCommandType = CL_COMMAND_NDRANGE_KERNEL;
    EXPECT_THROW(ClBlitProperties::obtainBlitDirection(wrongCommandType), std::exception);
}

struct BcsTestParam {
    Vec3<size_t> copySize;

    Vec3<size_t> hostPtrOffset;
    Vec3<size_t> copyOffset;

    size_t dstRowPitch;
    size_t dstSlicePitch;
    size_t srcRowPitch;
    size_t srcSlicePitch;
} blitterProperties[] = {
    {{(2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17, 1, 1},
     {0, 1, 1},
     {BlitterConstants::maxBlitWidth, 1, 1},
     (2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17,
     (2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17,
     (2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17,
     (2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17},
    {{(2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17, 2, 1},
     {BlitterConstants::maxBlitWidth, 2, 2},
     {BlitterConstants::maxBlitWidth, 1, 1},
     0,
     ((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) * 2,
     0,
     ((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) * 2},
    {{(2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17, 1, 3},
     {BlitterConstants::maxBlitWidth, 2, 2},
     {BlitterConstants::maxBlitWidth, 1, 1},
     0,
     ((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) * 2,
     0,
     ((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) * 2},
    {{(2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17, 4, 2},
     {0, 0, 0},
     {0, 0, 0},
     (2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17,
     ((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) * 4,
     (2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17,
     ((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) * 4},
    {{(2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17, 3, 2},
     {BlitterConstants::maxBlitWidth, 2, 2},
     {BlitterConstants::maxBlitWidth, 1, 1},
     ((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) + 2,
     (((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) * 3) + 2,
     ((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) + 2,
     (((2 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight) + 17) * 3) + 2}};

template <typename ParamType>
struct BcsDetaliedTests : public BcsTests,
                          public ::testing::WithParamInterface<ParamType> {
    void SetUp() override {
        BcsTests::SetUp();
    }

    void TearDown() override {
        BcsTests::TearDown();
    }
};

using BcsDetaliedTestsWithParams = BcsDetaliedTests<std::tuple<BcsTestParam, BlitterConstants::BlitDirection>>;

HWTEST_P(BcsDetaliedTestsWithParams, givenBltSizeWithLeftoverWhenDispatchedThenProgramAddresseForWriteReadBufferRect) {

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    static_cast<OsAgnosticMemoryManager *>(csr.getMemoryManager())->turnOnFakingBigAllocations();

    uint32_t bltLeftover = 17;
    Vec3<size_t> bltSize = std::get<0>(GetParam()).copySize;

    size_t numberOfBltsForSingleBltSizeProgramm = 3;
    size_t totalNumberOfBits = numberOfBltsForSingleBltSizeProgramm * bltSize.y * bltSize.z;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, static_cast<size_t>(8 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight), nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    Vec3<size_t> hostPtrOffset = std::get<0>(GetParam()).hostPtrOffset;
    Vec3<size_t> copyOffset = std::get<0>(GetParam()).copyOffset;

    size_t dstRowPitch = std::get<0>(GetParam()).dstRowPitch;
    size_t dstSlicePitch = std::get<0>(GetParam()).dstSlicePitch;
    size_t srcRowPitch = std::get<0>(GetParam()).srcRowPitch;
    size_t srcSlicePitch = std::get<0>(GetParam()).srcSlicePitch;
    auto allocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto bufferGpuVa = ptrOffset(allocation->getGpuAddress(), buffer->getOffset());

    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(std::get<1>(GetParam()), // blitDirection
                                                                          csr, allocation,         // commandStreamReceiver
                                                                          nullptr,                 // memObjAllocation
                                                                          hostPtr,                 // preallocatedHostAllocation
                                                                          bufferGpuVa,             // memObjGpuVa
                                                                          0,                       // hostAllocGpuVa
                                                                          hostPtrOffset,           // hostPtrOffset
                                                                          copyOffset,              // copyOffset
                                                                          bltSize,                 // copySize
                                                                          dstRowPitch,             // hostRowPitch
                                                                          dstSlicePitch,           // hostSlicePitch
                                                                          srcRowPitch,             // gpuRowPitch
                                                                          srcSlicePitch            // gpuSlicePitch
    );

    memoryManager->returnFakeAllocation = false;
    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream);

    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

    uint64_t offset = 0;
    for (uint32_t i = 0; i < totalNumberOfBits; i++) {
        auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*(cmdIterator++));
        EXPECT_NE(nullptr, bltCmd);

        uint32_t expectedWidth = static_cast<uint32_t>(BlitterConstants::maxBlitWidth);
        uint32_t expectedHeight = static_cast<uint32_t>(BlitterConstants::maxBlitHeight);
        if (i % numberOfBltsForSingleBltSizeProgramm == numberOfBltsForSingleBltSizeProgramm - 1) {
            expectedWidth = bltLeftover;
            expectedHeight = 1;
        }

        if (i % numberOfBltsForSingleBltSizeProgramm == 0) {
            offset = 0;
        }

        auto rowIndex = (i / numberOfBltsForSingleBltSizeProgramm) % blitProperties.copySize.y;
        auto sliceIndex = i / (numberOfBltsForSingleBltSizeProgramm * blitProperties.copySize.y);

        auto expectedDstAddr = blitProperties.dstGpuAddress + blitProperties.dstOffset.x + offset +
                               blitProperties.dstOffset.y * blitProperties.dstRowPitch +
                               blitProperties.dstOffset.z * blitProperties.dstSlicePitch +
                               rowIndex * blitProperties.dstRowPitch +
                               sliceIndex * blitProperties.dstSlicePitch;
        auto expectedSrcAddr = blitProperties.srcGpuAddress + blitProperties.srcOffset.x + offset +
                               blitProperties.srcOffset.y * blitProperties.srcRowPitch +
                               blitProperties.srcOffset.z * blitProperties.srcSlicePitch +
                               rowIndex * blitProperties.srcRowPitch +
                               sliceIndex * blitProperties.srcSlicePitch;

        auto dstAddr = NEO::BlitCommandsHelper<FamilyType>::calculateBlitCommandDestinationBaseAddress(blitProperties, offset, rowIndex, sliceIndex);
        auto srcAddr = NEO::BlitCommandsHelper<FamilyType>::calculateBlitCommandSourceBaseAddress(blitProperties, offset, rowIndex, sliceIndex);

        EXPECT_EQ(dstAddr, expectedDstAddr);
        EXPECT_EQ(srcAddr, expectedSrcAddr);

        offset += (expectedWidth * expectedHeight);

        if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
            auto miFlush = genCmdCast<typename FamilyType::MI_FLUSH_DW *>(*(cmdIterator++));
            EXPECT_NE(nullptr, miFlush);
            EncodeDummyBlitWaArgs waArgs{true, &(pDevice->getRootDeviceEnvironmentRef())};
            if (EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) - BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs) == 2 * sizeof(typename FamilyType::MI_FLUSH_DW)) {
                miFlush = genCmdCast<typename FamilyType::MI_FLUSH_DW *>(*(cmdIterator++));
                EXPECT_NE(nullptr, miFlush);
            }
        } else {
            UnitTestHelper<FamilyType>::verifyDummyBlitWa(&(pDevice->getRootDeviceEnvironmentRef()), cmdIterator);
        }

        auto miArbCheckCmd = genCmdCast<typename FamilyType::MI_ARB_CHECK *>(*(cmdIterator++));
        EXPECT_NE(nullptr, miArbCheckCmd);
        EXPECT_TRUE(memcmp(&FamilyType::cmdInitArbCheck, miArbCheckCmd, sizeof(typename FamilyType::MI_ARB_CHECK)) == 0);
    }
}

HWTEST_P(BcsDetaliedTestsWithParams, givenBltSizeWithLeftoverWhenDispatchedThenProgramAllRequiredCommandsForWriteReadBufferRect) {
    DebugManagerStateRestore restorer;
    debugManager.flags.LimitBlitterMaxHeight.set(BlitterConstants::maxBlitHeight);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    static_cast<OsAgnosticMemoryManager *>(csr.getMemoryManager())->turnOnFakingBigAllocations();

    uint32_t bltLeftover = 17;
    Vec3<size_t> bltSize = std::get<0>(GetParam()).copySize;

    size_t numberOfBltsForSingleBltSizeProgramm = 3;
    size_t totalNumberOfBits = numberOfBltsForSingleBltSizeProgramm * bltSize.y * bltSize.z;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, static_cast<size_t>(8 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight), nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    Vec3<size_t> hostPtrOffset = std::get<0>(GetParam()).hostPtrOffset;
    Vec3<size_t> copyOffset = std::get<0>(GetParam()).copyOffset;

    size_t dstRowPitch = std::get<0>(GetParam()).dstRowPitch;
    size_t dstSlicePitch = std::get<0>(GetParam()).dstSlicePitch;
    size_t srcRowPitch = std::get<0>(GetParam()).srcRowPitch;
    size_t srcSlicePitch = std::get<0>(GetParam()).srcSlicePitch;
    auto allocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto bufferGpuVa = ptrOffset(allocation->getGpuAddress(), buffer->getOffset());

    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(std::get<1>(GetParam()), // blitDirection
                                                                          csr, allocation,         // commandStreamReceiver
                                                                          nullptr,                 // memObjAllocation
                                                                          hostPtr,                 // preallocatedHostAllocation
                                                                          bufferGpuVa,             // memObjGpuVa
                                                                          0,                       // hostAllocGpuVa
                                                                          hostPtrOffset,           // hostPtrOffset
                                                                          copyOffset,              // copyOffset
                                                                          bltSize,                 // copySize
                                                                          dstRowPitch,             // hostRowPitch
                                                                          dstSlicePitch,           // hostSlicePitch
                                                                          srcRowPitch,             // gpuRowPitch
                                                                          srcSlicePitch            // gpuSlicePitch
    );

    memoryManager->returnFakeAllocation = false;
    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream);

    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

    uint64_t offset = 0;
    for (uint32_t i = 0; i < totalNumberOfBits; i++) {
        auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*(cmdIterator++));
        EXPECT_NE(nullptr, bltCmd);

        uint32_t expectedWidth = static_cast<uint32_t>(BlitterConstants::maxBlitWidth);
        uint32_t expectedHeight = static_cast<uint32_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironment(), blitProperties.isSystemMemoryPoolUsed));
        if (i % numberOfBltsForSingleBltSizeProgramm == numberOfBltsForSingleBltSizeProgramm - 1) {
            expectedWidth = bltLeftover;
            expectedHeight = 1;
        }

        if (i % numberOfBltsForSingleBltSizeProgramm == 0) {
            offset = 0;
        }

        EXPECT_EQ(expectedWidth, bltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(expectedHeight, bltCmd->getDestinationY2CoordinateBottom());
        EXPECT_EQ(expectedWidth, bltCmd->getDestinationPitch());
        EXPECT_EQ(expectedWidth, bltCmd->getSourcePitch());

        auto rowIndex = (i / numberOfBltsForSingleBltSizeProgramm) % blitProperties.copySize.y;
        auto sliceIndex = i / (numberOfBltsForSingleBltSizeProgramm * blitProperties.copySize.y);

        auto dstAddr = NEO::BlitCommandsHelper<FamilyType>::calculateBlitCommandDestinationBaseAddress(blitProperties, offset, rowIndex, sliceIndex);
        auto srcAddr = NEO::BlitCommandsHelper<FamilyType>::calculateBlitCommandSourceBaseAddress(blitProperties, offset, rowIndex, sliceIndex);

        EXPECT_EQ(dstAddr, bltCmd->getDestinationBaseAddress());
        EXPECT_EQ(srcAddr, bltCmd->getSourceBaseAddress());

        offset += (expectedWidth * expectedHeight);

        if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
            auto miFlush = genCmdCast<typename FamilyType::MI_FLUSH_DW *>(*(cmdIterator++));
            EXPECT_NE(nullptr, miFlush);
            EncodeDummyBlitWaArgs waArgs{true, &(pDevice->getRootDeviceEnvironmentRef())};
            if (EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) - BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs) == 2 * sizeof(typename FamilyType::MI_FLUSH_DW)) {
                miFlush = genCmdCast<typename FamilyType::MI_FLUSH_DW *>(*(cmdIterator++));
                EXPECT_NE(nullptr, miFlush);
            }
        } else {
            UnitTestHelper<FamilyType>::verifyDummyBlitWa(&(pDevice->getRootDeviceEnvironmentRef()), cmdIterator);
        }
        auto miArbCheckCmd = genCmdCast<typename FamilyType::MI_ARB_CHECK *>(*(cmdIterator++));
        EXPECT_NE(nullptr, miArbCheckCmd);
        EXPECT_TRUE(memcmp(&FamilyType::cmdInitArbCheck, miArbCheckCmd, sizeof(typename FamilyType::MI_ARB_CHECK)) == 0);
    }
}

HWTEST_P(BcsDetaliedTestsWithParams, givenBltSizeWithLeftoverWhenDispatchedThenProgramAllRequiredCommandsForCopyBufferRect) {
    DebugManagerStateRestore restorer;
    debugManager.flags.LimitBlitterMaxHeight.set(BlitterConstants::maxBlitHeight);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    static_cast<OsAgnosticMemoryManager *>(csr.getMemoryManager())->turnOnFakingBigAllocations();

    uint32_t bltLeftover = 17;
    Vec3<size_t> bltSize = std::get<0>(GetParam()).copySize;

    size_t numberOfBltsForSingleBltSizeProgramm = 3;
    size_t totalNumberOfBits = numberOfBltsForSingleBltSizeProgramm * bltSize.y * bltSize.z;

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, static_cast<size_t>(8 * BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight), nullptr, retVal));

    Vec3<size_t> buffer1Offset = std::get<0>(GetParam()).hostPtrOffset;
    Vec3<size_t> buffer2Offset = std::get<0>(GetParam()).copyOffset;

    size_t buffer1RowPitch = std::get<0>(GetParam()).dstRowPitch;
    size_t buffer1SlicePitch = std::get<0>(GetParam()).dstSlicePitch;
    size_t buffer2RowPitch = std::get<0>(GetParam()).srcRowPitch;
    size_t buffer2SlicePitch = std::get<0>(GetParam()).srcSlicePitch;
    auto allocation = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocation, 0,
        allocation, 0,
        buffer1Offset, buffer2Offset, bltSize,
        buffer1RowPitch, buffer1SlicePitch, buffer2RowPitch, buffer2SlicePitch, csr.getClearColorAllocation());
    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream);

    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

    uint64_t offset = 0;
    for (uint32_t i = 0; i < totalNumberOfBits; i++) {
        auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*(cmdIterator++));
        EXPECT_NE(nullptr, bltCmd);

        uint32_t expectedWidth = static_cast<uint32_t>(BlitterConstants::maxBlitWidth);
        uint32_t expectedHeight = static_cast<uint32_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironment(), true));
        if (i % numberOfBltsForSingleBltSizeProgramm == numberOfBltsForSingleBltSizeProgramm - 1) {
            expectedWidth = bltLeftover;
            expectedHeight = 1;
        }

        if (i % numberOfBltsForSingleBltSizeProgramm == 0) {
            offset = 0;
        }

        EXPECT_EQ(expectedWidth, bltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(expectedHeight, bltCmd->getDestinationY2CoordinateBottom());
        EXPECT_EQ(expectedWidth, bltCmd->getDestinationPitch());
        EXPECT_EQ(expectedWidth, bltCmd->getSourcePitch());

        auto rowIndex = (i / numberOfBltsForSingleBltSizeProgramm) % blitProperties.copySize.y;
        auto sliceIndex = i / (numberOfBltsForSingleBltSizeProgramm * blitProperties.copySize.y);

        auto dstAddr = NEO::BlitCommandsHelper<FamilyType>::calculateBlitCommandDestinationBaseAddress(blitProperties, offset, rowIndex, sliceIndex);
        auto srcAddr = NEO::BlitCommandsHelper<FamilyType>::calculateBlitCommandSourceBaseAddress(blitProperties, offset, rowIndex, sliceIndex);

        EXPECT_EQ(dstAddr, bltCmd->getDestinationBaseAddress());
        EXPECT_EQ(srcAddr, bltCmd->getSourceBaseAddress());

        offset += (expectedWidth * expectedHeight);

        if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
            auto miFlush = genCmdCast<typename FamilyType::MI_FLUSH_DW *>(*(cmdIterator++));
            EXPECT_NE(nullptr, miFlush);
            EncodeDummyBlitWaArgs waArgs{true, &(pDevice->getRootDeviceEnvironmentRef())};
            if (EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) - BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs) == 2 * sizeof(typename FamilyType::MI_FLUSH_DW)) {
                miFlush = genCmdCast<typename FamilyType::MI_FLUSH_DW *>(*(cmdIterator++));
                EXPECT_NE(nullptr, miFlush);
            }
        }
        auto miArbCheckCmd = genCmdCast<typename FamilyType::MI_ARB_CHECK *>(*(cmdIterator++));
        EXPECT_NE(nullptr, miArbCheckCmd);
        EXPECT_TRUE(memcmp(&FamilyType::cmdInitArbCheck, miArbCheckCmd, sizeof(typename FamilyType::MI_ARB_CHECK)) == 0);
    }
}

INSTANTIATE_TEST_SUITE_P(BcsDetaliedTest,
                         BcsDetaliedTestsWithParams,
                         ::testing::Combine(
                             ::testing::ValuesIn(blitterProperties),
                             ::testing::Values(BlitterConstants::BlitDirection::hostPtrToBuffer, BlitterConstants::BlitDirection::bufferToHostPtr)));

HWCMDTEST_F(IGFX_GEN12LP_CORE, UltCommandStreamReceiverTest, WhenProgrammingActivePartitionsThenExpectNoAction) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t expectedCmdSize = 0;
    EXPECT_EQ(expectedCmdSize, commandStreamReceiver.getCmdSizeForActivePartitionConfig());
    size_t usedBefore = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.programActivePartitionConfig(commandStreamReceiver.commandStream);
    size_t usedAfter = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, UltCommandStreamReceiverTest, givenBarrierNodeSetWhenProgrammingBarrierCommandThenExpectPostSyncPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto commandStreamReceiver = &pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &commandStreamCSR = commandStreamReceiver->getCS();

    TagNodeBase *tagNode = commandStreamReceiver->getTimestampPacketAllocator()->getTag();
    uint64_t gpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tagNode);

    TimestampPacketDependencies timestampPacketDependencies;
    timestampPacketDependencies.barrierNodes.add(tagNode);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies.barrierNodes;

    size_t expectedCmdSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData);
    size_t estimatedCmdSize = commandStreamReceiver->getCmdSizeForStallingCommands(dispatchFlags);
    EXPECT_EQ(expectedCmdSize, estimatedCmdSize);

    commandStreamReceiver->programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags.barrierTimestampPacketNodes, false);
    EXPECT_EQ(estimatedCmdSize, commandStreamCSR.getUsed());

    parseCommands<FamilyType>(commandStreamCSR, 0);
    findHardwareCommands<FamilyType>();
    auto cmdItor = cmdList.begin();

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(rootDeviceEnvironment)) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pipeControl);
        cmdItor++;
        if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment) > 0) {
            cmdItor++;
        }
    }
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(0u, pipeControl->getImmediateData());
    EXPECT_EQ(gpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
}
