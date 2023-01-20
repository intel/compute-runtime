/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
using namespace NEO;
namespace Ult {
using PoolAllocator = Context::BufferPoolAllocator;
using MockBufferPoolAllocator = MockContext::MockBufferPoolAllocator;

template <int32_t poolBufferFlag = -1, bool failMainStorageAllocation = false, bool runSetup = true>
class AggregatedSmallBuffersTestTemplate : public ::testing::Test {
  public:
    void SetUp() override {
        if constexpr (runSetup) {
            this->setUpImpl();
        }
    }

    void TearDown() override {
        if (this->context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get())) {
            this->context->getBufferPoolAllocator().releaseSmallBufferPool();
        }
    }

    void setAllocationToFail(bool shouldFail) {
        this->mockMemoryManager->failInDevicePoolWithError = shouldFail;
    }

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
};

class AggregatedSmallBuffersKernelTest : public AggregatedSmallBuffersTestTemplate<1, false, true> {
  public:
    void SetUp() override {
        AggregatedSmallBuffersTestTemplate::SetUp();

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        constexpr uint32_t sizeOfPointer = sizeof(void *);
        pKernelInfo->addArgBuffer(0, 0x10, sizeOfPointer);
        pProgram.reset(new MockProgram(context.get(), false, toClDeviceVector(*device)));

        retVal = CL_INVALID_VALUE;
        pMultiDeviceKernel.reset(MultiDeviceKernel::create<MockKernel>(pProgram.get(), MockKernel::toKernelInfoContainer(*pKernelInfo, device->getRootDeviceIndex()), &retVal));
        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(device->getRootDeviceIndex()));
        ASSERT_NE(pKernel, nullptr);
        ASSERT_EQ(retVal, CL_SUCCESS);

        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
        pKernelArg = (void **)(pKernel->getCrossThreadData() + pKernelInfo->argAsPtr(0).stateless);
    };

    std::unique_ptr<MockKernelInfo> pKernelInfo;
    std::unique_ptr<MockProgram> pProgram;
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel;
    MockKernel *pKernel = nullptr;
    char pCrossThreadData[64];
    void **pKernelArg = nullptr;
};

using AggregatedSmallBuffersDefaultTest = AggregatedSmallBuffersTestTemplate<-1>;

HWTEST_F(AggregatedSmallBuffersDefaultTest, givenDifferentFlagValuesAndSingleOrMultiDeviceContextWhenCheckIfEnabledThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    // Single device context
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(-1);
        EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
        EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
        EXPECT_TRUE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(2);
        EXPECT_TRUE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    // Multi device context
    context->devices.push_back(nullptr);
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(-1);
        EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
        EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
        EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(2);
        EXPECT_TRUE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    context->devices.pop_back();
}

using AggregatedSmallBuffersDisabledTest = AggregatedSmallBuffersTestTemplate<0>;

TEST_F(AggregatedSmallBuffersDisabledTest, givenAggregatedSmallBuffersDisabledWhenBufferCreateCalledThenDoNotUsePool) {
    ASSERT_FALSE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    ASSERT_EQ(poolAllocator->mainStorage, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(poolAllocator->mainStorage, nullptr);
}

using AggregatedSmallBuffersEnabledTest = AggregatedSmallBuffersTestTemplate<1>;

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledWhenAllocatingMainStorageThenMakeDeviceBufferLockable) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    ASSERT_NE(mockMemoryManager->lastAllocationProperties, nullptr);
    EXPECT_TRUE(mockMemoryManager->lastAllocationProperties->makeDeviceBufferLockable);
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeLargerThanThresholdWhenBufferCreateCalledThenDoNotUsePool) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    size = PoolAllocator::smallBufferThreshold + 1;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeLowerThenChunkAlignmentWhenBufferCreatedAndDestroyedThenSizeIsAsRequestedAndCorrectSizeIsFreed) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    ASSERT_EQ(poolAllocator->chunkAllocator->getUsedSize(), 0u);
    size = PoolAllocator::chunkAlignment / 2;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(buffer->getSize(), size);
    EXPECT_EQ(poolAllocator->chunkAllocator->getUsedSize(), PoolAllocator::chunkAlignment);
    auto mockBuffer = static_cast<MockBuffer *>(buffer.get());
    EXPECT_EQ(mockBuffer->sizeInPoolAllocator, PoolAllocator::chunkAlignment);

    buffer.reset(nullptr);
    EXPECT_EQ(poolAllocator->chunkAllocator->getUsedSize(), 0u);
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledThenUsePool) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
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

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledWhenClReleaseMemObjectCalledThenWaitForEnginesCompletionCalled) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));

    ASSERT_NE(buffer, nullptr);
    ASSERT_EQ(retVal, CL_SUCCESS);

    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    auto mockBuffer = static_cast<MockBuffer *>(buffer.get());
    ASSERT_TRUE(mockBuffer->isSubBuffer());
    ASSERT_EQ(poolAllocator->mainStorage, mockBuffer->associatedMemObject);

    ASSERT_EQ(mockMemoryManager->waitForEnginesCompletionCalled, 0u);
    retVal = clReleaseMemObject(buffer.release());
    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(mockMemoryManager->waitForEnginesCompletionCalled, 1u);
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenCopyHostPointerWhenCreatingBufferButCopyFailedThenDoNotUsePool) {
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

    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
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

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledMultipleTimesThenUsePool) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
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

TEST_F(AggregatedSmallBuffersKernelTest, givenBufferFromPoolWhenOffsetSubbufferIsPassedToSetKernelArgThenCorrectGpuVAIsPatched) {
    std::unique_ptr<Buffer> unusedBuffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(buffer->getOffset(), 0u);
    cl_buffer_region region;
    region.origin = 0xc0;
    region.size = 32;
    cl_int error = 0;
    std::unique_ptr<Buffer> subBuffer(buffer->createSubBuffer(buffer->getFlags(), buffer->getFlagsIntel(), &region, error));
    ASSERT_NE(subBuffer, nullptr);
    EXPECT_EQ(ptrOffset(buffer->getCpuAddress(), region.origin), subBuffer->getCpuAddress());

    const auto graphicsAllocation = subBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(graphicsAllocation, nullptr);
    const auto gpuAddress = graphicsAllocation->getGpuAddress();
    EXPECT_EQ(ptrOffset(gpuAddress, buffer->getOffset() + region.origin), subBuffer->getBufferAddress(device->getRootDeviceIndex()));

    subBuffer->setArgStateless(pKernelArg, pKernelInfo->argAsPtr(0).pointerSize, device->getRootDeviceIndex(), false);
    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress + region.origin + buffer->getOffset()), *pKernelArg);
}

using AggregatedSmallBuffersEnabledTestFailPoolInit = AggregatedSmallBuffersTestTemplate<1, true>;

TEST_F(AggregatedSmallBuffersEnabledTestFailPoolInit, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledButPoolCreateFailedThenDoNotUsePool) {
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    ASSERT_EQ(poolAllocator->mainStorage, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(buffer.get(), nullptr);
    EXPECT_EQ(poolAllocator->mainStorage, nullptr);
}

using AggregatedSmallBuffersEnabledTestDoNotRunSetup = AggregatedSmallBuffersTestTemplate<1, false, false>;

TEST_F(AggregatedSmallBuffersEnabledTestDoNotRunSetup, givenAggregatedSmallBuffersEnabledWhenPoolInitializedThenPerformanceHintsNotProvided) {
    testing::internal::CaptureStdout();
    DebugManager.flags.PrintDriverDiagnostics.set(1);
    setUpImpl();
    ASSERT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    ASSERT_NE(poolAllocator->mainStorage, nullptr);
    ASSERT_NE(context->driverDiagnostics, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(0u, output.size());
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
        poolAllocator = static_cast<MockBufferPoolAllocator *>(&context->getBufferPoolAllocator());
    }

  public:
    std::unique_ptr<UltClDeviceFactory> deviceFactory;

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    size_t size = PoolAllocator::smallBufferThreshold;
    cl_int retVal = CL_SUCCESS;
    void *hostPtr{nullptr};
    cl_context clContext{nullptr};
    Context *context{nullptr};
    MockBufferPoolAllocator *poolAllocator{nullptr};

    DebugManagerStateRestore restore;
};

using AggregatedSmallBuffersDisabledApiTest = AggregatedSmallBuffersApiTestTemplate<0>;
TEST_F(AggregatedSmallBuffersDisabledApiTest, givenNoBufferCreatedWhenReleasingContextThenDoNotLeakMemory) {
    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

using AggregatedSmallBuffersEnabledApiTest = AggregatedSmallBuffersApiTestTemplate<1>;
TEST_F(AggregatedSmallBuffersEnabledApiTest, givenNoBufferCreatedWhenReleasingContextThenDoNotLeakMemory) {
    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenNotSmallBufferWhenCreatingBufferThenDoNotUsePool) {
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

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenSmallBufferWhenCreatingBufferThenUsePool) {
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

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenSmallBufferWhenCreatingBufferWithNullPropertiesThenUsePool) {
    auto contextRefCountBefore = context->getRefInternalCount();
    cl_mem smallBuffer = clCreateBufferWithProperties(clContext, nullptr, flags, size, hostPtr, &retVal);
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

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenSmallBufferWhenCreatingBufferWithEmptyPropertiesThenUsePool) {
    auto contextRefCountBefore = context->getRefInternalCount();
    cl_mem_properties memProperties{};
    cl_mem smallBuffer = clCreateBufferWithProperties(clContext, &memProperties, flags, size, hostPtr, &retVal);
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

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenBufferFromPoolWhenGetMemObjInfoCalledThenReturnValuesLikeForNormalBuffer) {
    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);

    MockBuffer *asBuffer = static_cast<MockBuffer *>(buffer);
    EXPECT_TRUE(asBuffer->isSubBuffer());

    cl_mem associatedMemObj = nullptr;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(cl_mem), &associatedMemObj, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(associatedMemObj, nullptr);

    size_t offset = 1u;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_OFFSET, sizeof(size_t), &offset, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(offset, 0u);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenSubBufferNotFromPoolAndAggregatedSmallBuffersEnabledWhenReleaseMemObjectCalledThenItSucceeds) {
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

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenCopyHostPointerWhenCreatingBufferThenUsePoolAndCopyHostPointer) {
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

using AggregatedSmallBuffersSubBufferApiTest = AggregatedSmallBuffersEnabledApiTest;

TEST_F(AggregatedSmallBuffersSubBufferApiTest, givenBufferFromPoolWhenCreateSubBufferCalledThenItSucceeds) {
    cl_mem notUsedBuffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(notUsedBuffer, nullptr);

    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);
    MockBuffer *mockBuffer = static_cast<MockBuffer *>(buffer);
    EXPECT_GT(mockBuffer->offset, 0u);
    EXPECT_EQ(ptrOffset(poolAllocator->mainStorage->getCpuAddress(), mockBuffer->getOffset()), mockBuffer->getCpuAddress());

    cl_buffer_region region{};
    region.size = 1;
    region.origin = size / 2;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(subBuffer, nullptr);
    MockBuffer *mockSubBuffer = static_cast<MockBuffer *>(subBuffer);
    EXPECT_EQ(mockSubBuffer->associatedMemObject, buffer);
    EXPECT_EQ(ptrOffset(mockBuffer->getCpuAddress(), region.origin), mockSubBuffer->getCpuAddress());

    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(notUsedBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersSubBufferApiTest, givenSubBufferFromBufferPoolWhenGetMemObjInfoCalledThenReturnValuesLikeForNormalSubBuffer) {
    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(buffer, nullptr);
    MockBuffer *mockBuffer = static_cast<MockBuffer *>(buffer);
    ASSERT_TRUE(context->getBufferPoolAllocator().isPoolBuffer(mockBuffer->associatedMemObject));

    cl_buffer_region region{};
    region.size = 1;
    region.origin = size / 2;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(subBuffer, nullptr);

    cl_mem associatedMemObj = nullptr;
    retVal = clGetMemObjectInfo(subBuffer, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(cl_mem), &associatedMemObj, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(associatedMemObj, buffer);

    size_t offset = 0u;
    retVal = clGetMemObjectInfo(subBuffer, CL_MEM_OFFSET, sizeof(size_t), &offset, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(offset, region.origin);

    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersSubBufferApiTest, givenBufferFromPoolWhenCreateSubBufferCalledWithRegionOutsideBufferThenItFails) {
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

TEST_F(AggregatedSmallBuffersSubBufferApiTest, givenSubBufferFromBufferFromPoolWhenCreateSubBufferCalledThenItFails) {
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
