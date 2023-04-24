/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using ClGetGLTextureInfo_ = ApiTests;

namespace ULT {

TEST_F(ClGetGLTextureInfo_, givenNullMemObjectWhenGetGLTextureInfoIsCalledThenInvalidMemObjectIsReturned) {
    auto retVal = clGetGLTextureInfo(nullptr,              // cl_mem memobj
                                     CL_GL_TEXTURE_TARGET, // cl_gl_texture_info param_name
                                     0,                    // size_t param_value_size
                                     nullptr,              // void *param_value
                                     nullptr               // size_t *param_value_size_ret
    );
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}
} // namespace ULT
