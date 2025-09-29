/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using ClCreateProgramWithILTests = ApiTests;

namespace ULT {
TEST_F(ClCreateProgramWithILTests, GivenIncorrectIlWhenCreatingProgramWithIlThenExpectedErrorIsReturned) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(ClCreateProgramWithILTests, GivenIncorrectIlAndNoErrorPointerWhenCreatingProgramWithIlThenExpectedErrorIsReturned) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), nullptr);
    EXPECT_EQ(nullptr, prog);
}
} // namespace ULT