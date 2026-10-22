/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/sharings/va/cl_va_api.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

TEST(ClGetExtensionFunctionAddressTests, GivenVaApiMediaFunctionNameWhenGettingExtensionFunctionAddressThenCorrectAddressIsReturned) {
    EXPECT_EQ(reinterpret_cast<void *>(clCreateFromVA_APIMediaSurfaceINTEL), clGetExtensionFunctionAddress("clCreateFromVA_APIMediaSurfaceINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueAcquireVA_APIMediaSurfacesINTEL), clGetExtensionFunctionAddress("clEnqueueAcquireVA_APIMediaSurfacesINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueReleaseVA_APIMediaSurfacesINTEL), clGetExtensionFunctionAddress("clEnqueueReleaseVA_APIMediaSurfacesINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetDeviceIDsFromVA_APIMediaAdapterINTEL), clGetExtensionFunctionAddress("clGetDeviceIDsFromVA_APIMediaAdapterINTEL"));
}

TEST(ClGetExtensionFunctionAddressTests, givenEnabledFormatQueryWhenGettingFuncionAddressThenCorrectAddressIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(true);

    auto retVal = clGetExtensionFunctionAddress("clGetSupportedVA_APIMediaSurfaceFormatsINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetSupportedVA_APIMediaSurfaceFormatsINTEL));
}

TEST(ClGetExtensionFunctionAddressTests, givenDisabledFormatQueryWhenGettingFuncionAddressThenNullptrIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(false);

    auto retVal = clGetExtensionFunctionAddress("clGetSupportedVA_APIMediaSurfaceFormatsINTEL");
    EXPECT_EQ(retVal, nullptr);
}
} // namespace ULT
