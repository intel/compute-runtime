/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/source/device/cl_device.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"
#include "opencl/test/unit_test/os_interface/windows/wddm_fixture.h"
#include "test.h"

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

TEST(WddmResidentBufferTests, whenBuffersIsCreatedWithMakeResidentFlagSetThenItIsMadeResidentUponCreation) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedGetDevicesFunc = false;
    DebugManagerStateRestore restorer;
    DebugManager.flags.MakeAllBuffersResident.set(true);

    initPlatform();
    auto device = platform()->getClDevice(0u);

    MockContext context(device, false);
    auto retValue = CL_SUCCESS;
    auto clBuffer = clCreateBuffer(&context, 0u, 4096u, nullptr, &retValue);
    ASSERT_EQ(retValue, CL_SUCCESS);

    auto memoryOperationsHandler = context.getDevice(0)->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto neoBuffer = castToObject<MemObj>(clBuffer);
    auto bufferAllocation = neoBuffer->getGraphicsAllocation();
    auto status = memoryOperationsHandler->isResident(*bufferAllocation);

    EXPECT_EQ(status, MemoryOperationsStatus::SUCCESS);

    clReleaseMemObject(clBuffer);
}
