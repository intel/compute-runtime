/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

struct WddmMemoryOperationsHandlerTest : public WddmTest {
    void SetUp() override {
        WddmTest::SetUp();
        wddmMemoryOperationsHandler = std::make_unique<WddmMemoryOperationsHandler>(wddm);
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

    std::unique_ptr<WddmMemoryOperationsHandler> wddmMemoryOperationsHandler;
    std::unique_ptr<MockWddmAllocation> wddmAllocation;
    std::unique_ptr<MockWddmAllocation> wddmFragmentedAllocation;
    std::unique_ptr<OsHandleWin> osHandleStorageFirst;
    std::unique_ptr<OsHandleWin> osHandleStorageSecond;
    GraphicsAllocation *allocationPtr;
    StackVec<GraphicsAllocation *, 2> allocationData;
};

TEST_F(WddmMemoryOperationsHandlerTest, givenRegularAllocationWhenMakingResidentAllocationThenMakeResidentIsCalledAndAllocationIsMarkedAsExplicitlyResident) {
    wddmAllocation->setExplicitlyMadeResident(false);
    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(wddmAllocation->isExplicitlyMadeResident());
}

TEST_F(WddmMemoryOperationsHandlerTest, givenFragmentedAllocationWhenMakingResidentAllocationThenMakeResidentIsCalledAndAllocationIsMarkedAsExplicitlyResident) {
    allocationPtr = wddmFragmentedAllocation.get();
    allocationPtr->setExplicitlyMadeResident(false);

    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(allocationPtr->isExplicitlyMadeResident());
}

TEST_F(WddmMemoryOperationsHandlerTest, givenVariousAllocationsWhenMakingResidentAllocationThenMakeResidentCalled) {

    for (auto &allocation : allocationData) {
        allocation->setExplicitlyMadeResident(false);
    }

    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(allocationData), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);

    for (auto &allocation : allocationData) {
        EXPECT_TRUE(allocation->isExplicitlyMadeResident());
    }
}

TEST_F(WddmMemoryOperationsHandlerTest, givenRegularAllocationWhenEvictingResidentAllocationThenEvictCalled) {
    wddm->callBaseEvict = true;
    allocationPtr->setExplicitlyMadeResident(false);

    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_TRUE(allocationPtr->isExplicitlyMadeResident());

    EXPECT_EQ(wddmMemoryOperationsHandler->evict(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_FALSE(allocationPtr->isExplicitlyMadeResident());

    EXPECT_EQ(0u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::memoryNotFound);
}

TEST_F(WddmMemoryOperationsHandlerTest, givenFragmentedAllocationWhenEvictingResidentAllocationThenEvictCalled) {
    allocationPtr = wddmFragmentedAllocation.get();
    allocationPtr->setExplicitlyMadeResident(false);

    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_TRUE(allocationPtr->isExplicitlyMadeResident());

    EXPECT_EQ(wddmMemoryOperationsHandler->evict(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);
    EXPECT_FALSE(allocationPtr->isExplicitlyMadeResident());

    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::memoryNotFound);
}

TEST_F(WddmMemoryOperationsHandlerTest, givenVariousAllocationsWhenEvictingResidentAllocationThenEvictCalled) {
    for (auto &allocation : allocationData) {
        allocation->setExplicitlyMadeResident(false);
    }
    wddm->evictResult.called = 0;
    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(allocationData), false, false), MemoryOperationsStatus::success);
    EXPECT_TRUE(wddmAllocation->isExplicitlyMadeResident());
    EXPECT_TRUE(wddmFragmentedAllocation->isExplicitlyMadeResident());

    EXPECT_EQ(wddmMemoryOperationsHandler->evict(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_FALSE(wddmAllocation->isExplicitlyMadeResident());
    EXPECT_TRUE(wddmFragmentedAllocation->isExplicitlyMadeResident());
    EXPECT_EQ(1u, wddm->evictResult.called);

    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::memoryNotFound);

    EXPECT_EQ(wddmMemoryOperationsHandler->evict(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);
    EXPECT_FALSE(wddmAllocation->isExplicitlyMadeResident());
    EXPECT_FALSE(wddmFragmentedAllocation->isExplicitlyMadeResident());
    EXPECT_EQ(2u, wddm->evictResult.called);

    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::memoryNotFound);
}

TEST_F(WddmMemoryOperationsHandlerTest, givenVariousAllocationsWhenFreeResidentAllocationThenAllocationIsRemovedButEvictIsNotCalled) {
    for (auto &allocation : allocationData) {
        allocation->setExplicitlyMadeResident(false);
    }
    wddm->evictResult.called = 0;
    EXPECT_EQ(wddmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(allocationData), false, false), MemoryOperationsStatus::success);
    EXPECT_TRUE(wddmAllocation->isExplicitlyMadeResident());
    EXPECT_TRUE(wddmFragmentedAllocation->isExplicitlyMadeResident());

    EXPECT_EQ(wddmMemoryOperationsHandler->free(nullptr, *wddmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(wddmAllocation->isExplicitlyMadeResident());
    EXPECT_TRUE(wddmFragmentedAllocation->isExplicitlyMadeResident());
    EXPECT_EQ(0u, wddm->evictResult.called);

    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmAllocation), MemoryOperationsStatus::memoryNotFound);

    EXPECT_EQ(wddmMemoryOperationsHandler->free(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(wddmAllocation->isExplicitlyMadeResident());
    EXPECT_TRUE(wddmFragmentedAllocation->isExplicitlyMadeResident());
    EXPECT_EQ(0u, wddm->evictResult.called);

    EXPECT_EQ(wddmMemoryOperationsHandler->isResident(nullptr, *wddmFragmentedAllocation), MemoryOperationsStatus::memoryNotFound);
}

TEST(WddmResidentBufferTests, whenBuffersIsCreatedWithMakeResidentFlagSetThenItIsMadeResidentUponCreation) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    ultHwConfig.forceOsAgnosticMemoryManager = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeAllBuffersResident.set(true);

    initPlatform();
    auto device = platform()->getClDevice(0u);

    MockContext context(device, false);
    auto retValue = CL_SUCCESS;
    auto clBuffer = clCreateBuffer(&context, 0u, 4096u, nullptr, &retValue);
    ASSERT_EQ(retValue, CL_SUCCESS);

    auto memoryOperationsHandler = device->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto neoBuffer = castToObject<MemObj>(clBuffer);
    auto bufferAllocation = neoBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    auto status = memoryOperationsHandler->isResident(nullptr, *bufferAllocation);

    EXPECT_EQ(status, MemoryOperationsStatus::success);

    clReleaseMemObject(clBuffer);
}
