/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/dispatch_flags_helper.h"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"

using namespace NEO;

#include "opencl/test/unit_test/command_stream/command_stream_receiver_hw_tests.inl"

using CommandStreamReceiverHwTestGen8 = CommandStreamReceiverHwTest<BDWFamily>;

GEN8TEST_F(CommandStreamReceiverHwTestGen8, GivenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3Config) {
    givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl();
}

GEN8TEST_F(CommandStreamReceiverHwTestGen8, GivenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentOnThenProgramL3WithSLML3ConfigAfterUnblocking) {
    givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl();
}

GEN8TEST_F(CommandStreamReceiverHwTestGen8, GivenChangedL3ConfigWhenL3IsProgrammedThenClearSLMWorkAroundIsAdded) {
    MockCsrHw2<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    csr.csrSizeRequestFlags.l3ConfigChanged = true;
    csr.isPreambleSent = true;

    size_t bufferSize = 2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM) + sizeof(typename FamilyType::PIPE_CONTROL);
    void *buffer = alignedMalloc(bufferSize, 64);

    LinearStream stream(buffer, bufferSize);
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

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
