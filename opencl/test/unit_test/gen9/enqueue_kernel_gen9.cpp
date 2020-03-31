/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/reg_configs.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/helpers/static_size3.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

namespace NEO {

using Gen9EnqueueTest = Test<DeviceFixture>;
GEN9TEST_F(Gen9EnqueueTest, givenKernelRequiringIndependentForwardProgressWhenKernelIsSubmittedThenRoundRobinPolicyIsProgrammed) {
    MockContext mc;
    CommandQueueHw<SKLFamily> cmdQ{&mc, pClDevice, 0, false};
    SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.SubgroupIndependentForwardProgressRequired = true;
    MockKernelWithInternals mockKernel(*pClDevice, executionEnvironment);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, StatickSize3<1, 1, 1>(), nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ);

    auto cmd = findMmioCmd<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), DebugControlReg2::address);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(DebugControlReg2::getRegData(PreambleHelper<FamilyType>::getDefaultThreadArbitrationPolicy()), cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), DebugControlReg2::address));
}

GEN9TEST_F(Gen9EnqueueTest, givenKernelNotRequiringIndependentForwardProgressWhenKernelIsSubmittedThenAgeBasedPolicyIsProgrammed) {
    MockContext mc;
    CommandQueueHw<SKLFamily> cmdQ{&mc, pClDevice, 0, false};
    SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.SubgroupIndependentForwardProgressRequired = false;
    MockKernelWithInternals mockKernel(*pClDevice, executionEnvironment);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, StatickSize3<1, 1, 1>(), nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ);

    auto cmd = findMmioCmd<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), DebugControlReg2::address);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(DebugControlReg2::getRegData(ThreadArbitrationPolicy::AgeBased), cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), DebugControlReg2::address));
}
} // namespace NEO
