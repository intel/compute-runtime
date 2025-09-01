/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

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

    void setAllocationToFail(bool shouldFail) {
        this->mockMemoryManager->failInDevicePoolWithError = shouldFail;
    }

    std::unique_ptr<UltClDeviceFactoryWithPlatform> deviceFactory;
    MockClDevice *device;
    MockDevice *mockNeoDevice;
    std::unique_ptr<MockContext> context;
    MockBufferPoolAllocator *poolAllocator;
    MockMemoryManager *mockMemoryManager;

    cl_mem_flags flags{};
    size_t size{0u};
    void *hostPtr = nullptr;
    cl_int retVal = CL_SUCCESS;
    static const auto rootDeviceIndex = 1u;

    DebugManagerStateRestore restore;

    void setUpImpl() {
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(poolBufferFlag);
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        debugManager.flags.EnableHostUsmAllocationPool.set(0);
        debugManager.flags.RenderCompressedBuffersEnabled.set(1);
        this->deviceFactory = std::make_unique<UltClDeviceFactoryWithPlatform>(2, 0);
        this->device = deviceFactory->rootDevices[rootDeviceIndex];
        this->mockNeoDevice = static_cast<MockDevice *>(&this->device->getDevice());
        const auto bitfield = mockNeoDevice->getDeviceBitfield();
        const auto deviceMemory = mockNeoDevice->getGlobalMemorySize(static_cast<uint32_t>(bitfield.to_ulong()));
        const auto expectedMaxPoolCount = Context::BufferPoolAllocator::calculateMaxPoolCount(SmallBuffersParams::getPreferredBufferPoolParams(this->device->getProductHelper()), deviceMemory, 2);
        EXPECT_EQ(expectedMaxPoolCount, mockNeoDevice->maxBufferPoolCount);
        this->mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
        this->mockMemoryManager->localMemorySupported[rootDeviceIndex] = true;
        this->setAllocationToFail(failMainStorageAllocation);
        cl_device_id devices[] = {device};
        this->context.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
        this->context->initializeDeviceUsmAllocationPool();
        EXPECT_EQ(retVal, CL_SUCCESS);
        this->setAllocationToFail(false);
        this->poolAllocator = static_cast<MockBufferPoolAllocator *>(&context->getBufferPoolAllocator());
        this->mockNeoDevice->updateMaxPoolCount(1u);
        size = this->poolAllocator->params.smallBufferThreshold;
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
        pMultiDeviceKernel.reset(MultiDeviceKernel::create<MockKernel>(pProgram.get(), MockKernel::toKernelInfoContainer(*pKernelInfo, device->getRootDeviceIndex()), retVal));
        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(device->getRootDeviceIndex()));
        EXPECT_NE(pKernel, nullptr);
        EXPECT_EQ(retVal, CL_SUCCESS);

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
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
        EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
        EXPECT_TRUE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(2);
        EXPECT_TRUE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    // Multi device context
    context->devices.push_back(nullptr);
    {
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
        EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
        EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    {
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(2);
        EXPECT_TRUE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
    }
    context->devices.pop_back();
}

HWTEST2_F(AggregatedSmallBuffersDefaultTest, givenSupportsOclBufferPoolCapabilityWhenCheckIfEnabledThenReturnCorrectValue, MatchAny) {
    auto &rootDeviceEnvironment = *device->getExecutionEnvironment()->rootDeviceEnvironments[1];
    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(rootDeviceEnvironment);

    rootDeviceEnvironment.ailConfiguration.reset(new MockAILConfiguration());
    auto mockAIL = static_cast<MockAILConfiguration *>(rootDeviceEnvironment.ailConfiguration.get());

    raii.mockProductHelper->isBufferPoolAllocatorSupportedValue = true;
    mockAIL->isBufferPoolEnabledReturn = true;
    EXPECT_TRUE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));

    raii.mockProductHelper->isBufferPoolAllocatorSupportedValue = true;
    mockAIL->isBufferPoolEnabledReturn = false;
    EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));

    raii.mockProductHelper->isBufferPoolAllocatorSupportedValue = false;
    mockAIL->isBufferPoolEnabledReturn = true;
    EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));

    raii.mockProductHelper->isBufferPoolAllocatorSupportedValue = false;
    mockAIL->isBufferPoolEnabledReturn = false;
    EXPECT_FALSE(context->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(context.get()));
}

using AggregatedSmallBuffersDisabledTest = AggregatedSmallBuffersTestTemplate<0>;

TEST_F(AggregatedSmallBuffersDisabledTest, givenAggregatedSmallBuffersDisabledWhenBufferCreateCalledThenDoNotUsePool) {
    EXPECT_FALSE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(0u, poolAllocator->bufferPools.size());
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, poolAllocator->bufferPools.size());
}

using AggregatedSmallBuffersEnabledTest = AggregatedSmallBuffersTestTemplate<1>;

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledWhenCalculateMaxPoolCountCalledThenCorrectValueIsReturned) {
    if (device->getProductHelper().is2MBLocalMemAlignmentEnabled()) {
        EXPECT_EQ(10u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), 8 * MemoryConstants::gigaByte, 2));
        EXPECT_EQ(25u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), 8 * MemoryConstants::gigaByte, 5));
        EXPECT_EQ(1u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), 128 * MemoryConstants::megaByte, 2));
        EXPECT_EQ(1u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), 64 * MemoryConstants::megaByte, 2));
    } else {
        EXPECT_EQ(81u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), 8 * MemoryConstants::gigaByte, 2));
        EXPECT_EQ(204u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), 8 * MemoryConstants::gigaByte, 5));
        EXPECT_EQ(1u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), 128 * MemoryConstants::megaByte, 2));
        EXPECT_EQ(1u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), 64 * MemoryConstants::megaByte, 2));
    }
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledWhenAllocatingMainStorageThenMakeDeviceBufferLockableAndNotCompressed) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    EXPECT_NE(nullptr, mockMemoryManager->lastAllocationProperties);
    EXPECT_TRUE(mockMemoryManager->lastAllocationProperties->makeDeviceBufferLockable);
    EXPECT_FALSE(mockMemoryManager->lastAllocationProperties->flags.preferCompressed);
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeLargerThanThresholdWhenBufferCreateCalledThenDoNotUsePool) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    size = poolAllocator->params.smallBufferThreshold + 1;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndFlagCompressedPreferredWhenBufferCreateCalledThenDoNotUsePool) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    size = poolAllocator->params.smallBufferThreshold;
    flags |= CL_MEM_COMPRESSED_HINT_INTEL;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeLowerThenChunkAlignmentWhenBufferCreatedAndDestroyedThenSizeIsAsRequestedAndCorrectSizeIsNotFreed) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    EXPECT_EQ(0u, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    size = poolAllocator->params.chunkAlignment / 2;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(size, buffer->getSize());
    EXPECT_EQ(poolAllocator->params.chunkAlignment, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    auto mockBuffer = static_cast<MockBuffer *>(buffer.get());
    EXPECT_EQ(poolAllocator->params.chunkAlignment, mockBuffer->sizeInPoolAllocator);

    buffer.reset(nullptr);
    EXPECT_EQ(poolAllocator->params.chunkAlignment, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledThenUsePool) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));

    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    auto mockBuffer = static_cast<MockBuffer *>(buffer.get());
    EXPECT_GE(mockBuffer->getSize(), size);
    EXPECT_GE(mockBuffer->getOffset(), 0u);
    EXPECT_LE(mockBuffer->getOffset(), poolAllocator->params.aggregatedSmallBuffersPoolSize - size);
    EXPECT_TRUE(mockBuffer->isSubBuffer());
    EXPECT_EQ(mockBuffer->associatedMemObject, poolAllocator->bufferPools[0].mainStorage.get());

    retVal = clReleaseMemObject(buffer.release());
    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledWhenClReleaseMemObjectCalledThenWaitForEnginesCompletionNotCalledAndMemoryRegionIsNotFreed) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    EXPECT_EQ(0u, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));

    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    auto mockBuffer = static_cast<MockBuffer *>(buffer.get());
    EXPECT_TRUE(mockBuffer->isSubBuffer());
    EXPECT_EQ(mockBuffer->associatedMemObject, poolAllocator->bufferPools[0].mainStorage.get());

    EXPECT_EQ(mockMemoryManager->waitForEnginesCompletionCalled, 0u);
    retVal = clReleaseMemObject(buffer.release());
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(mockMemoryManager->waitForEnginesCompletionCalled, 0u);
    EXPECT_EQ(size, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndBufferPoolIsExhaustedAndAllocationsAreNotInUseAndBufferWasFreedThenPoolIsReused) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());

    auto buffersToCreate = poolAllocator->params.aggregatedSmallBuffersPoolSize / poolAllocator->params.smallBufferThreshold;
    std::vector<std::unique_ptr<Buffer>> buffers(buffersToCreate);
    for (auto i = 0u; i < buffersToCreate; i++) {
        buffers[i].reset(Buffer::create(context.get(), flags, size, hostPtr, retVal));
        EXPECT_EQ(retVal, CL_SUCCESS);
    }

    EXPECT_EQ(size * buffersToCreate, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    EXPECT_EQ(0u, mockMemoryManager->allocInUseCalled);
    mockMemoryManager->deferAllocInUse = false;

    buffers[0] = nullptr;

    std::unique_ptr<Buffer> bufferAfterFreeMustSucceed(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_EQ(1u, mockMemoryManager->allocInUseCalled);
    EXPECT_EQ(size * buffersToCreate, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndBufferPoolIsExhaustedAndAllocationsAreNotInUseAndNoBuffersFreedThenNewPoolIsCreated) {
    mockNeoDevice->updateMaxPoolCount(2u);
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());

    auto buffersToCreate = poolAllocator->params.aggregatedSmallBuffersPoolSize / poolAllocator->params.smallBufferThreshold;
    std::vector<std::unique_ptr<Buffer>> buffers(buffersToCreate);
    for (auto i = 0u; i < buffersToCreate; i++) {
        buffers[i].reset(Buffer::create(context.get(), flags, size, hostPtr, retVal));
        EXPECT_EQ(retVal, CL_SUCCESS);
    }

    EXPECT_EQ(size * buffersToCreate, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    EXPECT_EQ(0u, mockMemoryManager->allocInUseCalled);
    mockMemoryManager->deferAllocInUse = false;

    std::unique_ptr<Buffer> bufferAfterExhaustMustSucceed(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(2u, poolAllocator->bufferPools.size());
    EXPECT_EQ(1u, mockMemoryManager->allocInUseCalled);
    EXPECT_EQ(size * buffersToCreate, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    EXPECT_EQ(size, poolAllocator->bufferPools[1].chunkAllocator->getUsedSize());
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndBufferPoolIsExhaustedAndAllocationsAreInUseThenNewPoolIsCreated) {
    mockNeoDevice->updateMaxPoolCount(2u);
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());

    auto buffersToCreate = poolAllocator->params.aggregatedSmallBuffersPoolSize / poolAllocator->params.smallBufferThreshold;
    std::vector<std::unique_ptr<Buffer>> buffers(buffersToCreate);
    for (auto i = 0u; i < buffersToCreate; i++) {
        buffers[i].reset(Buffer::create(context.get(), flags, size, hostPtr, retVal));
        EXPECT_EQ(retVal, CL_SUCCESS);
    }

    EXPECT_EQ(size * buffersToCreate, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    EXPECT_EQ(0u, mockMemoryManager->allocInUseCalled);
    mockMemoryManager->deferAllocInUse = true;

    std::unique_ptr<Buffer> bufferAfterExhaustMustSucceed(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(2u, poolAllocator->bufferPools.size());
    EXPECT_EQ(1u, mockMemoryManager->allocInUseCalled);
    EXPECT_EQ(size * buffersToCreate, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    EXPECT_EQ(size, poolAllocator->bufferPools[1].chunkAllocator->getUsedSize());
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndBufferPoolIsExhaustedAndAllocationsAreInUseAndPoolLimitIsReachedThenNewPoolIsNotCreated) {
    mockNeoDevice->updateMaxPoolCount(2u);
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());

    const std::vector<std::unique_ptr<Buffer>>::size_type buffersToCreate = (poolAllocator->params.aggregatedSmallBuffersPoolSize / poolAllocator->params.smallBufferThreshold) * mockNeoDevice->maxBufferPoolCount;
    std::vector<std::unique_ptr<Buffer>> buffers(buffersToCreate);
    for (auto i = 0u; i < buffersToCreate; ++i) {
        buffers[i].reset(Buffer::create(context.get(), flags, size, hostPtr, retVal));
        EXPECT_EQ(retVal, CL_SUCCESS);
    }
    EXPECT_EQ(mockNeoDevice->maxBufferPoolCount, poolAllocator->bufferPools.size());
    for (auto i = 0u; i < mockNeoDevice->maxBufferPoolCount; ++i) {
        EXPECT_EQ(poolAllocator->params.aggregatedSmallBuffersPoolSize, poolAllocator->bufferPools[i].chunkAllocator->getUsedSize());
    }
    EXPECT_EQ(1u, mockMemoryManager->allocInUseCalled);
    mockMemoryManager->deferAllocInUse = true;
    mockMemoryManager->failInDevicePoolWithError = true;

    std::unique_ptr<Buffer> bufferAfterExhaustMustFail(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_EQ(nullptr, bufferAfterExhaustMustFail.get());
    EXPECT_NE(retVal, CL_SUCCESS);
    EXPECT_EQ(mockNeoDevice->maxBufferPoolCount, poolAllocator->bufferPools.size());
    EXPECT_EQ(3u, mockMemoryManager->allocInUseCalled);
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
    debugManager.flags.CopyHostPtrOnCpu.set(0);

    auto commandQueue = new MockCommandQueueFailFirstEnqueueWrite();
    context->getSpecialQueue(rootDeviceIndex)->decRefInternal();
    context->setSpecialQueue(commandQueue, rootDeviceIndex);

    flags = CL_MEM_COPY_HOST_PTR;
    auto dataToCopy = std::unique_ptr<unsigned char[]>(new unsigned char[poolAllocator->params.smallBufferThreshold]);
    hostPtr = dataToCopy.get();

    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    if (commandQueue->writeBufferCounter == 0) {
        GTEST_SKIP();
    }
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(buffer, nullptr);

    auto mockBuffer = static_cast<MockBuffer *>(buffer.get());
    EXPECT_FALSE(mockBuffer->isSubBuffer());
    retVal = clReleaseMemObject(buffer.release());
    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledMultipleTimesThenUsePool) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());

    auto buffersToCreate = poolAllocator->params.aggregatedSmallBuffersPoolSize / poolAllocator->params.smallBufferThreshold;
    std::vector<std::unique_ptr<Buffer>> buffers(buffersToCreate);
    for (auto i = 0u; i < buffersToCreate; i++) {
        buffers[i].reset(Buffer::create(context.get(), flags, size, hostPtr, retVal));
        EXPECT_EQ(retVal, CL_SUCCESS);
    }

    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());

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
        EXPECT_EQ(mockBuffer->associatedMemObject, poolAllocator->bufferPools[0].mainStorage.get());
        EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
        EXPECT_GE(mockBuffer->getSize(), size);
        EXPECT_GE(mockBuffer->getOffset(), 0u);
        EXPECT_LE(mockBuffer->getOffset(), poolAllocator->params.aggregatedSmallBuffersPoolSize - size);

        subBuffersBounds[i] = Bounds{mockBuffer->getOffset(), mockBuffer->getOffset() + mockBuffer->getSize()};
    }

    for (auto i = 0u; i < buffersToCreate; i++) {
        for (auto j = i + 1; j < buffersToCreate; j++) {
            // subbuffers do not overlap
            EXPECT_TRUE(subBuffersBounds[i].right <= subBuffersBounds[j].left ||
                        subBuffersBounds[j].right <= subBuffersBounds[i].left);
        }
    }
}

TEST_F(AggregatedSmallBuffersEnabledTest, givenAggregatedSmallBuffersEnabledAndMultipleContextsThenPoolLimitIsTrackedAcrossContexts) {
    mockNeoDevice->updateMaxPoolCount(2u);
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_NE(nullptr, poolAllocator->bufferPools[0].mainStorage.get());
    EXPECT_EQ(1u, mockNeoDevice->bufferPoolCount.load());
    std::unique_ptr<MockContext> secondContext;
    cl_device_id devices[] = {device};
    secondContext.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    this->setAllocationToFail(false);
    EXPECT_EQ(2u, mockNeoDevice->bufferPoolCount.load());

    auto buffersToCreate = poolAllocator->params.aggregatedSmallBuffersPoolSize / poolAllocator->params.smallBufferThreshold;
    std::vector<std::unique_ptr<Buffer>> buffers(buffersToCreate);
    for (auto i = 0u; i < buffersToCreate; i++) {
        buffers[i].reset(Buffer::create(context.get(), flags, size, hostPtr, retVal));
        EXPECT_EQ(retVal, CL_SUCCESS);
    }

    std::unique_ptr<Buffer> bufferAfterExhaustMustSucceed(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(1u, poolAllocator->bufferPools.size());
    EXPECT_EQ(size * buffersToCreate, poolAllocator->bufferPools[0].chunkAllocator->getUsedSize());
    EXPECT_FALSE(bufferAfterExhaustMustSucceed->isSubBuffer());

    mockNeoDevice->callBaseGetGlobalMemorySize = false;
    if (mockNeoDevice->getProductHelper().is2MBLocalMemAlignmentEnabled()) {
        mockNeoDevice->getGlobalMemorySizeReturn = static_cast<uint64_t>(16 * 2 * MemoryConstants::megaByte / 0.02);
    } else {
        mockNeoDevice->getGlobalMemorySizeReturn = static_cast<uint64_t>(2 * 2 * MemoryConstants::megaByte / 0.02);
    }
    const auto bitfield = mockNeoDevice->getDeviceBitfield();
    const auto deviceMemory = mockNeoDevice->getGlobalMemorySize(static_cast<uint32_t>(bitfield.to_ulong()));
    EXPECT_EQ(2u, MockBufferPoolAllocator::calculateMaxPoolCount(this->poolAllocator->getParams(), deviceMemory, 2));
    std::unique_ptr<MockContext> thirdContext;
    thirdContext.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    MockBufferPoolAllocator *thirdPoolAllocator = static_cast<MockBufferPoolAllocator *>(&thirdContext->getBufferPoolAllocator());
    EXPECT_EQ(0u, thirdPoolAllocator->bufferPools.size());
    EXPECT_EQ(2u, mockNeoDevice->bufferPoolCount.load());

    secondContext.reset(nullptr);
    EXPECT_EQ(1u, mockNeoDevice->bufferPoolCount.load());

    buffers.clear();
    bufferAfterExhaustMustSucceed.reset(nullptr);
    context.reset(nullptr);
    EXPECT_EQ(0u, mockNeoDevice->bufferPoolCount.load());
}

TEST_F(AggregatedSmallBuffersKernelTest, givenBufferFromPoolWhenOffsetSubbufferIsPassedToSetKernelArgThenCorrectGpuVAIsPatched) {
    std::unique_ptr<Buffer> unusedBuffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(buffer, nullptr);
    cl_buffer_region region;
    region.origin = 0xc0;
    region.size = 32;
    cl_int error = 0;
    std::unique_ptr<Buffer> subBuffer(buffer->createSubBuffer(buffer->getFlags(), buffer->getFlagsIntel(), &region, error));
    EXPECT_NE(subBuffer, nullptr);
    EXPECT_EQ(ptrOffset(buffer->getCpuAddress(), region.origin), subBuffer->getCpuAddress());

    const auto graphicsAllocation = subBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(graphicsAllocation, nullptr);
    const auto gpuAddress = graphicsAllocation->getGpuAddress();
    EXPECT_EQ(ptrOffset(gpuAddress, buffer->getOffset() + region.origin), subBuffer->getBufferAddress(device->getRootDeviceIndex()));

    subBuffer->setArgStateless(pKernelArg, pKernelInfo->argAsPtr(0).pointerSize, device->getRootDeviceIndex(), false);
    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress + region.origin + buffer->getOffset()), *pKernelArg);
}

using AggregatedSmallBuffersEnabledTestFailPoolInit = AggregatedSmallBuffersTestTemplate<1, true>;

TEST_F(AggregatedSmallBuffersEnabledTestFailPoolInit, givenAggregatedSmallBuffersEnabledAndSizeEqualToThresholdWhenBufferCreateCalledButPoolCreateFailedThenDoNotUsePool) {
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_TRUE(poolAllocator->bufferPools.empty());
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, hostPtr, retVal));

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(buffer.get(), nullptr);
    EXPECT_TRUE(poolAllocator->bufferPools.empty());
}

using AggregatedSmallBuffersEnabledTestDoNotRunSetup = AggregatedSmallBuffersTestTemplate<1, false, false>;

TEST_F(AggregatedSmallBuffersEnabledTestDoNotRunSetup, givenAggregatedSmallBuffersEnabledWhenPoolInitializedThenPerformanceHintsNotProvided) {
    StreamCapture capture;
    capture.captureStdout();
    debugManager.flags.PrintDriverDiagnostics.set(1);
    setUpImpl();
    EXPECT_TRUE(poolAllocator->isAggregatedSmallBuffersEnabled(context.get()));
    EXPECT_FALSE(poolAllocator->bufferPools.empty());
    EXPECT_NE(context->driverDiagnostics, nullptr);
    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());
}

TEST_F(AggregatedSmallBuffersEnabledTestDoNotRunSetup, givenProductWithAndWithout2MBLocalMemAlignmentWhenCreatingContextThenBufferPoolAllocatorHasCorrectParams) {
    auto compareSmallBuffersParams = [](const NEO::SmallBuffersParams &first, const NEO::SmallBuffersParams &second) {
        return first.aggregatedSmallBuffersPoolSize == second.aggregatedSmallBuffersPoolSize &&
               first.smallBufferThreshold == second.smallBufferThreshold &&
               first.chunkAlignment == second.chunkAlignment &&
               first.startingOffset == second.startingOffset;
    };

    debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
    debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
    debugManager.flags.EnableHostUsmAllocationPool.set(0);
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    this->deviceFactory = std::make_unique<UltClDeviceFactoryWithPlatform>(2, 0);
    this->device = deviceFactory->rootDevices[rootDeviceIndex];
    this->mockNeoDevice = static_cast<MockDevice *>(&this->device->getDevice());

    auto mockProductHelper = new MockProductHelper;
    mockNeoDevice->getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;

    auto &productHelper = mockNeoDevice->getRootDeviceEnvironment().getProductHelper();
    EXPECT_FALSE(productHelper.is2MBLocalMemAlignmentEnabled());

    cl_device_id devices[] = {device};
    this->context.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
    auto &bufferPoolAllocator = context->getBufferPoolAllocator();
    auto bufferPoolAllocatorParams = bufferPoolAllocator.getParams();

    auto preferredParams = NEO::SmallBuffersParams::getPreferredBufferPoolParams(productHelper);
    EXPECT_TRUE(compareSmallBuffersParams(bufferPoolAllocatorParams, preferredParams));

    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    EXPECT_TRUE(productHelper.is2MBLocalMemAlignmentEnabled());

    std::unique_ptr<MockContext> secondContext;
    secondContext.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));

    auto &bufferPoolAllocator2 = secondContext->getBufferPoolAllocator();
    auto bufferPoolAllocatorParams2 = bufferPoolAllocator2.getParams();

    preferredParams = NEO::SmallBuffersParams::getPreferredBufferPoolParams(productHelper);
    EXPECT_TRUE(compareSmallBuffersParams(bufferPoolAllocatorParams2, preferredParams));
}

template <int32_t poolBufferFlag = -1>
class AggregatedSmallBuffersApiTestTemplate : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(poolBufferFlag);
        this->deviceFactory = std::make_unique<UltClDeviceFactoryWithPlatform>(1, 0);
        auto device = deviceFactory->rootDevices[0];
        cl_device_id devices[] = {device};
        clContext = clCreateContext(nullptr, 1, devices, nullptr, nullptr, &retVal);
        EXPECT_EQ(retVal, CL_SUCCESS);
        context = castToObject<Context>(clContext);
        poolAllocator = static_cast<MockBufferPoolAllocator *>(&context->getBufferPoolAllocator());
        size = poolAllocator->params.smallBufferThreshold;
    }

  public:
    std::unique_ptr<UltClDeviceFactoryWithPlatform> deviceFactory;

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    size_t size{0u};
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
    size = poolAllocator->params.smallBufferThreshold + 1;
    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(buffer, nullptr);

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
    EXPECT_NE(smallBuffer, nullptr);

    MockBuffer *asBuffer = static_cast<MockBuffer *>(smallBuffer);
    EXPECT_TRUE(asBuffer->isSubBuffer());
    Buffer *parentBuffer = static_cast<Buffer *>(asBuffer->associatedMemObject);
    EXPECT_EQ(2, parentBuffer->getRefInternalCount());
    EXPECT_EQ(parentBuffer, poolAllocator->bufferPools[0].mainStorage.get());

    retVal = clReleaseMemObject(smallBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(context->getRefInternalCount(), contextRefCountBefore);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenUseHostPointerWhenCreatingBufferThenDoNotUsePool) {
    flags |= CL_MEM_USE_HOST_PTR;
    auto hostData = std::unique_ptr<unsigned char[]>(new unsigned char[poolAllocator->params.smallBufferThreshold]);
    hostPtr = hostData.get();
    cl_mem smallBuffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(smallBuffer, nullptr);

    MockBuffer *asBuffer = static_cast<MockBuffer *>(smallBuffer);
    EXPECT_FALSE(asBuffer->isSubBuffer());

    retVal = clReleaseMemObject(smallBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenSmallBufferWhenCreatingBufferWithEmptyPropertiesThenUsePool) {
    auto contextRefCountBefore = context->getRefInternalCount();
    cl_mem_properties memProperties{};
    cl_mem smallBuffer = clCreateBufferWithProperties(clContext, &memProperties, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(smallBuffer, nullptr);

    MockBuffer *asBuffer = static_cast<MockBuffer *>(smallBuffer);
    EXPECT_TRUE(asBuffer->isSubBuffer());
    Buffer *parentBuffer = static_cast<Buffer *>(asBuffer->associatedMemObject);
    EXPECT_EQ(2, parentBuffer->getRefInternalCount());
    EXPECT_EQ(parentBuffer, poolAllocator->bufferPools[0].mainStorage.get());

    retVal = clReleaseMemObject(smallBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(context->getRefInternalCount(), contextRefCountBefore);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenBufferFromPoolWhenGetMemObjInfoCalledThenReturnValuesLikeForNormalBuffer) {
    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(buffer, nullptr);

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
    debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
    size_t size = poolAllocator->params.smallBufferThreshold + 1;

    cl_mem largeBuffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(largeBuffer, nullptr);

    cl_buffer_region region{};
    region.size = 1;
    cl_mem subBuffer = clCreateSubBuffer(largeBuffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(subBuffer, nullptr);

    debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(largeBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(clReleaseContext(context), CL_SUCCESS);
}

TEST_F(AggregatedSmallBuffersEnabledApiTest, givenCopyHostPointerWhenCreatingBufferThenUsePoolAndCopyHostPointer) {
    flags |= CL_MEM_COPY_HOST_PTR;
    auto dataToCopy = std::unique_ptr<unsigned char[]>(new unsigned char[poolAllocator->params.smallBufferThreshold]);
    dataToCopy[0] = 123;
    hostPtr = dataToCopy.get();
    auto contextRefCountBefore = context->getRefInternalCount();
    cl_mem smallBuffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(context->getRefInternalCount(), contextRefCountBefore + 1);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(smallBuffer, nullptr);

    MockBuffer *asBuffer = static_cast<MockBuffer *>(smallBuffer);
    EXPECT_TRUE(asBuffer->isSubBuffer());
    Buffer *parentBuffer = static_cast<Buffer *>(asBuffer->associatedMemObject);
    EXPECT_EQ(2, parentBuffer->getRefInternalCount());
    EXPECT_EQ(parentBuffer, poolAllocator->bufferPools[0].mainStorage.get());

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
    EXPECT_NE(buffer, nullptr);
    MockBuffer *mockBuffer = static_cast<MockBuffer *>(buffer);
    EXPECT_EQ(ptrOffset(poolAllocator->bufferPools[0].mainStorage->getCpuAddress(), mockBuffer->getOffset()), mockBuffer->getCpuAddress());

    cl_buffer_region region{};
    region.size = 1;
    region.origin = size / 2;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(subBuffer, nullptr);
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
    cl_mem buffer{};
    cl_mem buffer1 = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer1);
    auto mockBuffer1 = static_cast<MockBuffer *>(buffer1);
    EXPECT_TRUE(context->getBufferPoolAllocator().isPoolBuffer(mockBuffer1->associatedMemObject));

    // need buffer to have non-zero offset, to verify offset calculations in clGemMemObjectInfo
    // so if we get first pool buffer with offset 0, use a second buffer
    if (mockBuffer1->getOffset() != 0u) {
        buffer = buffer1;
    } else {
        cl_mem buffer2 = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, buffer2);
        auto mockBuffer2 = static_cast<MockBuffer *>(buffer2);
        EXPECT_TRUE(context->getBufferPoolAllocator().isPoolBuffer(mockBuffer2->associatedMemObject));
        EXPECT_NE(0u, mockBuffer2->getOffset());
        buffer = buffer2;
        retVal = clReleaseMemObject(buffer1);
        EXPECT_EQ(retVal, CL_SUCCESS);
    }

    cl_buffer_region region{};
    region.size = 1;
    region.origin = size / 2;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, subBuffer);

    cl_mem associatedMemObj = nullptr;
    retVal = clGetMemObjectInfo(subBuffer, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(cl_mem), &associatedMemObj, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(associatedMemObj, buffer);

    auto mockSubBuffer = static_cast<MockBuffer *>(subBuffer);
    const auto offsetInternal = mockSubBuffer->getOffset();
    size_t offset = 0u;
    retVal = clGetMemObjectInfo(subBuffer, CL_MEM_OFFSET, sizeof(size_t), &offset, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(region.origin, offset);
    EXPECT_EQ(offsetInternal, mockSubBuffer->getOffset()); // internal offset should not be modified after call

    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(CL_SUCCESS, clReleaseContext(context));
}

TEST_F(AggregatedSmallBuffersSubBufferApiTest, givenBufferFromPoolWhenCreateSubBufferCalledWithRegionOutsideBufferThenItFails) {
    cl_mem buffer = clCreateBuffer(clContext, flags, size, hostPtr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(buffer, nullptr);

    cl_buffer_region region{};
    region.size = size + 1;
    region.origin = 0;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
    EXPECT_EQ(subBuffer, nullptr);

    region.size = 1;
    region.origin = poolAllocator->params.smallBufferThreshold;
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
    EXPECT_NE(buffer, nullptr);

    cl_buffer_region region{};
    region.size = 1;
    region.origin = size / 2;
    cl_mem subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(subBuffer, nullptr);

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