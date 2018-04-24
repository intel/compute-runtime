/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/gen9/reg_configs.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/static_size3.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "test.h"

namespace OCLRT {

using Gen9EnqueueTest = Test<DeviceFixture>;
GEN9TEST_F(Gen9EnqueueTest, givenKernelRequiringIndependentForwardProgressWhenKernelIsSubmittedThenRoundRobinPolicyIsProgrammed) {
    MockContext mc;
    CommandQueueHw<SKLFamily> cmdQ{&mc, pDevice, 0};
    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.executionEnvironment.SubgroupIndependentForwardProgressRequired = true;

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
    CommandQueueHw<SKLFamily> cmdQ{&mc, pDevice, 0};
    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.executionEnvironment.SubgroupIndependentForwardProgressRequired = false;

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, StatickSize3<1, 1, 1>(), nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ);

    auto cmd = findMmioCmd<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), DebugControlReg2::address);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(DebugControlReg2::getRegData(ThreadArbitrationPolicy::AgeBased), cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), DebugControlReg2::address));
}
} // namespace OCLRT
