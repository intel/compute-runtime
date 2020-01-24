/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/allocations_list.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/page_fault_manager/mock_cpu_page_fault_manager.h"
#include "runtime/api/api.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "test.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_svm_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

template <bool enableLocalMemory>
struct SVMMemoryAllocatorFixture {
    SVMMemoryAllocatorFixture() : executionEnvironment(*platformDevices) {}

    virtual void SetUp() {
        bool svmSupported = executionEnvironment.getHardwareInfo()->capabilityTable.ftrSvm;
        if (!svmSupported) {
            GTEST_SKIP();
        }
        executionEnvironment.initGmm();
        memoryManager = std::make_unique<MockMemoryManager>(false, enableLocalMemory, executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
        if (enableLocalMemory) {
            memoryManager->pageFaultManager.reset(new MockPageFaultManager);
        }
    }
    virtual void TearDown() {
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockMemoryManager> memoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
};

using SVMMemoryAllocatorTest = Test<SVMMemoryAllocatorFixture<false>>;

using SVMLocalMemoryAllocatorTest = Test<SVMMemoryAllocatorFixture<true>>;

TEST_F(SVMMemoryAllocatorTest, whenCreateZeroSizedSVMAllocationThenReturnNullptr) {
    auto ptr = svmManager->createSVMAlloc(0, 0, {});

    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(SVMMemoryAllocatorTest, whenRequestSVMAllocsThenReturnNonNullptr) {
    auto svmAllocs = svmManager->getSVMAllocs();
    EXPECT_NE(svmAllocs, nullptr);
}

TEST_F(SVMMemoryAllocatorTest, whenSVMAllocationIsFreedThenCannotBeGotAgain) {
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_NE(nullptr, ptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    EXPECT_NE(nullptr, svmData->gpuAllocation);
    svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    EXPECT_NE(nullptr, svmData->gpuAllocation);
    EXPECT_EQ(1u, svmManager->SVMAllocs.getNumAllocs());
    auto svmAllocation = svmManager->getSVMAlloc(ptr)->gpuAllocation;
    EXPECT_FALSE(svmAllocation->isCoherent());

    svmManager->freeSVMAlloc(ptr);
    EXPECT_EQ(nullptr, svmManager->getSVMAlloc(ptr));
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
}

TEST_F(SVMMemoryAllocatorTest, whenGetSVMAllocationFromReturnedPointerAreaThenReturnSameAllocation) {
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_NE(ptr, nullptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *graphicsAllocation = svmData->gpuAllocation;
    EXPECT_NE(nullptr, graphicsAllocation);

    auto ptrInRange = ptrOffset(ptr, MemoryConstants::pageSize - 4);
    svmData = svmManager->getSVMAlloc(ptrInRange);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *graphicsAllocationInRange = svmData->gpuAllocation;
    EXPECT_NE(nullptr, graphicsAllocationInRange);

    EXPECT_EQ(graphicsAllocation, graphicsAllocationInRange);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenGetSVMAllocationFromOutsideOfReturnedPointerAreaThenDontReturnThisAllocation) {
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_NE(ptr, nullptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *graphicsAllocation = svmData->gpuAllocation;
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
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCouldNotAllocateInMemoryManagerThenCreateUnifiedMemoryAllocationReturnsNullAndDoesNotChangeAllocsMap) {
    FailMemoryManager failMemoryManager(executionEnvironment);
    svmManager->memoryManager = &failMemoryManager;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    auto ptr = svmManager->createUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedWhenAllocatingSvmMemoryThenDontPreferRenderCompression) {
    MockMemoryManager memoryManager64Kb(true, false, executionEnvironment);
    svmManager->memoryManager = &memoryManager64Kb;
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_FALSE(memoryManager64Kb.preferRenderCompressedFlagPassed);
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedwhenAllocatingSvmMemoryThenAllocationIsIn64kbPagePool) {
    MockMemoryManager memoryManager64Kb(true, false, executionEnvironment);
    svmManager->memoryManager = &memoryManager64Kb;
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_EQ(MemoryPool::System64KBPages, svmManager->getSVMAlloc(ptr)->gpuAllocation->getMemoryPool());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbDisallowedWhenAllocatingSvmMemoryThenAllocationIsIn4kbPagePool) {
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_EQ(MemoryPool::System4KBPages, svmManager->getSVMAlloc(ptr)->gpuAllocation->getMemoryPool());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCoherentFlagIsPassedThenAllocationIsCoherent) {
    SVMAllocsManager::SvmAllocationProperties svmProperties;
    svmProperties.coherent = true;
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, svmProperties);
    EXPECT_TRUE(svmManager->getSVMAlloc(ptr)->gpuAllocation->isCoherent());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenDeviceAllocationIsCreatedThenItIsStoredWithWriteCombinedTypeInAllocationMap) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined = true;
    auto allocationSize = 4000u;
    auto ptr = svmManager->createUnifiedMemoryAllocation(0, 4000u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, allocation->gpuAllocation);
    EXPECT_EQ(InternalMemoryType::DEVICE_UNIFIED_MEMORY, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);
    EXPECT_EQ(allocation->gpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), allocation->gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(GraphicsAllocation::AllocationType::WRITE_COMBINED, allocation->gpuAllocation->getAllocationType());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, givenNoWriteCombinedFlagwhenDeviceAllocationIsCreatedThenItIsStoredWithProperTypeInAllocationMap) {
    if (is32bit) {
        GTEST_SKIP();
    }
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined = false;
    auto allocationSize = 4096u;
    auto ptr = svmManager->createUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, allocation->gpuAllocation);
    EXPECT_EQ(InternalMemoryType::DEVICE_UNIFIED_MEMORY, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), allocation->gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, allocation->gpuAllocation->getAllocationType());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenHostAllocationIsCreatedThenItIsStoredWithProperTypeInAllocationMap) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::HOST_UNIFIED_MEMORY;
    auto allocationSize = 4096u;
    auto ptr = svmManager->createUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, allocation->gpuAllocation);
    EXPECT_EQ(InternalMemoryType::HOST_UNIFIED_MEMORY, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), allocation->gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, allocation->gpuAllocation->getAllocationType());
    EXPECT_NE(allocation->gpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);
    EXPECT_NE(nullptr, allocation->gpuAllocation->getUnderlyingBuffer());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCouldNotAllocateInMemoryManagerThenCreateSharedUnifiedMemoryAllocationReturnsNullAndDoesNotChangeAllocsMap) {
    MockCommandQueue cmdQ;
    DebugManagerStateRestore restore;
    DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);
    FailMemoryManager failMemoryManager(executionEnvironment);
    svmManager->memoryManager = &failMemoryManager;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties, &cmdQ);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenSharedAllocationIsCreatedThenItIsStoredWithProperTypeInAllocationMap) {
    MockCommandQueue cmdQ;
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    auto allocationSize = 4096u;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, allocation->gpuAllocation);
    EXPECT_EQ(InternalMemoryType::SHARED_UNIFIED_MEMORY, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), allocation->gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, allocation->gpuAllocation->getAllocationType());
    EXPECT_NE(allocation->gpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);
    EXPECT_NE(nullptr, allocation->gpuAllocation->getUnderlyingBuffer());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenSharedAllocationIsCreatedWithDebugFlagSetThenItIsStoredWithProperTypeInAllocationMapAndHasCpuAndGpuStorage) {
    MockCommandQueue cmdQ;
    DebugManagerStateRestore restore;
    DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    auto allocationSize = 4096u;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, allocation->gpuAllocation);
    EXPECT_EQ(InternalMemoryType::SHARED_UNIFIED_MEMORY, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, 2u * MB), allocation->gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(alignUp(allocationSize, 2u * MB), allocation->cpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(GraphicsAllocation::AllocationType::SVM_GPU, allocation->gpuAllocation->getAllocationType());
    EXPECT_EQ(GraphicsAllocation::AllocationType::SVM_CPU, allocation->cpuAllocation->getAllocationType());

    EXPECT_EQ(allocation->gpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);
    EXPECT_NE(allocation->cpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);

    EXPECT_NE(nullptr, allocation->gpuAllocation->getUnderlyingBuffer());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenSharedAllocationIsCreatedWithLocalMemoryAndRegisteredPageFaultHandlerThenItIsStoredWithProperTypeInAllocationMapAndHasCpuAndGpuStorage) {
    MockCommandQueue cmdQ;
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableLocalMemory.set(1);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    auto allocationSize = 4096u;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, allocation->gpuAllocation);
    EXPECT_EQ(InternalMemoryType::SHARED_UNIFIED_MEMORY, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, 2u * MB), allocation->gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(alignUp(allocationSize, 2u * MB), allocation->cpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(GraphicsAllocation::AllocationType::SVM_GPU, allocation->gpuAllocation->getAllocationType());
    EXPECT_EQ(GraphicsAllocation::AllocationType::SVM_CPU, allocation->cpuAllocation->getAllocationType());

    EXPECT_EQ(allocation->gpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);
    EXPECT_NE(allocation->cpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);

    EXPECT_NE(nullptr, allocation->gpuAllocation->getUnderlyingBuffer());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, givenSharedAllocationsDebugFlagWhenDeviceMemoryIsAllocatedThenOneStorageIsProduced) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    auto allocationSize = 4096u;
    auto ptr = svmManager->createUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    EXPECT_EQ(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, allocation->gpuAllocation);
    EXPECT_EQ(InternalMemoryType::DEVICE_UNIFIED_MEMORY, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize64k), allocation->gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, allocation->gpuAllocation->getAllocationType());

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
    void *svm = svmManager->createSVMAlloc(0, 4096, svmProperties);
    EXPECT_NE(nullptr, svm);

    auto svmData = svmManager->getSVMAlloc(svm);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *svmAllocation = svmData->gpuAllocation;
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_FALSE(svmAllocation->isMemObjectsAllocationWithWritableFlags());

    svmManager->freeSVMAlloc(svm);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenAllocatingSvmThenExpectCpuAllocationWithPointerAndGpuAllocationWithSameGpuAddress) {
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_NE(ptr, nullptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *cpuAllocation = svmData->cpuAllocation;
    EXPECT_NE(nullptr, cpuAllocation);
    EXPECT_EQ(ptr, cpuAllocation->getUnderlyingBuffer());

    GraphicsAllocation *gpuAllocation = svmData->gpuAllocation;
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(reinterpret_cast<uint64_t>(ptr), gpuAllocation->getGpuAddress());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenGetSVMAllocationFromOutsideOfReturnedPointerAreaThenDontReturnThisAllocation) {
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_NE(ptr, nullptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *graphicsAllocation = svmData->gpuAllocation;
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
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenCouldNotAllocateGpuAllocationInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    FailMemoryManager failMemoryManager(1, executionEnvironment, true);
    svmManager->memoryManager = &failMemoryManager;
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenCouldNotReserveCpuAddressRangeInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    memoryManager->failReserveAddress = true;
    auto ptr = svmManager->createSVMAlloc(0, MemoryConstants::pageSize, {});
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
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
        bool svmSupported = executionEnvironment.getHardwareInfo()->capabilityTable.ftrSvm;
        if (!svmSupported) {
            GTEST_SKIP();
        }
        memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
        memoryManager->pageFaultManager.reset(new MockPageFaultManager);
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MemoryManagerPropertiesCheck> memoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
};

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDeviceBitfieldWithMultipleBitsSetWhenSharedUnifiedMemoryAllocationIsCreatedThenProperPropertiesArePassedToMemoryManager) {
    MockCommandQueue cmdQ;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    unifiedMemoryProperties.subdeviceBitfield = DeviceBitfield(0xf);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties, &cmdQ);

    EXPECT_TRUE(memoryManager->multiOsContextCapablePassed);
    EXPECT_FALSE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfield, memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDeviceBitfieldWithSingleBitSetWhenSharedUnifiedMemoryAllocationIsCreatedThenProperPropertiesArePassedToMemoryManager) {
    MockCommandQueue cmdQ;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    unifiedMemoryProperties.subdeviceBitfield = DeviceBitfield(0x8);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties, &cmdQ);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_FALSE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfield, memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDeviceBitfieldWithMultipleBitsSetWhenDeviceUnifiedMemoryAllocationIsCreatedThenProperPropertiesArePassedToMemoryManager) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    unifiedMemoryProperties.subdeviceBitfield = DeviceBitfield(0xf);

    auto ptr = svmManager->createUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties);

    EXPECT_TRUE(memoryManager->multiOsContextCapablePassed);
    EXPECT_TRUE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfield, memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(UnifiedMemoryManagerPropertiesTest, givenDeviceBitfieldWithSingleBitSetWhenDeviceUnifiedMemoryAllocationIsCreatedThenProperPropertiesArePassedToMemoryManager) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    unifiedMemoryProperties.subdeviceBitfield = DeviceBitfield(0x8);

    auto ptr = svmManager->createUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties);

    EXPECT_FALSE(memoryManager->multiOsContextCapablePassed);
    EXPECT_FALSE(memoryManager->multiStorageResourcePassed);
    EXPECT_EQ(unifiedMemoryProperties.subdeviceBitfield, memoryManager->subDevicesBitfieldPassed);

    svmManager->freeSVMAlloc(ptr);
}

struct ShareableUnifiedMemoryManagerPropertiesTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        bool svmSupported = executionEnvironment->getHardwareInfo()->capabilityTable.ftrSvm;
        if (!svmSupported) {
            GTEST_SKIP();
        }
        memoryManager = std::make_unique<MemoryManagerPropertiesCheck>(false, true, *executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
        memoryManager->pageFaultManager.reset(new MockPageFaultManager);
    }

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MemoryManagerPropertiesCheck> memoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
};

TEST_F(ShareableUnifiedMemoryManagerPropertiesTest, givenShareableUnifiedPropertyFlagThenShareableAllocationPropertyFlagIsSet) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    unifiedMemoryProperties.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    unifiedMemoryProperties.allocationFlags.flags.shareable = 1;

    auto ptr = svmManager->createUnifiedMemoryAllocation(0, 4096u, unifiedMemoryProperties);

    EXPECT_TRUE(memoryManager->shareablePassed);
    svmManager->freeSVMAlloc(ptr);
}

TEST(UnfiedSharedMemoryTransferCalls, givenHostUSMllocationWhenPointerIsUsedForTransferCallsThenUSMAllocationIsReused) {
    MockContext mockContext;
    cl_context clContext = &mockContext;

    auto status = CL_SUCCESS;

    auto hostMemory = clHostMemAllocINTEL(clContext, nullptr, 4096u, 0u, &status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(hostMemory);

    ASSERT_EQ(CL_SUCCESS, status);
    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    cl_device_id clDevice = mockContext.getDevice(0u);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, hostMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(1u, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, hostMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    EXPECT_EQ(2u, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, hostMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}
TEST(UnfiedSharedMemoryTransferCalls, givenDeviceUsmAllocationWhenPtrIsUsedForTransferCallsThenUsmAllocationIsReused) {
    MockContext mockContext;
    cl_context clContext = &mockContext;

    auto status = CL_SUCCESS;
    cl_device_id clDevice = mockContext.getDevice(0u);

    auto deviceMemory = clDeviceMemAllocINTEL(clContext, clDevice, nullptr, 4096u, 0u, &status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(deviceMemory);

    ASSERT_EQ(CL_SUCCESS, status);
    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, deviceMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(1u, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, deviceMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    EXPECT_EQ(2u, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, deviceMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnfiedSharedMemoryTransferCalls, givenDeviceUsmAllocationWhenPtrIsUsedForTransferCallsThenCPUPathIsNotChoosen) {
    MockContext mockContext;
    cl_context clContext = &mockContext;

    auto status = CL_SUCCESS;
    cl_device_id clDevice = mockContext.getDevice(0u);

    auto deviceMemory = clDeviceMemAllocINTEL(clContext, clDevice, nullptr, 4096u, 0u, &status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(deviceMemory);

    ASSERT_EQ(CL_SUCCESS, status);
    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 4096u, nullptr, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    auto commandQueue = clCreateCommandQueue(clContext, clDevice, 0u, &status);
    ASSERT_EQ(CL_SUCCESS, status);

    status = clEnqueueWriteBuffer(commandQueue, buffer, true, 0u, 4096u, deviceMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(1u, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, true, 0u, 4096u, deviceMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    EXPECT_EQ(2u, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, deviceMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnfiedSharedMemoryTransferCalls, givenHostUsmAllocationWhenPtrIsUsedForTransferCallsThenCPUPathIsChoosen) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(false);
    MockContext mockContext;
    cl_context clContext = &mockContext;

    if (mockContext.getDevice(0u)->getHardwareInfo().capabilityTable.clVersionSupport < 20) {
        GTEST_SKIP();
    }

    auto status = CL_SUCCESS;
    cl_device_id clDevice = mockContext.getDevice(0u);

    auto sharedMemory = clSharedMemAllocINTEL(clContext, clDevice, nullptr, 4096u, 0u, &status);
    auto svmAllocation = mockContext.getSVMAllocsManager()->getSVMAlloc(sharedMemory);

    ASSERT_EQ(CL_SUCCESS, status);
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

    EXPECT_EQ(GraphicsAllocation::objectNotUsed, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, true, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    EXPECT_EQ(GraphicsAllocation::objectNotUsed, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, sharedMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnfiedSharedMemoryTransferCalls, givenHostAllocationThatIsSmallerThenTransferRequirementsThenErrorIsReturned) {
    MockContext mockContext;
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

TEST(UnfiedSharedMemoryTransferCalls, givenSharedUSMllocationWithoutLocalMemoryWhenPointerIsUsedAsTranfserParameterThenUSMAllocationIsReused) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableLocalMemory.set(0);

    MockContext mockContext;
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

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    auto osContextId = neoQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId();

    EXPECT_EQ(1u, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    EXPECT_EQ(2u, svmAllocation->gpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, sharedMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}

TEST(UnfiedSharedMemoryTransferCalls, givenSharedUSMllocationWithLocalMemoryWhenPointerIsUsedAsTransferParameterThenUSMAllocationIsReused) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableLocalMemory.set(1);

    MockContext mockContext;
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

    EXPECT_EQ(1u, svmAllocation->cpuAllocation->getTaskCount(osContextId));

    status = clEnqueueWriteBuffer(commandQueue, buffer, false, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    auto &temporaryAllocations = neoQueue->getGpgpuCommandStreamReceiver().getTemporaryAllocations();
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());

    EXPECT_EQ(2u, svmAllocation->cpuAllocation->getTaskCount(osContextId));

    status = clEnqueueReadBuffer(commandQueue, buffer, false, 0u, 4096u, sharedMemory, 0u, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(temporaryAllocations.peekIsEmpty());
    EXPECT_EQ(3u, svmAllocation->cpuAllocation->getTaskCount(osContextId));

    status = clReleaseMemObject(buffer);
    ASSERT_EQ(CL_SUCCESS, status);
    status = clMemFreeINTEL(clContext, sharedMemory);
    ASSERT_EQ(CL_SUCCESS, status);
    clReleaseCommandQueue(commandQueue);
}
