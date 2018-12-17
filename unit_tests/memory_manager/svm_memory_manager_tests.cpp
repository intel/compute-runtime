/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_svm_manager.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct SVMMemoryAllocatorTest : ::testing::Test {
    SVMMemoryAllocatorTest() : memoryManager(false, false, executionEnvironment), svmManager(&memoryManager) {}
    ExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager;
    MockSVMAllocsManager svmManager;
};

TEST_F(SVMMemoryAllocatorTest, whenCreateZeroSizedSVMAllocationThenReturnNullptr) {
    auto ptr = svmManager.createSVMAlloc(0, false, false);

    EXPECT_EQ(0u, svmManager.SVMAllocs.getNumAllocs());
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(SVMMemoryAllocatorTest, whenSVMAllocationIsFreedThenCannotBeGotAgain) {
    auto ptr = svmManager.createSVMAlloc(MemoryConstants::pageSize, false, false);
    EXPECT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, svmManager.getSVMAlloc(ptr));
    EXPECT_NE(nullptr, svmManager.getSVMAlloc(ptr));
    EXPECT_EQ(1u, svmManager.SVMAllocs.getNumAllocs());
    EXPECT_FALSE(svmManager.getSVMAlloc(ptr)->isCoherent());

    svmManager.freeSVMAlloc(ptr);
    EXPECT_EQ(nullptr, svmManager.getSVMAlloc(ptr));
    EXPECT_EQ(0u, svmManager.SVMAllocs.getNumAllocs());
}

TEST_F(SVMMemoryAllocatorTest, whenGetSVMAllocationFromReturnedPointerAreaThenReturnSameAllocation) {
    auto ptr = svmManager.createSVMAlloc(MemoryConstants::pageSize, false, false);
    EXPECT_NE(ptr, nullptr);
    GraphicsAllocation *graphicsAllocation = svmManager.getSVMAlloc(ptr);
    EXPECT_NE(nullptr, graphicsAllocation);

    auto ptrInRange = ptrOffset(ptr, MemoryConstants::pageSize - 4);
    GraphicsAllocation *graphicsAllocationInRange = svmManager.getSVMAlloc(ptrInRange);
    EXPECT_NE(nullptr, graphicsAllocationInRange);

    EXPECT_EQ(graphicsAllocation, graphicsAllocationInRange);

    svmManager.freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenGetSVMAllocationFromOutsideOfReturnedPointerAreaThenDontReturnThisAllocation) {
    auto ptr = svmManager.createSVMAlloc(MemoryConstants::pageSize, false, false);
    EXPECT_NE(ptr, nullptr);
    GraphicsAllocation *graphicsAllocation = svmManager.getSVMAlloc(ptr);
    EXPECT_NE(nullptr, graphicsAllocation);

    auto ptrBefore = ptrOffset(ptr, -4);
    GraphicsAllocation *graphicsAllocationBefore = svmManager.getSVMAlloc(ptrBefore);
    EXPECT_EQ(nullptr, graphicsAllocationBefore);

    auto ptrAfter = ptrOffset(ptr, MemoryConstants::pageSize);
    GraphicsAllocation *graphicsAllocationAfter = svmManager.getSVMAlloc(ptrAfter);
    EXPECT_EQ(nullptr, graphicsAllocationAfter);

    svmManager.freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCouldNotAllocateInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    FailMemoryManager failMemoryManager(executionEnvironment);
    svmManager.memoryManager = &failMemoryManager;
    auto ptr = svmManager.createSVMAlloc(MemoryConstants::pageSize, false, false);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, svmManager.SVMAllocs.getNumAllocs());
    svmManager.freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedWhenAllocatingSvmMemoryThenDontPreferRenderCompression) {
    MockMemoryManager memoryManager64Kb(true, false, executionEnvironment);
    svmManager.memoryManager = &memoryManager64Kb;
    auto ptr = svmManager.createSVMAlloc(MemoryConstants::pageSize, false, false);
    EXPECT_FALSE(memoryManager64Kb.preferRenderCompressedFlagPassed);
    svmManager.freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedwhenAllocatingSvmMemoryThenAllocationIsIn64kbPagePool) {
    MockMemoryManager memoryManager64Kb(true, false, executionEnvironment);
    svmManager.memoryManager = &memoryManager64Kb;
    auto ptr = svmManager.createSVMAlloc(MemoryConstants::pageSize, false, false);
    EXPECT_EQ(MemoryPool::System64KBPages, svmManager.getSVMAlloc(ptr)->getMemoryPool());
    svmManager.freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, given64kbDisallowedWhenAllocatingSvmMemoryThenAllocationIsIn4kbPagePool) {
    auto ptr = svmManager.createSVMAlloc(MemoryConstants::pageSize, false, false);
    EXPECT_EQ(MemoryPool::System4KBPages, svmManager.getSVMAlloc(ptr)->getMemoryPool());
    svmManager.freeSVMAlloc(ptr);
}

TEST_F(SVMMemoryAllocatorTest, whenCoherentFlagIsPassedThenAllocationIsCoherent) {
    auto ptr = svmManager.createSVMAlloc(MemoryConstants::pageSize, true, false);
    EXPECT_TRUE(svmManager.getSVMAlloc(ptr)->isCoherent());
    svmManager.freeSVMAlloc(ptr);
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
    void *svm = svmManager.createSVMAlloc(4096, false, true);
    EXPECT_NE(nullptr, svm);

    GraphicsAllocation *svmAllocation = svmManager.getSVMAlloc(svm);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_FALSE(svmAllocation->isMemObjectsAllocationWithWritableFlags());

    svmManager.freeSVMAlloc(svm);
}
