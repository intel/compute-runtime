/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"

using namespace OCLRT;

#include "unit_tests/command_stream/command_stream_receiver_hw_tests.inl"

using CommandStreamReceiverHwTestGen8 = CommandStreamReceiverHwTest<BDWFamily>;

GEN8TEST_F(CommandStreamReceiverHwTestGen8, GivenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3Config) {
    givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl();
}

GEN8TEST_F(CommandStreamReceiverHwTestGen8, GivenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentOnThenProgramL3WithSLML3ConfigAfterUnblocking) {
    givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl();
}

GEN8TEST_F(CommandStreamReceiverHwTestGen8, GivenChangedL3ConfigWhenL3IsProgrammedThenClearSLMWorkAroundIsAdded) {
    MockCsrHw2<FamilyType> csr(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    csr.csrSizeRequestFlags.l3ConfigChanged = true;
    csr.isPreambleSent = true;

    size_t bufferSize = 2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM) + sizeof(typename FamilyType::PIPE_CONTROL);
    void *buffer = alignedMalloc(bufferSize, 64);

    LinearStream stream(buffer, bufferSize);
    DispatchFlags flags;
    uint32_t l3Config = 0x12345678;

    csr.programL3(stream, flags, l3Config);

    this->parseCommands<FamilyType>(stream);

    typename FamilyType::PIPE_CONTROL *pc = getCommand<typename FamilyType::PIPE_CONTROL>();
    ASSERT_NE(nullptr, pc);
    EXPECT_TRUE(pc->getProtectedMemoryDisable() != 0);

    typename FamilyType::MI_LOAD_REGISTER_IMM *lri = getCommand<typename FamilyType::MI_LOAD_REGISTER_IMM>();
    ASSERT_NE(nullptr, lri);
    EXPECT_EQ(l3Config, lri->getDataDword());

    alignedFree(buffer);
}
