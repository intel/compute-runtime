/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"

#include "test.h"

#include "reg_configs_common.h"

using namespace NEO;

using CommandEncoderTest = Test<DeviceFixture>;

GEN12LPTEST_F(CommandEncoderTest, WhenAdjustComputeModeIsCalledThenStateComputeModeShowsNonCoherencySet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    CommandContainer cmdContainer;

    auto ret = cmdContainer.initialize(pDevice);
    ASSERT_EQ(ErrorCode::SUCCESS, ret);

    auto usedSpaceBefore = cmdContainer.getCommandStream()->getUsed();

    // Adjust the State Compute Mode which sets FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT
    StreamProperties properties{};
    properties.stateComputeMode.setProperties(false, cmdContainer.lastSentNumGrfRequired, 0);
    NEO::EncodeComputeMode<FamilyType>::adjustComputeMode(*cmdContainer.getCommandStream(), nullptr,
                                                          properties.stateComputeMode, *defaultHwInfo);

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

GEN12LPTEST_F(CommandEncoderTest, givenCommandContainerWhenEncodeL3StateThenDoNotDispatchMMIOCommand) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeL3State<FamilyType>::encode(cmdContainer, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    EXPECT_EQ(itorLRI, commands.end());
}

struct MockOsContext : public OsContext {
    using OsContext::engineType;
};

GEN12LPTEST_F(CommandEncoderTest, givenVariousEngineTypesWhenEncodeSBAThenAdditionalPipelineSelectWAIsAppliedOnlyToRcs) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    CommandContainer cmdContainer;

    auto ret = cmdContainer.initialize(pDevice);
    ASSERT_EQ(ErrorCode::SUCCESS, ret);

    {
        STATE_BASE_ADDRESS sba;
        EncodeStateBaseAddress<FamilyType>::encode(cmdContainer, sba);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());
        auto itorLRI = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
        EXPECT_NE(itorLRI, commands.end());
    }

    cmdContainer.reset();

    {
        static_cast<MockOsContext *>(pDevice->getDefaultEngine().osContext)->engineType = aub_stream::ENGINE_CCS;

        STATE_BASE_ADDRESS sba;
        EncodeStateBaseAddress<FamilyType>::encode(cmdContainer, sba);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());
        auto itorLRI = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
        EXPECT_EQ(itorLRI, commands.end());
    }
}

GEN12LPTEST_F(CommandEncoderTest, givenVariousEngineTypesWhenEstimateCommandBufferSizeThenRcsHasAdditionalPipelineSelectWASize) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto sizeWA = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice, Vec3<size_t>(0, 0, 0),
                                                                                         Vec3<size_t>(1, 1, 1), false, false, false, nullptr);
    static_cast<MockOsContext *>(pDevice->getDefaultEngine().osContext)->engineType = aub_stream::ENGINE_CCS;
    auto size = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice, Vec3<size_t>(0, 0, 0),
                                                                                       Vec3<size_t>(1, 1, 1), false, false, false, nullptr);

    auto expectedDiff = 2 * PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(pDevice->getHardwareInfo());
    auto diff = sizeWA - size;

    EXPECT_EQ(expectedDiff, diff);
}

GEN12LPTEST_F(CommandEncoderTest, GivenGen12LpWhenProgrammingL3StateOnThenExpectNoCommandsDispatched) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);

    EncodeL3State<FamilyType>::encode(cmdContainer, true);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    EXPECT_EQ(itorLRI, commands.end());
}

GEN12LPTEST_F(CommandEncoderTest, GivenGen12LpWhenProgrammingL3StateOffThenExpectNoCommandsDispatched) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);

    EncodeL3State<FamilyType>::encode(cmdContainer, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    EXPECT_EQ(itorLRI, commands.end());
}

using Gen12lpCommandEncodeTest = testing::Test;

GEN12LPTEST_F(Gen12lpCommandEncodeTest, givenBcsCommandsHelperWhenMiArbCheckWaRequiredThenReturnTrue) {
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::miArbCheckWaRequired());
}
