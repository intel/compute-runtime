/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

using namespace NEO;

struct MockDrmMemoryOperationsHandlerDefault : public DrmMemoryOperationsHandlerDefault {
    using DrmMemoryOperationsHandlerDefault::DrmMemoryOperationsHandlerDefault;
    using DrmMemoryOperationsHandlerDefault::residency;
};
struct DrmMemoryOperationsHandlerBaseTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->calculateMaxOsContextCount();
        mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[0]);
        mock->setBindAvailable();

        drmMemoryOperationsHandler = std::make_unique<MockDrmMemoryOperationsHandlerDefault>(0);
    }
    void initializeAllocation(int numBos) {
        if (!drmAllocation) {
            for (auto i = 0; i < numBos; i++) {
                mockBos.push_back(new MockBufferObject(0, mock, 3, 0, 0, 1));
            }
            drmAllocation = new MockDrmAllocation(AllocationType::unknown, MemoryPool::localMemory, mockBos);
            for (auto i = 0; i < numBos; i++) {
                drmAllocation->storageInfo.memoryBanks[i] = 1;
            }
            allocationPtr = drmAllocation;
        }
    }
    void TearDown() override {
        for (auto i = 0u; i < mockBos.size(); i++) {
            delete mockBos[i];
        }
        mockBos.clear();
        delete drmAllocation;
        delete mock;
        delete executionEnvironment;
    }

    ExecutionEnvironment *executionEnvironment;
    DrmQueryMock *mock;
    BufferObjects mockBos;
    MockDrmAllocation *drmAllocation = nullptr;
    GraphicsAllocation *allocationPtr = nullptr;
    std::unique_ptr<MockDrmMemoryOperationsHandlerDefault> drmMemoryOperationsHandler;
};

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenMakingAllocationResidentThenAllocationIsResident) {
    initializeAllocation(1);
    EXPECT_EQ(1u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmMemoryOperationsHandler->residency.find(allocationPtr) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *allocationPtr), MemoryOperationsStatus::success);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingResidentAllocationThenAllocationIsNotResident) {
    initializeAllocation(1);
    EXPECT_EQ(1u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *allocationPtr), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmMemoryOperationsHandler->residency.find(allocationPtr) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, *allocationPtr), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *allocationPtr), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenLockingAllocationThenAllocationIsResident) {
    initializeAllocation(2);
    EXPECT_EQ(2u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmMemoryOperationsHandler->residency.find(drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(drmAllocation->isLockedMemory());
    EXPECT_TRUE(mockBos[0]->isExplicitLockedMemoryRequired());
    EXPECT_TRUE(mockBos[1]->isExplicitLockedMemoryRequired());
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingLockedAllocationThenAllocationIsNotResident) {
    initializeAllocation(1);
    EXPECT_EQ(1u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);

    EXPECT_EQ(drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(drmMemoryOperationsHandler->residency.find(drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmAllocation->isLockedMemory());
    EXPECT_TRUE(mockBos[0]->isExplicitLockedMemoryRequired());

    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_FALSE(drmAllocation->isLockedMemory());
    EXPECT_FALSE(mockBos[0]->isExplicitLockedMemoryRequired());
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingLockedAllocationWithMultipleBOsThenAllocationIsNotResident) {
    initializeAllocation(2);
    EXPECT_EQ(2u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);

    EXPECT_EQ(drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(drmMemoryOperationsHandler->residency.find(drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmAllocation->isLockedMemory());
    EXPECT_TRUE(mockBos[0]->isExplicitLockedMemoryRequired());
    EXPECT_TRUE(mockBos[1]->isExplicitLockedMemoryRequired());

    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_FALSE(drmAllocation->isLockedMemory());
    EXPECT_FALSE(mockBos[0]->isExplicitLockedMemoryRequired());
    EXPECT_FALSE(mockBos[1]->isExplicitLockedMemoryRequired());
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingLockedAllocationWithChunkedThenAllocationIsNotResident) {
    initializeAllocation(1);
    EXPECT_EQ(1u, drmAllocation->storageInfo.getNumBanks());
    drmAllocation->storageInfo.isChunked = true;

    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);

    EXPECT_EQ(drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(drmMemoryOperationsHandler->residency.find(drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmAllocation->isLockedMemory());
    EXPECT_TRUE(mockBos[0]->isExplicitLockedMemoryRequired());

    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_FALSE(drmAllocation->isLockedMemory());
    EXPECT_FALSE(mockBos[0]->isExplicitLockedMemoryRequired());
}
