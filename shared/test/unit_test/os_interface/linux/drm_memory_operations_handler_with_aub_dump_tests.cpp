/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_with_aub_dump.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

using namespace NEO;

template <typename BaseMemoryOperationsHandler>
class DrmMemoryOperationsHandlerWithAubDumpMock : public DrmMemoryOperationsHandlerWithAubDump<BaseMemoryOperationsHandler> {
  public:
    using DrmMemoryOperationsHandlerWithAubDump<BaseMemoryOperationsHandler>::aubMemoryOperationsHandler;
    using DrmMemoryOperationsHandlerWithAubDump<BaseMemoryOperationsHandler>::residency;

    DrmMemoryOperationsHandlerWithAubDumpMock(RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
        : DrmMemoryOperationsHandlerWithAubDump<BaseMemoryOperationsHandler>(rootDeviceEnvironment, rootDeviceIndex) {
        aubMemoryOperationsHandler = std::make_unique<MockAubMemoryOperationsHandler>(nullptr);
    }
};

struct DrmMemoryOperationsHandlerWithAubDumpTest : public ::testing::Test {
    void SetUp() override {
        device.reset(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0));
        device->executionEnvironment->prepareRootDeviceEnvironments(1);
        device->executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        auto rootDeviceEnv = device->executionEnvironment->rootDeviceEnvironments[0].get();
        drmMemoryOperationsHandlerWithAubDumpMock = std::make_unique<DrmMemoryOperationsHandlerWithAubDumpMock<DrmMemoryOperationsHandlerDefault>>(*rootDeviceEnv, 0);
        mockAubMemoryOperationsHandler = static_cast<MockAubMemoryOperationsHandler *>(drmMemoryOperationsHandlerWithAubDumpMock->aubMemoryOperationsHandler.get());

        allocationPtr = &graphicsAllocation;
    }

    MockGraphicsAllocation graphicsAllocation;
    GraphicsAllocation *allocationPtr;
    std::unique_ptr<DrmMemoryOperationsHandlerWithAubDumpMock<DrmMemoryOperationsHandlerDefault>> drmMemoryOperationsHandlerWithAubDumpMock;
    MockAubMemoryOperationsHandler *mockAubMemoryOperationsHandler;
    std::unique_ptr<NEO::MockDevice> device;
};

TEST_F(DrmMemoryOperationsHandlerWithAubDumpTest, whenMakingAllocationResidentThenAllocationIsResident) {
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->residency.size(), 1u);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandlerWithAubDumpMock->residency.begin(), drmMemoryOperationsHandlerWithAubDumpMock->residency.end(), allocationPtr) != drmMemoryOperationsHandlerWithAubDumpMock->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, graphicsAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
}

TEST_F(DrmMemoryOperationsHandlerWithAubDumpTest, givenRegularAllocationWhenFreeResidentAllocationThenFreeCalledAndAllocationIsRemovedFromResidencyInAubOperationsHandler) {
    MockAubManager aubManager;
    mockAubMemoryOperationsHandler->setAubManager(&aubManager);
    allocationPtr->setAubWritable(true, 0xff);

    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->residency.size(), 1u);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandlerWithAubDumpMock->residency.begin(), drmMemoryOperationsHandlerWithAubDumpMock->residency.end(), allocationPtr) != drmMemoryOperationsHandlerWithAubDumpMock->residency.end());
    EXPECT_EQ(1u, mockAubMemoryOperationsHandler->residentAllocations.size());

    drmMemoryOperationsHandlerWithAubDumpMock->free(device.get(), *allocationPtr);

    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->freeCalled);

    EXPECT_EQ(0u, mockAubMemoryOperationsHandler->residentAllocations.size());
}

TEST_F(DrmMemoryOperationsHandlerWithAubDumpTest, whenEvictingResidentAllocationThenAllocationIsNotResident) {
    auto mock = new DrmQueryMock(*device->executionEnvironment->rootDeviceEnvironments[0]);
    mock->setBindAvailable();

    BufferObjects bos;
    MockBufferObject mockBo(0, mock, 3, 0, 0, 1);
    mockBo.setSize(1024);
    bos.push_back(&mockBo);
    GraphicsAllocation *mockDrmAllocation = new MockDrmAllocation(AllocationType::unknown, MemoryPool::localMemory, bos);

    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->residency.size(), 0u);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&mockDrmAllocation, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *mockDrmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->residency.size(), 1u);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandlerWithAubDumpMock->residency.begin(), drmMemoryOperationsHandlerWithAubDumpMock->residency.end(), mockDrmAllocation) != drmMemoryOperationsHandlerWithAubDumpMock->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->evict(nullptr, *mockDrmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *mockDrmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->residency.size(), 0u);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->evictCalled);

    delete mockDrmAllocation;
    delete mock;
}

TEST_F(DrmMemoryOperationsHandlerWithAubDumpTest, whenEvictingLockedAllocationThenAllocationIsNotResident) {
    auto mock = new DrmQueryMock(*device->executionEnvironment->rootDeviceEnvironments[0]);
    mock->setBindAvailable();

    BufferObjects bos;
    MockBufferObject mockBo(0, mock, 3, 0, 0, 1);
    mockBo.setSize(1024);
    bos.push_back(&mockBo);
    GraphicsAllocation *mockDrmAllocation = new MockDrmAllocation(AllocationType::unknown, MemoryPool::localMemory, bos);

    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->residency.size(), 0u);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->lock(nullptr, ArrayRef<GraphicsAllocation *>(&mockDrmAllocation, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *mockDrmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->residency.size(), 1u);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandlerWithAubDumpMock->residency.begin(), drmMemoryOperationsHandlerWithAubDumpMock->residency.end(), mockDrmAllocation) != drmMemoryOperationsHandlerWithAubDumpMock->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->evict(nullptr, *mockDrmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *mockDrmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandlerWithAubDumpMock->residency.size(), 0u);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->evictCalled);

    delete mockDrmAllocation;
    delete mock;
}

TEST_F(DrmMemoryOperationsHandlerWithAubDumpTest, whenConstructingDrmMemoryOperationsHandlerWithAubDumpWithoutAubCenterThenAubCenterIsInitialized) {
    auto rootDeviceEnv = device->executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnv->aubCenter.reset();
    ASSERT_EQ(nullptr, rootDeviceEnv->aubCenter.get());
    auto drmMemoryOperationsHandlerWithAubDump = std::make_unique<DrmMemoryOperationsHandlerWithAubDumpMock<DrmMemoryOperationsHandlerDefault>>(*rootDeviceEnv, 0);
    EXPECT_NE(nullptr, rootDeviceEnv->aubCenter.get());
}
