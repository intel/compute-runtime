/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/fixtures/media_kernel_fixture.h"
#include "runtime/helpers/preamble.h"
#include "test.h"

using namespace OCLRT;
typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueBlockedVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;

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

    parseCommands<SKLFamily>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueBlockedNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;

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

    parseCommands<SKLFamily>(*pCmdQ);
    ASSERT_NE(cmdPipelineSelect, nullptr);
    auto *pCmd = genCmdCast<PIPELINE_SELECT *>(cmdPipelineSelect);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<SKLFamily>();

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueNonVmeKernelFirstTimeThenProgramPipelineSelectionAndMediaSampler) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueRegularKernel<SKLFamily>();

    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);

    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<SKLFamily>();
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueNonVmeKernelTwiceThenProgramPipelineSelectOnce) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<SKLFamily>();
    auto numCommands = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueVmeKernelAfterNonVmeKernelThenProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueRegularKernel<SKLFamily>();
    enqueueVmeKernel<SKLFamily>();

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_FALSE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueNonVmeKernelAfterVmeKernelThenProgramProgramPipelineSelectionAndMediaSamplerTwice) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<SKLFamily>();
    enqueueRegularKernel<SKLFamily>();

    auto commands = getCommandsList<PIPELINE_SELECT>();
    EXPECT_EQ(2u, commands.size());

    auto pCmd = static_cast<PIPELINE_SELECT *>(commands.back());

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_TRUE(pCmd->getMediaSamplerDopClockGateEnable());
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToFalse) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = true;
    enqueueVmeKernel<FamilyType>();
    EXPECT_TRUE(csr->lastVmeSubslicesConfig);
}

GEN9TEST_F(MediaKernelTest, givenGen9CsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = false;
    enqueueVmeKernel<FamilyType>();
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

GEN9TEST_F(MediaKernelTest, gen9CmdSizeForMediaSampler) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getCommandStreamReceiver());

    csr->lastVmeSubslicesConfig = false;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));

    csr->lastVmeSubslicesConfig = true;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));
}
