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

typedef api_tests clGetExtensionFunctionAddressForPlatformTests;

namespace ULT {

TEST_F(clGetExtensionFunctionAddressForPlatformTests, invalidPlatform) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(nullptr, "clCreateAcceleratorINTEL");
    EXPECT_EQ(retVal, nullptr);
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, unknownExtension) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "__some__function__");
    EXPECT_EQ(retVal, nullptr);
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, clCreateAcceleratorINTEL) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clCreateAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, clGetAcceleratorInfoINTEL) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clGetAcceleratorInfoINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetAcceleratorInfoINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, clRetainAcceleratorINTEL) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clRetainAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clRetainAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, clReleaseAcceleratorINTEL) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clReleaseAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clReleaseAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, clCreateProgramWithILKHR) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clCreateProgramWithILKHR");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateProgramWithILKHR));
}
} // namespace ULT
