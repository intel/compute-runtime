/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/sharings/va/cl_va_api.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetExtensionFunctionAddressTests;

namespace ULT {

TEST_F(clGetExtensionFunctionAddressTests, GivenClCreateFromVaMediaSurfaceIntelWhenGettingFunctionAddressThenCorrectPointerReturned) {
    auto retVal = clGetExtensionFunctionAddress("clCreateFromVA_APIMediaSurfaceINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateFromVA_APIMediaSurfaceINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnqueueAcquireVaApiMediaSurfacesIntelWhenGettingFunctionAddressThenCorrectPointerReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueAcquireVA_APIMediaSurfacesINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueAcquireVA_APIMediaSurfacesINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnqueueReleaseVaApiMediaSurfacesIntelWhenGettingFunctionAddressThenCorrectPointerReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueReleaseVA_APIMediaSurfacesINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueReleaseVA_APIMediaSurfacesINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClGetDeviceIDsFromVaApiMediaAdapterIntelWhenGettingFunctionAddressThenCorrectPointerReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetDeviceIDsFromVA_APIMediaAdapterINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetDeviceIDsFromVA_APIMediaAdapterINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, givenEnabledFormatQueryWhenGettingFuncionAddressThenCorrectAddressIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableFormatQuery.set(true);

    auto retVal = clGetExtensionFunctionAddress("clGetSupportedVA_APIMediaSurfaceFormatsINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetSupportedVA_APIMediaSurfaceFormatsINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, givenDisabledFormatQueryWhenGettingFuncionAddressThenNullptrIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableFormatQuery.set(false);

    auto retVal = clGetExtensionFunctionAddress("clGetSupportedVA_APIMediaSurfaceFormatsINTEL");
    EXPECT_EQ(retVal, nullptr);
}
} // namespace ULT
