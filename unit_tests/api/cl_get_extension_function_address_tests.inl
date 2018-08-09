/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clGetExtensionFunctionAddressTests;

namespace ULT {

TEST_F(clGetExtensionFunctionAddressTests, unknownExtension) {
    auto retVal = clGetExtensionFunctionAddress("__some__function__");
    EXPECT_EQ(nullptr, retVal);
}

TEST_F(clGetExtensionFunctionAddressTests, clIcdGetPlatformIDsKHR) {
    auto retVal = clGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clIcdGetPlatformIDsKHR));
}

TEST_F(clGetExtensionFunctionAddressTests, clCreateAcceleratorINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clCreateAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, clGetAcceleratorInfoINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clGetAcceleratorInfoINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetAcceleratorInfoINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, clRetainAcceleratorINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clRetainAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clRetainAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, clReleaseAcceleratorINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clReleaseAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clReleaseAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, clCreatePerfCountersCommandQueueINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clCreatePerfCountersCommandQueueINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreatePerfCountersCommandQueueINTEL));
}
TEST_F(clGetExtensionFunctionAddressTests, clSetPerformanceConfigurationINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clSetPerformanceConfigurationINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetPerformanceConfigurationINTEL));
}
} // namespace ULT
