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

using CommandStreamReceiverHwTestGen11 = CommandStreamReceiverHwTest<ICLFamily>;

GEN11TEST_F(CommandStreamReceiverHwTestGen11, GivenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3Config) {
    givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl();
}

GEN11TEST_F(CommandStreamReceiverHwTestGen11, GivenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblocking) {
    givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl();
}

GEN11TEST_F(CommandStreamReceiverHwTestGen11, whenProgrammingMiSemaphoreWaitThenSetRegisterPollModeMemoryPoll) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    MI_SEMAPHORE_WAIT miSemaphoreWait = FamilyType::cmdInitMiSemaphoreWait;
    EXPECT_EQ(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL, miSemaphoreWait.getRegisterPollMode());
}
