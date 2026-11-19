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

TEST_F(ExternalMemoryBufferTest, givenDmaBufHandleWhenCreatingBufferThenFileDescriptorIsImported) {
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR), supportedExternalMemoryHandleKey);
    capturingContext->allocDeviceMemPtr = &dummyStorage;

    for (const cl_mem_properties handle : {0x10, 0x0}) {
        cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, handle, 0};
        cl_int errcode = CL_INVALID_VALUE;
        capturingContext->allocDeviceMemArgs.records.clear();

        auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);

        ASSERT_EQ(CL_SUCCESS, errcode) << handle;
        ASSERT_EQ(1u, capturingContext->allocDeviceMemArgs.count());
        EXPECT_TRUE(capturingContext->allocDeviceMemArgs[0].fdImport);
        EXPECT_FALSE(capturingContext->allocDeviceMemArgs[0].win32Import);
        EXPECT_EQ(static_cast<int>(handle), capturingContext->allocDeviceMemArgs[0].fd);

        EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
    }
}

TEST_F(ExternalMemoryBufferTest, givenWin32HandleWhenCreatingBufferThenInvalidPropertyIsReturnedAndNothingIsImported) {
    cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0x10, 0};
    cl_int errcode = CL_SUCCESS;

    auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);

    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);
    EXPECT_FALSE(capturingContext->allocDeviceMemArgs.wasCalled());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
