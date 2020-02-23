/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"

#include <memory>

using namespace NEO;

typedef api_tests clEnqueueUnmapMemObjTests;

TEST_F(clEnqueueUnmapMemObjTests, givenValidAddressWhenUnmappingThenReturnSuccess) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueUnmapMemObjTests, givenInvalidAddressWhenUnmappingOnCpuThenReturnError) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        ptrOffset(mappedPtr, buffer->getSize() + 1),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueUnmapMemObjTests, givenInvalidAddressWhenUnmappingOnGpuThenReturnError) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    buffer->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        ptrOffset(mappedPtr, buffer->getSize() + 1),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}
