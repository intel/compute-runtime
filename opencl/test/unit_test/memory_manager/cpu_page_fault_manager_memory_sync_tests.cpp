/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/page_fault_manager/cpu_page_fault_manager_tests_fixture.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "gtest/gtest.h"

using namespace NEO;

struct CommandQueueMock : public MockCommandQueue {
    cl_int enqueueSVMUnmap(void *svmPtr,
                           cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                           cl_event *event, bool externalAppCall) override {
        transferToGpuCalled++;
        return CL_SUCCESS;
    }
    cl_int enqueueSVMMap(cl_bool blockingMap, cl_map_flags mapFlags,
                         void *svmPtr, size_t size,
                         cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                         cl_event *event, bool externalAppCall) override {
        transferToCpuCalled++;
        passedMapFlags = mapFlags;
        return CL_SUCCESS;
    }
    cl_int finish() override {
        finishCalled++;
        return CL_SUCCESS;
    }

    int transferToCpuCalled = 0;
    int transferToGpuCalled = 0;
    int finishCalled = 0;
    uint64_t passedMapFlags = 0;
};

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenSynchronizeMemoryThenEnqueueProperCalls) {
    MockExecutionEnvironment executionEnvironment;
    REQUIRE_SVM_OR_SKIP(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo());

    auto memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
    auto svmAllocsManager = std::make_unique<SVMAllocsManager>(memoryManager.get(), false);
    auto device = std::unique_ptr<MockClDevice>(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    auto rootDeviceIndex = device->getRootDeviceIndex();
    std::set<uint32_t> rootDeviceIndices{rootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{rootDeviceIndex, device->getDeviceBitfield()}};
    void *alloc = svmAllocsManager->createSVMAlloc(256, {}, rootDeviceIndices, deviceBitfields);

    auto cmdQ = std::make_unique<CommandQueueMock>();
    cmdQ->device = device.get();
    pageFaultManager->insertAllocation(alloc, 256, svmAllocsManager.get(), cmdQ.get(), {});

    pageFaultManager->baseCpuTransfer(alloc, 10, cmdQ.get());
    EXPECT_EQ(cmdQ->transferToCpuCalled, 1);
    EXPECT_EQ(cmdQ->transferToGpuCalled, 0);
    EXPECT_EQ(cmdQ->finishCalled, 0);

    pageFaultManager->baseGpuTransfer(alloc, cmdQ.get());
    EXPECT_EQ(cmdQ->transferToCpuCalled, 1);
    EXPECT_EQ(cmdQ->transferToGpuCalled, 1);
    EXPECT_EQ(cmdQ->finishCalled, 1);

    svmAllocsManager->freeSVMAlloc(alloc);
    cmdQ->device = nullptr;
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenGpuTransferIsInvokedThenInsertMapOperation) {
    MockExecutionEnvironment executionEnvironment;
    REQUIRE_SVM_OR_SKIP(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo());

    struct MockSVMAllocsManager : SVMAllocsManager {
        using SVMAllocsManager::SVMAllocsManager;
        void insertSvmMapOperation(void *regionSvmPtr, size_t regionSize, void *baseSvmPtr, size_t offset, bool readOnlyMap) override {
            SVMAllocsManager::insertSvmMapOperation(regionSvmPtr, regionSize, baseSvmPtr, offset, readOnlyMap);
            insertSvmMapOperationCalled++;
        }
        int insertSvmMapOperationCalled = 0;
    };
    auto memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
    auto svmAllocsManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get(), false);
    auto device = std::unique_ptr<MockClDevice>(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    auto rootDeviceIndex = device->getRootDeviceIndex();
    std::set<uint32_t> rootDeviceIndices{rootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{rootDeviceIndex, device->getDeviceBitfield()}};
    void *alloc = svmAllocsManager->createSVMAlloc(256, {}, rootDeviceIndices, deviceBitfields);
    auto cmdQ = std::make_unique<CommandQueueMock>();
    cmdQ->device = device.get();
    pageFaultManager->insertAllocation(alloc, 256, svmAllocsManager.get(), cmdQ.get(), {});

    EXPECT_EQ(svmAllocsManager->insertSvmMapOperationCalled, 0);
    pageFaultManager->baseGpuTransfer(alloc, cmdQ.get());
    EXPECT_EQ(svmAllocsManager->insertSvmMapOperationCalled, 1);

    svmAllocsManager->freeSVMAlloc(alloc);
    cmdQ->device = nullptr;
}
