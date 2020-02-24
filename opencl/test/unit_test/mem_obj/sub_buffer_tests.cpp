/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_constants.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace ULT {

static const unsigned int sizeTestBufferInBytes = 32;

class SubBufferTest : public DeviceFixture,
                      public ::testing::Test {
  public:
    SubBufferTest() {
    }

  protected:
    void SetUp() override {
        buffer = Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                sizeTestBufferInBytes, pHostPtr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, buffer);
    }

    void TearDown() override {
        delete buffer;
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    unsigned char pHostPtr[sizeTestBufferInBytes];
    Buffer *buffer = nullptr;
};

TEST_F(SubBufferTest, createSubBuffer) {
    cl_buffer_region region = {2, 12};
    EXPECT_EQ(1, buffer->getRefInternalCount());

    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal);
    EXPECT_EQ(2, buffer->getRefInternalCount());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, subBuffer);

    delete subBuffer;
    EXPECT_EQ(1, buffer->getRefInternalCount());
}

TEST_F(SubBufferTest, GivenUnalignedHostPtrBufferWhenSubBufferIsCreatedThenItIsNonZeroCopy) {
    cl_buffer_region region = {2, 2};
    cl_int retVal = 0;

    void *pUnalignedHostPtr = alignUp(&pHostPtr, 4);
    Buffer *buffer = Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                    sizeTestBufferInBytes, pUnalignedHostPtr, retVal);
    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(subBuffer->isMemObjZeroCopy());

    subBuffer->release();
    buffer->release();
}

TEST_F(SubBufferTest, GivenAlignmentThatIsHigherThen4BytesWhenCheckedForValidityThenTrueIsReturned) {

    cl_buffer_region region = {2, 2};
    EXPECT_FALSE(buffer->isValidSubBufferOffset(region.origin));
    cl_buffer_region region2 = {4, 4};
    EXPECT_TRUE(buffer->isValidSubBufferOffset(region2.origin));
    cl_buffer_region region3 = {8, 4};
    EXPECT_TRUE(buffer->isValidSubBufferOffset(region3.origin));

    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    EXPECT_FALSE(buffer->isValidSubBufferOffset(region.origin));
    EXPECT_FALSE(buffer->isValidSubBufferOffset(region2.origin));
    cl_buffer_region region4 = {1025, 4};
    EXPECT_FALSE(buffer->isValidSubBufferOffset(region4.origin));
    cl_buffer_region region5 = {1024, 4};
    EXPECT_TRUE(buffer->isValidSubBufferOffset(region5.origin));
    cl_buffer_region region6 = {127, 4};
    EXPECT_FALSE(buffer->isValidSubBufferOffset(region6.origin));
    cl_buffer_region region7 = {128, 4};
    EXPECT_TRUE(buffer->isValidSubBufferOffset(region7.origin));
    cl_buffer_region region8 = {129, 4};
    EXPECT_FALSE(buffer->isValidSubBufferOffset(region8.origin));
}

TEST_F(SubBufferTest, givenSharingHandlerFromParentBufferWhenCreateThenShareHandler) {
    cl_buffer_region region = {2, 12};
    auto handler = new SharingHandler();
    buffer->setSharingHandler(handler);

    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal);
    ASSERT_NE(nullptr, subBuffer);
    EXPECT_EQ(subBuffer->getSharingHandler().get(), handler);

    delete subBuffer;
    EXPECT_EQ(1, buffer->getRefInternalCount());
}

TEST_F(SubBufferTest, GivenBufferWithAlignedHostPtrAndSameMemoryStorageWhenSubBufferIsCreatedThenHostPtrAndMemoryStorageAreOffseted) {
    cl_buffer_region region = {2, 2};
    cl_int retVal = 0;

    void *alignedPointer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::preferredAlignment);
    Buffer *buffer = Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL,
                                    MemoryConstants::pageSize, alignedPointer, retVal);

    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(alignedPointer, buffer->getHostPtr());
    EXPECT_EQ(alignedPointer, buffer->getCpuAddress());

    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(ptrOffset(alignedPointer, 2), subBuffer->getHostPtr());
    EXPECT_EQ(ptrOffset(alignedPointer, 2), subBuffer->getCpuAddress());

    subBuffer->release();
    buffer->release();
    alignedFree(alignedPointer);
}

TEST_F(SubBufferTest, GivenBufferWithMemoryStorageAndNullHostPtrWhenSubBufferIsCreatedThenMemoryStorageIsOffsetedAndHostPtrIsNull) {
    cl_buffer_region region = {2, 2};
    cl_int retVal = 0;

    Buffer *buffer = Buffer::create(&context, CL_MEM_READ_WRITE,
                                    MemoryConstants::pageSize, nullptr, retVal);

    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, buffer->getHostPtr());
    EXPECT_NE(nullptr, buffer->getCpuAddress());

    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, subBuffer->getHostPtr());
    EXPECT_EQ(ptrOffset(buffer->getCpuAddress(), 2), subBuffer->getCpuAddress());

    subBuffer->release();
    buffer->release();
}

TEST_F(SubBufferTest, givenBufferWithHostPtrWhenSubbufferGetsMapPtrThenExpectBufferHostPtr) {
    cl_buffer_region region = {0, 16};

    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region, retVal);
    ASSERT_NE(nullptr, subBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    void *mapPtr = subBuffer->getBasePtrForMap(0);
    EXPECT_EQ(pHostPtr, mapPtr);
    mapPtr = subBuffer->getBasePtrForMap(0);
    EXPECT_EQ(pHostPtr, mapPtr);

    subBuffer->release();
}

TEST_F(SubBufferTest, givenBufferWithNoHostPtrWhenSubbufferGetsMapPtrThenExpectBufferMap) {
    cl_buffer_region region = {0, 16};

    Buffer *buffer = Buffer::create(&context, CL_MEM_READ_WRITE,
                                    MemoryConstants::pageSize, nullptr, retVal);
    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region, retVal);
    ASSERT_NE(nullptr, subBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    void *mapPtr = subBuffer->getBasePtrForMap(0);
    void *bufferMapPtr = buffer->getBasePtrForMap(0);
    EXPECT_EQ(bufferMapPtr, mapPtr);
    auto mapAllocation = subBuffer->getMapAllocation();
    auto bufferMapAllocation = buffer->getMapAllocation();
    ASSERT_NE(nullptr, bufferMapAllocation);
    EXPECT_EQ(bufferMapAllocation, mapAllocation);
    EXPECT_EQ(bufferMapPtr, mapAllocation->getUnderlyingBuffer());

    mapPtr = subBuffer->getBasePtrForMap(0);
    EXPECT_EQ(bufferMapPtr, mapPtr);

    subBuffer->release();
    buffer->release();
}

} // namespace ULT
