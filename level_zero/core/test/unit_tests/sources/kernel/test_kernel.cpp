/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

TEST(Kernel, givenPassInlineDataTrueWhenCallingIsInlineDataRequiredThenTrueIsReturned) {
    Mock<Kernel> kernel;

    kernel.descriptor.kernelAttributes.flags.passInlineData = true;
    EXPECT_TRUE(kernel.isInlineDataRequired());
}

TEST(Kernel, givenPassInlineDataFalseWhenCallingIsInlineDataRequiredThenFalseIsReturned) {
    Mock<Kernel> kernel;

    kernel.descriptor.kernelAttributes.flags.passInlineData = false;
    EXPECT_FALSE(kernel.isInlineDataRequired());
}

} // namespace ult
} // namespace L0
