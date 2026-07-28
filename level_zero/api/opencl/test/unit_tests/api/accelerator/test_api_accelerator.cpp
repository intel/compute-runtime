/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(CreateAcceleratorTests, givenAnyInputWhenCreateAcceleratorThenErrcodeIsSetToInvalidOperation) {
    cl_int errcode = CL_SUCCESS;
    auto accelerator = clCreateAcceleratorINTEL(nullptr, CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, 0, nullptr, &errcode);
    EXPECT_EQ(nullptr, accelerator);
    EXPECT_EQ(CL_INVALID_OPERATION, errcode);
}

TEST(CreateAcceleratorTests, givenNullErrcodeWhenCreateAcceleratorThenReturnsNullptr) {
    EXPECT_EQ(nullptr, clCreateAcceleratorINTEL(nullptr, CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, 0, nullptr, nullptr));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
