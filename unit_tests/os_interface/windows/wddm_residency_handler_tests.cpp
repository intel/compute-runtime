/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/utilities/stackvec.h"
#include "runtime/os_interface/windows/wddm_memory_operations_handler.h"
#include "test.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"

using namespace NEO;

struct WddmMemoryOperationsHandlerTest : public WddmTest {
    void SetUp() override {
        WddmTest::SetUp();
        wddmMemoryOperationsHandler = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        wddmAllocation.handle = 0x2u;

        osHandleStorageFirst = std::make_unique<OsHandle>();
        osHandleStorageSecond = std::make_unique<OsHandle>();

        wddmFragmentedAllocation.fragmentsStorage.fragmentCount = 2;
        wddmFragmentedAllocation.fragmentsStorage.fragmentStorageData[0].osHandleStorage = osHandleStorageFirst.get();
        wddmFragmentedAllocation.fragmentsStorage.fragmentStorageData[0].osHandleStorage->handle = 0x3u;
        wddmFragmentedAllocation.fragmentsStorage.fragmentStorageData[1].osHandleStorage = osHandleStorageSecond.get();
        wddmFragmentedAllocation.fragmentsStorage.fragmentStorageData[1].osHandleStorage->handle = 0x4u;

        allocationPtr = &wddmAllocation;

        allocationData.push_back(&wddmAllocation);
        allocationData.push_back(&wddmFragmentedAllocation);
    }

    std::unique_ptr<WddmMemoryOperationsHandler> wddmMemoryOperationsHandler;
    MockWddmAllocation wddmAllocation;
    MockWddmAllocation wddmFragmentedAllocation;
    std::unique_ptr<OsHandle> osHandleStorageFirst;
    std::unique_ptr<OsHandle> osHandleStorageSecond;
    GraphicsAllocation *allocationPtr;
    StackVec<GraphicsAllocation *, 2> allocationData;
};

TEST_F(WddmMemoryOperationsHandlerTest, givenRegularAllocatioWhenMakingResidentAllocaionExpectMakeResidentCalled) {
    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(wddmAllocation), MemoryOperationsStatus::SUCCESS);
}

TEST_F(WddmMemoryOperationsHandlerTest, givenFragmentedAllocationWhenMakingResidentAllocaionExpectMakeResidentCalled) {
    allocationPtr = &wddmFragmentedAllocation;

    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(wddmFragmentedAllocation), MemoryOperationsStatus::SUCCESS);
}

TEST_F(WddmMemoryOperationsHandlerTest, givenVariousAllocationsWhenMakingResidentAllocaionExpectMakeResidentCalled) {
    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(ArrayRef<GraphicsAllocation *>(allocationData)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(wddmAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(wddmFragmentedAllocation), MemoryOperationsStatus::SUCCESS);
}

TEST_F(WddmMemoryOperationsHandlerTest, givenRegularAllocatioWhenEvictingResidentAllocationExpectEvictCalled) {
    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->evict(wddmAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(wddmAllocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
}

TEST_F(WddmMemoryOperationsHandlerTest, givenFragmentedAllocationWhenEvictingResidentAllocationExpectEvictCalled) {
    allocationPtr = &wddmFragmentedAllocation;

    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->evict(wddmFragmentedAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(wddmFragmentedAllocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
}

TEST_F(WddmMemoryOperationsHandlerTest, givenVariousAllocationsWhenEvictingResidentAllocationExpectEvictCalled) {
    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(ArrayRef<GraphicsAllocation *>(allocationData)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->evict(wddmAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(wddmAllocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(wddmMemoryOperationsHandler->evict(wddmFragmentedAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(wddmFragmentedAllocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
}
