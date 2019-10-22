/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

struct GetMemObjectSubBufferInfo : public ::testing::Test {
    GetMemObjectSubBufferInfo()

    {
    }

    void SetUp() override {
        bufferStorage = alignedMalloc(4096, MemoryConstants::preferredAlignment);
        region.origin = 4;
        region.size = 12;
    }

    void TearDown() override {
        delete subBuffer;
        delete buffer;
        alignedFree(bufferStorage);
    }

    void createBuffer(cl_mem_flags flags = CL_MEM_READ_WRITE) {
        auto retVal = CL_INVALID_VALUE;
        buffer = Buffer::create(&context, flags, bufferSize, nullptr, retVal);
        ASSERT_NE(nullptr, buffer);
    }
    void createSubBuffer(cl_mem_flags flags = CL_MEM_READ_WRITE) {
        cl_int retVal;
        subBuffer = buffer->createSubBuffer(flags, 0, &region, retVal);
        ASSERT_NE(nullptr, subBuffer);
    }

    void createHostPtrBuffer(cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR) {
        auto retVal = CL_INVALID_VALUE;
        buffer = Buffer::create(&context, flags, bufferSize, bufferStorage, retVal);
        ASSERT_NE(nullptr, buffer);
    }

    MockContext context;
    Buffer *buffer = nullptr;
    Buffer *subBuffer = nullptr;
    void *bufferStorage;
    static const size_t bufferSize = 256;
    cl_buffer_region region;
    cl_int retVal;
    size_t sizeReturned = 0;
};

TEST_F(GetMemObjectSubBufferInfo, MEM_ASSOCIATED_MEMOBJECT) {
    createBuffer();
    createSubBuffer();

    cl_mem object = nullptr;
    retVal = subBuffer->getMemObjectInfo(CL_MEM_ASSOCIATED_MEMOBJECT, 0, nullptr, &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(object), sizeReturned);

    retVal = subBuffer->getMemObjectInfo(CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(object),
                                         &object, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem clBuffer = (cl_mem)buffer;
    EXPECT_EQ(clBuffer, object);
}

TEST_F(GetMemObjectSubBufferInfo, MEM_OFFSET) {
    createBuffer();
    createSubBuffer();

    size_t offset = 0;
    retVal = subBuffer->getMemObjectInfo(CL_MEM_OFFSET, 0, nullptr, &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(offset), sizeReturned);

    retVal = subBuffer->getMemObjectInfo(CL_MEM_OFFSET, sizeof(offset), &offset, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(region.origin, offset);
}

TEST_F(GetMemObjectSubBufferInfo, MEM_FLAGS) {
    createBuffer();
    createSubBuffer();

    cl_mem_flags flags = 0;
    retVal = subBuffer->getMemObjectInfo(CL_MEM_FLAGS, 0, nullptr, &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(flags), sizeReturned);

    retVal = subBuffer->getMemObjectInfo(CL_MEM_FLAGS, sizeof(flags), &flags, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), flags);
}

TEST_F(GetMemObjectSubBufferInfo, MEM_FLAGS_empty) {
    createBuffer(CL_MEM_READ_ONLY);
    createSubBuffer(0);

    cl_mem_flags flags = 0;
    retVal = subBuffer->getMemObjectInfo(CL_MEM_FLAGS, 0, nullptr, &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(flags), sizeReturned);

    retVal = subBuffer->getMemObjectInfo(
        CL_MEM_FLAGS,
        sizeof(flags),
        &flags,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_mem_flags>(0), flags);
}

TEST_F(GetMemObjectSubBufferInfo, MEM_HOST_PTR) {
    createHostPtrBuffer();
    createSubBuffer();

    void *hostPtr = 0;
    retVal = subBuffer->getMemObjectInfo(CL_MEM_HOST_PTR, 0, nullptr, &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(hostPtr), sizeReturned);

    retVal = subBuffer->getMemObjectInfo(CL_MEM_HOST_PTR, sizeof(hostPtr), &hostPtr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto expected = ptrOffset(this->bufferStorage, region.origin);
    EXPECT_EQ(expected, hostPtr);
}
