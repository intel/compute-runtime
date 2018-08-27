/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/api/cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clCreateFromGLTexture3D_;

namespace ULT {

TEST_F(clCreateFromGLTexture3D_, givenNullConxtextWhenClCreateFromGlTexture2DIsCalledThenInvalidContextIsReturned) {
    int errCode = CL_SUCCESS;
    auto retVal = clCreateFromGLTexture3D(nullptr,           // cl_context context
                                          CL_MEM_READ_WRITE, // cl_mem_flags flags
                                          0,                 // GLenum texture_target
                                          0,                 // GLint miplevel
                                          0,                 // GLuint texture
                                          &errCode           // cl_int *errcode_ret
    );
    EXPECT_EQ(nullptr, retVal);
    EXPECT_EQ(errCode, CL_INVALID_CONTEXT);
}
} // namespace ULT
