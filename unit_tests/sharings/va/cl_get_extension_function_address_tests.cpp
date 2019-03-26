/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/api/cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetExtensionFunctionAddressTests;

namespace ULT {

TEST_F(clGetExtensionFunctionAddressTests, clCreateFromVaMediaSurfaceINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clCreateFromVA_APIMediaSurfaceINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateFromVA_APIMediaSurfaceINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, clEnqueueAcquireVA_APIMediaSurfacesINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueAcquireVA_APIMediaSurfacesINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueAcquireVA_APIMediaSurfacesINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, clEnqueueReleaseVA_APIMediaSurfacesINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueReleaseVA_APIMediaSurfacesINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueReleaseVA_APIMediaSurfacesINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, clGetDeviceIDsFromVA_APIMediaAdapterINTEL) {
    auto retVal = clGetExtensionFunctionAddress("clGetDeviceIDsFromVA_APIMediaAdapterINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetDeviceIDsFromVA_APIMediaAdapterINTEL));
}
} // namespace ULT
