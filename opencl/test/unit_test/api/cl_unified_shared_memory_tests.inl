/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/api/api.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

struct ClUnifiedSharedMemoryTests : ::testing::Test {
    void SetUp() override {
        debugManager.flags.ExperimentalEnableHostAllocationCache.set(0);
        debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(0);
        debugManager.flags.EnableHostUsmAllocationPool.set(0);
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        deviceFactory = std::make_unique<UltClDeviceFactoryWithPlatform>(1, 0);
        mockContext = std::make_unique<MockContext>(deviceFactory->rootDevices[0]);
    }

    std::unique_ptr<UltClDeviceFactoryWithPlatform> deviceFactory;
    std::unique_ptr<MockContext> mockContext;
    DebugManagerStateRestore restorer;
};

TEST_F(ClUnifiedSharedMemoryTests, whenClHostMemAllocINTELisCalledWithoutContextThenInvalidContextIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto ptr = clHostMemAllocINTEL(0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClHostMemAllocIntelIsCalledWithSizeZeroThenInvalidBufferSizeIsReturned) {

    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 0u, 0, &retVal);
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
    EXPECT_EQ(nullptr, unifiedMemoryHostAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClHostMemAllocIntelIsCalledThenItAllocatesHostUnifiedMemoryAllocation) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemoryHostAllocation);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);
    EXPECT_EQ(graphicsAllocation->size, 4u);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::hostUnifiedMemory);
    EXPECT_EQ(graphicsAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress(),
              castToUint64(unifiedMemoryHostAllocation));

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, GivenForceExtendedUSMBufferSizeDebugFlagWhenUSMAllocationIsCreatedThenSizeIsProperlyExtended) {
    DebugManagerStateRestore restorer;

    constexpr auto bufferSize = 16;
    auto pageSizeNumber = 2;
    debugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
    auto extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;

    cl_int retVal = CL_SUCCESS;
    auto usmAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, usmAllocation);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(usmAllocation);
    EXPECT_EQ(graphicsAllocation->size, extendedBufferSize);

    retVal = clMemFreeINTEL(mockContext.get(), usmAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pageSizeNumber = 4;
    debugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
    extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;

    allocationsManager = mockContext->getSVMAllocsManager();
    usmAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, usmAllocation);

    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    graphicsAllocation = allocationsManager->getSVMAlloc(usmAllocation);
    EXPECT_EQ(graphicsAllocation->size, extendedBufferSize);

    retVal = clMemFreeINTEL(mockContext.get(), usmAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pageSizeNumber = 8;
    debugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
    extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;

    usmAllocation = clSharedMemAllocINTEL(mockContext.get(), nullptr, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, usmAllocation);

    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    graphicsAllocation = allocationsManager->getSVMAlloc(usmAllocation);
    EXPECT_EQ(graphicsAllocation->size, extendedBufferSize);

    retVal = clMemFreeINTEL(mockContext.get(), usmAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenMappedAllocationWhenClMemFreeIntelIscalledThenMappingIsRemoved) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemorySharedAllocation);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    allocationsManager->insertSvmMapOperation(unifiedMemorySharedAllocation, 4u, unifiedMemorySharedAllocation, 0u, false);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, allocationsManager->getSvmMapOperation(unifiedMemorySharedAllocation));
}

TEST_F(ClUnifiedSharedMemoryTests, whenClDeviceMemAllocINTELisCalledWithWrongContextThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto ptr = clDeviceMemAllocINTEL(0, 0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClDeviceMemAllocIntelIsCalledWithSizeZeroThenItInvalidBufferSizeIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 0u, 0, &retVal);
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
    EXPECT_EQ(nullptr, unifiedMemoryDeviceAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClDeviceMemAllocIntelIsCalledThenItAllocatesDeviceUnifiedMemoryAllocation) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemoryDeviceAllocation);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryDeviceAllocation);
    EXPECT_EQ(graphicsAllocation->size, 4u);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::deviceUnifiedMemory);
    EXPECT_EQ(graphicsAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress(),
              castToUint64(unifiedMemoryDeviceAllocation));

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenUnifiedSharedMemoryAllocationCallsAreCalledWithSizeGreaterThenMaxMemAllocSizeThenErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto maxMemAllocSize = mockContext->getDevice(0u)->getDevice().getDeviceInfo().maxMemAllocSize;
    size_t requestedSize = static_cast<size_t>(maxMemAllocSize) + 1u;

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, requestedSize, 0, &retVal);
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
    EXPECT_EQ(nullptr, unifiedMemoryDeviceAllocation);
    unifiedMemoryDeviceAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, requestedSize, 0, &retVal);
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
    EXPECT_EQ(nullptr, unifiedMemoryDeviceAllocation);
    unifiedMemoryDeviceAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, requestedSize, 0, &retVal);
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
    EXPECT_EQ(nullptr, unifiedMemoryDeviceAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, givenSharedMemAllocCallWhenAllocatingGraphicsMemoryFailsThenOutOfResourcesErrorIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto executionEnvironment = deviceFactory.rootDevices[0]->getExecutionEnvironment();
    std::unique_ptr<MemoryManager> memoryManager = std::make_unique<FailMemoryManager>(0, *executionEnvironment);
    std::swap(memoryManager, executionEnvironment->memoryManager);
    MockContext context(deviceFactory.rootDevices[0]);

    cl_int retVal = CL_INVALID_CONTEXT;

    auto allocation = clSharedMemAllocINTEL(&context, nullptr, nullptr, MemoryConstants::pageSize, 0, &retVal);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_EQ(nullptr, allocation);
    std::swap(memoryManager, executionEnvironment->memoryManager);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClSharedMemAllocINTELisCalledWithWrongContextThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto ptr = clSharedMemAllocINTEL(0, 0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClSharedMemAllocIntelIsCalledWithSizeZeroThenInvalidBufferSizeIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 0u, 0, &retVal);
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
    EXPECT_EQ(nullptr, unifiedMemorySharedAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClSharedMemAllocINTELisCalledWithWrongDeviceThenInvalidDeviceErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    MockContext context0;
    MockContext context1;
    auto ptr = clSharedMemAllocINTEL(&context0, context1.getDevice(0), nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClSharedMemAllocIntelIsCalledThenItAllocatesSharedUnifiedMemoryAllocation) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemorySharedAllocation);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);
    EXPECT_EQ(graphicsAllocation->size, 4u);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::sharedUnifiedMemory);
    EXPECT_EQ(graphicsAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress(),
              castToUint64(unifiedMemorySharedAllocation));

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithIncorrectContextThenReturnError) {
    auto retVal = clMemFreeINTEL(0, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithNullPointerThenNoActionOccurs) {
    auto retVal = clMemFreeINTEL(mockContext.get(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClMemBlockingFreeINTELisCalledWithNullPointerThenNoActionOccurs) {
    auto retVal = clMemBlockingFreeINTEL(mockContext.get(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithValidUmPointerThenMemoryIsFreed) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, allocationsManager->getNumAllocs());
}

HWTEST_F(ClUnifiedSharedMemoryTests, givenTemporaryAllocationWhenBlockingFreeCalledThenClearTemporaryStorage) {
    auto device = mockContext->getDevice(0u);

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);

    auto memManager = mockContext->getMemoryManager();

    MockCommandQueueHw<FamilyType> queue(mockContext.get(), device, nullptr);

    auto &csr = queue.getUltCommandStreamReceiver();
    auto contextId = csr.getOsContext().getContextId();
    auto tagAlloc = csr.getTagAddress();
    *tagAlloc = 0;

    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation1 = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);
    auto unifiedMemoryHostAllocation2 = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);

    EXPECT_TRUE(memManager->getTemporaryAllocationsList().peekIsEmpty());

    auto ptr = std::make_unique<std::byte[]>(4);
    EXPECT_EQ(CL_SUCCESS, clEnqueueMemcpyINTEL(&queue, 0, unifiedMemoryHostAllocation2, ptr.get(), 1, 0, nullptr, nullptr));
    EXPECT_FALSE(memManager->getTemporaryAllocationsList().peekIsEmpty());

    EXPECT_EQ(CL_SUCCESS, clMemBlockingFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation1));
    ASSERT_FALSE(memManager->getTemporaryAllocationsList().peekIsEmpty());

    *tagAlloc = memManager->getTemporaryAllocationsList().peekHead()->getTaskCount(contextId);

    EXPECT_EQ(CL_SUCCESS, clMemBlockingFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation2));
    EXPECT_TRUE(memManager->getTemporaryAllocationsList().peekIsEmpty());
}

TEST_F(ClUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithInvalidUmPointerThenMemoryIsNotFreed) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());

    retVal = clMemFreeINTEL(mockContext.get(), ptrOffset(unifiedMemoryHostAllocation, 4));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, allocationsManager->getNumAllocs());
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutContextThenInvalidContextIsReturned) {
    auto retVal = clGetMemAllocInfoINTEL(0, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutAllocationThenInvalidValueIsReturned) {
    auto retVal = clGetMemAllocInfoINTEL(mockContext.get(), nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutAllocationAndWithPropertiesThenProperValueIsReturned) {
    cl_int retVal = CL_INVALID_VALUE;
    size_t paramValueSize = sizeof(void *);
    size_t paramValueSizeRet = 0;

    {
        void *paramValue = reinterpret_cast<void *>(0xfeedbac);
        retVal = clGetMemAllocInfoINTEL(mockContext.get(), mockContext->getDevice(0), CL_MEM_ALLOC_BASE_PTR_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(sizeof(void *), paramValueSizeRet);
        EXPECT_EQ(static_cast<void *>(nullptr), paramValue);
    }
    {
        size_t paramValue = 1;
        paramValueSize = sizeof(size_t);
        retVal = clGetMemAllocInfoINTEL(mockContext.get(), mockContext->getDevice(0), CL_MEM_ALLOC_SIZE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(sizeof(size_t), paramValueSizeRet);
        EXPECT_EQ(static_cast<size_t>(0u), paramValue);
    }
    {
        cl_device_id paramValue = mockContext->getDevice(0);
        paramValueSize = sizeof(cl_device_id);
        retVal = clGetMemAllocInfoINTEL(mockContext.get(), mockContext->getDevice(0), CL_MEM_ALLOC_DEVICE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(sizeof(cl_device_id), paramValueSizeRet);
        EXPECT_EQ(static_cast<cl_device_id>(nullptr), paramValue);
    }
    {
        cl_mem_alloc_flags_intel paramValue = 1;
        paramValueSize = sizeof(cl_mem_properties_intel);
        retVal = clGetMemAllocInfoINTEL(mockContext.get(), mockContext->getDevice(0), CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(sizeof(cl_mem_properties_intel), paramValueSizeRet);
        EXPECT_EQ(static_cast<cl_mem_alloc_flags_intel>(0u), paramValue);
    }
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutSVMAllocationThenInvalidValueIsReturned) {
    SVMAllocsManager *svmAllocsManager = nullptr;
    std::swap(svmAllocsManager, mockContext->svmAllocsManager);
    auto retVal = clGetMemAllocInfoINTEL(mockContext.get(), nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    std::swap(svmAllocsManager, mockContext->svmAllocsManager);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithAllocationTypeParamNameAndWithoutUnifiedSharedMemoryAllocationThenProperFieldsAreSet) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_int);
    cl_int paramValue = 0;
    size_t paramValueSizeRet = 0;

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), nullptr, CL_MEM_ALLOC_TYPE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(CL_MEM_TYPE_UNKNOWN_INTEL, paramValue);
    EXPECT_EQ(sizeof(cl_int), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithValidUnifiedMemoryHostAllocationThenProperFieldsAreSet) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_int);
    cl_int paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext->getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryHostAllocation, CL_MEM_ALLOC_TYPE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::hostUnifiedMemory);
    EXPECT_EQ(CL_MEM_TYPE_HOST_INTEL, paramValue);
    EXPECT_EQ(sizeof(cl_int), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenHostMemAllocWithInvalidPropertiesTokenThenErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {0x1234, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), properties, 4, 0, &retVal);

    EXPECT_EQ(nullptr, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenHostMemAllocWithInvalidWriteCombinedTokenThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), properties, 4, 0, &retVal);

    EXPECT_NE(nullptr, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenDeviceMemAllocWithInvalidPropertiesTokenThenErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {0x1234, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, 4, 0, &retVal);

    EXPECT_EQ(nullptr, unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenSharedMemAllocWithInvalidPropertiesTokenThenErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    const uint64_t invalidToken = 0x1234;
    cl_mem_properties_intel properties[] = {invalidToken, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, 4, 0, &retVal);

    EXPECT_EQ(nullptr, unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenSharedMemAllocWithInvalidWriteCombinedTokenThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, 4, 0, &retVal);

    EXPECT_NE(nullptr, unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenUnifiedMemoryAllocWithoutPropertiesWhenGetMemAllocFlagsThenDefaultValueIsReturned) {
    uint64_t defaultValue = CL_MEM_ALLOC_DEFAULT_INTEL;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_mem_properties_intel);
    cl_mem_properties_intel paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryHostAllocation, CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(defaultValue, paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocTypeIsCalledWithValidUnifiedMemoryHostAllocationThenProperTypeIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_mem_properties_intel);
    cl_mem_properties_intel paramValue = 0;
    size_t paramValueSizeRet = 0;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_DEFAULT_INTEL, 0};

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), properties, 4, 0, &retVal);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryHostAllocation, CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(properties[1], paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocTypeIsCalledWithValidUnifiedMemoryDeviceAllocationThenProperTypeIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_mem_properties_intel);
    cl_mem_properties_intel paramValue = 0;
    size_t paramValueSizeRet = 0;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, 4, 0, &retVal);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryDeviceAllocation, CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(properties[1], paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocTypeIsCalledWithValidUnifiedMemorySharedAllocationThenProperTypeIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_mem_properties_intel);
    cl_mem_properties_intel paramValue = 0;
    size_t paramValueSizeRet = 0;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_DEFAULT_INTEL, 0};

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, 4, 0, &retVal);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemorySharedAllocation, CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(properties[1], paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithValidUnifiedMemoryDeviceAllocationThenProperFieldsAreSet) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_int);
    cl_int paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext->getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryDeviceAllocation);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryDeviceAllocation, CL_MEM_ALLOC_TYPE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::deviceUnifiedMemory);
    EXPECT_EQ(CL_MEM_TYPE_DEVICE_INTEL, paramValue);
    EXPECT_EQ(sizeof(cl_int), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithValidUnifiedMemorySharedAllocationThenProperFieldsAreSet) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_int);
    cl_int paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext->getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemorySharedAllocation, CL_MEM_ALLOC_TYPE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::sharedUnifiedMemory);
    EXPECT_EQ(CL_MEM_TYPE_SHARED_INTEL, paramValue);
    EXPECT_EQ(sizeof(cl_int), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenDeviceAllocationWhenItIsQueriedForDeviceThenProperDeviceIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSizeRet = 0;
    auto device = mockContext->getDevice(0u);
    cl_device_id clDevice = device;
    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), device, nullptr, 4, 0, &retVal);

    cl_device_id returnedDevice;

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryDeviceAllocation, CL_MEM_ALLOC_DEVICE_INTEL, sizeof(returnedDevice), &returnedDevice, &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSizeRet, sizeof(returnedDevice));
    EXPECT_EQ(returnedDevice, clDevice);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenSharedAllocationWhenItIsQueriedForDeviceThenProperDeviceIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSizeRet = 0;
    auto device = mockContext->getDevice(0u);
    cl_device_id clDevice = device;
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), device, nullptr, 4, 0, &retVal);

    cl_device_id returnedDevice;

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemorySharedAllocation, CL_MEM_ALLOC_DEVICE_INTEL, sizeof(returnedDevice), &returnedDevice, &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSizeRet, sizeof(returnedDevice));
    EXPECT_EQ(returnedDevice, clDevice);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenSharedAllocationWithoutDeviceWhenItIsQueriedForDeviceThenNullIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSizeRet = 0;
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), nullptr, nullptr, 4, 0, &retVal);

    cl_device_id returnedDevice;

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemorySharedAllocation, CL_MEM_ALLOC_DEVICE_INTEL, sizeof(returnedDevice), &returnedDevice, &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSizeRet, sizeof(returnedDevice));
    EXPECT_EQ(returnedDevice, nullptr);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenHostAllocationWhenItIsQueriedForDeviceThenProperDeviceIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSizeRet = 0;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);

    cl_device_id returnedDevice;

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryHostAllocation, CL_MEM_ALLOC_DEVICE_INTEL, sizeof(returnedDevice), &returnedDevice, &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSizeRet, sizeof(returnedDevice));
    EXPECT_EQ(returnedDevice, nullptr);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithAllocationBasePtrParamNameThenProperFieldsAreSet) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(uint64_t);
    uint64_t paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext->getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemorySharedAllocation, CL_MEM_ALLOC_BASE_PTR_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::sharedUnifiedMemory);
    EXPECT_EQ(graphicsAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress(), paramValue);
    EXPECT_EQ(sizeof(uint64_t), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithAllocationSizeParamNameThenProperFieldsAreSet) {

    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(size_t);
    size_t paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext->getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);

    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryHostAllocation, CL_MEM_ALLOC_SIZE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::hostUnifiedMemory);
    EXPECT_EQ(graphicsAllocation->size, paramValue);
    EXPECT_EQ(sizeof(size_t), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenSVMAllocationPoolWhenClGetMemAllocInfoINTELisCalledWithAllocationSizeParamNameThenProperFieldsAreSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(2);
    debugManager.flags.EnableDeviceUsmAllocationPool.set(2);

    auto device = mockContext->getDevice(0u);

    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(size_t);
    size_t paramValue = 0;
    size_t paramValueSizeRet = 0;
    const size_t allocationSize = 4u;

    {
        auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, allocationSize, 0, &retVal);
        auto allocationsManager = mockContext->getSVMAllocsManager();
        auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);

        retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryHostAllocation, CL_MEM_ALLOC_SIZE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

        EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::hostUnifiedMemory);
        EXPECT_EQ(allocationSize, paramValue);
        EXPECT_EQ(sizeof(size_t), paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clGetMemAllocInfoINTEL(mockContext.get(), ptrOffset(unifiedMemoryHostAllocation, allocationSize - 1), CL_MEM_ALLOC_SIZE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

        EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::hostUnifiedMemory);
        EXPECT_EQ(allocationSize, paramValue);
        EXPECT_EQ(sizeof(size_t), paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    {
        auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), device, nullptr, 4, 0, &retVal);
        auto allocationsManager = mockContext->getSVMAllocsManager();
        auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryDeviceAllocation);

        retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryDeviceAllocation, CL_MEM_ALLOC_SIZE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

        EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::deviceUnifiedMemory);
        EXPECT_EQ(allocationSize, paramValue);
        EXPECT_EQ(sizeof(size_t), paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clGetMemAllocInfoINTEL(mockContext.get(), ptrOffset(unifiedMemoryDeviceAllocation, allocationSize - 1), CL_MEM_ALLOC_SIZE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

        EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::deviceUnifiedMemory);
        EXPECT_EQ(allocationSize, paramValue);
        EXPECT_EQ(sizeof(size_t), paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ClUnifiedSharedMemoryTests, givenSVMAllocationPoolWhenClGetMemAllocInfoINTELisCalledWithAllocationBasePtrParamNameThenProperFieldsAreSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(2);
    debugManager.flags.EnableDeviceUsmAllocationPool.set(2);

    auto device = mockContext->getDevice(0u);

    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(uint64_t);
    uint64_t paramValue = 0;
    size_t paramValueSizeRet = 0;

    {
        auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(mockContext.get(), nullptr, 4, 0, &retVal);
        auto allocationsManager = mockContext->getSVMAllocsManager();
        auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);

        retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryHostAllocation, CL_MEM_ALLOC_BASE_PTR_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

        EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::hostUnifiedMemory);
        EXPECT_EQ(unifiedMemoryHostAllocation, addrToPtr(paramValue));
        EXPECT_EQ(sizeof(uint64_t), paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clGetMemAllocInfoINTEL(mockContext.get(), ptrOffset(unifiedMemoryHostAllocation, 3), CL_MEM_ALLOC_BASE_PTR_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

        EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::hostUnifiedMemory);
        EXPECT_EQ(unifiedMemoryHostAllocation, addrToPtr(paramValue));
        EXPECT_EQ(sizeof(uint64_t), paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryHostAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    {
        auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), device, nullptr, 4, 0, &retVal);
        auto allocationsManager = mockContext->getSVMAllocsManager();
        auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryDeviceAllocation);

        retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemoryDeviceAllocation, CL_MEM_ALLOC_BASE_PTR_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

        EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::deviceUnifiedMemory);
        EXPECT_EQ(unifiedMemoryDeviceAllocation, addrToPtr(paramValue));
        EXPECT_EQ(sizeof(uint64_t), paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clGetMemAllocInfoINTEL(mockContext.get(), ptrOffset(unifiedMemoryDeviceAllocation, 3), CL_MEM_ALLOC_BASE_PTR_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

        EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::deviceUnifiedMemory);
        EXPECT_EQ(unifiedMemoryDeviceAllocation, addrToPtr(paramValue));
        EXPECT_EQ(sizeof(uint64_t), paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ClUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutParamNameThenInvalidValueIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_uint);
    cl_uint paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    retVal = clGetMemAllocInfoINTEL(mockContext.get(), unifiedMemorySharedAllocation, 0, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClSetKernelArgMemPointerINTELisCalledWithInvalidKernelThenInvaliKernelErrorIsReturned) {
    auto retVal = clSetKernelArgMemPointerINTEL(0, 0, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenDeviceSupportSharedMemoryAllocationsAndSystemPointerIsPassedThenItIsProperlySetInKernel) {
    auto device = mockContext->getDevice(0u);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);

    MockKernelWithInternals mockKernel(*device, mockContext.get(), true);

    auto systemPointer = reinterpret_cast<void *>(0xfeedbac);

    auto kernel = mockKernel.mockMultiDeviceKernel->getKernel(device->getRootDeviceIndex());
    EXPECT_FALSE(kernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = clSetKernelArgMemPointerINTEL(mockKernel.mockMultiDeviceKernel, 0, systemPointer);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(kernel->isAnyKernelArgumentUsingSystemMemory());

    // check if cross thread is updated
    auto crossThreadLocation = reinterpret_cast<uintptr_t *>(ptrOffset(mockKernel.mockKernel->getCrossThreadData(), mockKernel.kernelInfo.argAsPtr(0).stateless));
    auto systemAddress = reinterpret_cast<uintptr_t>(systemPointer);

    EXPECT_EQ(*crossThreadLocation, systemAddress);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClSetKernelArgMemPointerINTELisCalledWithValidUnifiedMemoryAllocationThenProperFieldsAreSet) {
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MockKernelWithInternals mockKernel(*mockContext->getDevice(0u), mockContext.get(), true);

    retVal = clSetKernelArgMemPointerINTEL(mockKernel.mockMultiDeviceKernel, 0, unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmAlloc = mockContext->getSVMAllocsManager()->getSVMAlloc(unifiedMemoryDeviceAllocation);
    EXPECT_EQ(mockKernel.mockKernel->kernelArguments[0].object,
              svmAlloc->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex()));

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenclEnqueueMemsetINTELisCalledWithoutIncorrectCommandQueueThenInvaliQueueErrorIsReturned) {
    auto retVal = clEnqueueMemsetINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenclEnqueueMemsetINTELisCalledWithProperParametersThenParametersArePassedCorrectly) {
    const ClDeviceInfo &devInfo = mockContext->getDevice(0u)->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 400, 0, &retVal);

    struct MockedCommandQueue : public MockCommandQueue {
        using MockCommandQueue::MockCommandQueue;
        cl_int enqueueSVMMemFill(void *svmPtr,
                                 const void *pattern,
                                 size_t patternSize,
                                 size_t size,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event) override {

            EXPECT_EQ(12, *reinterpret_cast<const char *>(pattern));
            EXPECT_EQ(expectedDstPtr, svmPtr);
            EXPECT_EQ(400u, size);
            EXPECT_EQ(1u, patternSize);
            EXPECT_EQ(0u, numEventsInWaitList);
            EXPECT_EQ(nullptr, eventWaitList);
            EXPECT_EQ(nullptr, event);
            return CL_SUCCESS;
        }
        void *expectedDstPtr = nullptr;
    };

    MockedCommandQueue queue{*mockContext};
    queue.expectedDstPtr = unifiedMemoryDeviceAllocation;
    cl_int setValue = 12u;

    retVal = clEnqueueMemsetINTEL(&queue, unifiedMemoryDeviceAllocation, setValue, 400u, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, whenclEnqueueMemFillINTELisCalledWithoutIncorrectCommandQueueThenInvaliQueueErrorIsReturned) {
    cl_int setValue = 12u;
    auto retVal = clEnqueueMemFillINTEL(0, nullptr, &setValue, 0u, 0u, 0u, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenclEnqueueMemFillINTELisCalledWithProperParametersThenParametersArePassedCorrectly) {
    const ClDeviceInfo &devInfo = mockContext->getDevice(0u)->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 400, 0, &retVal);

    struct MockedCommandQueue : public MockCommandQueue {
        using MockCommandQueue::MockCommandQueue;
        cl_int enqueueSVMMemFill(void *svmPtr,
                                 const void *pattern,
                                 size_t patternSize,
                                 size_t size,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event) override {

            EXPECT_EQ(12, *reinterpret_cast<const char *>(pattern));
            EXPECT_EQ(expectedDstPtr, svmPtr);
            EXPECT_EQ(400u, size);
            EXPECT_EQ(4u, patternSize);
            EXPECT_EQ(0u, numEventsInWaitList);
            EXPECT_EQ(nullptr, eventWaitList);
            EXPECT_EQ(nullptr, event);
            return CL_SUCCESS;
        }
        void *expectedDstPtr = nullptr;
    };

    MockedCommandQueue queue{*mockContext};
    queue.expectedDstPtr = unifiedMemoryDeviceAllocation;
    cl_int setValue = 12u;

    retVal = clEnqueueMemFillINTEL(&queue, unifiedMemoryDeviceAllocation, &setValue, sizeof(setValue), 400u, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClEnqueueMemcpyINTELisCalledWithWrongQueueThenInvalidQueueErrorIsReturned) {
    auto retVal = clEnqueueMemcpyINTEL(0, 0, nullptr, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
TEST_F(ClUnifiedSharedMemoryTests, givenTwoUnifiedMemoryAllocationsWhenTheyAreCopiedThenProperParametersArePassed) {
    const ClDeviceInfo &devInfo = mockContext->getDevice(0u)->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 400, 0, &retVal);
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 400, 0, &retVal);

    struct MockedCommandQueue : public MockCommandQueue {
        using MockCommandQueue::MockCommandQueue;
        cl_int enqueueSVMMemcpy(cl_bool blockingCopy,
                                void *dstPtr,
                                const void *srcPtr,
                                size_t size,
                                cl_uint numEventsInWaitList,
                                const cl_event *eventWaitList,
                                cl_event *event, CommandStreamReceiver *csrParam) override {
            EXPECT_EQ(0u, blockingCopy);
            EXPECT_EQ(expectedDstPtr, dstPtr);
            EXPECT_EQ(expectedSrcPtr, srcPtr);
            EXPECT_EQ(400u, size);
            EXPECT_EQ(0u, numEventsInWaitList);
            EXPECT_EQ(nullptr, eventWaitList);
            EXPECT_EQ(nullptr, event);
            return CL_SUCCESS;
        }
        void *expectedDstPtr = nullptr;
        const void *expectedSrcPtr = nullptr;
    };
    MockedCommandQueue queue{*mockContext};
    queue.expectedDstPtr = unifiedMemoryDeviceAllocation;
    queue.expectedSrcPtr = unifiedMemorySharedAllocation;

    retVal = clEnqueueMemcpyINTEL(&queue, 0, unifiedMemoryDeviceAllocation, unifiedMemorySharedAllocation, 400u, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
    clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClEnqueueMigrateMemINTELisCalledWithWrongQueueThenInvalidQueueErrorIsReturned) {
    auto retVal = clEnqueueMigrateMemINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClEnqueueMigrateMemINTELisCalledWithProperParametersAndDebugKeyDisabledThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    MockCommandQueue cmdQ;
    void *unifiedMemoryAlloc = reinterpret_cast<void *>(0x1234);
    debugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(0);

    auto retVal = clEnqueueMigrateMemINTEL(&cmdQ, unifiedMemoryAlloc, 10, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenUseKmdMigrationAndAppendMemoryPrefetchForKmdMigratedSharedAllocationsWhenClEnqueueMigrateMemINTELisCalledThenExplicitlyMigrateMemoryToTheDeviceAssociatedWithCommandQueue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);

    auto device = mockContext->getDevice(0u);

    MockCommandQueue mockCmdQueue{*mockContext.get()};
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), device, nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemorySharedAllocation);

    retVal = clEnqueueMigrateMemINTEL(&mockCmdQueue, unifiedMemorySharedAllocation, 10, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setMemPrefetchCalled);
    EXPECT_EQ(0u, mockMemoryManager->memPrefetchSubDeviceIds[0]);

    clMemFreeINTEL(mockContext.get(), unifiedMemorySharedAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, givenContextWithMultipleSubdevicesWhenClEnqueueMigrateMemINTELisCalledThenExplicitlyMigrateMemoryToTheSubDeviceAssociatedWithCommandQueue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);

    UltClDeviceFactoryWithPlatform deviceFactory{1, 4};
    cl_device_id allDevices[] = {deviceFactory.rootDevices[0], deviceFactory.subDevices[0], deviceFactory.subDevices[1],
                                 deviceFactory.subDevices[2], deviceFactory.subDevices[3]};
    MockContext multiTileContext(ClDeviceVector{allDevices, 5});
    auto subDevice = deviceFactory.subDevices[1];

    MockCommandQueue mockCmdQueue(&multiTileContext, subDevice, 0, false);
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(&multiTileContext, subDevice, nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemorySharedAllocation);

    retVal = clEnqueueMigrateMemINTEL(&mockCmdQueue, unifiedMemorySharedAllocation, 10, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(subDevice->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setMemPrefetchCalled);
    EXPECT_EQ(1u, mockMemoryManager->memPrefetchSubDeviceIds[0]);

    clMemFreeINTEL(&multiTileContext, unifiedMemorySharedAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, givenContextWithMultipleSubdevicesWhenClEnqueueMigrateMemINTELisCalledThenExplicitlyMigrateMemoryToTheRootDeviceAssociatedWithCommandQueue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);

    UltClDeviceFactoryWithPlatform deviceFactory{1, 4};
    cl_device_id allDevices[] = {deviceFactory.rootDevices[0], deviceFactory.subDevices[0], deviceFactory.subDevices[1],
                                 deviceFactory.subDevices[2], deviceFactory.subDevices[3]};
    MockContext multiTileContext(ClDeviceVector{allDevices, 5});
    auto device = deviceFactory.rootDevices[0];

    MockCommandQueue mockCmdQueue(&multiTileContext, device, 0, false);
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(&multiTileContext, device, nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemorySharedAllocation);

    retVal = clEnqueueMigrateMemINTEL(&mockCmdQueue, unifiedMemorySharedAllocation, 10, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setMemPrefetchCalled);
    for (auto index = 0u; index < mockMemoryManager->memPrefetchSubDeviceIds.size(); index++) {
        EXPECT_EQ(index, mockMemoryManager->memPrefetchSubDeviceIds[index]);
    }

    clMemFreeINTEL(&multiTileContext, unifiedMemorySharedAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClEnqueueMemAdviseINTELisCalledWithWrongQueueThenInvalidQueueErrorIsReturned) {
    auto retVal = clEnqueueMemAdviseINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, whenClEnqueueMemAdviseINTELisCalledWithProperParametersThenSuccessIsReturned) {
    MockCommandQueue cmdQ;
    void *unifiedMemoryAlloc = reinterpret_cast<void *>(0x1234);

    auto retVal = clEnqueueMemAdviseINTEL(&cmdQ, unifiedMemoryAlloc, 10, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

class ClUnifiedSharedMemoryEventTests : public CommandQueueHwFixture,
                                        public ClUnifiedSharedMemoryTests {
  public:
    void SetUp() override {
        ClUnifiedSharedMemoryTests::SetUp();
        this->context = mockContext.get();
        this->pCmdQ = createCommandQueue(nullptr);
    }
    void TearDown() override {
        clReleaseEvent(event);
        context = nullptr;
        CommandQueueHwFixture::tearDown();
        ClUnifiedSharedMemoryTests::TearDown();
    }

    cl_event event = nullptr;
    std::unique_ptr<UltClDeviceFactoryWithPlatform> deviceFactory;
};

TEST_F(ClUnifiedSharedMemoryEventTests, whenClEnqueueMigrateMemINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    void *unifiedMemoryAlloc = reinterpret_cast<void *>(0x1234);

    auto retVal = clEnqueueMigrateMemINTEL(this->pCmdQ, unifiedMemoryAlloc, 10, 0, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MIGRATEMEM_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
}

TEST_F(ClUnifiedSharedMemoryEventTests, whenClEnqueueMemAdviseINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    void *unifiedMemoryAlloc = reinterpret_cast<void *>(0x1234);

    auto retVal = clEnqueueMemAdviseINTEL(this->pCmdQ, unifiedMemoryAlloc, 10, 0, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MEMADVISE_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
}

TEST_F(ClUnifiedSharedMemoryEventTests, whenClEnqueueMemcpyINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    const ClDeviceInfo &devInfo = this->context->getDevice(0u)->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemoryDst = clSharedMemAllocINTEL(this->context, this->context->getDevice(0u), nullptr, 400, 0, &retVal);
    auto unifiedMemorySrc = clSharedMemAllocINTEL(this->context, this->context->getDevice(0u), nullptr, 400, 0, &retVal);

    retVal = clEnqueueMemcpyINTEL(this->pCmdQ, 0, unifiedMemoryDst, unifiedMemorySrc, 400u, 0, nullptr, &event);
    EXPECT_EQ(retVal, CL_SUCCESS);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MEMCPY_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);

    clMemFreeINTEL(this->context, unifiedMemoryDst);
    clMemFreeINTEL(this->context, unifiedMemorySrc);
}

TEST_F(ClUnifiedSharedMemoryEventTests, whenClEnqueueMemsetINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    const ClDeviceInfo &devInfo = this->context->getDevice(0u)->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(this->context, this->context->getDevice(0u), nullptr, 400, 0, &retVal);
    cl_int setValue = 12u;

    retVal = clEnqueueMemsetINTEL(this->pCmdQ, unifiedMemorySharedAllocation, setValue, 400u, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MEMSET_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
    clMemFreeINTEL(this->context, unifiedMemorySharedAllocation);
}

TEST_F(ClUnifiedSharedMemoryEventTests, whenClEnqueueMemFillINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    const ClDeviceInfo &devInfo = this->context->getDevice(0u)->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    cl_int retVal = CL_SUCCESS;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(this->context, this->context->getDevice(0u), nullptr, 400, 0, &retVal);
    cl_int setValue = 12u;

    retVal = clEnqueueMemFillINTEL(this->pCmdQ, unifiedMemorySharedAllocation, &setValue, sizeof(setValue), 400u, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MEMFILL_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
    clMemFreeINTEL(this->context, unifiedMemorySharedAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, givenDefaulMemPropertiesWhenClDeviceMemAllocIntelIsCalledThenItAllocatesDeviceUnifiedMemoryAllocationWithProperAllocationTypeAndSize) {
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_DEFAULT_INTEL, 0};
    auto allocationSize = 4000u;
    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, allocationSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemoryDeviceAllocation);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryDeviceAllocation);
    auto gpuAllocation = graphicsAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(graphicsAllocation->size, allocationSize);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::deviceUnifiedMemory);
    EXPECT_EQ(AllocationType::buffer, gpuAllocation->getAllocationType());
    EXPECT_EQ(gpuAllocation->getGpuAddress(), castToUint64(unifiedMemoryDeviceAllocation));
    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenValidMemPropertiesWhenClDeviceMemAllocIntelIsCalledThenItAllocatesDeviceUnifiedMemoryAllocationWithProperAllocationTypeAndSize) {
    cl_int retVal = CL_SUCCESS;
    auto allocationSize = 4000u;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};
    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, allocationSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemoryDeviceAllocation);

    auto allocationsManager = mockContext->getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryDeviceAllocation);
    auto gpuAllocation = graphicsAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(graphicsAllocation->size, allocationSize);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::deviceUnifiedMemory);
    EXPECT_EQ(gpuAllocation->getAllocationType(), AllocationType::writeCombined);
    EXPECT_EQ(gpuAllocation->getGpuAddress(), castToUint64(unifiedMemoryDeviceAllocation));
    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());

    retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClUnifiedSharedMemoryTests, givenInvalidMemPropertiesWhenClSharedMemAllocIntelIsCalledThenInvalidValueIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, 4, 0, &retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, unifiedMemorySharedAllocation);
}

TEST_F(ClUnifiedSharedMemoryTests, givenUnifiedMemoryAllocationSizeGreaterThanMaxMemAllocSizeAndClMemAllowUnrestrictedSizeFlagWhenCreateAllocationThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL, 0};
    auto bigSize = MemoryConstants::gigaByte * 10;
    auto allocationSize = static_cast<size_t>(bigSize);
    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(mockContext->getDevice(0u)->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();
    if (memoryManager->peekForce32BitAllocations() || is32bit) {
        GTEST_SKIP();
    }

    {
        auto unifiedMemoryAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, allocationSize, 0, &retVal);

        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, unifiedMemoryAllocation);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    {
        auto unifiedMemoryAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, allocationSize, 0, &retVal);

        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, unifiedMemoryAllocation);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    {
        auto unifiedMemoryAllocation = clHostMemAllocINTEL(mockContext.get(), properties, allocationSize, 0, &retVal);

        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, unifiedMemoryAllocation);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ClUnifiedSharedMemoryTests, givenUnifiedMemoryAllocationSizeGreaterThanMaxMemAllocSizeAndDebugFlagSetWhenCreateAllocationThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowUnrestrictedSize.set(1);
    cl_int retVal = CL_SUCCESS;
    auto allocationSize = static_cast<size_t>(mockContext->getDevice(0u)->getSharedDeviceInfo().maxMemAllocSize) + 1;
    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(mockContext->getDevice(0u)->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();
    if (memoryManager->peekForce32BitAllocations() || is32bit) {
        GTEST_SKIP();
    }

    {
        auto unifiedMemoryAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), 0, allocationSize, 0, &retVal);

        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, unifiedMemoryAllocation);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    {
        auto unifiedMemoryAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), 0, allocationSize, 0, &retVal);

        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, unifiedMemoryAllocation);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    {
        auto unifiedMemoryAllocation = clHostMemAllocINTEL(mockContext.get(), 0, allocationSize, 0, &retVal);

        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, unifiedMemoryAllocation);

        retVal = clMemFreeINTEL(mockContext.get(), unifiedMemoryAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ClUnifiedSharedMemoryTests, givenUnifiedMemoryAllocationSizeGreaterThanMaxMemAllocSizeWhenCreateAllocationThenErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {0};
    auto bigSize = MemoryConstants::gigaByte * 20;
    auto allocationSize = static_cast<size_t>(bigSize);
    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(mockContext->getDevice(0u)->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();
    if (memoryManager->peekForce32BitAllocations() || is32bit) {
        GTEST_SKIP();
    }

    {
        auto unifiedMemoryAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, allocationSize, 0, &retVal);

        EXPECT_NE(CL_SUCCESS, retVal);
        EXPECT_EQ(nullptr, unifiedMemoryAllocation);
    }

    {
        auto unifiedMemoryAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), properties, allocationSize, 0, &retVal);

        EXPECT_NE(CL_SUCCESS, retVal);
        EXPECT_EQ(nullptr, unifiedMemoryAllocation);
    }

    {
        auto unifiedMemoryAllocation = clHostMemAllocINTEL(mockContext.get(), properties, allocationSize, 0, &retVal);

        EXPECT_NE(CL_SUCCESS, retVal);
        EXPECT_EQ(nullptr, unifiedMemoryAllocation);
    }
}

using MultiRootDeviceClUnifiedSharedMemoryTests = MultiRootDeviceFixture;

TEST_F(MultiRootDeviceClUnifiedSharedMemoryTests, WhenClHostMemAllocIntelIsCalledInMultiRootDeviceEnvironmentThenItAllocatesHostUnifiedMemoryAllocations) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(0);

    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(context.get(), nullptr, 4, 0, &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemoryHostAllocation);

    auto allocationsManager = context->getSVMAllocsManager();

    EXPECT_EQ(allocationsManager->getNumAllocs(), 1u);

    auto svmAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);
    auto graphicsAllocation1 = svmAllocation->gpuAllocations.getGraphicsAllocation(1u);
    auto graphicsAllocation2 = svmAllocation->gpuAllocations.getGraphicsAllocation(2u);

    EXPECT_EQ(svmAllocation->size, 4u);
    EXPECT_EQ(svmAllocation->memoryType, InternalMemoryType::hostUnifiedMemory);

    EXPECT_NE(graphicsAllocation1, nullptr);
    EXPECT_NE(graphicsAllocation2, nullptr);

    EXPECT_EQ(graphicsAllocation1->getRootDeviceIndex(), 1u);
    EXPECT_EQ(graphicsAllocation2->getRootDeviceIndex(), 2u);

    EXPECT_EQ(graphicsAllocation1->getAllocationType(), AllocationType::bufferHostMemory);
    EXPECT_EQ(graphicsAllocation2->getAllocationType(), AllocationType::bufferHostMemory);

    EXPECT_EQ(graphicsAllocation1->getGpuAddress(), castToUint64(unifiedMemoryHostAllocation));
    EXPECT_EQ(graphicsAllocation2->getGpuAddress(), castToUint64(unifiedMemoryHostAllocation));

    retVal = clMemFreeINTEL(context.get(), unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MultiRootDeviceClUnifiedSharedMemoryTests, WhenClSharedMemAllocIntelIsCalledWithoutDeviceInMultiRootDeviceEnvironmentThenItAllocatesHostUnifiedMemoryAllocations) {

    cl_int retVal = CL_SUCCESS;
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(context.get(), nullptr, nullptr, 4, 0, &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemorySharedAllocation);

    auto allocationsManager = context->getSVMAllocsManager();

    EXPECT_EQ(allocationsManager->getNumAllocs(), 1u);

    auto svmAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);
    auto graphicsAllocation1 = svmAllocation->gpuAllocations.getGraphicsAllocation(1u);
    auto graphicsAllocation2 = svmAllocation->gpuAllocations.getGraphicsAllocation(2u);

    EXPECT_EQ(svmAllocation->size, 4u);
    EXPECT_EQ(svmAllocation->memoryType, InternalMemoryType::sharedUnifiedMemory);

    EXPECT_NE(graphicsAllocation1, nullptr);
    EXPECT_NE(graphicsAllocation2, nullptr);

    EXPECT_EQ(graphicsAllocation1->getRootDeviceIndex(), 1u);
    EXPECT_EQ(graphicsAllocation2->getRootDeviceIndex(), 2u);

    EXPECT_EQ(graphicsAllocation1->getAllocationType(), AllocationType::bufferHostMemory);
    EXPECT_EQ(graphicsAllocation2->getAllocationType(), AllocationType::bufferHostMemory);

    EXPECT_EQ(graphicsAllocation1->getGpuAddress(), castToUint64(unifiedMemorySharedAllocation));
    EXPECT_EQ(graphicsAllocation2->getGpuAddress(), castToUint64(unifiedMemorySharedAllocation));

    retVal = clMemFreeINTEL(context.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
TEST_F(MultiRootDeviceClUnifiedSharedMemoryTests, WhenClSharedMemAllocIntelIsCalledWithoutDeviceInMultiRootDeviceEnvironmentThenItWaitsForAllGpuAllocations) {

    mockMemoryManager->waitAllocations.reset(new MultiGraphicsAllocation(2u));

    cl_int retVal = CL_SUCCESS;
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(context.get(), nullptr, nullptr, 4, 0, &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemorySharedAllocation);

    auto allocationsManager = context->getSVMAllocsManager();

    EXPECT_EQ(allocationsManager->getNumAllocs(), 1u);

    auto svmAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);
    auto graphicsAllocation1 = svmAllocation->gpuAllocations.getGraphicsAllocation(1u);
    auto graphicsAllocation2 = svmAllocation->gpuAllocations.getGraphicsAllocation(2u);

    EXPECT_EQ(svmAllocation->size, 4u);

    EXPECT_NE(graphicsAllocation1, nullptr);
    EXPECT_NE(graphicsAllocation2, nullptr);

    retVal = clMemBlockingFreeINTEL(context.get(), unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(mockMemoryManager->waitForEnginesCompletionCalled, 2u);
    EXPECT_EQ(mockMemoryManager->waitAllocations->getGraphicsAllocation(1u), graphicsAllocation1);
    EXPECT_EQ(mockMemoryManager->waitAllocations->getGraphicsAllocation(2u), graphicsAllocation2);

    EXPECT_EQ(allocationsManager->getNumAllocs(), 0u);

    svmAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);
    EXPECT_EQ(nullptr, svmAllocation);
}
