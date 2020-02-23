/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetGLObjectInfo_;

namespace ULT {

TEST_F(clGetGLObjectInfo_, givenNullMemObjectWhenGetGlObjectInfoIsCalledThenInvalidMemObjectIsReturned) {
    auto retVal = clGetGLObjectInfo(nullptr, // cl_mem memobj
                                    nullptr, // 	cl_gl_object_type *gl_object_type
                                    nullptr  // GLuint *gl_object_name
    );
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}
} // namespace ULT
