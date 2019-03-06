/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_svm_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

template <bool enableLocalMemory>
struct SVMMemoryAllocatorFixture {
    SVMMemoryAllocatorFixture() : executionEnvironment(*platformDevices) {}

    virtual void SetUp() {
        memoryManager = std::make_unique<MockMemoryManager>(false, enableLocalMemory, executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
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
    auto ptr = svmManager->createSVMAlloc(0, 0);

    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(SVMMemoryAllocatorTest, whenSVMAllocationIsFreedThenCannotBeGotAgain) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
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
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
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
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
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
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedWhenAllocatingSvmMemoryThenDontPreferRenderCompression) {
    MockMemoryManager memoryManager64Kb(true, false, executionEnvironment);
    svmManager->memoryManager = &memoryManager64Kb;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
    EXPECT_FALSE(memoryManager64Kb.preferRenderCompressedFlagPassed);
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedwhenAllocatingSvmMemoryThenAllocationIsIn64kbPagePool) {
    MockMemoryManager memoryManager64Kb(true, false, executionEnvironment);
    svmManager->memoryManager = &memoryManager64Kb;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
    EXPECT_EQ(MemoryPool::System64KBPages, svmManager->getSVMAlloc(ptr)->gpuAllocation->getMemoryPool());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbDisallowedWhenAllocatingSvmMemoryThenAllocationIsIn4kbPagePool) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
    EXPECT_EQ(MemoryPool::System4KBPages, svmManager->getSVMAlloc(ptr)->gpuAllocation->getMemoryPool());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCoherentFlagIsPassedThenAllocationIsCoherent) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, CL_MEM_SVM_FINE_GRAIN_BUFFER);
    EXPECT_TRUE(svmManager->getSVMAlloc(ptr)->gpuAllocation->isCoherent());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenReadOnlyFlagIsPresentThenReturnTrue) {
    EXPECT_TRUE(SVMAllocsManager::memFlagIsReadOnly(CL_MEM_READ_ONLY));
    EXPECT_TRUE(SVMAllocsManager::memFlagIsReadOnly(CL_MEM_HOST_READ_ONLY));
    EXPECT_TRUE(SVMAllocsManager::memFlagIsReadOnly(CL_MEM_READ_ONLY));
}

TEST_F(SVMMemoryAllocatorTest, whenNoReadOnlyFlagIsPresentThenReturnFalse) {
    EXPECT_FALSE(SVMAllocsManager::memFlagIsReadOnly(CL_MEM_READ_WRITE));
    EXPECT_FALSE(SVMAllocsManager::memFlagIsReadOnly(CL_MEM_WRITE_ONLY));
}

TEST_F(SVMMemoryAllocatorTest, whenReadOnlySvmAllocationCreatedThenGraphicsAllocationHasWriteableFlagFalse) {
    void *svm = svmManager->createSVMAlloc(4096, CL_MEM_READ_ONLY);
    EXPECT_NE(nullptr, svm);

    auto svmData = svmManager->getSVMAlloc(svm);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *svmAllocation = svmData->gpuAllocation;
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_FALSE(svmAllocation->isMemObjectsAllocationWithWritableFlags());

    svmManager->freeSVMAlloc(svm);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenAllocatingSvmThenExpectCpuAllocationWithPointerAndGpuAllocationWithSameGpuAddress) {
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
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
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
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
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenCouldNotAllocateGpuAllocationInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    FailMemoryManager failMemoryManager(1, executionEnvironment, true);
    svmManager->memoryManager = &failMemoryManager;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenCouldNotReserveCpuAddressRangeInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    memoryManager->failReserveAddress = true;
    auto ptr = svmManager->createSVMAlloc(MemoryConstants::pageSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager->SVMAllocs.getNumAllocs());
}
