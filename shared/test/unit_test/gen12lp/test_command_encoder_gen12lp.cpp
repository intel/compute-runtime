/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncoderTest = Test<DeviceFixture>;
using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

GEN12LPTEST_F(CommandEncoderTest, WhenAdjustComputeModeIsCalledThenStateComputeModeShowsNonCoherencySet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    CommandContainer cmdContainer;

    auto ret = cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    ASSERT_EQ(CommandContainer::ErrorCode::success, ret);

    auto usedSpaceBefore = cmdContainer.getCommandStream()->getUsed();
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    // Adjust the State Compute Mode which sets FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT
    StreamProperties properties{};
    properties.initSupport(rootDeviceEnvironment);
    properties.stateComputeMode.setPropertiesAll(false, GrfConfig::defaultGrfNumber, 0, PreemptionMode::Disabled);
    NEO::EncodeComputeMode<FamilyType>::programComputeModeCommand(*cmdContainer.getCommandStream(),
                                                                  properties.stateComputeMode, rootDeviceEnvironment);

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
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
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

GEN12LPTEST_F(CommandEncodeStatesTest, givenVariousEngineTypesWhenEncodeSbaThenAdditionalPipelineSelectWAIsAppliedOnlyToRcs) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    CommandContainer cmdContainer;

    auto ret = cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    ASSERT_EQ(CommandContainer::ErrorCode::success, ret);

    auto gmmHelper = cmdContainer.getDevice()->getRootDeviceEnvironment().getGmmHelper();
    uint32_t statelessMocsIndex = (gmmHelper->getL3EnabledMOCS() >> 1);

    {
        STATE_BASE_ADDRESS sba;
        EncodeStateBaseAddressArgs<FamilyType> args = createDefaultEncodeStateBaseAddressArgs<FamilyType>(&cmdContainer, sba, statelessMocsIndex);
        args.isRcs = true;
        EncodeStateBaseAddress<FamilyType>::encode(args);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());
        auto itorLRI = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
        MockExecutionEnvironment mockExecutionEnvironment{};
        auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
        if (productHelper.is3DPipelineSelectWARequired()) {
            EXPECT_NE(itorLRI, commands.end());
        } else {
            EXPECT_EQ(itorLRI, commands.end());
        }
    }

    cmdContainer.reset();

    {
        STATE_BASE_ADDRESS sba;
        EncodeStateBaseAddressArgs<FamilyType> args = createDefaultEncodeStateBaseAddressArgs<FamilyType>(&cmdContainer, sba, statelessMocsIndex);
        args.isRcs = false;
        EncodeStateBaseAddress<FamilyType>::encode(args);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());
        auto itorLRI = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
        EXPECT_EQ(itorLRI, commands.end());
    }
}

GEN12LPTEST_F(CommandEncoderTest, GivenGen12LpWhenProgrammingL3StateOnThenExpectNoCommandsDispatched) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    EncodeL3State<FamilyType>::encode(cmdContainer, true);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    EXPECT_EQ(itorLRI, commands.end());
}

GEN12LPTEST_F(CommandEncoderTest, GivenGen12LpWhenProgrammingL3StateOffThenExpectNoCommandsDispatched) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

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

GEN12LPTEST_F(CommandEncodeStatesTest, givenGen12LpPlatformWhenAdjustPipelineSelectIsCalledThenPipelineIsDispatched) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    size_t barrierSize = 0;
    if (MemorySynchronizationCommands<FamilyType>::isBarrierPriorToPipelineSelectWaRequired(pDevice->getRootDeviceEnvironment())) {
        barrierSize = MemorySynchronizationCommands<FamilyType>::getSizeForSingleBarrier();
    }

    auto &cmdStream = *cmdContainer->getCommandStream();

    cmdContainer->systolicModeSupportRef() = false;
    descriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    auto sizeUsed = cmdStream.getUsed();
    void *ptr = ptrOffset(cmdStream.getCpuBase(), (barrierSize + sizeUsed));

    NEO::EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer, descriptor);
    auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(ptr);
    ASSERT_NE(nullptr, pipelineSelectCmd);

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;

    EXPECT_EQ(mask, pipelineSelectCmd->getMaskBits());
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, pipelineSelectCmd->getPipelineSelection());
    EXPECT_EQ(true, pipelineSelectCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_EQ(false, pipelineSelectCmd->getSpecialModeEnable());

    cmdContainer->systolicModeSupportRef() = true;
    sizeUsed = cmdStream.getUsed();
    ptr = ptrOffset(cmdStream.getCpuBase(), (barrierSize + sizeUsed));

    NEO::EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer, descriptor);
    pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(ptr);
    ASSERT_NE(nullptr, pipelineSelectCmd);

    mask |= pipelineSelectSystolicModeEnableMaskBits;

    EXPECT_EQ(mask, pipelineSelectCmd->getMaskBits());
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, pipelineSelectCmd->getPipelineSelection());
    EXPECT_EQ(true, pipelineSelectCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_EQ(true, pipelineSelectCmd->getSpecialModeEnable());

    descriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = false;
    sizeUsed = cmdStream.getUsed();
    ptr = ptrOffset(cmdStream.getCpuBase(), (barrierSize + sizeUsed));

    NEO::EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer, descriptor);
    pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(ptr);
    ASSERT_NE(nullptr, pipelineSelectCmd);

    EXPECT_EQ(mask, pipelineSelectCmd->getMaskBits());
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, pipelineSelectCmd->getPipelineSelection());
    EXPECT_EQ(true, pipelineSelectCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_EQ(false, pipelineSelectCmd->getSpecialModeEnable());
}
