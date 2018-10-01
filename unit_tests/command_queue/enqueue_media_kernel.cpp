/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/media_kernel_fixture.h"
#include "test.h"

using namespace OCLRT;

typedef MediaKernelFixture<HelloWorldFixtureFactory> MediaKernelTest;

TEST_F(MediaKernelTest, VmeKernelProperlyIdentifiesItself) {
    ASSERT_NE(true, pKernel->isVmeKernel());
    ASSERT_EQ(true, pVmeKernel->isVmeKernel());
}

HWTEST_F(MediaKernelTest, EnqueueVmeKernelUsesSinglePipelineSelect) {
    enqueueVmeKernel<FamilyType>();
    auto numCommands = getCommandsList<typename FamilyType::PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}

HWTEST_F(MediaKernelTest, EnqueueRegularKernelUsesSinglePipelineSelect) {
    enqueueRegularKernel<FamilyType>();
    auto numCommands = getCommandsList<typename FamilyType::PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numCommands);
}
