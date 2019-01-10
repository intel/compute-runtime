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

using namespace NEO;

#include "unit_tests/command_stream/command_stream_receiver_hw_tests.inl"

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
