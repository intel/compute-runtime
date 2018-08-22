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
#include "test.h"

using namespace OCLRT;

typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

TEST_F(MediaKernelTest, VmeKernelProperlyIdentifiesItself) {
    ASSERT_NE(true, pKernel->isVmeKernel());
    ASSERT_EQ(true, pVmeKernel->isVmeKernel());
}

HWCMDTEST_F(IGFX_GEN8_CORE, MediaKernelTest, EnqueueVmeKernelUsesSinglePipelineSelect) {
    enqueueVmeKernel<FamilyType>();
    auto numCommands = getCommandsList<typename FamilyType::PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, MediaKernelTest, EnqueueRegularKernelUsesSinglePipelineSelect) {
    enqueueRegularKernel<FamilyType>();
    auto numCommands = getCommandsList<typename FamilyType::PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}
