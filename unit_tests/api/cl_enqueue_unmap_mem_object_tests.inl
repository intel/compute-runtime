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

#include "runtime/command_queue/command_queue.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/api/cl_api_tests.h"
#include <memory>

using namespace OCLRT;

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
