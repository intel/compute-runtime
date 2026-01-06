/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

using GetPlatformInfoLinuxTest = Test<PlatformFixture>;

TEST_F(GetPlatformInfoLinuxTest, GivenPlatformWhenGettingInfoForExternalMemoryThenCorrectHandlesAreReturned) {
    size_t retSize = 0;
    auto retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t handlesCount = retSize / sizeof(cl_external_memory_handle_type_khr);
    ASSERT_EQ(1u, handlesCount);

    auto handleTypes = std::make_unique<cl_external_memory_handle_type_khr[]>(handlesCount);

    retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR,
                               retSize, handleTypes.get(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(pPlatform->getClDevice(0)->getDeviceInfo().externalMemorySharing, handleTypes[0]);
    EXPECT_EQ(static_cast<cl_external_memory_handle_type_khr>(CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR), handleTypes[0]);
}
