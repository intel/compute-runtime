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

#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/two_walker_fixture.h"
#include "test.h"

using namespace OCLRT;

struct OOQFixtureFactory : public HelloWorldFixtureFactory {
    typedef OOQueueFixture CommandQueueFixture;
};

typedef TwoWalkerTest<OOQFixtureFactory> OOQWithTwoWalkers;

HWCMDTEST_F(IGFX_GEN8_CORE, OOQWithTwoWalkers, shouldHaveTwoWalkers) {
    enqueueTwoKernels<FamilyType>();

    EXPECT_NE(itorWalker1, itorWalker2);
}

HWCMDTEST_F(IGFX_GEN8_CORE, OOQWithTwoWalkers, shouldHaveOnePS) {
    enqueueTwoKernels<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, OOQWithTwoWalkers, shouldHaveOneVFEState) {
    enqueueTwoKernels<FamilyType>();

    auto numCommands = getCommandsList<typename FamilyType::MEDIA_VFE_STATE>().size();
    EXPECT_EQ(1u, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, OOQWithTwoWalkers, shouldntHaveAPipecontrolBetweenWalkers) {
    enqueueTwoKernels<FamilyType>();

    auto itorCmd = find<typename FamilyType::PIPE_CONTROL *>(itorWalker1, itorWalker2);
    // Workaround for DRM i915 coherency patch
    // EXPECT_EQ(itorWalker2, itorCmd);
    EXPECT_NE(itorWalker2, itorCmd);
}
