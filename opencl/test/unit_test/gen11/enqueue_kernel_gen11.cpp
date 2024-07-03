/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/static_size3.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

namespace NEO {

using Gen11EnqueueTest = Test<ClDeviceFixture>;

GEN11TEST_F(Gen11EnqueueTest, givenKernelRequiringIndependentForwardProgressWhenKernelIsSubmittedThenRoundRobinPolicyIsProgrammed) {
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
    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[ThreadArbitrationPolicy::RoundRobin], cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address));
}

GEN11TEST_F(Gen11EnqueueTest, givenKernelNotRequiringIndependentForwardProgressWhenKernelIsSubmittedThenDefaultPolicyIsProgrammed) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
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
    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[gfxCoreHelper.getDefaultThreadArbitrationPolicy()], cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address));
}

} // namespace NEO
