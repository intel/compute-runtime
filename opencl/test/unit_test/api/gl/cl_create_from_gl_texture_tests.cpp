/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

#include <GL/gl.h>

using namespace NEO;

typedef api_tests clCreateFromGLTexture_;

namespace ULT {
TEST_F(clCreateFromGLTexture_, givenNullContextWhenCreateIsCalledThenErrorIsReturned) {
    int errCode = CL_SUCCESS;
    auto image = clCreateFromGLTexture(nullptr, CL_MEM_READ_WRITE, GL_TEXTURE_1D, 0, 0, &errCode);
    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(errCode, CL_INVALID_CONTEXT);
}
} // namespace ULT
