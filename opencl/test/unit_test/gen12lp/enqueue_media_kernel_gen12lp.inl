/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/media_kernel_fixture.h"
#include "test.h"

using namespace NEO;
typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueBlockedVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename TGLLPFamily::PIPELINE_SELECT PIPELINE_SELECT;

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

    parseCommands<TGLLPFamily>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueBlockedNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename TGLLPFamily::PIPELINE_SELECT PIPELINE_SELECT;

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

    parseCommands<TGLLPFamily>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename TGLLPFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<TGLLPFamily>();

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename TGLLPFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueRegularKernel<TGLLPFamily>();

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename TGLLPFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<TGLLPFamily>();
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueNonVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename TGLLPFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<TGLLPFamily>();
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueVmeKernelAfterNonVmeKernelThenProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename TGLLPFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueRegularKernel<TGLLPFamily>();
    enqueueVmeKernel<TGLLPFamily>();

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueNonVmeKernelAfterVmeKernelThenProgramProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename TGLLPFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<TGLLPFamily>();
    enqueueRegularKernel<TGLLPFamily>();

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToFalse) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = true;
    enqueueVmeKernel<FamilyType>();
    EXPECT_TRUE(csr->lastVmeSubslicesConfig);
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = false;
    enqueueVmeKernel<FamilyType>();
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

GEN12LPTEST_F(MediaKernelTest, givenGen12LpCmdSizeForVme) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    csr->lastVmeSubslicesConfig = false;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));

    csr->lastVmeSubslicesConfig = true;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));
}
