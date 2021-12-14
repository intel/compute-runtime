/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/media_kernel_fixture.h"

using namespace NEO;

typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

TEST_F(MediaKernelTest, GivenKernelWhenCheckingIsVmeKernelThenOnlyVmeKernelReportsTrue) {
    ASSERT_NE(true, pKernel->isVmeKernel());
    ASSERT_EQ(true, pVmeKernel->isVmeKernel());
}

HWTEST_F(MediaKernelTest, GivenVmeKernelWhenEnqueuingKernelThenSinglePipelineSelectIsProgrammed) {
    enqueueVmeKernel<FamilyType>();
    auto numCommands = getCommandsList<typename FamilyType::PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

HWTEST_F(MediaKernelTest, GivenNonVmeKernelWhenEnqueuingKernelThenSinglePipelineSelectIsProgrammed) {
    enqueueRegularKernel<FamilyType>();
    auto numCommands = getCommandsList<typename FamilyType::PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}
