/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;
namespace Ult {
using PoolAllocator = Context::BufferPoolAllocator;
using MockBufferPoolAllocator = MockContext::MockBufferPoolAllocator;

template <int32_t poolBufferFlag = -1, bool failMainStorageAllocation = false>
class AggregatedSmallBuffersTestTemplate : public ::testing::Test {
    void SetUp() override {
        this->setUpImpl();
    }

    void setUpImpl() {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(poolBufferFlag);
        this->deviceFactory = std::make_unique<UltClDeviceFactory>(1, 0);
        this->device = deviceFactory->rootDevices[0];
        this->mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
        this->mockMemoryManager->localMemorySupported[mockRootDeviceIndex] = true;
        this->setAllocationToFail(failMainStorageAllocation);
        cl_device_id devices[] = {device};
        this->context.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
        ASSERT_EQ(retVal, CL_SUCCESS);
        this->setAllocationToFail(false);
        this->poolAllocator = static_cast<MockBufferPoolAllocator *>(&context->smallBufferPoolAllocator);
    }

    void TearDown() override {
        if (this->context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled()) {
            this->context->getBufferPoolAllocator().releaseSmallBufferPool();
        }
    }

    void setAllocationToFail(bool shouldFail) {
        this->mockMemoryManager->failInDevicePoolWithError = shouldFail;
    }

  public:
    std::unique_ptr<UltClDeviceFactory> deviceFactory;
    MockClDevice *device;
    std::unique_ptr<MockContext> context;
    MockBufferPoolAllocator *poolAllocator;
    MockMemoryManager *mockMemoryManager;

    cl_mem_flags flags{};
    size_t size = PoolAllocator::smallBufferThreshold;
    void *hostPtr = nullptr;
    cl_int retVal = CL_SUCCESS;

    DebugManagerStateRestore restore;
};

using aggregatedSmallBuffersDefaultTest = AggregatedSmallBuffersTestTemplate<-1>;

TEST_F(aggregatedSmallBuffersDefaultTest, givenAggregatedSmallBuffersDefaultWhenCheckIfEnabledThenReturnFalse) {
    EXPECT_FALSE(poolAllocator->isAggregatedSmallBuffersEnabled());
}

using aggregatedSmallBuffersDisabledTest = AggregatedSmallBuffersTestTemplate<0>;

TEST_F(aggregatedSmallBuffersDisabledTest, givenAggregatedSmallBuffersDisabledWhenBufferCreateCalledThenDoNotUsePool) {
    ASSERT_FALSE(poolAllocator->isAggregatedSmallBuffersEnabled());
    ASSERT_EQ(poolAllocator->mainStorage, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(poolAllocator->mainStorage, nullptr);
}

using aggregatedSmallBuffersEnabledTest = AggregatedSmallBuffersTestTemplate<1>;

TEST_F(aggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeLargerThanThresholdWhenBufferCreateCalledThenDoNotUsePool) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled());
    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    size = PoolAllocator::smallBufferThreshold + 1;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_NE(poolAllocator->mainStorage, nullptr);
}

TEST_F(aggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledThenUsePool) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled());
    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));

    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_NE(poolAllocator->mainStorage, nullptr);
    auto mockBuffer = static_cast<MockBuffer *>(buffer.get());
    EXPECT_GE(mockBuffer->getSize(), size);
    EXPECT_GE(mockBuffer->getOffset(), 0u);
    EXPECT_LE(mockBuffer->getOffset(), PoolAllocator::aggregatedSmallBuffersPoolSize - size);
    EXPECT_TRUE(mockBuffer->isSubBuffer());
    EXPECT_EQ(poolAllocator->mainStorage, mockBuffer->associatedMemObject);

    retVal = clReleaseMemObject(buffer.release());
    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(aggregatedSmallBuffersEnabledTest, givenCopyHostPointerWhenCreatingBufferButCopyFailedThenDoNotUsePool) {
    class MockCommandQueueFailFirstEnqueueWrite : public MockCommandQueue {
      public:
        cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t size, const void *ptr,
                                  GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                  cl_event *event) override {
            if (writeBufferCounter == 0) {
                ++writeBufferCounter;
                return CL_OUT_OF_RESOURCES;
            }
            return MockCommandQueue::enqueueWriteBuffer(buffer, blockingWrite, offset, size, ptr, mapAllocation, numEventsInWaitList, eventWaitList, event);
        }
    };
    DebugManager.flags.CopyHostPtrOnCpu.set(0);

    auto commandQueue = new MockCommandQueueFailFirstEnqueueWrite();
    context->getSpecialQueue(mockRootDeviceIndex)->decRefInternal();
    context->setSpecialQueue(commandQueue, mockRootDeviceIndex);

    flags = CL_MEM_COPY_HOST_PTR;
    unsigned char dataToCopy[PoolAllocator::smallBufferThreshold];
    hostPtr = dataToCopy;

    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled());
    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    if (commandQueue->writeBufferCounter == 0) {
        GTEST_SKIP();
    }
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);

    auto mockBuffer = static_cast<MockBuffer *>(buffer.get());
    EXPECT_FALSE(mockBuffer->isSubBuffer());
    retVal = clReleaseMemObject(buffer.release());
    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(aggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledMultipleTimesThenUsePool) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled());
    ASSERT_NE(poolAllocator->mainStorage, nullptr);

    constexpr auto buffersToCreate = PoolAllocator::aggregatedSmallBuffersPoolSize / PoolAllocator::smallBufferThreshold;
    std::vector<std::unique_ptr<Buffer>> buffers(buffersToCreate);
    for (auto i = 0u; i < buffersToCreate; i++) {
        buffers[i].reset(Buffer::create(context.get(), flags, size, hostPtr, retVal));
        EXPECT_EQ(retVal, CL_SUCCESS);
    }
    EXPECT_NE(poolAllocator->mainStorage, nullptr);
    std::unique_ptr<Buffer> bufferAfterPoolIsFull(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(bufferAfterPoolIsFull, nullptr);
    EXPECT_FALSE(bufferAfterPoolIsFull->isSubBuffer());

    using Bounds = struct {
        size_t left;
        size_t right;
    };

    std::vector<Bounds> subBuffersBounds(buffersToCreate);

    for (auto i = 0u; i < buffersToCreate; i++) {
        // subbuffers are within pool buffer
        EXPECT_NE(buffers[i], nullptr);
        EXPECT_TRUE(buffers[i]->isSubBuffer());
        auto mockBuffer = static_cast<MockBuffer *>(buffers[i].get());
        EXPECT_EQ(poolAllocator->mainStorage, mockBuffer->associatedMemObject);
        EXPECT_GE(mockBuffer->getSize(), size);
        EXPECT_GE(mockBuffer->getOffset(), 0u);
        EXPECT_LE(mockBuffer->getOffset(), PoolAllocator::aggregatedSmallBuffersPoolSize - size);

        subBuffersBounds[i] = Bounds{mockBuffer->getOffset(), mockBuffer->getOffset() + mockBuffer->getSize()};
    }

    for (auto i = 0u; i < buffersToCreate; i++) {
        for (auto j = i + 1; j < buffersToCreate; j++) {
            // subbuffers do not overlap
            EXPECT_TRUE(subBuffersBounds[i].right <= subBuffersBounds[j].left ||
                        subBuffersBounds[j].right <= subBuffersBounds[i].left);
        }
    }

    // freeing subbuffer frees space in pool
    ASSERT_LT(poolAllocator->chunkAllocator->getLeftSize(), size);
    clReleaseMemObject(buffers[0].release());
    EXPECT_GE(poolAllocator->chunkAllocator->getLeftSize(), size);
    std::unique_ptr<Buffer> bufferAfterPoolHasSpaceAgain(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(bufferAfterPoolHasSpaceAgain, nullptr);
    EXPECT_TRUE(bufferAfterPoolHasSpaceAgain->isSubBuffer());

    // subbuffer after free does not overlap
    subBuffersBounds[0] = Bounds{bufferAfterPoolHasSpaceAgain->getOffset(), bufferAfterPoolHasSpaceAgain->getOffset() + bufferAfterPoolHasSpaceAgain->getSize()};
    for (auto i = 0u; i < buffersToCreate; i++) {
        for (auto j = i + 1; j < buffersToCreate; j++) {
            EXPECT_TRUE(subBuffersBounds[i].right <= subBuffersBounds[j].left ||
                        subBuffersBounds[j].right <= subBuffersBounds[i].left);
        }
    }
}

using aggregatedSmallBuffersEnabledTestDoNotRunSetup = AggregatedSmallBuffersTestTemplate<1, true>;

TEST_F(aggregatedSmallBuffersEnabledTestDoNotRunSetup, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledButPoolCreateFailedThenDoNotUsePool) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled());
    ASSERT_EQ(poolAllocator->mainStorage, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(buffer.get(), nullptr);
    EXPECT_EQ(poolAllocator->mainStorage, nullptr);
}

template <int32_t poolBufferFlag = -1>
class AggregatedSmallBuffersApiTestTemplate : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(poolBufferFlag);
        this->deviceFactory = std::make_unique<UltClDeviceFactory>(1, 0);
        auto device = deviceFactory->rootDevices[0];
        cl_device_id devices[] = {device};
        clContext = clCreateContext(nullptr, 1, devices, nullptr, nullptr, &retVal);
        ASSERT_EQ(retVal, CL_SUCCESS);
        context = castToObject<Context>(clContext);
    }

  public:
    std::unique_ptr<UltClDeviceFactory> deviceFactory;

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    size_t size = PoolAllocator::smallBufferThreshold;
    cl_int retVal = CL_SUCCESS;
    void *hostPtr{nullptr};
    cl_context clContext{nullptr};
    Context *context{nullptr};

    DebugManagerStateRestore restore;
};

using aggregatedSmallBuffersDefaultApiTest = AggregatedSmallBuffersApiTestTemplate<-1>;
TEST_F(aggregatedSmallBuffersDefaultApiTest, givenNoBufferCreatedWhenReleasingContextThenDoNotLeakMemory) {
    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

using aggregatedSmallBuffersEnabledApiTest = AggregatedSmallBuffersApiTestTemplate<1>;
TEST_F(aggregatedSmallBuffersEnabledApiTest, givenNoBufferCreatedWhenReleasingContextThenDoNotLeakMemory) {
    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(aggregatedSmallBuffersEnabledApiTest, givenNotSmallBufferWhenCreatingBufferThenDoNotUsePool) {
    size = PoolAllocator::smallBufferThreshold + 1;
    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);

    MockBuffer *asBuffer = static_cast<MockBuffer *>(buffer);
    EXPECT_FALSE(asBuffer->isSubBuffer());

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(aggregatedSmallBuffersEnabledApiTest, givenSmallBufferWhenCreatingBufferThenUsePool) {
    auto contextRefCountBefore = context->getRefInternalCount();
    cl_mem smallBuffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(smallBuffer, nullptr);

    MockBuffer *asBuffer = static_cast<MockBuffer *>(smallBuffer);
    EXPECT_TRUE(asBuffer->isSubBuffer());
    Buffer *parentBuffer = static_cast<Buffer *>(asBuffer->associatedMemObject);
    EXPECT_EQ(2, parentBuffer->getRefInternalCount());
    MockBufferPoolAllocator *mockBufferPoolAllocator = static_cast<MockBufferPoolAllocator *>(&context->getBufferPoolAllocator());
    EXPECT_EQ(parentBuffer, mockBufferPoolAllocator->mainStorage);

    retVal = clReleaseMemObject(smallBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(context->getRefInternalCount(), contextRefCountBefore);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(aggregatedSmallBuffersEnabledApiTest, givenSubBufferNotFromPoolAndAggregatedSmallBuffersEnabledWhenReleaseMemObjectCalledThenItSucceeds) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
    size_t size = PoolAllocator::smallBufferThreshold + 1;

    cl_mem largeBuffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(largeBuffer, nullptr);

    cl_buffer_region region{};
    region.size = 1;
    cl_mem subBuffer = clCreateSubBuffer(largeBuffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(subBuffer, nullptr);

    DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(largeBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(aggregatedSmallBuffersEnabledApiTest, givenCopyHostPointerWhenCreatingBufferThenUsePoolAndCopyHostPointer) {
    flags |= CL_MEM_COPY_HOST_PTR;
    unsigned char dataToCopy[PoolAllocator::smallBufferThreshold];
    dataToCopy[0] = 123;
    hostPtr = dataToCopy;
    auto contextRefCountBefore = context->getRefInternalCount();
    cl_mem smallBuffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(context->getRefInternalCount(), contextRefCountBefore + 1);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(smallBuffer, nullptr);

    MockBuffer *asBuffer = static_cast<MockBuffer *>(smallBuffer);
    EXPECT_TRUE(asBuffer->isSubBuffer());
    Buffer *parentBuffer = static_cast<Buffer *>(asBuffer->associatedMemObject);
    EXPECT_EQ(2, parentBuffer->getRefInternalCount());
    MockBufferPoolAllocator *mockBufferPoolAllocator = static_cast<MockBufferPoolAllocator *>(&context->getBufferPoolAllocator());
    EXPECT_EQ(parentBuffer, mockBufferPoolAllocator->mainStorage);

    // check that data has been copied
    auto address = asBuffer->getCpuAddress();
    EXPECT_EQ(0, memcmp(hostPtr, address, size));

    retVal = clReleaseMemObject(smallBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(context->getRefInternalCount(), contextRefCountBefore);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

using aggregatedSmallBuffersSubBufferApiTest = aggregatedSmallBuffersEnabledApiTest;

TEST_F(aggregatedSmallBuffersSubBufferApiTest, givenBufferFromPoolWhenCreateSubBufferCalledThenItSucceeds) {
    cl_mem notUsedBuffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(notUsedBuffer, nullptr);

    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);
    MockBuffer *mockBuffer = static_cast<MockBuffer *>(buffer);
    EXPECT_GT(mockBuffer->offset, 0u);

    cl_buffer_region region{};
    region.size = 1;
    region.origin = size / 2;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(subBuffer, nullptr);
    MockBuffer *mockSubBuffer = static_cast<MockBuffer *>(subBuffer);
    EXPECT_EQ(mockSubBuffer->offset, mockBuffer->offset + region.origin);
    MockBufferPoolAllocator *mockBufferPoolAllocator = static_cast<MockBufferPoolAllocator *>(&context->getBufferPoolAllocator());
    EXPECT_EQ(mockSubBuffer->associatedMemObject, mockBufferPoolAllocator->mainStorage);

    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(notUsedBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(aggregatedSmallBuffersSubBufferApiTest, givenBufferFromPoolWhenCreateSubBufferCalledWithRegionOutsideBufferThenItFails) {
    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);

    cl_buffer_region region{};
    region.size = size + 1;
    region.origin = 0;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
    EXPECT_EQ(subBuffer, nullptr);

    region.size = 1;
    region.origin = PoolAllocator::smallBufferThreshold;
    subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
    EXPECT_EQ(subBuffer, nullptr);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(aggregatedSmallBuffersSubBufferApiTest, givenSubBufferFromBufferFromPoolWhenCreateSubBufferCalledThenItFails) {
    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);

    cl_buffer_region region{};
    region.size = 1;
    region.origin = size / 2;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(subBuffer, nullptr);

    region.origin = 0;
    cl_mem subSubBuffer = clCreateSubBuffer(subBuffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_INVALID_MEM_OBJECT);
    EXPECT_EQ(subSubBuffer, nullptr);

    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}
} // namespace Ult