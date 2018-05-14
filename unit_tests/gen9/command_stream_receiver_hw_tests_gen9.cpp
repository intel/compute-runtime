/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "reg_configs_common.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"

#include "test.h"
#include "gtest/gtest.h"
using namespace OCLRT;

#include "unit_tests/command_stream/command_stream_receiver_hw_tests.inl"

using CommandStreamReceiverHwTestGen9 = CommandStreamReceiverHwTest<SKLFamily>;

GEN9TEST_F(UltCommandStreamReceiverTest, givenNotSentPreambleAndMidThreadPreemptionWhenPreambleIsProgrammedThenCorrectSipKernelGpuAddressIsProgrammed) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;

    size_t minCsrSize = pDevice->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);
    commandStreamReceiver.setPreemptionCsrAllocation(&csrSurface);

    pDevice->setPreemptionMode(PreemptionMode::MidThread);
    uint32_t newL3Config;
    DispatchFlags dispatchFlags;

    auto cmdSizePreambleMidThread = commandStreamReceiver.getRequiredCmdSizeForPreamble();
    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreambleMidThread);
    LinearStream preambleStream(&*preemptionBuffer.begin(), preemptionBuffer.size());
    auto sipAllocation = BuiltIns::getInstance().getSipKernel(SipKernelType::Csr, *pDevice).getSipAllocation();
    commandStreamReceiver.programPreamble(preambleStream, dispatchFlags, newL3Config);

    this->parseCommands<FamilyType>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(this->cmdList.begin(), this->cmdList.end());
    ASSERT_NE(this->cmdList.end(), itorStateSip);

    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();
    EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress);
}

GEN9TEST_F(UltCommandStreamReceiverTest, givenNotSentPreambleAndKernelDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelGpuAddressIsProgrammed) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;
    size_t minCsrSize = pDevice->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);
    commandStreamReceiver.setPreemptionCsrAllocation(&csrSurface);

    pDevice->setPreemptionMode(PreemptionMode::Disabled);
    pDevice->setSourceLevelDebuggerActive(true);
    uint32_t newL3Config;
    DispatchFlags dispatchFlags;

    auto cmdSizePreambleMidThread = commandStreamReceiver.getRequiredCmdSizeForPreamble();
    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreambleMidThread);
    LinearStream preambleStream(&*preemptionBuffer.begin(), preemptionBuffer.size());
    auto dbgLocalSipAllocation = BuiltIns::getInstance().getSipKernel(SipKernelType::DbgCsrLocal, *pDevice).getSipAllocation();
    auto sipAllocation = BuiltIns::getInstance().getSipKernel(SipKernelType::Csr, *pDevice).getSipAllocation();

    ASSERT_NE(BuiltIns::getInstance().getSipKernel(SipKernelType::DbgCsrLocal, *pDevice).getType(), BuiltIns::getInstance().getSipKernel(SipKernelType::Csr, *pDevice).getType());
    ASSERT_NE(dbgLocalSipAllocation, nullptr);
    ASSERT_NE(sipAllocation, nullptr);

    commandStreamReceiver.programPreamble(preambleStream, dispatchFlags, newL3Config);

    this->parseCommands<FamilyType>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(this->cmdList.begin(), this->cmdList.end());
    ASSERT_NE(this->cmdList.end(), itorStateSip);

    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();
    EXPECT_EQ(dbgLocalSipAllocation->getGpuAddressToPatch(), sipAddress);
}

GEN9TEST_F(CommandStreamReceiverHwTestGen9, GivenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3Config) {
    givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl();
}

GEN9TEST_F(CommandStreamReceiverHwTestGen9, GivenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentOnThenProgramL3WithSLML3ConfigAfterUnblocking) {
    givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl();
}
