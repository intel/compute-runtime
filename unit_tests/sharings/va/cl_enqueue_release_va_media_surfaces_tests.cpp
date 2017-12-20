/*
 * Copyright (c) 2017, Intel Corporation
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
#include <CL/cl_va_api_media_sharing_intel.h>

using namespace OCLRT;

typedef api_tests clEnqueueReleaseVaMediaSurfacesTests;

namespace ULT {

TEST_F(clEnqueueReleaseVaMediaSurfacesTests, givenNullCommandQueueWhenReleaseObjectsIsCalledThenInvalidCommandQueueIsReturned) {
    retVal = clEnqueueReleaseVA_APIMediaSurfacesINTEL(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_COMMAND_QUEUE);
}
} // namespace ULT
