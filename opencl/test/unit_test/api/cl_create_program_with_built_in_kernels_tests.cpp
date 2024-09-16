/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

using ClCreateProgramWithBuiltInKernelsTests = ApiTests;

namespace ULT {

TEST_F(ClCreateProgramWithBuiltInKernelsTests, GivenMediaKernelsWhenCreatingProgramWithBuiltInKernelsThenProgramIsNotCreated) {
    cl_int retVal = CL_SUCCESS;

    const char *kernelNamesString = {
        "block_advanced_motion_estimate_bidirectional_check_intel;"
        "block_motion_estimate_intel;"
        "block_advanced_motion_estimate_check_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        &testedClDevice,   // device_list
        kernelNamesString, // kernel_names
        &retVal);

    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}
} // namespace ULT
