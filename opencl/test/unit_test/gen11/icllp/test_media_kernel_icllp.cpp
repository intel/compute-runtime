/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble.h"

#include "opencl/test/unit_test/fixtures/media_kernel_fixture.h"
#include "test.h"

using namespace NEO;
typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

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
