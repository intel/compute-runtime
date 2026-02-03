/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

struct GraphicsAllocationViewTest : public ::testing::Test {
    void SetUp() override {
        parentAllocation = std::make_unique<MockGraphicsAllocation>(
            0u, 1u, AllocationType::buffer, cpuPtr, gpuAddress, 0u, allocationSize, MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    }

    const size_t allocationSize = 4096u;
    const size_t viewOffset = 1024u;
    const size_t viewSize = 512u;
    const uint64_t gpuAddress = 0x1000000u;
    void *cpuPtr = reinterpret_cast<void *>(0x2000000u);

    std::unique_ptr<MockGraphicsAllocation> parentAllocation;
};

TEST_F(GraphicsAllocationViewTest, whenCreatingViewThenViewHasCorrectOffsetAndSize) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_EQ(viewSize, view->getUnderlyingBufferSize());
    EXPECT_EQ(viewOffset, view->getOffsetInParent());
}

TEST_F(GraphicsAllocationViewTest, whenCreatingViewThenIsViewReturnsTrue) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_TRUE(view->isView());
    EXPECT_FALSE(parentAllocation->isView());
}

TEST_F(GraphicsAllocationViewTest, whenCreatingViewThenGetParentAllocationReturnsCorrectParent) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_EQ(parentAllocation.get(), view->getParentAllocation());
    EXPECT_EQ(nullptr, parentAllocation->getParentAllocation());
}

TEST_F(GraphicsAllocationViewTest, whenCreatingViewThenGpuAddressIncludesOffset) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_EQ(gpuAddress + viewOffset, view->getGpuAddress());
}

TEST_F(GraphicsAllocationViewTest, whenCreatingViewThenCpuPtrIncludesOffset) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    auto expectedCpuPtr = ptrOffset(cpuPtr, viewOffset);
    EXPECT_EQ(expectedCpuPtr, view->getUnderlyingBuffer());
}

TEST_F(GraphicsAllocationViewTest, whenCreatingViewFromAllocationWithNullCpuPtrThenViewCpuPtrIsNull) {
    parentAllocation = std::make_unique<MockGraphicsAllocation>(
        0u, 1u, AllocationType::buffer, nullptr, gpuAddress, 0u, allocationSize, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_EQ(nullptr, view->getUnderlyingBuffer());
}

TEST_F(GraphicsAllocationViewTest, whenUpdatingTaskCountOnViewThenParentTaskCountIsUpdated) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));
    const uint32_t contextId = 0u;
    const TaskCountType taskCount = 42u;

    view->updateTaskCount(taskCount, contextId);

    EXPECT_EQ(taskCount, parentAllocation->getTaskCount(contextId));
    EXPECT_EQ(taskCount, view->getTaskCount(contextId));
}

TEST_F(GraphicsAllocationViewTest, whenUpdatingResidencyTaskCountOnViewThenViewTracksItsOwnResidencyTaskCount) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));
    const uint32_t contextId = 0u;
    const TaskCountType viewResidencyTaskCount = 42u;
    const TaskCountType parentResidencyTaskCount = 100u;

    parentAllocation->updateResidencyTaskCount(parentResidencyTaskCount, contextId);
    view->updateResidencyTaskCount(viewResidencyTaskCount, contextId);

    EXPECT_EQ(parentResidencyTaskCount, parentAllocation->getResidencyTaskCount(contextId));
    EXPECT_EQ(viewResidencyTaskCount, view->getResidencyTaskCount(contextId));
}

TEST_F(GraphicsAllocationViewTest, whenLockingViewThenParentIsLocked) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));
    void *lockedPtr = reinterpret_cast<void *>(0x3000000u);

    view->lock(lockedPtr);

    EXPECT_TRUE(parentAllocation->isLocked());
    EXPECT_TRUE(view->isLocked());
    EXPECT_EQ(lockedPtr, parentAllocation->getLockedPtr());

    view->unlock();

    EXPECT_FALSE(parentAllocation->isLocked());
    EXPECT_FALSE(view->isLocked());
}

TEST_F(GraphicsAllocationViewTest, whenGettingLockedPtrOnViewThenReturnsPtrWithOffset) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));
    void *parentLockedPtr = reinterpret_cast<void *>(0x3000000u);

    parentAllocation->lock(parentLockedPtr);

    auto expectedViewLockedPtr = ptrOffset(parentLockedPtr, viewOffset);
    EXPECT_EQ(expectedViewLockedPtr, view->getLockedPtr());

    parentAllocation->unlock();
}

TEST_F(GraphicsAllocationViewTest, whenParentNotLockedThenViewLockedPtrReturnsNull) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_EQ(nullptr, view->getLockedPtr());
}

TEST_F(GraphicsAllocationViewTest, whenSettingHostPtrTaskCountAssignmentOnViewThenParentIsUpdated) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    view->setHostPtrTaskCountAssignment(5u);

    EXPECT_EQ(5u, parentAllocation->getHostPtrTaskCountAssignment());
    EXPECT_EQ(5u, view->getHostPtrTaskCountAssignment());

    view->incrementHostPtrTaskCountAssignment();

    EXPECT_EQ(6u, parentAllocation->getHostPtrTaskCountAssignment());
    EXPECT_EQ(6u, view->getHostPtrTaskCountAssignment());

    view->decrementHostPtrTaskCountAssignment();

    EXPECT_EQ(5u, parentAllocation->getHostPtrTaskCountAssignment());
    EXPECT_EQ(5u, view->getHostPtrTaskCountAssignment());
}

TEST_F(GraphicsAllocationViewTest, whenSettingExplicitlyMadeResidentOnViewThenParentIsUpdated) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_FALSE(parentAllocation->isExplicitlyMadeResident());
    EXPECT_FALSE(view->isExplicitlyMadeResident());

    view->setExplicitlyMadeResident(true);

    EXPECT_TRUE(parentAllocation->isExplicitlyMadeResident());
    EXPECT_TRUE(view->isExplicitlyMadeResident());

    view->setExplicitlyMadeResident(false);

    EXPECT_FALSE(parentAllocation->isExplicitlyMadeResident());
    EXPECT_FALSE(view->isExplicitlyMadeResident());
}

TEST_F(GraphicsAllocationViewTest, whenSettingIsImportedOnViewThenParentIsUpdated) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_FALSE(parentAllocation->getIsImported());
    EXPECT_FALSE(view->getIsImported());

    view->setIsImported();

    EXPECT_TRUE(parentAllocation->getIsImported());
    EXPECT_TRUE(view->getIsImported());
}

TEST_F(GraphicsAllocationViewTest, whenCreatingViewThenViewInheritsPropertiesFromParent) {
    auto gmm1 = reinterpret_cast<Gmm *>(0x1234);
    auto gmm2 = reinterpret_cast<Gmm *>(0x5678);

    parentAllocation->setCoherent(true);
    parentAllocation->setEvictable(false);
    parentAllocation->setFlushL3Required(false);
    parentAllocation->setUncacheable(true);
    parentAllocation->set32BitAllocation(true);
    parentAllocation->setLockedMemory(true);
    parentAllocation->storageInfo.memoryBanks = 0x3u;
    parentAllocation->resizeGmms(2);
    parentAllocation->setGmm(gmm1, 0);
    parentAllocation->setGmm(gmm2, 1);

    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_EQ(parentAllocation->getRootDeviceIndex(), view->getRootDeviceIndex());
    EXPECT_EQ(parentAllocation->getAllocationType(), view->getAllocationType());
    EXPECT_EQ(parentAllocation->getMemoryPool(), view->getMemoryPool());
    EXPECT_EQ(parentAllocation->getGpuBaseAddress(), view->getGpuBaseAddress());

    EXPECT_EQ(parentAllocation->isCoherent(), view->isCoherent());
    EXPECT_EQ(parentAllocation->peekEvictable(), view->peekEvictable());
    EXPECT_EQ(parentAllocation->isFlushL3Required(), view->isFlushL3Required());
    EXPECT_EQ(parentAllocation->isUncacheable(), view->isUncacheable());
    EXPECT_EQ(parentAllocation->is32BitAllocation(), view->is32BitAllocation());
    EXPECT_EQ(parentAllocation->isLockedMemory(), view->isLockedMemory());

    EXPECT_EQ(parentAllocation->peekSharedHandle(), view->peekSharedHandle());

    EXPECT_EQ(parentAllocation->storageInfo.memoryBanks, view->storageInfo.memoryBanks);

    EXPECT_EQ(parentAllocation->getNumGmms(), view->getNumGmms());
    EXPECT_EQ(gmm1, view->getGmm(0));
    EXPECT_EQ(gmm2, view->getGmm(1));
}

TEST_F(GraphicsAllocationViewTest, whenGettingResidencyDataOnViewThenParentResidencyDataIsReturned) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    auto &parentResidencyData = parentAllocation->getResidencyData();
    auto &viewResidencyData = view->getResidencyData();

    EXPECT_EQ(&parentResidencyData, &viewResidencyData);
}

TEST_F(GraphicsAllocationViewTest, whenSettingInspectionIdOnViewThenParentIsUpdated) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));
    const uint32_t contextId = 0u;
    const uint32_t inspectionId = 123u;

    view->setInspectionId(inspectionId, contextId);

    EXPECT_EQ(inspectionId, parentAllocation->getInspectionId(contextId));
    EXPECT_EQ(inspectionId, view->getInspectionId(contextId));
}

TEST_F(GraphicsAllocationViewTest, whenCheckingIsUsedOnViewThenDelegatedToParent) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));
    const uint32_t contextId = 0u;

    EXPECT_FALSE(parentAllocation->isUsed());
    EXPECT_FALSE(view->isUsed());

    view->updateTaskCount(1u, contextId);

    EXPECT_TRUE(parentAllocation->isUsed());
    EXPECT_TRUE(view->isUsed());
}

TEST_F(GraphicsAllocationViewTest, whenCheckingRegisteredContextsOnViewThenDelegatedToParent) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    view->updateTaskCount(1u, 0u);
    view->updateTaskCount(1u, 1u);

    EXPECT_EQ(2u, parentAllocation->getNumRegisteredContexts());
    EXPECT_EQ(2u, view->getNumRegisteredContexts());
}

TEST_F(GraphicsAllocationViewTest, whenCheckingIsUsedByManyOsContextsOnViewThenDelegatedToParent) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_FALSE(parentAllocation->isUsedByManyOsContexts());
    EXPECT_FALSE(view->isUsedByManyOsContexts());

    view->updateTaskCount(1u, 0u);

    EXPECT_FALSE(parentAllocation->isUsedByManyOsContexts());
    EXPECT_FALSE(view->isUsedByManyOsContexts());

    view->updateTaskCount(1u, 1u);

    EXPECT_TRUE(parentAllocation->isUsedByManyOsContexts());
    EXPECT_TRUE(view->isUsedByManyOsContexts());
}

TEST_F(GraphicsAllocationViewTest, givenInvalidContextIdWhenGetResidencyTaskCountThenReturnObjectNotResident) {
    auto invalidContextId = static_cast<uint32_t>(MemoryManager::maxOsContextCount + 1);
    EXPECT_EQ(GraphicsAllocation::objectNotResident, parentAllocation->getResidencyTaskCount(invalidContextId));
}