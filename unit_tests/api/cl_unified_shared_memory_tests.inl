/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/unified_memory_manager.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/api/api.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

TEST(clUnifiedSharedMemoryTests, whenClHostMemAllocINTELisCalledWithoutContextThenInvalidContextIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto ptr = clHostMemAllocINTEL(0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClHostMemAllocIntelIsCalledThenItAllocatesHostUnifiedMemoryAllocation) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemoryHostAllocation);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);
    EXPECT_EQ(graphicsAllocation->size, 4u);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::HOST_UNIFIED_MEMORY);
    EXPECT_EQ(graphicsAllocation->gpuAllocation->getGpuAddress(), castToUint64(unifiedMemoryHostAllocation));

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, givenMappedAllocationWhenClMemFreeIntelIscalledThenMappingIsRemoved) {

    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(&mockContext, mockContext.getDevice(0u), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemorySharedAllocation);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    allocationsManager->insertSvmMapOperation(unifiedMemorySharedAllocation, 4u, unifiedMemorySharedAllocation, 0u, false);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, allocationsManager->getSvmMapOperation(unifiedMemorySharedAllocation));
}

TEST(clUnifiedSharedMemoryTests, whenClDeviceMemAllocINTELisCalledWithWrongContextThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto ptr = clDeviceMemAllocINTEL(0, 0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClDeviceMemAllocIntelIsCalledThenItAllocatesDeviceUnifiedMemoryAllocation) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unfiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(&mockContext, mockContext.getDevice(0u), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unfiedMemoryDeviceAllocation);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unfiedMemoryDeviceAllocation);
    EXPECT_EQ(graphicsAllocation->size, 4u);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::DEVICE_UNIFIED_MEMORY);
    EXPECT_EQ(graphicsAllocation->gpuAllocation->getGpuAddress(), castToUint64(unfiedMemoryDeviceAllocation));

    retVal = clMemFreeINTEL(&mockContext, unfiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClSharedMemAllocINTELisCalledWithWrongContextThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto ptr = clSharedMemAllocINTEL(0, 0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClSharedMemAllocIntelIsCalledThenItAllocatesSharedUnifiedMemoryAllocation) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unfiedMemorySharedAllocation = clSharedMemAllocINTEL(&mockContext, mockContext.getDevice(0u), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unfiedMemorySharedAllocation);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unfiedMemorySharedAllocation);
    EXPECT_EQ(graphicsAllocation->size, 4u);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::SHARED_UNIFIED_MEMORY);
    EXPECT_EQ(graphicsAllocation->gpuAllocation->getGpuAddress(), castToUint64(unfiedMemorySharedAllocation));

    retVal = clMemFreeINTEL(&mockContext, unfiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithIncorrectContextThenReturnError) {
    auto retVal = clMemFreeINTEL(0, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithNullPointerThenNoActionOccurs) {
    MockContext mockContext;
    auto retVal = clMemFreeINTEL(&mockContext, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithValidUmPointerThenMemoryIsFreed) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, allocationsManager->getNumAllocs());
}

TEST(clUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithInvalidUmPointerThenMemoryIsNotFreed) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());

    retVal = clMemFreeINTEL(&mockContext, ptrOffset(unifiedMemoryHostAllocation, 4));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, allocationsManager->getNumAllocs());
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutContextThenInvalidContextIsReturned) {
    auto retVal = clGetMemAllocInfoINTEL(0, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutAllocationThenInvalidValueIsReturned) {
    MockContext mockContext;
    auto retVal = clGetMemAllocInfoINTEL(&mockContext, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutSVMAllocationThenInvalidValueIsReturned) {
    MockContext mockContext;
    delete mockContext.svmAllocsManager;
    mockContext.svmAllocsManager = nullptr;
    auto retVal = clGetMemAllocInfoINTEL(&mockContext, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithAllocationTypeParamNameAndWithoutUnifiedSharedMemoryAllocationThenProperFieldsAreSet) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_int);
    cl_int paramValue = 0;
    size_t paramValueSizeRet = 0;

    retVal = clGetMemAllocInfoINTEL(&mockContext, nullptr, CL_MEM_ALLOC_TYPE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(CL_MEM_TYPE_UNKNOWN_INTEL, paramValue);
    EXPECT_EQ(sizeof(cl_int), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithValidUnifiedMemoryHostAllocationThenProperFieldsAreSet) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_int);
    cl_int paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext.getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemoryHostAllocation, CL_MEM_ALLOC_TYPE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::HOST_UNIFIED_MEMORY);
    EXPECT_EQ(CL_MEM_TYPE_HOST_INTEL, paramValue);
    EXPECT_EQ(sizeof(cl_int), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenHostMemAllocWithInvalidPropertiesTokenThenErrorIsReturned) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {0x1234, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, properties, 4, 0, &retVal);

    EXPECT_EQ(nullptr, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenDeviceMemAllocWithInvalidPropertiesTokenThenErrorIsReturned) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    cl_mem_properties_intel properties[] = {0x1234, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(&mockContext, mockContext.getDevice(0u), properties, 4, 0, &retVal);

    EXPECT_EQ(nullptr, unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenSharedMemAllocWithInvalidPropertiesTokenThenErrorIsReturned) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    const uint64_t invalidToken = 0x1234;
    cl_mem_properties_intel properties[] = {invalidToken, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(&mockContext, mockContext.getDevice(0u), properties, 4, 0, &retVal);

    EXPECT_EQ(nullptr, unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, givenUnifiedMemoryAllocWithoutPropertiesWhenGetMemAllocFlagsThenDefaultValueIsReturned) {
    uint64_t defaultValue = CL_MEM_ALLOC_DEFAULT_INTEL;
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_mem_properties_intel);
    cl_mem_properties_intel paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemoryHostAllocation, CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(defaultValue, paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocTypeIsCalledWithValidUnifiedMemoryHostAllocationThenProperTypeIsReturned) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_mem_properties_intel);
    cl_mem_properties_intel paramValue = 0;
    size_t paramValueSizeRet = 0;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, properties, 4, 0, &retVal);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemoryHostAllocation, CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(properties[1], paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocTypeIsCalledWithValidUnifiedMemoryDeviceAllocationThenProperTypeIsReturned) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_mem_properties_intel);
    cl_mem_properties_intel paramValue = 0;
    size_t paramValueSizeRet = 0;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(&mockContext, mockContext.getDevice(0u), properties, 4, 0, &retVal);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemoryDeviceAllocation, CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(properties[1], paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocTypeIsCalledWithValidUnifiedMemorySharedAllocationThenProperTypeIsReturned) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_mem_properties_intel);
    cl_mem_properties_intel paramValue = 0;
    size_t paramValueSizeRet = 0;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(&mockContext, mockContext.getDevice(0u), properties, 4, 0, &retVal);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemorySharedAllocation, CL_MEM_ALLOC_FLAGS_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(properties[1], paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithValidUnifiedMemoryDeviceAllocationThenProperFieldsAreSet) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_int);
    cl_int paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(&mockContext, mockContext.getDevice(0u), nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext.getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryDeviceAllocation);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemoryDeviceAllocation, CL_MEM_ALLOC_TYPE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::DEVICE_UNIFIED_MEMORY);
    EXPECT_EQ(CL_MEM_TYPE_DEVICE_INTEL, paramValue);
    EXPECT_EQ(sizeof(cl_int), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithValidUnifiedMemorySharedAllocationThenProperFieldsAreSet) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_int);
    cl_int paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(&mockContext, mockContext.getDevice(0u), nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext.getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemorySharedAllocation, CL_MEM_ALLOC_TYPE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::SHARED_UNIFIED_MEMORY);
    EXPECT_EQ(CL_MEM_TYPE_SHARED_INTEL, paramValue);
    EXPECT_EQ(sizeof(cl_int), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithAllocationBasePtrParamNameThenProperFieldsAreSet) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(uint64_t);
    uint64_t paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(&mockContext, mockContext.getDevice(0u), nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext.getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemorySharedAllocation, CL_MEM_ALLOC_BASE_PTR_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::SHARED_UNIFIED_MEMORY);
    EXPECT_EQ(graphicsAllocation->gpuAllocation->getGpuAddress(), paramValue);
    EXPECT_EQ(sizeof(uint64_t), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithAllocationSizeParamNameThenProperFieldsAreSet) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(size_t);
    size_t paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);
    auto allocationsManager = mockContext.getSVMAllocsManager();
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);

    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemoryHostAllocation, CL_MEM_ALLOC_SIZE_INTEL, paramValueSize, &paramValue, &paramValueSizeRet);

    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::HOST_UNIFIED_MEMORY);
    EXPECT_EQ(graphicsAllocation->size, paramValue);
    EXPECT_EQ(sizeof(size_t), paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledWithoutParamNameThenInvalidValueIsReturned) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    size_t paramValueSize = sizeof(cl_uint);
    cl_uint paramValue = 0;
    size_t paramValueSizeRet = 0;

    auto unifiedMemorySharedAllocation = clSharedMemAllocINTEL(&mockContext, mockContext.getDevice(0u), nullptr, 4, 0, &retVal);
    retVal = clGetMemAllocInfoINTEL(&mockContext, unifiedMemorySharedAllocation, 0, paramValueSize, &paramValue, &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clMemFreeINTEL(&mockContext, unifiedMemorySharedAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClSetKernelArgMemPointerINTELisCalledWithInvalidKernelThenInvaliKernelErrorIsReturned) {
    auto retVal = clSetKernelArgMemPointerINTEL(0, 0, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenDeviceSupportSharedMemoryAllocationsAndSystemPointerIsPassedItIsProperlySetInKernel) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    auto mockContext = std::make_unique<MockContext>();

    MockKernelWithInternals mockKernel(*mockContext->getDevice(0u), mockContext.get(), true);

    auto systemPointer = reinterpret_cast<void *>(0xfeedbac);

    auto retVal = clSetKernelArgMemPointerINTEL(mockKernel.mockKernel, 0, systemPointer);
    EXPECT_EQ(retVal, CL_SUCCESS);

    //check if cross thread is updated
    auto crossThreadLocation = reinterpret_cast<uintptr_t *>(ptrOffset(mockKernel.mockKernel->getCrossThreadData(), mockKernel.kernelInfo.kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset));
    auto systemAddress = reinterpret_cast<uintptr_t>(systemPointer);

    EXPECT_EQ(*crossThreadLocation, systemAddress);
}

TEST(clUnifiedSharedMemoryTests, whenClSetKernelArgMemPointerINTELisCalledWithValidUnifiedMemoryAllocationThenProperFieldsAreSet) {
    auto mockContext = std::make_unique<MockContext>();
    cl_int retVal = CL_SUCCESS;
    auto unfiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MockKernelWithInternals mockKernel(*mockContext->getDevice(0u), mockContext.get(), true);

    retVal = clSetKernelArgMemPointerINTEL(mockKernel.mockKernel, 0, unfiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmAlloc = mockContext->getSVMAllocsManager()->getSVMAlloc(unfiedMemoryDeviceAllocation);
    EXPECT_EQ(mockKernel.mockKernel->kernelArguments[0].object, svmAlloc->gpuAllocation);

    retVal = clMemFreeINTEL(mockContext.get(), unfiedMemoryDeviceAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenclEnqueueMemsetINTELisCalledWithoutIncorrectCommandQueueThenInvaliQueueErrorIsReturned) {
    auto retVal = clEnqueueMemsetINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenclEnqueueMemsetINTELisCalledWithProperParametersThenParametersArePassedCorrectly) {
    auto mockContext = std::make_unique<MockContext>();
    cl_int retVal = CL_SUCCESS;

    auto unfiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 400, 0, &retVal);

    struct MockedCommandQueue : public CommandQueue {
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

    MockedCommandQueue queue;
    queue.expectedDstPtr = unfiedMemoryDeviceAllocation;
    cl_int setValue = 12u;

    retVal = clEnqueueMemsetINTEL(&queue, unfiedMemoryDeviceAllocation, setValue, 400u, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), unfiedMemoryDeviceAllocation);
}

TEST(clUnifiedSharedMemoryTests, whenclEnqueueMemFillINTELisCalledWithoutIncorrectCommandQueueThenInvaliQueueErrorIsReturned) {
    cl_int setValue = 12u;
    auto retVal = clEnqueueMemFillINTEL(0, nullptr, &setValue, 0u, 0u, 0u, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenclEnqueueMemFillINTELisCalledWithProperParametersThenParametersArePassedCorrectly) {
    auto mockContext = std::make_unique<MockContext>();
    cl_int retVal = CL_SUCCESS;

    auto unfiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 400, 0, &retVal);

    struct MockedCommandQueue : public CommandQueue {
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

    MockedCommandQueue queue;
    queue.expectedDstPtr = unfiedMemoryDeviceAllocation;
    cl_int setValue = 12u;

    retVal = clEnqueueMemFillINTEL(&queue, unfiedMemoryDeviceAllocation, &setValue, sizeof(setValue), 400u, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), unfiedMemoryDeviceAllocation);
}

TEST(clUnifiedSharedMemoryTests, whenClEnqueueMemcpyINTELisCalledWithWrongQueueThenInvalidQueueErrorIsReturned) {
    auto retVal = clEnqueueMemcpyINTEL(0, 0, nullptr, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
TEST(clUnifiedSharedMemoryTests, givenTwoUnifiedMemoryAllocationsWhenTheyAreCopiedThenProperParamtersArePassed) {

    auto mockContext = std::make_unique<MockContext>();
    cl_int retVal = CL_SUCCESS;

    auto unfiedMemoryDeviceAllocation = clDeviceMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 400, 0, &retVal);
    auto unfiedMemorySharedAllocation = clSharedMemAllocINTEL(mockContext.get(), mockContext->getDevice(0u), nullptr, 400, 0, &retVal);

    struct MockedCommandQueue : public CommandQueue {
        cl_int enqueueSVMMemcpy(cl_bool blockingCopy,
                                void *dstPtr,
                                const void *srcPtr,
                                size_t size,
                                cl_uint numEventsInWaitList,
                                const cl_event *eventWaitList,
                                cl_event *event) override {
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
    MockedCommandQueue queue;
    queue.expectedDstPtr = unfiedMemoryDeviceAllocation;
    queue.expectedSrcPtr = unfiedMemorySharedAllocation;

    retVal = clEnqueueMemcpyINTEL(&queue, 0, unfiedMemoryDeviceAllocation, unfiedMemorySharedAllocation, 400u, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    clMemFreeINTEL(mockContext.get(), unfiedMemoryDeviceAllocation);
    clMemFreeINTEL(mockContext.get(), unfiedMemorySharedAllocation);
}

TEST(clUnifiedSharedMemoryTests, whenClEnqueueMigrateMemINTELisCalledWithWrongQueueThenInvalidQueueErrorIsReturned) {
    auto retVal = clEnqueueMigrateMemINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClEnqueueMigrateMemINTELisCalledWithProperParametersThenSuccessIsReturned) {
    CommandQueue cmdQ;
    void *unifiedMemoryAlloc = reinterpret_cast<void *>(0x1234);

    auto retVal = clEnqueueMigrateMemINTEL(&cmdQ, unifiedMemoryAlloc, 10, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClEnqueueMemAdviseINTELisCalledWithWrongQueueThenInvalidQueueErrorIsReturned) {
    auto retVal = clEnqueueMemAdviseINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClEnqueueMemAdviseINTELisCalledWithProperParametersThenSuccessIsReturned) {
    CommandQueue cmdQ;
    void *unifiedMemoryAlloc = reinterpret_cast<void *>(0x1234);

    auto retVal = clEnqueueMemAdviseINTEL(&cmdQ, unifiedMemoryAlloc, 10, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

class clUnifiedSharedMemoryEventTests : public CommandQueueHwFixture,
                                        public ::testing::Test {
  public:
    void SetUp() override {
        this->pCmdQ = createCommandQueue(nullptr);
    }
    void TearDown() override {
        clReleaseEvent(event);
        CommandQueueHwFixture::TearDown();
    }

    cl_event event = nullptr;
};

TEST_F(clUnifiedSharedMemoryEventTests, whenClEnqueueMigrateMemINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    void *unifiedMemoryAlloc = reinterpret_cast<void *>(0x1234);

    auto retVal = clEnqueueMigrateMemINTEL(this->pCmdQ, unifiedMemoryAlloc, 10, 0, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MIGRATEMEM_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
}

TEST_F(clUnifiedSharedMemoryEventTests, whenClEnqueueMemAdviseINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    void *unifiedMemoryAlloc = reinterpret_cast<void *>(0x1234);

    auto retVal = clEnqueueMemAdviseINTEL(this->pCmdQ, unifiedMemoryAlloc, 10, 0, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MEMADVISE_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
}

TEST_F(clUnifiedSharedMemoryEventTests, whenClEnqueueMemcpyINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    cl_int retVal = CL_SUCCESS;

    auto unfiedMemoryDst = clSharedMemAllocINTEL(this->context, this->context->getDevice(0u), nullptr, 400, 0, &retVal);
    auto unfiedMemorySrc = clSharedMemAllocINTEL(this->context, this->context->getDevice(0u), nullptr, 400, 0, &retVal);

    retVal = clEnqueueMemcpyINTEL(this->pCmdQ, 0, unfiedMemoryDst, unfiedMemorySrc, 400u, 0, nullptr, &event);
    EXPECT_EQ(retVal, CL_SUCCESS);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MEMCPY_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);

    clMemFreeINTEL(this->context, unfiedMemoryDst);
    clMemFreeINTEL(this->context, unfiedMemorySrc);
}

TEST_F(clUnifiedSharedMemoryEventTests, whenClEnqueueMemsetINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    cl_int retVal = CL_SUCCESS;

    auto unfiedMemorySharedAllocation = clSharedMemAllocINTEL(this->context, this->context->getDevice(0u), nullptr, 400, 0, &retVal);
    cl_int setValue = 12u;

    retVal = clEnqueueMemsetINTEL(this->pCmdQ, unfiedMemorySharedAllocation, setValue, 400u, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MEMSET_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
    clMemFreeINTEL(this->context, unfiedMemorySharedAllocation);
}

TEST_F(clUnifiedSharedMemoryEventTests, whenClEnqueueMemFillINTELIsCalledWithEventThenProperCmdTypeIsSet) {
    cl_int retVal = CL_SUCCESS;

    auto unfiedMemorySharedAllocation = clSharedMemAllocINTEL(this->context, this->context->getDevice(0u), nullptr, 400, 0, &retVal);
    cl_int setValue = 12u;

    retVal = clEnqueueMemFillINTEL(this->pCmdQ, unfiedMemorySharedAllocation, &setValue, sizeof(setValue), 400u, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    constexpr cl_command_type expectedCmd = CL_COMMAND_MEMFILL_INTEL;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
    clMemFreeINTEL(this->context, unfiedMemorySharedAllocation);
}
