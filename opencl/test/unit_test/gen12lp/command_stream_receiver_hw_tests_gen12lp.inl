/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"

using namespace NEO;

#include "opencl/test/unit_test/command_stream/command_stream_receiver_hw_tests.inl"

using CommandStreamReceiverHwTestGen12lp = CommandStreamReceiverHwTest<Gen12LpFamily>;

GEN12LPTEST_F(CommandStreamReceiverHwTestGen12lp, givenPreambleSentWhenL3ConfigRequestChangedThenDontProgramL3Register) {
    size_t gws = 1;
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    auto &commandStreamCSR = commandStreamReceiver->getCS();

    commandStreamReceiver->isPreambleSent = true;
    commandStreamReceiver->lastSentL3Config = 0;

    commandQueue.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);

    parseCommands<FamilyType>(commandStreamCSR, 0);
    auto itorCmd = findMmio<FamilyType>(cmdList.begin(), cmdList.end(), L3CNTLRegisterOffset<FamilyType>::registerOffset);
    ASSERT_EQ(cmdList.end(), itorCmd);
}

GEN12LPTEST_F(CommandStreamReceiverHwTestGen12lp, whenProgrammingMiSemaphoreWaitThenSetRegisterPollModeMemoryPoll) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    MI_SEMAPHORE_WAIT miSemaphoreWait = FamilyType::cmdInitMiSemaphoreWait;
    EXPECT_EQ(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL, miSemaphoreWait.getRegisterPollMode());
}

using CommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
GEN12LPTEST_F(UltCommandStreamReceiverTest, givenStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToItWithTextureCacheFlushAndHdc) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getDcFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
}

using UltCommandStreamReceiverTestGen12Lp = UltCommandStreamReceiverTest;

GEN12LPTEST_F(UltCommandStreamReceiverTestGen12Lp, givenDebugEnablingCacheFlushWhenAddingPipeControlWithoutCacheFlushThenOverrideRequestAndEnableCacheFlushFlags) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.FlushAllCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    parseCommands<FamilyType>(stream, 0);

    PIPE_CONTROL *pipeControl = getCommand<PIPE_CONTROL>();

    ASSERT_NE(nullptr, pipeControl);

    // WA pipeControl added
    if (cmdList.size() == 2) {
        pipeControl++;
    }

    EXPECT_TRUE(pipeControl->getHdcPipelineFlush());
}

GEN12LPTEST_F(UltCommandStreamReceiverTestGen12Lp, givenDebugDisablingCacheFlushWhenAddingPipeControlWithCacheFlushThenOverrideRequestAndDisableCacheFlushFlags) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.DoNotFlushCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args;
    args.dcFlushEnable = true;
    args.hdcPipelineFlush = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    parseCommands<FamilyType>(stream, 0);

    PIPE_CONTROL *pipeControl = getCommand<PIPE_CONTROL>();

    ASSERT_NE(nullptr, pipeControl);

    // WA pipeControl added
    if (cmdList.size() == 2) {
        pipeControl++;
    }

    EXPECT_FALSE(pipeControl->getHdcPipelineFlush());
}
