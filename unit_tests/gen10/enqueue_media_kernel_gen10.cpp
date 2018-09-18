/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/media_kernel_fixture.h"
#include "runtime/helpers/preamble.h"
#include "test.h"

using namespace OCLRT;
typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueBlockedVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;

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

    parseCommands<CNLFamily>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueBlockedNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;

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

    parseCommands<CNLFamily>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<CNLFamily>();

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueRegularKernel<CNLFamily>();

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<CNLFamily>();
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueNonVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<CNLFamily>();
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueVmeKernelAfterNonVmeKernelThenProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueRegularKernel<CNLFamily>();
    enqueueVmeKernel<CNLFamily>();

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueNonVmeKernelAfterVmeKernelThenProgramProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<CNLFamily>();
    enqueueRegularKernel<CNLFamily>();

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToFalse) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = true;
    enqueueVmeKernel<FamilyType>();
    EXPECT_TRUE(csr->lastVmeSubslicesConfig);
}

GEN10TEST_F(MediaKernelTest, givenGen10CsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = false;
    enqueueVmeKernel<FamilyType>();
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

GEN10TEST_F(MediaKernelTest, gen10CmdSizeForVme) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getCommandStreamReceiver());

    csr->lastVmeSubslicesConfig = false;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));

    csr->lastVmeSubslicesConfig = true;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));
}
