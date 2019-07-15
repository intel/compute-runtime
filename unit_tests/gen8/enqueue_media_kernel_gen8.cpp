/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/media_kernel_fixture.h"

using namespace NEO;

typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

GEN8TEST_F(MediaKernelTest, givenGen8CSRWhenEnqueueVmeKernelThenProgramOnlyPipelineSelection) {
    typedef typename BDWFamily::PIPELINE_SELECT PIPELINE_SELECT;
    enqueueVmeKernel<FamilyType>();
    auto pCmd = getCommand<PIPELINE_SELECT>();
    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits;
    auto expectedPipelineSelection = PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(expectedPipelineSelection, pCmd->getPipelineSelection());
}

GEN8TEST_F(MediaKernelTest, givenGen8CsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToFalse) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = true;
    enqueueVmeKernel<FamilyType>();
    EXPECT_TRUE(csr->lastVmeSubslicesConfig);
}

GEN8TEST_F(MediaKernelTest, givenGen8CsrWhenEnqueueVmeKernelThenVmeSubslicesConfigDoesntChangeToTrue) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    csr->lastVmeSubslicesConfig = false;
    enqueueVmeKernel<FamilyType>();
    EXPECT_FALSE(csr->lastVmeSubslicesConfig);
}

GEN8TEST_F(MediaKernelTest, gen8CmdSizeForMediaSampler) {
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    csr->lastVmeSubslicesConfig = false;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));

    csr->lastVmeSubslicesConfig = true;
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(false));
    EXPECT_EQ(0u, csr->getCmdSizeForMediaSampler(true));
}
