/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/event/user_event.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"
using namespace OCLRT;

#include "unit_tests/command_stream/command_stream_receiver_hw_tests.inl"

using CommandStreamReceiverHwTestGen9 = CommandStreamReceiverHwTest<SKLFamily>;

GEN9TEST_F(UltCommandStreamReceiverTest, whenPreambleIsProgrammedThenStateSipCmdIsNotPresentInPreambleCmdStream) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;

    pDevice->setPreemptionMode(PreemptionMode::Disabled);
    pDevice->setSourceLevelDebuggerActive(true);
    uint32_t newL3Config;
    DispatchFlags dispatchFlags;

    auto cmdSizePreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    StackVec<char, 4096> preambleBuffer;
    preambleBuffer.resize(cmdSizePreamble);

    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    commandStreamReceiver.programPreamble(preambleStream, *pDevice, dispatchFlags, newL3Config);

    this->parseCommands<FamilyType>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(this->cmdList.begin(), this->cmdList.end());
    EXPECT_EQ(this->cmdList.end(), itorStateSip);
}

GEN9TEST_F(CommandStreamReceiverHwTestGen9, GivenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3Config) {
    givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl();
}

GEN9TEST_F(CommandStreamReceiverHwTestGen9, GivenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentOnThenProgramL3WithSLML3ConfigAfterUnblocking) {
    givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl();
}
