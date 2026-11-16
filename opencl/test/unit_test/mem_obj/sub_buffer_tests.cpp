/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace ULT {

static const unsigned int sizeTestBufferInBytes = 32;

class SubBufferTest : public ClDeviceFixture,
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
        ClDeviceFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    unsigned char pHostPtr[sizeTestBufferInBytes];
    Buffer *buffer = nullptr;
};

TEST_F(SubBufferTest, WhenCreatingSubBufferThenRefInternalCountIsIncremented) {
    cl_buffer_region region = {2, 12};
    EXPECT_EQ(1, buffer->getRefInternalCount());

    auto subBuffer = std::unique_ptr<Buffer>(buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal));
    EXPECT_EQ(2, buffer->getRefInternalCount());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, subBuffer);

    subBuffer.reset();
    EXPECT_EQ(1, buffer->getRefInternalCount());
}

TEST_F(SubBufferTest, givenSubBufferWhenGetHighestRootMemObjIsCalledThenProperMemObjIsReturned) {
    cl_buffer_region region0 = {2, 12};

    auto subBuffer = clUniquePtr(buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region0, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(static_cast<MemObj *>(buffer), buffer->getHighestRootMemObj());
    EXPECT_EQ(static_cast<MemObj *>(buffer), subBuffer->getHighestRootMemObj());
}

TEST_F(SubBufferTest, GivenUnalignedHostPtrBufferWhenSubBufferIsCreatedThenItIsNonZeroCopy) {
    cl_buffer_region region = {2, 2};
    cl_int retVal = 0;

    void *pUnalignedHostPtr = alignUp(&pHostPtr, 4);
    auto buffer = clUniquePtr(Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                             sizeTestBufferInBytes, pUnalignedHostPtr, retVal));
    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto subBuffer = clUniquePtr(buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal));
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(subBuffer->isMemObjZeroCopy());
}

TEST_F(SubBufferTest, GivenAlignmentThatIsHigherThen4BytesWhenCheckedForValidityThenTrueIsReturned) {

    cl_buffer_region region = {2, 2};
    EXPECT_FALSE(buffer->isValidSubBufferOffset(region.origin));
    cl_buffer_region region2 = {4, 4};
    EXPECT_TRUE(buffer->isValidSubBufferOffset(region2.origin));
    cl_buffer_region region3 = {8, 4};
    EXPECT_TRUE(buffer->isValidSubBufferOffset(region3.origin));

    MockBuffer::setAllocationType(buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()), context.getDevice(0)->getRootDeviceEnvironment().getGmmHelper(), true);

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

    auto subBuffer = std::unique_ptr<Buffer>(buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal));
    ASSERT_NE(nullptr, subBuffer);
    EXPECT_EQ(subBuffer->getSharingHandler().get(), handler);

    subBuffer.reset();
    EXPECT_EQ(1, buffer->getRefInternalCount());
}

TEST_F(SubBufferTest, GivenBufferWithAlignedHostPtrAndSameMemoryStorageWhenSubBufferIsCreatedThenHostPtrAndMemoryStorageAreOffset) {
    cl_buffer_region region = {2, 2};
    cl_int retVal = 0;

    void *alignedPointer = reinterpret_cast<void *>(0x100000);
    auto buffer = clUniquePtr(Buffer::create(&context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL,
                                             MemoryConstants::pageSize, alignedPointer, retVal));

    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(alignedPointer, buffer->getHostPtr());
    EXPECT_EQ(alignedPointer, buffer->getCpuAddress());

    auto subBuffer = clUniquePtr(buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal));
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(ptrOffset(alignedPointer, 2), subBuffer->getHostPtr());
    EXPECT_EQ(ptrOffset(alignedPointer, 2), subBuffer->getCpuAddress());
}

TEST_F(SubBufferTest, GivenBufferWithMemoryStorageAndNullHostPtrWhenSubBufferIsCreatedThenMemoryStorageIsOffsetAndHostPtrIsNull) {
    cl_buffer_region region = {2, 2};
    cl_int retVal = 0;

    auto buffer = clUniquePtr(Buffer::create(&context, CL_MEM_READ_WRITE,
                                             MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, buffer->getHostPtr());
    EXPECT_NE(nullptr, buffer->getCpuAddress());

    auto subBuffer = clUniquePtr(buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region, retVal));
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, subBuffer->getHostPtr());
    EXPECT_EQ(ptrOffset(buffer->getCpuAddress(), 2), subBuffer->getCpuAddress());
}

TEST_F(SubBufferTest, givenBufferWithNullMemoryStorageWhenSubBufferIsCreatedThenMemoryStorageIsNotOffset) {
    cl_buffer_region region = {1, 1};
    cl_int retVal = 0;

    MockBuffer *mockBuffer = static_cast<MockBuffer *>(buffer);
    void *savedMemoryStorage = mockBuffer->memoryStorage;
    mockBuffer->memoryStorage = nullptr;

    auto subBuffer = std::unique_ptr<Buffer>(buffer->createSubBuffer(0, 0, &region, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, subBuffer);

    MockBuffer *mockSubBuffer = static_cast<MockBuffer *>(subBuffer.get());
    EXPECT_EQ(nullptr, mockSubBuffer->memoryStorage);

    mockBuffer->memoryStorage = savedMemoryStorage;
}

TEST_F(SubBufferTest, givenBufferWithHostPtrWhenSubbufferGetsMapPtrThenExpectBufferHostPtr) {
    cl_buffer_region region = {0, 16};

    auto subBuffer = clUniquePtr(buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region, retVal));
    ASSERT_NE(nullptr, subBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    void *mapPtr = subBuffer->getBasePtrForMap(0);
    EXPECT_EQ(pHostPtr, mapPtr);
    mapPtr = subBuffer->getBasePtrForMap(0);
    EXPECT_EQ(pHostPtr, mapPtr);
}

TEST_F(SubBufferTest, givenBufferWithNoHostPtrWhenSubbufferGetsMapPtrThenExpectBufferMap) {
    cl_buffer_region region = {0, 16};

    auto buffer = clUniquePtr(Buffer::create(&context, CL_MEM_READ_WRITE,
                                             MemoryConstants::pageSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto subBuffer = clUniquePtr(buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region, retVal));
    ASSERT_NE(nullptr, subBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    void *mapPtr = subBuffer->getBasePtrForMap(0);
    void *bufferMapPtr = buffer->getBasePtrForMap(0);
    EXPECT_EQ(bufferMapPtr, mapPtr);
    auto mapAllocation = subBuffer->getMapAllocation(0);
    auto bufferMapAllocation = buffer->getMapAllocation(0);
    ASSERT_NE(nullptr, bufferMapAllocation);
    EXPECT_EQ(bufferMapAllocation, mapAllocation);
    EXPECT_EQ(bufferMapPtr, mapAllocation->getUnderlyingBuffer());

    mapPtr = subBuffer->getBasePtrForMap(0);
    EXPECT_EQ(bufferMapPtr, mapPtr);
}

TEST_F(SubBufferTest, givenSubBuffersWithMultipleDevicesWhenReleaseAllSubBuffersThenMainBufferProperlyDereferenced) {
    MockDefaultContext ctx;
    auto buffer = clUniquePtr(Buffer::create(&ctx, CL_MEM_READ_WRITE,
                                             MemoryConstants::pageSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    std::vector<ReleaseableObjectPtr<Buffer>> subBuffers;
    for (int i = 0; i < 8; i++) {
        cl_buffer_region region = {static_cast<size_t>(i * 4), 4};
        subBuffers.push_back(clUniquePtr(buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region, retVal)));
        EXPECT_EQ(3u, subBuffers[i]->getMultiGraphicsAllocation().getGraphicsAllocations().size());
    }

    EXPECT_EQ(9, buffer->getRefInternalCount());

    subBuffers.clear();

    EXPECT_EQ(1, buffer->getRefInternalCount());
}

} // namespace ULT
