/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/fixtures/device_fixture.h"
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
    EncodeStates<FamilyType>::adjustStateComputeMode(*cmdContainer.getCommandStream(), cmdContainer.lastSentNumGrfRequired, nullptr, false, false);

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

struct MockOsContext : public OsContext {
    using OsContext::engineType;
};

GEN12LPTEST_F(CommandEncoderTest, givenVariousEngineTypesWhenEncodeSBAThenAdditionalPipelineSelectWAIsAppliedOnlyToRcs) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    CommandContainer cmdContainer;

    bool ret = cmdContainer.initialize(pDevice);
    ASSERT_TRUE(ret);

    {
        EncodeStateBaseAddress<FamilyType>::encode(cmdContainer);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());
        auto itorLRI = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
        EXPECT_NE(itorLRI, commands.end());
    }

    cmdContainer.reset();

    {
        static_cast<MockOsContext *>(pDevice->getDefaultEngine().osContext)->engineType = aub_stream::ENGINE_CCS;

        EncodeStateBaseAddress<FamilyType>::encode(cmdContainer);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());
        auto itorLRI = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
        EXPECT_EQ(itorLRI, commands.end());
    }
}

GEN12LPTEST_F(CommandEncoderTest, givenVariousEngineTypesWhenEstimateCommandBufferSizeThenRcsHasAdditionalPipelineSelectWASize) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto sizeWA = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice);
    static_cast<MockOsContext *>(pDevice->getDefaultEngine().osContext)->engineType = aub_stream::ENGINE_CCS;
    auto size = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice);

    auto expectedDiff = 2 * PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(pDevice->getHardwareInfo());
    auto diff = sizeWA - size;

    EXPECT_EQ(expectedDiff, diff);
}
