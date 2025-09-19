/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/api/api.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"

using namespace NEO;

using SVMMemoryAllocatorTest = Test<SVMMemoryAllocatorFixture<false>>;
using SVMLocalMemoryAllocatorTest = Test<SVMMemoryAllocatorFixture<true>>;

TEST_F(SVMMemoryAllocatorTest, whenCreateZeroSizedSVMAllocationThenReturnNullptr) {
    auto ptr = svmManager->createSVMAlloc(0, {}, rootDeviceIndices, deviceBitfields);

    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(SVMMemoryAllocatorTest, whenRequestSVMAllocsThenReturnNonNullptr) {
    auto svmAllocs = svmManager->getSVMAllocs();
    EXPECT_NE(svmAllocs, nullptr);
}

using MultiDeviceSVMMemoryAllocatorTest = MultiRootDeviceWithSubDevicesFixture;

TEST_F(MultiDeviceSVMMemoryAllocatorTest, givenMultipleDevicesWhenCreatingSVMAllocThenCreateOneGraphicsAllocationPerRootDeviceIndex) {
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device1->getMemoryManager());

    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    EXPECT_NE(nullptr, ptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(1u, svmManager->svmAllocs.getNumAllocs());
    ASSERT_NE(nullptr, svmData);
    for (auto &rootDeviceIndex : context->getRootDeviceIndices()) {
        auto svmAllocation = svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
        EXPECT_NE(nullptr, svmAllocation);
        EXPECT_EQ(AllocationType::svmZeroCopy, svmAllocation->getAllocationType());
        EXPECT_FALSE(svmAllocation->isCoherent());
    }

    svmManager->freeSVMAlloc(ptr);
    EXPECT_EQ(nullptr, svmManager->getSVMAlloc(ptr));
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
}

TEST_F(SVMMemoryAllocatorTest, whenSVMAllocationIsFreedThenCannotBeGotAgain) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_NE(nullptr, ptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    EXPECT_NE(nullptr, svmData->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex));
    svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    EXPECT_NE(nullptr, svmData->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex));
    EXPECT_EQ(1u, svmManager->svmAllocs.getNumAllocs());
    auto svmAllocation = svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_FALSE(svmAllocation->isCoherent());

    svmManager->freeSVMAlloc(ptr);
    EXPECT_EQ(nullptr, svmManager->getSVMAlloc(ptr));
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
}

TEST_F(SVMMemoryAllocatorTest, givenSvmManagerWhenOperatedOnThenCorrectAllocationIsInsertedReturnedAndRemoved) {
    int data;
    size_t size = sizeof(data);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, size, AllocationType::bufferHostMemory, mockDeviceBitfield});

    NEO::SvmAllocationData svmData(mockRootDeviceIndex);
    svmData.gpuAllocations.addAllocation(allocation);
    svmData.cpuAllocation = nullptr;
    svmData.size = size;
    svmData.memoryType = InternalMemoryType::sharedUnifiedMemory;
    svmData.device = nullptr;
    auto ptr = reinterpret_cast<void *>(allocation->getGpuAddress());

    svmManager->insertSVMAlloc(svmData);
    auto svmDataTemp = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmDataTemp);
    EXPECT_NE(nullptr, svmDataTemp->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex));
    EXPECT_EQ(1u, svmManager->svmAllocs.getNumAllocs());
    auto svmAllocation = svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_FALSE(svmAllocation->isCoherent());

    svmManager->removeSVMAlloc(svmData);
    EXPECT_EQ(nullptr, svmManager->getSVMAlloc(ptr));
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());

    svmManager->memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(SVMMemoryAllocatorTest, whenGetSVMAllocationFromReturnedPointerAreaThenReturnSameAllocation) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_NE(ptr, nullptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *graphicsAllocation = svmData->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, graphicsAllocation);

    auto ptrInRange = ptrOffset(ptr, MemoryConstants::pageSize - 4);
    svmData = svmManager->getSVMAlloc(ptrInRange);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *graphicsAllocationInRange = svmData->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, graphicsAllocationInRange);

    EXPECT_EQ(graphicsAllocation, graphicsAllocationInRange);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenGetSVMAllocationFromOutsideOfReturnedPointerAreaThenDontReturnThisAllocation) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_NE(ptr, nullptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *graphicsAllocation = svmData->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, graphicsAllocation);

    auto ptrBefore = ptrOffset(ptr, -4);
    svmData = svmManager->getSVMAlloc(ptrBefore);
    EXPECT_EQ(nullptr, svmData);

    auto ptrAfter = ptrOffset(ptr, MemoryConstants::pageSize);
    svmData = svmManager->getSVMAlloc(ptrAfter);
    EXPECT_EQ(nullptr, svmData);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCouldNotAllocateInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    FailMemoryManager failMemoryManager(executionEnvironment);
    svmManager->memoryManager = &failMemoryManager;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCouldNotAllocateInMemoryManagerThenCreateUnifiedMemoryAllocationReturnsNullAndDoesNotChangeAllocsMap) {
    FailMemoryManager failMemoryManager(executionEnvironment);
    svmManager->memoryManager = &failMemoryManager;

    MockContext mockContext;
    auto device = mockContext.getDevice(0u);
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = &device->getDevice();
    auto ptr = svmManager->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedWhenAllocatingSvmMemoryThenDontPreferCompression) {
    MockMemoryManager memoryManager64Kb(true, false, executionEnvironment);
    svmManager->memoryManager = &memoryManager64Kb;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_FALSE(memoryManager64Kb.preferCompressedFlagPassed);
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedwhenAllocatingSvmMemoryThenAllocationIsIn64kbPagePool) {
    MockMemoryManager memoryManager64Kb(true, false, executionEnvironment);
    svmManager->memoryManager = &memoryManager64Kb;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_EQ(MemoryPool::system64KBPages,
              svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)->getMemoryPool());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbDisallowedWhenAllocatingSvmMemoryThenAllocationIsIn4kbPagePool) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_EQ(MemoryPool::system4KBPages,
              svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)->getMemoryPool());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCoherentFlagIsPassedThenAllocationIsCoherent) {
    SVMAllocsManager::SvmAllocationProperties svmProperties;
    svmProperties.coherent = true;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, svmProperties, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)->isCoherent());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenDeviceAllocationIsCreatedThenItIsStoredWithWriteCombinedTypeInAllocationMap) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined = true;
    auto allocationSize = 4000u;
    auto ptr = svmManager->createUnifiedMemoryAllocation(4000u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);
    EXPECT_EQ(gpuAllocation->getMemoryPool(), MemoryPool::localMemory);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(AllocationType::writeCombined, gpuAllocation->getAllocationType());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, givenNoWriteCombinedFlagwhenDeviceAllocationIsCreatedThenItIsStoredWithProperTypeInAllocationMap) {
    if (is32bit) {
        GTEST_SKIP();
    }
    MockContext mockContext;
    auto device = mockContext.getDevice(0u);
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = &device->getDevice();
    unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined = false;
    auto allocationSize = 4096u;
    auto ptr = svmManager->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(AllocationType::buffer, gpuAllocation->getAllocationType());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenHostAllocationIsCreatedThenItIsStoredWithProperTypeInAllocationMap) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocationSize = 4096u;
    auto ptr = svmManager->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(AllocationType::bufferHostMemory, gpuAllocation->getAllocationType());
    EXPECT_NE(gpuAllocation->getMemoryPool(), MemoryPool::localMemory);
    EXPECT_NE(nullptr, gpuAllocation->getUnderlyingBuffer());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCouldNotAllocateInMemoryManagerThenCreateSharedUnifiedMemoryAllocationReturnsNullAndDoesNotChangeAllocsMap) {
    MockCommandQueue cmdQ;
    DebugManagerStateRestore restore;
    debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);
    FailMemoryManager failMemoryManager(executionEnvironment);
    svmManager->memoryManager = &failMemoryManager;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenSharedAllocationIsCreatedThenItIsStoredWithProperTypeInAllocationMap) {
    MockCommandQueue cmdQ;
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocationSize = 4096u;

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(InternalMemoryType::sharedUnifiedMemory, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(AllocationType::bufferHostMemory, gpuAllocation->getAllocationType());
    EXPECT_NE(gpuAllocation->getMemoryPool(), MemoryPool::localMemory);
    EXPECT_NE(nullptr, gpuAllocation->getUnderlyingBuffer());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenSharedAllocationIsCreatedWithDebugFlagSetThenItIsStoredWithProperTypeInAllocationMapAndHasCpuAndGpuStorage) {
    MockCommandQueue cmdQ;
    MockContext mockContext;
    DebugManagerStateRestore restore;
    debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);
    auto device = mockContext.getDevice(0u);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = &device->getDevice();
    auto allocationSize = 4096u;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);
    auto alignment = executionEnvironment.rootDeviceEnvironments[0]->getProductHelper().getSvmCpuAlignment();
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(InternalMemoryType::sharedUnifiedMemory, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);
    EXPECT_EQ(mockContext.getDevice(0u), allocation->device->getSpecializedDevice<ClDevice>());

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(alignUp(allocationSize, alignment), allocation->cpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(AllocationType::svmGpu, gpuAllocation->getAllocationType());
    EXPECT_EQ(AllocationType::svmCpu, allocation->cpuAllocation->getAllocationType());

    EXPECT_EQ(gpuAllocation->getMemoryPool(), MemoryPool::localMemory);
    EXPECT_NE(allocation->cpuAllocation->getMemoryPool(), MemoryPool::localMemory);

    EXPECT_NE(nullptr, gpuAllocation->getUnderlyingBuffer());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenSharedAllocationIsCreatedWithLocalMemoryAndRegisteredPageFaultHandlerThenItIsStoredWithProperTypeInAllocationMapAndHasCpuAndGpuStorage) {
    MockCommandQueue cmdQ;
    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(1);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocationSize = 4096u;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);
    auto alignment = executionEnvironment.rootDeviceEnvironments[0]->getProductHelper().getSvmCpuAlignment();
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(InternalMemoryType::sharedUnifiedMemory, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(alignUp(allocationSize, alignment), allocation->cpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(AllocationType::svmGpu, gpuAllocation->getAllocationType());
    EXPECT_EQ(AllocationType::svmCpu, allocation->cpuAllocation->getAllocationType());

    EXPECT_EQ(gpuAllocation->getMemoryPool(), MemoryPool::localMemory);
    EXPECT_NE(allocation->cpuAllocation->getMemoryPool(), MemoryPool::localMemory);

    EXPECT_NE(nullptr, gpuAllocation->getUnderlyingBuffer());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, givenSharedAllocationsDebugFlagWhenDeviceMemoryIsAllocatedThenOneStorageIsProduced) {
    DebugManagerStateRestore restore;
    debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);

    MockContext mockContext;
    auto device = mockContext.getDevice(0u);
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = &device->getDevice();
    auto allocationSize = 4096u;
    auto ptr = svmManager->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(AllocationType::buffer, gpuAllocation->getAllocationType());

    svmManager->freeSVMAlloc(ptr);
}

TEST(SvmAllocationPropertiesTests, givenDifferentMemFlagsWhenGettingSvmAllocationPropertiesThenPropertiesAreCorrectlySet) {
    SVMAllocsManager::SvmAllocationProperties allocationProperties = MemObjHelper::getSvmAllocationProperties(0);
    EXPECT_FALSE(allocationProperties.coherent);
    EXPECT_FALSE(allocationProperties.hostPtrReadOnly);
    EXPECT_FALSE(allocationProperties.readOnly);

    allocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_SVM_FINE_GRAIN_BUFFER);
    EXPECT_TRUE(allocationProperties.coherent);
    EXPECT_FALSE(allocationProperties.hostPtrReadOnly);
    EXPECT_FALSE(allocationProperties.readOnly);

    allocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_HOST_READ_ONLY);
    EXPECT_FALSE(allocationProperties.coherent);
    EXPECT_TRUE(allocationProperties.hostPtrReadOnly);
    EXPECT_FALSE(allocationProperties.readOnly);

    allocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_HOST_NO_ACCESS);
    EXPECT_FALSE(allocationProperties.coherent);
    EXPECT_TRUE(allocationProperties.hostPtrReadOnly);
    EXPECT_FALSE(allocationProperties.readOnly);

    allocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_ONLY);
    EXPECT_FALSE(allocationProperties.coherent);
    EXPECT_FALSE(allocationProperties.hostPtrReadOnly);
    EXPECT_TRUE(allocationProperties.readOnly);

    allocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_HOST_READ_ONLY);
    EXPECT_TRUE(allocationProperties.coherent);
    EXPECT_TRUE(allocationProperties.hostPtrReadOnly);
    EXPECT_FALSE(allocationProperties.readOnly);

    allocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_READ_ONLY);
    EXPECT_TRUE(allocationProperties.coherent);
    EXPECT_FALSE(allocationProperties.hostPtrReadOnly);
    EXPECT_TRUE(allocationProperties.readOnly);
}

TEST_F(SVMMemoryAllocatorTest, whenReadOnlySvmAllocationCreatedThenGraphicsAllocationHasWriteableFlagFalse) {
    SVMAllocsManager::SvmAllocationProperties svmProperties;
    svmProperties.readOnly = true;
    void *svm = svmManager->createSVMAlloc(4096, svmProperties, rootDeviceIndices, deviceBitfields);
    EXPECT_NE(nullptr, svm);

    auto svmData = svmManager->getSVMAlloc(svm);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *svmAllocation = svmData->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_FALSE(svmAllocation->isMemObjectsAllocationWithWritableFlags());

    svmManager->freeSVMAlloc(svm);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenAllocatingSvmThenExpectCpuAllocationWithPointerAndGpuAllocationWithSameGpuAddress) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_NE(ptr, nullptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *cpuAllocation = svmData->cpuAllocation;
    EXPECT_NE(nullptr, cpuAllocation);
    EXPECT_EQ(ptr, cpuAllocation->getUnderlyingBuffer());

    GraphicsAllocation *gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(reinterpret_cast<uint64_t>(ptr), gpuAllocation->getGpuAddress());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenGetSVMAllocationFromOutsideOfReturnedPointerAreaThenDontReturnThisAllocation) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_NE(ptr, nullptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *graphicsAllocation = svmData->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, graphicsAllocation);

    auto ptrBefore = ptrOffset(ptr, -4);
    svmData = svmManager->getSVMAlloc(ptrBefore);
    EXPECT_EQ(nullptr, svmData);

    auto ptrAfter = ptrOffset(ptr, MemoryConstants::pageSize);
    svmData = svmManager->getSVMAlloc(ptrAfter);
    EXPECT_EQ(nullptr, svmData);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenCouldNotAllocateCpuAllocationInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    FailMemoryManager failMemoryManager(false, true, executionEnvironment);
    svmManager->memoryManager = &failMemoryManager;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenCouldNotAllocateGpuAllocationInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    FailMemoryManager failMemoryManager(1, executionEnvironment, true);
    svmManager->memoryManager = &failMemoryManager;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenCouldNotReserveCpuAddressRangeInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    memoryManager->failReserveAddress = true;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, rootDeviceIndices, deviceBitfields);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->svmAllocs.getNumAllocs());
}

struct MemoryManagerPropertiesCheck : public MockMemoryManager {
    using MockMemoryManager::MockMemoryManager;

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        return this->allocateGraphicsMemoryWithProperties(properties, nullptr);
    }

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) override {
        this->multiOsContextCapablePassed = properties.flags.multiOsContextCapable;
        this->multiStorageResourcePassed = properties.multiStorageResource;
        this->subDevicesBitfieldPassed = properties.subDevicesBitfield;
        this->shareablePassed = properties.flags.shareable;
        return MockMemoryManager::allocateGraphicsMemoryWithProperties(properties, ptr);
    }

    bool multiOsContextCapablePassed;
    bool multiStorageResourcePassed;
    bool shareablePassed;
    DeviceBitfield subDevicesBitfieldPassed;
};

struct UnifiedMemoryManagerPropertiesTest : public ::testing::Test {
    void SetUp() override {
        memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
        memoryManager->pageFaultManager.reset(new MockPageFaultManager);
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MemoryManagerPropertiesCheck> memoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
};

TEST(UnifiedMemoryTest, givenDeviceBitfieldWithMultipleBitsSetWhenSharedUnifiedMemoryAllocationIsCreatedThenProperPropertiesArePassedToMemoryManager) {
    MockCommandQueue cmdQ;
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(4);
    MockExecutionEnvironment executionEnvironment;
    auto memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, executionEnvironment);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
    memoryManager->pageFaultManager.reset(new MockPageFaultManager);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0xf)}};

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_TRUE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDeviceBitfieldWithSingleBitSetWhenSharedUnifiedMemoryAllocationIsCreatedThenProperPropertiesArePassedToMemoryManager) {
    MockCommandQueue cmdQ;
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x8)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_FALSE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST(UnifiedMemoryTest, givenDeviceBitfieldWithMultipleBitsSetWhenMultiOsContextFlagTrueThenProperPropertiesArePassedToMemoryManager) {
    MockCommandQueue cmdQ;
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(4);
    MockExecutionEnvironment executionEnvironment;
    auto memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, executionEnvironment);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
    memoryManager->pageFaultManager = std::make_unique<MockPageFaultManager>();

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0xf)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_TRUE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST(UnifiedMemoryTest, givenDeviceBitfieldWithMultipleBitsSetWhenMultiOsContextFlagTrueAndDeviceMemoryThenProperPropertiesArePassedToMemoryManager) {
    MockContext mockContext;
    auto device = mockContext.getDevice(0u);
    MockExecutionEnvironment executionEnvironment;
    auto memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, executionEnvironment);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0xf)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = &device->getDevice();
    auto ptr = svmManager->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_TRUE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST(UnifiedMemoryTest, givenDeviceBitfieldWithSingleBitsSetWhenMultiOsContextFlagTrueThenProperPropertiesArePassedToMemoryManager) {
    MockCommandQueue cmdQ;
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(1);
    MockExecutionEnvironment executionEnvironment;
    auto memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, executionEnvironment);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
    memoryManager->pageFaultManager = std::make_unique<MockPageFaultManager>();

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x1)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_FALSE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST(UnifiedMemoryTest, givenInternalAllocationsWhenTheyArePreparedForFreeingThenProperTaskCountIsAssigned) {
    MockCommandQueue cmdQ;
    MockDevice device;
    MockExecutionEnvironment executionEnvironment;
    auto memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, executionEnvironment);
    auto unifiedMemoryManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
    memoryManager->pageFaultManager = std::make_unique<MockPageFaultManager>();

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x1)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = unifiedMemoryManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);
    ASSERT_NE(nullptr, ptr);

    auto graphicsAllocation = unifiedMemoryManager->getSVMAlloc(ptr);
    auto &commandStreamReceiver = device.getGpgpuCommandStreamReceiver();
    unifiedMemoryManager->makeIndirectAllocationsResident(commandStreamReceiver, 1u);
    unifiedMemoryManager->makeIndirectAllocationsResident(commandStreamReceiver, 124u);

    EXPECT_EQ(1u, graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getTaskCount(commandStreamReceiver.getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getResidencyTaskCount(commandStreamReceiver.getOsContext().getContextId()));

    auto allocationData = unifiedMemoryManager->getSVMAlloc(ptr);

    unifiedMemoryManager->prepareIndirectAllocationForDestruction(allocationData, false);
    EXPECT_EQ(124u, graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getTaskCount(commandStreamReceiver.getOsContext().getContextId()));
    EXPECT_EQ(124u, graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getResidencyTaskCount(commandStreamReceiver.getOsContext().getContextId()));

    graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->updateTaskCount(1u, commandStreamReceiver.getOsContext().getContextId());
    graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, commandStreamReceiver.getOsContext().getContextId());
    unifiedMemoryManager->prepareIndirectAllocationForDestruction(allocationData, true);
    EXPECT_EQ(1u, graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getTaskCount(commandStreamReceiver.getOsContext().getContextId()));
    EXPECT_EQ(1u, graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getResidencyTaskCount(commandStreamReceiver.getOsContext().getContextId()));

    unifiedMemoryManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDeviceBitfieldWithSingleBitSetWhenDeviceUnifiedMemoryAllocationIsCreatedThenProperPropertiesArePassedToMemoryManager) {
    MockCommandQueue cmdQ;
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x8)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_FALSE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDeviceBitfieldWithMultiDeviceBitSetWhenMultiOsContextFlagTrueThenProperPropertiesArePassedToMemoryManager) {
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0xF)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createUnifiedAllocationWithDeviceStorage(10 * MemoryConstants::pageSize64k, {}, unifiedMemoryProperties);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_TRUE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDeviceBitfieldWithSingleDeviceBitSetWhenMultiOsContextFlagTrueThenProperPropertiesArePassedToMemoryManager) {
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x1)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createUnifiedAllocationWithDeviceStorage(10 * MemoryConstants::pageSize64k, {}, unifiedMemoryProperties);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_FALSE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDebugFlagSetWhenCreatingAllocationThenSet2MbAlignment) {
    DebugManagerStateRestore restore;
    debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x1)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createUnifiedAllocationWithDeviceStorage(MemoryConstants::pageSize64k, {}, unifiedMemoryProperties);

    EXPECT_TRUE(isAligned<MemoryConstants::pageSize2M>(ptr));

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest,
       givenSvmManagerMultiOsContextSupportFlagTrueWhenRootDeviceIsSingleThenMultiStorageFlagFalse) {
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x1)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_FALSE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest,
       givenSizeGreaterThan2mbWhenDiscretGpuAndCreateHostUSMThenAlignSizeAndVATo2mb) {
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x1)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createHostUnifiedMemoryAllocation(4 * MemoryConstants::pageSize2M + MemoryConstants::pageSize64k, unifiedMemoryProperties);

    auto allocation = svmManager->getSVMAlloc(ptr);

    if (executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(4 * MemoryConstants::pageSize2M + MemoryConstants::pageSize64k, allocation->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize());
        EXPECT_TRUE(isAligned(allocation->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress(), MemoryConstants::pageSize));
    } else {
        EXPECT_EQ(5 * MemoryConstants::pageSize2M, allocation->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize());
        EXPECT_TRUE(isAligned(allocation->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress(), MemoryConstants::pageSize2M));
    }

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest,
       givenCALAndSizeGreaterThan2mbWhenDiscretGpuAndCreateHostUSMThenDoNotAlignSizeAndVATo2mb) {
    DebugManagerStateRestore restorer;
    debugManager.flags.NEO_CAL_ENABLED.set(1);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x1)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createHostUnifiedMemoryAllocation(4 * MemoryConstants::pageSize2M + MemoryConstants::pageSize64k, unifiedMemoryProperties);

    auto allocation = svmManager->getSVMAlloc(ptr);

    EXPECT_EQ(4 * MemoryConstants::pageSize2M + MemoryConstants::pageSize64k, allocation->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_TRUE(isAligned(allocation->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress(), MemoryConstants::pageSize));

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest,
       given1ByteAsAllocationSizeWhenHostMemAllocIsCreatedItIsAlignedTo4k) {
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0x1)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);

    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(MemoryConstants::pageSize, allocation->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest,
       givenSvmManagerMultiOsContextSupportFlagTrueWhenRootDeviceIsMultiThenMultiStorageFlagTrue) {
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, DeviceBitfield(0xF)}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_TRUE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfields.at(mockRootDeviceIndex), memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

struct ShareableUnifiedMemoryManagerPropertiesTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = platform()->peekExecutionEnvironment();
        memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, *executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
        memoryManager->pageFaultManager.reset(new MockPageFaultManager);
    }

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MemoryManagerPropertiesCheck> memoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
};

TEST_F(ShareableUnifiedMemoryManagerPropertiesTest, givenShareableUnifiedPropertyFlagThenShareableAllocationPropertyFlagIsSet) {
    MockContext mockContext;
    auto device = mockContext.getDevice(0u);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = &device->getDevice();
    unifiedMemoryProperties.allocationFlags.flags.shareable = 1;

    auto ptr = svmManager->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);

    EXPECT_TRUE(memoryManager->shareablePassed);
    svmManager->freeSVMAlloc(ptr);
}

TEST(UnifiedSharedMemoryTransferCalls, givenHostUsmAllocationWhenPointerIsUsedForTransferCallsThenUSMAllocationIsReused) {
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    MockContext mockContext(deviceFactory.rootDevices[0]);

    cl_context clContext = &mockContext;

    auto status = CL_INVALID_PLATFORM;

    auto hostMemory = clHostMemAllocINTEL(clContext, nullptr, 4096u, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(hostMemory);
    auto gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext.getDevice(0)->getRootDeviceIndex());

    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    cl_device_id clDevice = mockContext.getDevice(0u);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, hostMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto heaplessStateInit = neoQueue->getHeaplessStateInitEnabled();

    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(heaplessStateInit ? 2u : 1u, gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, hostMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    EXPECT_EQ(heaplessStateInit ? 3u : 2u, gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, hostMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnifiedSharedMemoryTransferCalls, givenDeviceUsmAllocationWhenPtrIsUsedForTransferCallsThenUsmAllocationIsReused) {
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    MockContext mockContext(deviceFactory.rootDevices[0]);
    cl_context clContext = &mockContext;

    auto status = CL_INVALID_PLATFORM;
    cl_device_id clDevice = mockContext.getDevice(0u);

    auto deviceMemory = clDeviceMemAllocINTEL(clContext, clDevice, nullptr, 4096u, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(deviceMemory);
    auto gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext.getDevice(0)->getRootDeviceIndex());

    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, deviceMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto heaplessStateInit = neoQueue->getHeaplessStateInitEnabled();

    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(heaplessStateInit ? 2u : 1u, gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, deviceMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    EXPECT_EQ(heaplessStateInit ? 3u : 2u, gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, deviceMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnifiedSharedMemoryTransferCalls, givenDeviceUsmAllocationWhenPtrIsUsedForTransferCallsThenCPUPathIsNotChoosen) {
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    MockContext mockContext(deviceFactory.rootDevices[0]);
    cl_context clContext = &mockContext;

    auto status = CL_INVALID_PLATFORM;
    cl_device_id clDevice = mockContext.getDevice(0u);

    auto deviceMemory = clDeviceMemAllocINTEL(clContext, clDevice, nullptr, 4096u, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(deviceMemory);
    auto gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext.getDevice(0)->getRootDeviceIndex());

    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, true, 0u, 4096u, deviceMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto heaplessStateInit = neoQueue->getHeaplessStateInitEnabled();

    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(heaplessStateInit ? 2u : 1u, gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, true, 0u, 4096u, deviceMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    EXPECT_EQ(heaplessStateInit ? 3u : 2u, gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, deviceMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnifiedSharedMemoryTransferCalls, givenHostUsmAllocationWhenPtrIsUsedForTransferCallsThenCPUPathIsChoosen) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(false);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    MockContext mockContext(deviceFactory.rootDevices[0]);
    cl_context clContext = &mockContext;

    if (mockContext.getDevice(0u)->getHardwareInfo().capabilityTable.supportsOcl21Features == false) {
        GTEST_SKIP();
    }

    auto status = CL_INVALID_PLATFORM;
    cl_device_id clDevice = mockContext.getDevice(0u);

    auto sharedMemory = clSharedMemAllocINTEL(clContext, clDevice, nullptr, 4096u, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(sharedMemory);
    auto gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext.getDevice(0)->getRootDeviceIndex());

    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, true, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(GraphicsAllocation::objectNotUsed, gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, true, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    EXPECT_EQ(GraphicsAllocation::objectNotUsed, gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, sharedMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnifiedSharedMemoryTransferCalls, givenHostAllocationThatIsSmallerThenTransferRequirementsThenErrorIsReturned) {
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    MockContext mockContext(deviceFactory.rootDevices[0]);

    cl_context clContext = &mockContext;

    auto status = CL_SUCCESS;

    auto hostMemory = clHostMemAllocINTEL(clContext, nullptr, 4u, 0u, &status);

    ASSERT_EQ(CL_SUCCESS, status);
    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    cl_device_id clDevice = mockContext.getDevice(0u);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, hostMemory, 0u, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, status);

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, hostMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_INVALID_OPERATION, status);

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, hostMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnifiedSharedMemoryTransferCalls, givenSharedUsmAllocationWithoutLocalMemoryWhenPointerIsUsedAsTranfserParameterThenUSMAllocationIsReused) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(0);

    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    MockContext mockContext(deviceFactory.rootDevices[0]);
    cl_context clContext = &mockContext;
    cl_device_id clDevice = mockContext.getDevice(0u);

    auto status = CL_INVALID_PLATFORM;

    auto sharedMemory = clSharedMemAllocINTEL(clContext, clDevice, nullptr, 4096u, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(sharedMemory);
    auto gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext.getDevice(0)->getRootDeviceIndex());

    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto heaplessStateInit = neoQueue->getHeaplessStateInitEnabled();

    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(heaplessStateInit ? 2u : 1u, gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    EXPECT_EQ(heaplessStateInit ? 3u : 2u, gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, sharedMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnifiedSharedMemoryTransferCalls, givenSharedUsmAllocationWithLocalMemoryWhenPointerIsUsedAsTransferParameterThenUSMAllocationIsReused) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(1);

    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    MockContext mockContext(deviceFactory.rootDevices[0]);
    cl_context clContext = &mockContext;
    cl_device_id clDevice = mockContext.getDevice(0u);

    auto status = CL_SUCCESS;

    auto sharedMemory = clSharedMemAllocINTEL(clContext, clDevice, nullptr, 4096u, 0u, &status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(sharedMemory);

    ASSERT_EQ(CL_SUCCESS, status);
    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();
    auto heaplessStateInit = neoQueue->getHeaplessStateInitEnabled();

    EXPECT_EQ(GraphicsAllocation::objectNotUsed, svmAllocation->cpuAllocation->getTaskCount(osContextId));

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());

    EXPECT_EQ(heaplessStateInit ? 2u : 1u, svmAllocation->cpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    EXPECT_EQ(heaplessStateInit ? 3u : 2u, svmAllocation->cpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, sharedMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

class UnifiedSharedMemoryHWTest : public testing::Test {
  public:
    void SetUp() override {
        mockContext = std::make_unique<MockContext>(deviceFactory.rootDevices[0]);
    }

    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    std::unique_ptr<MockContext> mockContext;
};

template <typename GfxFamily>
class TestCommandQueueHw : public CommandQueueHw<GfxFamily> {
    typedef CommandQueueHw<GfxFamily> BaseClass;

  public:
    TestCommandQueueHw(Context *context, ClDevice *device, cl_queue_properties *properties) : BaseClass(context, device, properties, false){};
    void *srcPtr = nullptr;
    void *dstPtr = nullptr;
    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &multiDispatchInfo) override {
        auto svmEntrySrc = this->getContext().getSVMAllocsManager()->getSVMAlloc(multiDispatchInfo.peekBuiltinOpParams().srcPtr);
        if (svmEntrySrc) {
            srcPtr = multiDispatchInfo.peekBuiltinOpParams().srcPtr;
        } else {
            srcPtr = multiDispatchInfo.peekBuiltinOpParams().transferAllocation->getUnderlyingBuffer();
        }
        auto svmEntryDst = this->getContext().getSVMAllocsManager()->getSVMAlloc(multiDispatchInfo.peekBuiltinOpParams().dstPtr);
        if (svmEntryDst) {
            dstPtr = multiDispatchInfo.peekBuiltinOpParams().dstPtr;
        } else {
            dstPtr = multiDispatchInfo.peekBuiltinOpParams().transferAllocation->getUnderlyingBuffer();
        }
    }
};

HWTEST_F(UnifiedSharedMemoryHWTest, givenDeviceUsmAllocationWhenWriteBufferThenCpuPtrIsNotUsed) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1,
                                                                      mockContext->getRootDeviceIndices(), mockContext->getDeviceBitfields());
    unifiedMemoryProperties.device = &mockContext->getDevice(0)->getDevice();
    auto deviceMemory = mockContext->getSVMAllocsManager()->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    auto svmAllocation = mockContext->getSVMAllocsManager()->getSVMAlloc(deviceMemory);
    GraphicsAllocation *gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex());

    char *cpuPtr = static_cast<char *>(gpuAllocation->getUnderlyingBuffer());
    auto gpuAddress = gpuAllocation->getGpuAddress();
    void *gpuPtr = reinterpret_cast<void *>(gpuAddress);
    auto gmmHelper = mockContext->getDevice(0)->getGmmHelper();

    cl_mem_flags flags = 0;
    auto status = CL_INVALID_PLATFORM;
    auto buffer = Buffer::create(mockContext.get(), flags, 4096u, nullptr, status);
    ASSERT_EQ(CL_SUCCESS, status);

    TestCommandQueueHw<FamilyType> myCmdQ(mockContext.get(), mockContext->getDevice(0u), 0);
    myCmdQ.enqueueWriteBuffer(buffer, false, 0u, 4096u, deviceMemory, nullptr, 0u, nullptr, nullptr);
    EXPECT_EQ(gpuPtr, myCmdQ.srcPtr);

    auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
    gpuAllocation->setCpuPtrAndGpuAddress(cpuPtr, canonizedGpuAddress);
    delete buffer;
    clMemFreeINTEL(mockContext.get(), deviceMemory);
}

HWTEST_F(UnifiedSharedMemoryHWTest, givenDeviceUsmAllocationWhenReadBufferThenCpuPtrIsNotUsed) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1,
                                                                      mockContext->getRootDeviceIndices(), mockContext->getDeviceBitfields());
    unifiedMemoryProperties.device = &mockContext->getDevice(0)->getDevice();
    auto deviceMemory = mockContext->getSVMAllocsManager()->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    auto svmAllocation = mockContext->getSVMAllocsManager()->getSVMAlloc(deviceMemory);
    GraphicsAllocation *gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex());

    char *cpuPtr = static_cast<char *>(gpuAllocation->getUnderlyingBuffer());
    auto gpuAddress = gpuAllocation->getGpuAddress();
    void *gpuPtr = reinterpret_cast<void *>(gpuAddress);
    auto gmmHelper = mockContext->getDevice(0)->getGmmHelper();

    cl_mem_flags flags = 0;
    auto status = CL_INVALID_PLATFORM;
    auto buffer = Buffer::create(mockContext.get(), flags, 4096u, nullptr, status);
    ASSERT_EQ(CL_SUCCESS, status);

    TestCommandQueueHw<FamilyType> myCmdQ(mockContext.get(), mockContext->getDevice(0u), 0);
    myCmdQ.enqueueReadBuffer(buffer, false, 0u, 4096u, deviceMemory, nullptr, 0u, nullptr, nullptr);
    EXPECT_EQ(gpuPtr, myCmdQ.dstPtr);

    auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
    gpuAllocation->setCpuPtrAndGpuAddress(cpuPtr, canonizedGpuAddress);
    delete buffer;
    clMemFreeINTEL(mockContext.get(), deviceMemory);
}

HWTEST_F(UnifiedSharedMemoryHWTest, givenSharedUsmAllocationWhenWriteBufferThenCpuPtrIsNotUsed) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1,
                                                                      mockContext->getRootDeviceIndices(), mockContext->getDeviceBitfields());
    auto sharedMemory = mockContext->getSVMAllocsManager()->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    auto svmAllocation = mockContext->getSVMAllocsManager()->getSVMAlloc(sharedMemory);
    GraphicsAllocation *gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex());

    char *cpuPtr = static_cast<char *>(gpuAllocation->getUnderlyingBuffer());
    auto gpuAddress = gpuAllocation->getGpuAddress();
    void *gpuPtr = reinterpret_cast<void *>(gpuAddress);
    auto gmmHelper = mockContext->getDevice(0)->getGmmHelper();

    cl_mem_flags flags = 0;
    auto status = CL_INVALID_PLATFORM;
    auto buffer = Buffer::create(mockContext.get(), flags, 4096u, nullptr, status);
    ASSERT_EQ(CL_SUCCESS, status);

    TestCommandQueueHw<FamilyType> myCmdQ(mockContext.get(), mockContext->getDevice(0u), 0);
    myCmdQ.enqueueWriteBuffer(buffer, false, 0u, 4096u, sharedMemory, nullptr, 0u, nullptr, nullptr);
    EXPECT_EQ(gpuPtr, myCmdQ.srcPtr);

    auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
    gpuAllocation->setCpuPtrAndGpuAddress(cpuPtr, canonizedGpuAddress);
    delete buffer;
    clMemFreeINTEL(mockContext.get(), sharedMemory);
}

HWTEST_F(UnifiedSharedMemoryHWTest, givenSharedUsmAllocationWhenReadBufferThenCpuPtrIsNotUsed) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1,
                                                                      mockContext->getRootDeviceIndices(), mockContext->getDeviceBitfields());
    auto sharedMemory = mockContext->getSVMAllocsManager()->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    auto svmAllocation = mockContext->getSVMAllocsManager()->getSVMAlloc(sharedMemory);
    GraphicsAllocation *gpuAllocation = svmAllocation->gpuAllocations.getGraphicsAllocation(mockContext->getDevice(0)->getRootDeviceIndex());

    char *cpuPtr = static_cast<char *>(gpuAllocation->getUnderlyingBuffer());
    auto gpuAddress = gpuAllocation->getGpuAddress();
    void *gpuPtr = reinterpret_cast<void *>(gpuAddress);

    cl_mem_flags flags = 0;
    auto status = CL_INVALID_PLATFORM;
    auto buffer = Buffer::create(mockContext.get(), flags, 4096u, nullptr, status);
    ASSERT_EQ(CL_SUCCESS, status);

    TestCommandQueueHw<FamilyType> myCmdQ(mockContext.get(), mockContext->getDevice(0u), 0);
    myCmdQ.enqueueReadBuffer(buffer, false, 0u, 4096u, sharedMemory, nullptr, 0u, nullptr, nullptr);
    EXPECT_EQ(gpuPtr, myCmdQ.dstPtr);

    auto gmmHelper = mockContext->getDevice(0)->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
    gpuAllocation->setCpuPtrAndGpuAddress(cpuPtr, canonizedGpuAddress);
    delete buffer;
    clMemFreeINTEL(mockContext.get(), sharedMemory);
}

TEST(UnifiedMemoryManagerTest, givenEnableStatelessCompressionWhenDeviceAllocationIsCreatedThenAllocationTypeIsBufferCompressed) {
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    cl_int retVal = CL_SUCCESS;

    for (auto enable : {-1, 0, 1}) {
        UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
        MockContext mockContext(deviceFactory.rootDevices[0]);

        auto device = mockContext.getDevice(0u);
        auto allocationsManager = mockContext.getSVMAllocsManager();

        debugManager.flags.EnableStatelessCompression.set(enable);

        auto deviceMemAllocPtr = clDeviceMemAllocINTEL(&mockContext, device, nullptr, 2048, 0, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, deviceMemAllocPtr);

        auto deviceMemAlloc = allocationsManager->getSVMAllocs()->get(deviceMemAllocPtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, deviceMemAlloc);
        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto isCompressionEnabled = gfxCoreHelper.usmCompressionSupported(device->getHardwareInfo());
        EXPECT_EQ(enable > 0 || isCompressionEnabled, deviceMemAlloc->isCompressionEnabled());

        retVal = clMemFreeINTEL(&mockContext, deviceMemAllocPtr);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}
