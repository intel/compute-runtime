/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/test/common/fixtures/buffer_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST_F(ExternalMemoryBufferTest, givenWin32HandleWhenCreatingBufferThenWin32HandleIsImportedWithoutName) {
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR), supportedExternalMemoryHandleKey);
    capturingContext->allocDeviceMemPtr = &dummyStorage;
    cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0x10, 0};
    cl_int errcode = CL_INVALID_VALUE;

    auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);

    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_EQ(1u, capturingContext->allocDeviceMemArgs.count());
    EXPECT_TRUE(capturingContext->allocDeviceMemArgs[0].win32Import);
    EXPECT_FALSE(capturingContext->allocDeviceMemArgs[0].fdImport);
    EXPECT_EQ(reinterpret_cast<void *>(0x10), capturingContext->allocDeviceMemArgs[0].win32Handle);
    EXPECT_EQ(nullptr, capturingContext->allocDeviceMemArgs[0].win32Name);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(ExternalMemoryBufferTest, givenDmaBufHandleWhenCreatingBufferThenInvalidPropertyIsReturnedAndNothingIsImported) {
    cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, 0x10, 0};
    cl_int errcode = CL_SUCCESS;

    auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);

    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);
    EXPECT_FALSE(capturingContext->allocDeviceMemArgs.wasCalled());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
