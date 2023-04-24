/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using ClGetGLObjectInfo_ = ApiTests;

namespace ULT {

TEST_F(ClGetGLObjectInfo_, givenNullMemObjectWhenGetGlObjectInfoIsCalledThenInvalidMemObjectIsReturned) {
    auto retVal = clGetGLObjectInfo(nullptr, // cl_mem memobj
                                    nullptr, // 	cl_gl_object_type *gl_object_type
                                    nullptr  // GLuint *gl_object_name
    );
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}
} // namespace ULT
