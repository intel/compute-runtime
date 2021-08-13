/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/fixtures/media_kernel_fixture.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "test.h"

using namespace NEO;
typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueBlockedVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};

    UserEvent userEvent(context);
    cl_event blockedEvent = &userEvent;

    auto retVal = pCmdQ->enqueueKernel(
        pVmeKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        nullptr,
        1,
        &blockedEvent,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    userEvent.setStatus(CL_COMPLETE);

    parseCommands<FamilyType>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_FALSE(pCmd->getSystolicModeEnable());
    pCmdQ->releaseVirtualEvent();
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueBlockedNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};

    UserEvent userEvent(context);
    cl_event blockedEvent = &userEvent;

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        nullptr,
        1,
        &blockedEvent,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    userEvent.setStatus(CL_COMPLETE);

    parseCommands<FamilyType>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_FALSE(pCmd->getSystolicModeEnable());
    pCmdQ->releaseVirtualEvent();
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pVmeKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_FALSE(pCmd->getSystolicModeEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_FALSE(pCmd->getSystolicModeEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pVmeKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueNonVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pVmeKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueVmeKernelAfterNonVmeKernelThenProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);

    retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pVmeKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_FALSE(pCmd->getSystolicModeEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueNonVmeKernelAfterVmeKernelThenProgramProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pVmeKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);

    retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_FALSE(pCmd->getSystolicModeEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToFalse) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = true;
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pVmeKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);
    EXPECT_TRUE(csr->lastVmeSubslicesConfig);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterCsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = false;
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pVmeKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    itorWalker1 = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker1);
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, WhenGettingCmdSizeForVmeThenZeroIsReturned) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    csr->lastVmeSubslicesConfig = false;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));

    csr->lastVmeSubslicesConfig = true;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterWhenEnqueueSystolicKernelThenPipelineSelectEnablesSystolicMode) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    MockKernelWithInternals mockKernel(*pClDevice, context);
    mockKernel.mockKernel->setSpecialPipelineSelectMode(true);
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        mockKernel.mockKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockKernel.mockKernel->requiresSpecialPipelineSelectMode());

    parseCommands<FamilyType>(*pCmdQ);

    auto numCommands = getCommandCount<PIPELINE_SELECT>();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getSystolicModeEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterWhenEnqueueNonSystolicKernelThenPipelineSelectDisablesSystolicMode) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    MockKernelWithInternals mockKernel(*pClDevice, context);
    mockKernel.mockKernel->setSpecialPipelineSelectMode(false);
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        mockKernel.mockKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(mockKernel.mockKernel->requiresSpecialPipelineSelectMode());

    parseCommands<FamilyType>(*pCmdQ);

    auto numCommands = getCommandCount<PIPELINE_SELECT>();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getSystolicModeEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterWhenEnqueueTwoSystolicKernelsThenPipelineSelectEnablesSystolicModeOnce) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    MockKernelWithInternals mockKernel(*pClDevice, context);
    mockKernel.mockKernel->setSpecialPipelineSelectMode(true);
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        mockKernel.mockKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockKernel.mockKernel->requiresSpecialPipelineSelectMode());

    MockKernelWithInternals mockKernel2(*pClDevice, context);
    mockKernel2.mockKernel->setSpecialPipelineSelectMode(true);
    retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        mockKernel2.mockKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockKernel2.mockKernel->requiresSpecialPipelineSelectMode());

    parseCommands<FamilyType>(*pCmdQ);

    auto numCommands = getCommandCount<PIPELINE_SELECT>();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getSystolicModeEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MediaKernelTest, givenXeHPAndLaterWhenEnqueueTwoKernelsThenPipelineSelectEnablesSystolicModeWhenNeeded) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    MockKernelWithInternals mockKernel(*pClDevice, context);
    mockKernel.mockKernel->setSpecialPipelineSelectMode(false);
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        mockKernel.mockKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(mockKernel.mockKernel->requiresSpecialPipelineSelectMode());

    MockKernelWithInternals mockKernel2(*pClDevice, context);
    mockKernel2.mockKernel->setSpecialPipelineSelectMode(true);
    retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        mockKernel2.mockKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockKernel2.mockKernel->requiresSpecialPipelineSelectMode());

    parseCommands<FamilyType>(*pCmdQ);

    auto numCommands = getCommandCount<PIPELINE_SELECT>();
    EXPECT_EQ(2u, numCommands);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;

    auto itorCmd = find<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorCmd);
    auto pCmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getSystolicModeEnable());

    itorCmd = find<PIPELINE_SELECT *>(++itorCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorCmd);
    pCmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getSystolicModeEnable());
}
