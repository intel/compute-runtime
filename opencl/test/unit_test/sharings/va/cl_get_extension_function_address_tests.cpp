/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/sharings/va/cl_va_api.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using ClGetExtensionFunctionAddressTests = ApiTests;

namespace ULT {

TEST_F(ClGetExtensionFunctionAddressTests, GivenClCreateFromVaMediaSurfaceIntelWhenGettingFunctionAddressThenCorrectPointerReturned) {
    auto retVal = clGetExtensionFunctionAddress("clCreateFromVA_APIMediaSurfaceINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateFromVA_APIMediaSurfaceINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnqueueAcquireVaApiMediaSurfacesIntelWhenGettingFunctionAddressThenCorrectPointerReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueAcquireVA_APIMediaSurfacesINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueAcquireVA_APIMediaSurfacesINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnqueueReleaseVaApiMediaSurfacesIntelWhenGettingFunctionAddressThenCorrectPointerReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueReleaseVA_APIMediaSurfacesINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueReleaseVA_APIMediaSurfacesINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetDeviceIDsFromVaApiMediaAdapterIntelWhenGettingFunctionAddressThenCorrectPointerReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetDeviceIDsFromVA_APIMediaAdapterINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetDeviceIDsFromVA_APIMediaAdapterINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, givenEnabledFormatQueryWhenGettingFuncionAddressThenCorrectAddressIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(true);

    auto retVal = clGetExtensionFunctionAddress("clGetSupportedVA_APIMediaSurfaceFormatsINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetSupportedVA_APIMediaSurfaceFormatsINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, givenDisabledFormatQueryWhenGettingFuncionAddressThenNullptrIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(false);

    auto retVal = clGetExtensionFunctionAddress("clGetSupportedVA_APIMediaSurfaceFormatsINTEL");
    EXPECT_EQ(retVal, nullptr);
}
} // namespace ULT
