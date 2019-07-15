/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/pipeline_select_helper.h"
#include "runtime/helpers/preamble.h"
#include "test.h"
#include "unit_tests/fixtures/media_kernel_fixture.h"

using namespace NEO;
typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;
auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits |
                    pipelineSelectMediaSamplerDopClockGateMaskBits |
                    pipelineSelectMediaSamplerPowerClockGateMaskBits;

GEN11TEST_F(MediaKernelTest, givenGen11CsrWhenEnqueueBlockedVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;

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

    parseCommands<ICLFamily>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN11TEST_F(MediaKernelTest, givenGen11CsrWhenEnqueueBlockedNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;

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

    parseCommands<ICLFamily>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN11TEST_F(MediaKernelTest, givenGen11CsrWhenEnqueueVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<ICLFamily>();

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_EQ(1u, pCmd->getMediaSamplerPowerClockGateDisable());
}

GEN11TEST_F(MediaKernelTest, givenGen11CsrWhenEnqueueNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueRegularKernel<ICLFamily>();

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_EQ(0u, pCmd->getMediaSamplerPowerClockGateDisable());
}

GEN11TEST_F(MediaKernelTest, givenGen11CsrWhenEnqueueVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<ICLFamily>();
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

GEN11TEST_F(MediaKernelTest, givenGen11CsrWhenEnqueueNonVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<ICLFamily>();
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

GEN11TEST_F(MediaKernelTest, givenGen11CsrWhenEnqueueVmeKernelAfterNonVmeKernelThenProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueRegularKernel<ICLFamily>();
    enqueueVmeKernel<ICLFamily>();

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_EQ(1u, pCmd->getMediaSamplerPowerClockGateDisable());
}

GEN11TEST_F(MediaKernelTest, givenGen11CsrWhenEnqueueNonVmeKernelAfterVmeKernelThenProgramProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<ICLFamily>();
    enqueueRegularKernel<ICLFamily>();

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
    EXPECT_EQ(0u, pCmd->getMediaSamplerPowerClockGateDisable());
}

ICLLPTEST_F(MediaKernelTest, givenIcllpDefaultLastVmeSubsliceConfigIsFalse) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

ICLLPTEST_F(MediaKernelTest, givenIcllpCSRWhenEnqueueVmeKernelThenVmeSubslicesConfigChangesToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    enqueueVmeKernel<FamilyType>();
    EXPECT_TRUE(csr->lastVmeSubslicesConfig);
}

ICLLPTEST_F(MediaKernelTest, givenIcllpCSRWhenEnqueueRegularKernelAfterVmeKernelThenVmeSubslicesConfigChangesToFalse) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    enqueueVmeKernel<FamilyType>();
    enqueueRegularKernel<FamilyType>();
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

ICLLPTEST_F(MediaKernelTest, givenIcllpCSRWhenEnqueueRegularKernelThenVmeSubslicesConfigDoesntChangeToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    enqueueRegularKernel<FamilyType>();
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

ICLLPTEST_F(MediaKernelTest, givenIcllpCSRWhenEnqueueRegularKernelAfterRegularKernelThenVmeSubslicesConfigDoesntChangeToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    enqueueRegularKernel<FamilyType>();
    enqueueRegularKernel<FamilyType>();
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

ICLLPTEST_F(MediaKernelTest, givenIcllpCSRWhenEnqueueVmeKernelAfterRegularKernelThenVmeSubslicesConfigChangesToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    enqueueRegularKernel<FamilyType>();
    enqueueVmeKernel<FamilyType>();
    EXPECT_TRUE(csr->lastVmeSubslicesConfig);
}

ICLLPTEST_F(MediaKernelTest, icllpCmdSizeForVme) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    size_t programVmeCmdSize = sizeof(MI_LOAD_REGISTER_IMM) + 2 * sizeof(PIPE_CONTROL);
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(programVmeCmdSize, csr->getCmdSizeForMediaSampler(true));
}
