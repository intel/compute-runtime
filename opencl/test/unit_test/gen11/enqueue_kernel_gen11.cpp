/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/test/common/helpers/static_size3.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "reg_configs_common.h"

namespace NEO {

using Gen11EnqueueTest = Test<ClDeviceFixture>;
GEN11TEST_F(Gen11EnqueueTest, givenKernelRequiringIndependentForwardProgressWhenKernelIsSubmittedThenDefaultPolicyIsProgrammed) {
    MockContext mc;
    CommandQueueHw<FamilyType> cmdQ{&mc, pClDevice, 0, false};

    SPatchExecutionEnvironment sPatchExecEnv = {};
    sPatchExecEnv.SubgroupIndependentForwardProgressRequired = true;
    MockKernelWithInternals mockKernel(*pClDevice, sPatchExecEnv);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, StatickSize3<1, 1, 1>(), nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ);

    auto cmd = findMmioCmd<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[HwHelperHw<FamilyType>::get().getDefaultThreadArbitrationPolicy()], cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address));
}

GEN11TEST_F(Gen11EnqueueTest, givenKernelNotRequiringIndependentForwardProgressWhenKernelIsSubmittedThenAgeBasedPolicyIsProgrammed) {
    MockContext mc;
    CommandQueueHw<FamilyType> cmdQ{&mc, pClDevice, 0, false};

    SPatchExecutionEnvironment sPatchExecEnv = {};
    sPatchExecEnv.SubgroupIndependentForwardProgressRequired = false;
    MockKernelWithInternals mockKernel(*pClDevice, sPatchExecEnv);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, StatickSize3<1, 1, 1>(), nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ);

    auto cmd = findMmioCmd<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[ThreadArbitrationPolicy::AgeBased], cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address));
}
} // namespace NEO
