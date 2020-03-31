/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/helpers/dispatch_flags_helper.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"
using namespace NEO;

#include "opencl/test/unit_test/command_stream/command_stream_receiver_hw_tests.inl"

using CommandStreamReceiverHwTestGen9 = CommandStreamReceiverHwTest<SKLFamily>;

GEN9TEST_F(UltCommandStreamReceiverTest, whenPreambleIsProgrammedThenStateSipCmdIsNotPresentInPreambleCmdStream) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;

    pDevice->setPreemptionMode(PreemptionMode::Disabled);
    pDevice->setDebuggerActive(true);
    uint32_t newL3Config;
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto cmdSizePreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    StackVec<char, 4096> preambleBuffer;
    preambleBuffer.resize(cmdSizePreamble);

    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    commandStreamReceiver.programPreamble(preambleStream, *pDevice, dispatchFlags, newL3Config);

    this->parseCommands<FamilyType>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(this->cmdList.begin(), this->cmdList.end());
    EXPECT_EQ(this->cmdList.end(), itorStateSip);
    pDevice->setDebuggerActive(false);
}

GEN9TEST_F(CommandStreamReceiverHwTestGen9, GivenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3Config) {
    givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl();
}

GEN9TEST_F(CommandStreamReceiverHwTestGen9, GivenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentOnThenProgramL3WithSLML3ConfigAfterUnblocking) {
    givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl();
}
