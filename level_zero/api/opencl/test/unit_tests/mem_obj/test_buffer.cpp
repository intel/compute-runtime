/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing.h"
#include "level_zero/api/opencl/test/common/fixtures/buffer_fixture.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <iterator>
#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

void CL_CALLBACK destructorCallback(cl_mem, void *) {}

TEST_F(BufferDestructorTest, givenBufferWithoutSvmAndWithoutExternalHandleWhenDestroyedThenZeMemFreeExtIsCalledWithDeferPolicy) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(false);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE, freeMem.freePolicy);
    EXPECT_EQ(usmPtr, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithCpuAllocationAndSvmWhenDestroyedThenMemObjCallsZeMemFreeExtWithDeferPolicy) {
    uint64_t dummyCpuStorage = 0u;
    auto buffer = createBuffer(&dummyStorage, false, &dummyCpuStorage);
    buffer->setUsesSvm(true);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE, freeMem.freePolicy);
    EXPECT_EQ(&dummyCpuStorage, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithUseHostPtrWhenDestroyedThenZeMemFreeExtIsCalledWithBlockingPolicy) {
    void *usmPtr = &dummyStorage;
    uint64_t dummyCpuStorage = 0u;
    auto buffer = createBuffer(CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, usmPtr, false, &dummyCpuStorage);
    buffer->setUsesSvm(false);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE, freeMem.freePolicy);
    EXPECT_EQ(usmPtr, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithDestructorCallbackWhenDestroyedThenZeMemFreeExtIsCalledWithBlockingPolicy) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(false);
    buffer->addCallback(&destructorCallback, nullptr);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE, freeMem.freePolicy);
    EXPECT_EQ(usmPtr, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenMemObjWithDestructorCallbackAndCpuAllocationWhenDestroyedThenZeMemFreeExtIsCalledWithBlockingPolicy) {
    uint64_t dummyCpuStorage = 0u;
    auto buffer = createBuffer(&dummyStorage, false, &dummyCpuStorage);
    buffer->setUsesSvm(true);
    buffer->addCallback(&destructorCallback, nullptr);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE, freeMem.freePolicy);
    EXPECT_EQ(&dummyCpuStorage, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithCpuPtrAliasingUsmPtrWhenDestroyedThenZeMemFreeExtIsCalledOnce) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr, false, usmPtr);
    buffer->setUsesSvm(false);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    EXPECT_EQ(usmPtr, capturingContext->freeMemExtArgs[0].ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithSvmWhenDestroyedThenZeMemFreeExtIsNotCalled) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(true);

    delete buffer;

    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

TEST_F(BufferDestructorTest, givenBufferWithExternalHandleWhenDestroyedThenZeMemFreeExtIsNotCalled) {
    void *usmPtr = &dummyStorage;
    constexpr bool externalHandle = true;
    auto buffer = createBuffer(usmPtr, externalHandle);

    delete buffer;

    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

TEST_F(ExternalMemoryBufferTest, givenExternalMemoryHandleWhenCreatingBufferThenHandleIsImportedOnDefaultDeviceAsOwnedBuffer) {
    capturingContext->allocDeviceMemPtr = &dummyStorage;

    for (const cl_mem_properties handle : {0x10, 0x0}) {
        cl_mem_properties properties[] = {supportedExternalMemoryHandleKey, handle, 0};
        cl_int errcode = CL_INVALID_VALUE;
        capturingContext->allocDeviceMemArgs.records.clear();
        capturingContext->freeMemExtArgs.records.clear();

        auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);

        ASSERT_EQ(CL_SUCCESS, errcode) << handle;
        ASSERT_EQ(1u, capturingContext->allocDeviceMemArgs.count());
        EXPECT_EQ(clDevice->getL0Handle(), capturingContext->allocDeviceMemArgs[0].hDevice);
        EXPECT_EQ(64u, capturingContext->allocDeviceMemArgs[0].size);
        auto pBuffer = castToObject<Buffer>(buffer);
        EXPECT_EQ(&dummyStorage, pBuffer->getUsmPtr());
        EXPECT_EQ(nullptr, pBuffer->peekSharingHandler());

        EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
        ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
        EXPECT_EQ(&dummyStorage, capturingContext->freeMemExtArgs[0].ptr);
    }
}

TEST_F(ExternalMemoryBufferTest, givenExternalMemoryBufferWhenQueryingPropertiesThenCreationPropertiesAreReturned) {
    capturingContext->allocDeviceMemPtr = &dummyStorage;
    cl_mem_properties properties[] = {supportedExternalMemoryHandleKey, 0x10, 0};
    cl_int errcode = CL_INVALID_VALUE;
    auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    cl_mem_properties queried[3] = {};
    size_t queriedSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetMemObjectInfo(buffer, CL_MEM_PROPERTIES, sizeof(queried), queried, &queriedSize));
    EXPECT_EQ(sizeof(properties), queriedSize);
    EXPECT_EQ(0, memcmp(properties, queried, sizeof(properties)));

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(ExternalMemoryBufferTest, givenInvalidArgumentsWithExternalMemoryHandleWhenCreatingBufferThenMatchingErrorIsReturnedAndNothingIsImported) {
    const auto aboveMaxAllocSize = static_cast<size_t>(clDevice->getSharedDeviceInfo().maxMemAllocSize) + 1u;
    uint32_t hostStorage = 0;
    struct InvalidCase {
        cl_mem_properties extraProperty;
        cl_mem_properties handleKey;
        cl_mem_flags flags;
        size_t size;
        void *hostPtr;
        cl_int expectedError;
    };
    const InvalidCase cases[] = {
        {CL_MEM_L0_HANDLE_INTEL, supportedExternalMemoryHandleKey, 0, 64u, nullptr, CL_INVALID_PROPERTY},
        {supportedExternalMemoryHandleKey, supportedExternalMemoryHandleKey, 0, 64u, nullptr, CL_INVALID_PROPERTY},
        {CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0, 64u, nullptr, CL_INVALID_PROPERTY},
        {0, supportedExternalMemoryHandleKey, CL_MEM_USE_HOST_PTR, sizeof(hostStorage), &hostStorage, CL_INVALID_VALUE},
        {0, supportedExternalMemoryHandleKey, CL_MEM_ALLOC_HOST_PTR, sizeof(hostStorage), nullptr, CL_INVALID_VALUE},
        {0, supportedExternalMemoryHandleKey, CL_MEM_COPY_HOST_PTR, sizeof(hostStorage), &hostStorage, CL_INVALID_VALUE},
        {0, supportedExternalMemoryHandleKey, 0, sizeof(hostStorage), &hostStorage, CL_INVALID_HOST_PTR},
        {0, supportedExternalMemoryHandleKey, 0, 0u, nullptr, CL_INVALID_BUFFER_SIZE},
        {0, supportedExternalMemoryHandleKey, 0, aboveMaxAllocSize, nullptr, CL_INVALID_BUFFER_SIZE},
    };

    for (size_t i = 0; i < std::size(cases); i++) {
        const auto &testCase = cases[i];
        cl_mem_properties properties[] = {testCase.handleKey, 0x10, testCase.extraProperty, reinterpret_cast<cl_mem_properties>(&dummyStorage), 0};
        if (testCase.extraProperty == 0) {
            properties[2] = 0;
        }
        cl_int errcode = CL_SUCCESS;

        auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE | testCase.flags, testCase.size, testCase.hostPtr, &errcode);

        EXPECT_EQ(nullptr, buffer) << "case " << i;
        EXPECT_EQ(testCase.expectedError, errcode) << "case " << i;
    }
    EXPECT_FALSE(capturingContext->allocDeviceMemArgs.wasCalled());
}

TEST_F(ExternalMemoryBufferTest, givenSecondExternalMemoryHandleAfterDeviceHandleListWhenCreatingBufferThenInvalidPropertyIsReturnedAndNothingIsImported) {
    cl_mem_properties properties[] = {supportedExternalMemoryHandleKey, 0x10,
                                      CL_MEM_DEVICE_HANDLE_LIST_KHR, reinterpret_cast<cl_mem_properties>(static_cast<cl_device_id>(clDevice)), CL_MEM_DEVICE_HANDLE_LIST_END_KHR,
                                      supportedExternalMemoryHandleKey, 0x20,
                                      0};
    cl_int errcode = CL_SUCCESS;

    auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);

    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);
    EXPECT_FALSE(capturingContext->allocDeviceMemArgs.wasCalled());
}

TEST_F(ExternalMemoryBufferTest, givenExternalMemoryHandleAndSizeAboveMaxAllocSizeWithUnrestrictedSizeFlagWhenCreatingBufferThenHandleIsImported) {
    capturingContext->allocDeviceMemPtr = &dummyStorage;
    cl_mem_properties properties[] = {supportedExternalMemoryHandleKey, 0x10, 0};
    cl_int errcode = CL_INVALID_VALUE;
    const auto size = static_cast<size_t>(clDevice->getSharedDeviceInfo().maxMemAllocSize) + 1u;

    auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL, size, nullptr, &errcode);

    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_EQ(1u, capturingContext->allocDeviceMemArgs.count());
    EXPECT_EQ(size, capturingContext->allocDeviceMemArgs[0].size);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(ExternalMemoryBufferTest, givenImportFailureWhenCreatingBufferThenInvalidMemObjectIsReturnedAndNothingIsFreed) {
    capturingContext->allocDeviceMemResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    cl_mem_properties properties[] = {supportedExternalMemoryHandleKey, 0x10, 0};
    cl_int errcode = CL_SUCCESS;

    auto buffer = clCreateBufferWithProperties(leoContext.get(), properties, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);

    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, errcode);
    EXPECT_EQ(1u, capturingContext->allocDeviceMemArgs.count());
    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

struct BufferZeroCopyForHostPtrTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        memoryManager = driverHandle->getMemoryManager();
        hostPtr = alignedMalloc(bufferSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, hostPtr);
    }

    void TearDown() override {
        alignedFree(hostPtr);
        Test<OclFixture>::TearDown();
    }

    static constexpr size_t bufferSize = 2 * MemoryConstants::pageSize;

    MemoryManager *memoryManager = nullptr;
    void *hostPtr = nullptr;
};

TEST_F(BufferZeroCopyForHostPtrTest, givenCacheLineAlignedHostPtrAndSizeThenZeroCopyIsAllowed) {
    EXPECT_TRUE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, bufferSize, memoryManager));
    EXPECT_TRUE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, MemoryConstants::cacheLineSize, memoryManager));

    auto cacheLineAlignedPtr = ptrOffset(hostPtr, MemoryConstants::cacheLineSize);
    EXPECT_TRUE(Buffer::isZeroCopyAllowedForHostPtr(cacheLineAlignedPtr, MemoryConstants::cacheLineSize, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenNullHostPtrThenZeroCopyIsNotAllowed) {
    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(nullptr, bufferSize, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenHostPtrNotAlignedToCacheLineThenZeroCopyIsNotAllowed) {
    auto misalignedPtr = ptrOffset(hostPtr, 1u);
    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(misalignedPtr, MemoryConstants::cacheLineSize, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenSizeNotAlignedToCacheLineThenZeroCopyIsNotAllowed) {
    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, MemoryConstants::cacheLineSize - 1u, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenZeroCopyForUseHostPtrDisabledThenZeroCopyIsNotAllowed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableZeroCopyForUseHostPtr.set(true);

    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, bufferSize, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenHostPtrBelowMinAddressRestrictionThenZeroCopyIsNotAllowed) {
    MockAllocSysMemAgnosticMemoryManager restrictedMemoryManager{*neoDevice->getExecutionEnvironment()};
    restrictedMemoryManager.testRestrictions.minAddress = castToUint64(hostPtr) + MemoryConstants::pageSize;
    restrictedMemoryManager.ptrRestrictions = &restrictedMemoryManager.testRestrictions;

    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, bufferSize, &restrictedMemoryManager));

    restrictedMemoryManager.testRestrictions.minAddress = castToUint64(hostPtr);
    EXPECT_TRUE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, bufferSize, &restrictedMemoryManager));
}

struct BufferUnalignedHostPtrTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        capturingContext->getDriverHandleCallBase = true;
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
        rootDeviceIndex = clDevice->getRootDeviceIndex();
        svmAllocsManager = driverHandle->getSvmAllocsManager();

        basePtr = alignedMalloc(allocSize + 2 * MemoryConstants::pageSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, basePtr);
        unalignedHostPtr = ptrOffset(basePtr, MemoryConstants::cacheLineSize);
    }

    void TearDown() override {
        alignedFree(basePtr);
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    MemoryProperties createUseHostPtrProperties() {
        return ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, 0, 0, &clDevice->getDevice());
    }

    static constexpr size_t allocSize = 2 * MemoryConstants::pageSize;

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
    SVMAllocsManager *svmAllocsManager = nullptr;
    uint32_t rootDeviceIndex = 0;
    void *basePtr = nullptr;
    void *unalignedHostPtr = nullptr;
};

TEST_F(BufferUnalignedHostPtrTest, givenPageUnalignedHostPtrWhenAllocatingThenApplicationStorageIsWrappedAtPageAlignedBase) {
    auto memoryProperties = createUseHostPtrProperties();

    auto devicePtr = Buffer::allocateWithHostPtr(leoContext.get(), unalignedHostPtr, allocSize, memoryProperties);
    ASSERT_NE(nullptr, devicePtr);

    auto svmData = svmAllocsManager->getSVMAlloc(devicePtr);
    ASSERT_NE(nullptr, svmData);

    auto allocation = svmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(unalignedHostPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(AllocationType::bufferHostMemory, allocation->getAllocationType());

    EXPECT_EQ(MemoryConstants::cacheLineSize, allocation->getAllocationOffset());
    EXPECT_TRUE(isAligned<MemoryConstants::pageSize>(allocation->getGpuAddressWithoutOffset()));
    EXPECT_EQ(allocation->getGpuAddressWithoutOffset() + allocation->getAllocationOffset(), castToUint64(devicePtr));

    svmAllocsManager->freeSVMAlloc(devicePtr, true);
}

TEST_F(BufferUnalignedHostPtrTest, givenPageUnalignedHostPtrWhenAllocatingThenTrackedRangeCoversWholeBuffer) {
    auto memoryProperties = createUseHostPtrProperties();

    auto devicePtr = Buffer::allocateWithHostPtr(leoContext.get(), unalignedHostPtr, allocSize, memoryProperties);
    ASSERT_NE(nullptr, devicePtr);

    auto svmData = svmAllocsManager->getSVMAlloc(devicePtr);
    ASSERT_NE(nullptr, svmData);
    EXPECT_EQ(MemoryConstants::cacheLineSize + allocSize, svmData->size);
    EXPECT_EQ(svmData, svmAllocsManager->getSVMAlloc(ptrOffset(devicePtr, allocSize - 1u)));

    svmAllocsManager->freeSVMAlloc(devicePtr, true);
}

TEST_F(BufferUnalignedHostPtrTest, givenPageUnalignedHostPtrWhenAllocatingThenAllocationIsNotEligibleForUsmReuseCache) {
    auto memoryProperties = createUseHostPtrProperties();

    auto devicePtr = Buffer::allocateWithHostPtr(leoContext.get(), unalignedHostPtr, allocSize, memoryProperties);
    ASSERT_NE(nullptr, devicePtr);

    auto svmData = svmAllocsManager->getSVMAlloc(devicePtr);
    ASSERT_NE(nullptr, svmData);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, svmData->memoryType);
    EXPECT_EQ(castToUint64(unalignedHostPtr), svmData->allocationFlagsProperty.hostptr);

    svmAllocsManager->freeSVMAlloc(devicePtr, true);
}

TEST_F(BufferUnalignedHostPtrTest, givenAllocationWithHostPtrWhenFreedThenSvmDataIsRemoved) {
    auto memoryProperties = createUseHostPtrProperties();

    auto devicePtr = Buffer::allocateWithHostPtr(leoContext.get(), unalignedHostPtr, allocSize, memoryProperties);
    ASSERT_NE(nullptr, devicePtr);

    EXPECT_TRUE(svmAllocsManager->freeSVMAlloc(devicePtr, true));
    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(devicePtr));
}

struct SharedBufferDeviceAddressTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        capturingContext->getDriverHandleCallBase = true;
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
        rootDeviceIndex = clDevice->getRootDeviceIndex();
        memoryManager = driverHandle->getMemoryManager();
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    GraphicsAllocation *allocate() {
        return memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, allocationSize});
    }

    Buffer *createSharedBuffer(GraphicsAllocation *graphicsAllocation) {
        return Buffer::createSharedBuffer(leoContext.get(), CL_MEM_READ_WRITE, new SharingHandler(),
                                          GraphicsAllocationHelper::toMultiGraphicsAllocation(graphicsAllocation));
    }

    static constexpr size_t allocationSize = 2 * MemoryConstants::pageSize;

    ClDevice *clDevice = nullptr;
    uint32_t rootDeviceIndex = 0u;
    MemoryManager *memoryManager = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
};

TEST_F(SharedBufferDeviceAddressTest, givenAllocationOffsetSetAfterCreationWhenRefreshingDeviceAddressThenUsmPtrIncludesTheOffset) {
    auto graphicsAllocation = allocate();
    ASSERT_NE(nullptr, graphicsAllocation);
    const auto baseAddress = graphicsAllocation->getGpuAddress();

    auto buffer = createSharedBuffer(graphicsAllocation);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(baseAddress, castToUint64(buffer->getUsmPtr()));

    graphicsAllocation->setAllocationOffset(0x40u);
    buffer->refreshDeviceAddress(rootDeviceIndex);
    EXPECT_EQ(baseAddress + 0x40u, castToUint64(buffer->getUsmPtr()));

    graphicsAllocation->setAllocationOffset(0x80u);
    buffer->refreshDeviceAddress(rootDeviceIndex);
    EXPECT_EQ(baseAddress + 0x80u, castToUint64(buffer->getUsmPtr()));

    graphicsAllocation->setAllocationOffset(0u);
    buffer->refreshDeviceAddress(rootDeviceIndex);
    EXPECT_EQ(baseAddress, castToUint64(buffer->getUsmPtr()));

    delete buffer;
}

TEST_F(SharedBufferDeviceAddressTest, givenGraphicsAllocationReplacedWhenResettingItThenUsmPtrAndAllocDataFollowTheNewAllocation) {
    auto firstAllocation = allocate();
    ASSERT_NE(nullptr, firstAllocation);
    auto buffer = createSharedBuffer(firstAllocation);
    ASSERT_NE(nullptr, buffer);
    ASSERT_EQ(firstAllocation, buffer->getGraphicsAllocation(rootDeviceIndex));

    auto secondAllocation = allocate();
    ASSERT_NE(nullptr, secondAllocation);
    ASSERT_NE(firstAllocation->getGpuAddress(), secondAllocation->getGpuAddress());

    buffer->resetGraphicsAllocation(secondAllocation);

    EXPECT_EQ(castToUint64(buffer->getUsmPtr()), secondAllocation->getGpuAddress());
    EXPECT_EQ(secondAllocation, buffer->getGraphicsAllocation(rootDeviceIndex));

    delete buffer;
}

TEST_F(SharedBufferDeviceAddressTest, givenSubBufferWhenParentAddressChangesThenGetUsmPtrFollowsItWithoutExplicitRefresh) {
    auto graphicsAllocation = allocate();
    ASSERT_NE(nullptr, graphicsAllocation);
    const auto baseAddress = graphicsAllocation->getGpuAddress();

    auto buffer = createSharedBuffer(graphicsAllocation);
    ASSERT_NE(nullptr, buffer);

    constexpr size_t regionOrigin = 0x100u;
    cl_buffer_region region{regionOrigin, MemoryConstants::pageSize};
    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region);
    ASSERT_NE(nullptr, subBuffer);
    EXPECT_EQ(baseAddress + regionOrigin, castToUint64(subBuffer->getUsmPtr()));

    graphicsAllocation->setAllocationOffset(0x40u);
    buffer->refreshDeviceAddress(rootDeviceIndex);

    EXPECT_EQ(baseAddress + 0x40u, castToUint64(buffer->getUsmPtr()));
    EXPECT_EQ(baseAddress + 0x40u + regionOrigin, castToUint64(subBuffer->getUsmPtr()));

    graphicsAllocation->setAllocationOffset(0x80u);
    buffer->refreshDeviceAddress(rootDeviceIndex);

    EXPECT_EQ(baseAddress + 0x80u + regionOrigin, castToUint64(subBuffer->getUsmPtr()));
    EXPECT_EQ(baseAddress + 0x80u + regionOrigin, castToUint64(*subBuffer->getUsmPtrRef()));

    delete subBuffer;
    delete buffer;
}

TEST_F(SharedBufferDeviceAddressTest, givenSubBufferWhenRefreshingItsDeviceAddressThenParentIsRefreshedAndRegionOriginIsNotDropped) {
    auto graphicsAllocation = allocate();
    ASSERT_NE(nullptr, graphicsAllocation);
    const auto baseAddress = graphicsAllocation->getGpuAddress();

    auto buffer = createSharedBuffer(graphicsAllocation);
    ASSERT_NE(nullptr, buffer);

    constexpr size_t regionOrigin = 0x100u;
    cl_buffer_region region{regionOrigin, 0x100u};
    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region);
    ASSERT_NE(nullptr, subBuffer);

    graphicsAllocation->setAllocationOffset(0x40u);
    subBuffer->refreshDeviceAddress(rootDeviceIndex);

    EXPECT_EQ(baseAddress + 0x40u, castToUint64(buffer->getUsmPtr()));
    EXPECT_EQ(baseAddress + 0x40u + regionOrigin, castToUint64(subBuffer->getUsmPtr()));
    EXPECT_EQ(baseAddress + 0x40u + regionOrigin, castToUint64(*subBuffer->getUsmPtrRef()));

    delete subBuffer;
    delete buffer;
}

TEST_F(SharedBufferDeviceAddressTest, givenBufferWithoutRegisteredAllocationWhenRefreshingDeviceAddressThenUsmPtrIsLeftUntouched) {
    uint64_t dummyStorage = 0u;
    void *usmPtr = &dummyStorage;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
    auto buffer = std::make_unique<Buffer>(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, usmPtr, nullptr, 64u, true);

    buffer->refreshDeviceAddress(rootDeviceIndex);

    EXPECT_EQ(usmPtr, buffer->getUsmPtr());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
