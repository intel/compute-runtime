/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_operations_handler_with_aub_dump.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_aub_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

template <typename BaseMemoryOperationsHandler>
class WddmMemoryOperationsHandlerWithAubDumpMock : public WddmMemoryOperationsHandlerWithAubDump<BaseMemoryOperationsHandler> {
  public:
    using WddmMemoryOperationsHandlerWithAubDump<BaseMemoryOperationsHandler>::aubMemoryOperationsHandler;

    WddmMemoryOperationsHandlerWithAubDumpMock(Wddm *wddm, RootDeviceEnvironment &rootDeviceEnvironment)
        : WddmMemoryOperationsHandlerWithAubDump<BaseMemoryOperationsHandler>(wddm, rootDeviceEnvironment) {
        aubMemoryOperationsHandler = std::make_unique<MockAubMemoryOperationsHandler>(nullptr);
    }
};

struct WddmMemoryOperationsHandlerWithAubDumpTest : public WddmTest {
    void SetUp() override {
        WddmTest::SetUp();
        wddmMemoryOperationsHandlerWithAubDumpMock = std::make_unique<WddmMemoryOperationsHandlerWithAubDumpMock<WddmMemoryOperationsHandler>>(wddm, *rootDeviceEnvironment);
        mockAubMemoryOperationsHandler = static_cast<MockAubMemoryOperationsHandler *>(wddmMemoryOperationsHandlerWithAubDumpMock->aubMemoryOperationsHandler.get());

        wddmAllocation = std::make_unique<MockWddmAllocation>(rootDeviceEnvironment->getGmmHelper());
        wddmFragmentedAllocation = std::make_unique<MockWddmAllocation>(rootDeviceEnvironment->getGmmHelper());
        wddmAllocation->handle = 0x2u;

        osHandleStorageFirst = std::make_unique<OsHandleWin>();
        osHandleStorageSecond = std::make_unique<OsHandleWin>();

        wddmFragmentedAllocation->fragmentsStorage.fragmentCount = 2;
        wddmFragmentedAllocation->fragmentsStorage.fragmentStorageData[0].osHandleStorage = osHandleStorageFirst.get();
        static_cast<OsHandleWin *>(wddmFragmentedAllocation->fragmentsStorage.fragmentStorageData[0].osHandleStorage)->handle = 0x3u;
        wddmFragmentedAllocation->fragmentsStorage.fragmentStorageData[1].osHandleStorage = osHandleStorageSecond.get();
        static_cast<OsHandleWin *>(wddmFragmentedAllocation->fragmentsStorage.fragmentStorageData[1].osHandleStorage)->handle = 0x4u;

        allocationPtr = wddmAllocation.get();

        allocationData.push_back(wddmAllocation.get());
        allocationData.push_back(wddmFragmentedAllocation.get());
    }

    std::unique_ptr<WddmMemoryOperationsHandlerWithAubDumpMock<WddmMemoryOperationsHandler>> wddmMemoryOperationsHandlerWithAubDumpMock;
    std::unique_ptr<MockWddmAllocation> wddmAllocation;
    std::unique_ptr<MockWddmAllocation> wddmFragmentedAllocation;
    std::unique_ptr<OsHandleWin> osHandleStorageFirst;
    std::unique_ptr<OsHandleWin> osHandleStorageSecond;
    GraphicsAllocation *allocationPtr;
    StackVec<GraphicsAllocation *, 2> allocationData;
    MockAubMemoryOperationsHandler *mockAubMemoryOperationsHandler;
};

TEST_F(WddmMemoryOperationsHandlerWithAubDumpTest, givenRegularAllocationWhenMakingResidentAllocationThenMakeResidentCalled) {
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
}

TEST_F(WddmMemoryOperationsHandlerWithAubDumpTest, givenFragmentedAllocationWhenMakingResidentAllocationThenMakeResidentCalled) {
    allocationPtr = wddmFragmentedAllocation.get();
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
}

TEST_F(WddmMemoryOperationsHandlerWithAubDumpTest, givenVariousAllocationsWhenMakingResidentAllocationThenMakeResidentCalled) {
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(allocationData), false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
}

TEST_F(WddmMemoryOperationsHandlerWithAubDumpTest, givenRegularAllocationWhenEvictingResidentAllocationThenEvictCalled) {
    wddm->callBaseEvict = true;
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->evict(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(0u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
}

TEST_F(WddmMemoryOperationsHandlerWithAubDumpTest, givenFragmentedAllocationWhenEvictingResidentAllocationThenEvictCalled) {
    allocationPtr = wddmFragmentedAllocation.get();
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->evict(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
}

TEST_F(WddmMemoryOperationsHandlerWithAubDumpTest, givenVariousAllocationsWhenEvictingResidentAllocationThenEvictCalled) {
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(allocationData), false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->evict(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->evict(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->makeResidentCalled);
    EXPECT_TRUE(mockAubMemoryOperationsHandler->isResidentCalled);
}

TEST_F(WddmMemoryOperationsHandlerWithAubDumpTest, whenConstructingWddmMemoryOperationsHandlerWithAubDumpWithoutAubCenterThenAubCenterIsInitialized) {
    rootDeviceEnvironment->aubCenter.reset();
    ASSERT_EQ(nullptr, rootDeviceEnvironment->aubCenter.get());
    auto wddmMemoryOperationsHandlerWithAubDump = std::make_unique<WddmMemoryOperationsHandlerWithAubDump<WddmMemoryOperationsHandler>>(wddm, *rootDeviceEnvironment);
    EXPECT_NE(nullptr, rootDeviceEnvironment->aubCenter.get());
}

TEST_F(WddmMemoryOperationsHandlerWithAubDumpTest, givenRegularAllocationWhenLockingAllocationThenUnsupportIsReturned) {
    EXPECT_EQ(wddmMemoryOperationsHandlerWithAubDumpMock->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::unsupported);
}
