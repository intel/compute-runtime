/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_mdi.h"

using namespace NEO;

struct MultiDispatchInfoTest : public ::testing::Test {

    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(MultiDispatchInfoTest, GivenNullKernelWhenCreatingMultiDispatchInfoThenExpectationsAreMet) {

    MockMultiDispatchInfo multiDispatchInfo(nullptr, nullptr);

    EXPECT_FALSE(multiDispatchInfo.begin()->usesSlm());
    EXPECT_FALSE(multiDispatchInfo.begin()->usesStatelessPrintfSurface());
    EXPECT_EQ(0u, multiDispatchInfo.begin()->getRequiredScratchSize());
}