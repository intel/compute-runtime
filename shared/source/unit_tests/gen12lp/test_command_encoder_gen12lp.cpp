/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"
#include "test.h"

#include "reg_configs_common.h"

using namespace NEO;

using CommandEncoderTest = Test<DeviceFixture>;

GEN12LPTEST_F(CommandEncoderTest, givenAdjustStateComputeModeStateComputeModeShowsNonCoherencySet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    CommandContainer cmdContainer;

    bool ret = cmdContainer.initialize(pDevice);
    ASSERT_TRUE(ret);

    auto usedSpaceBefore = cmdContainer.getCommandStream()->getUsed();

    // Adjust the State Compute Mode which sets FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT
    EncodeStates<FamilyType>::adjustStateComputeMode(cmdContainer);

    auto usedSpaceAfter = cmdContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto expectedCmdSize = sizeof(STATE_COMPUTE_MODE);
    auto cmdAddedSize = usedSpaceAfter - usedSpaceBefore;
    EXPECT_EQ(expectedCmdSize, cmdAddedSize);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask);

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), usedSpaceBefore));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

GEN12LPTEST_F(CommandEncoderTest, givenCommandContainerWhenEncodeL3StateThenSetCorrectMMIO) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeL3State<FamilyType>::encode(cmdContainer, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    EXPECT_EQ(cmd->getRegisterOffset(), 0xB134u);
    EXPECT_EQ(cmd->getDataDword(), 0xD0000020u);
}
