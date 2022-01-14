/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"

using namespace NEO;

struct CommandQueueHwSvmTest
    : public ClDeviceFixture,
      public ContextFixture,
      public CommandQueueHwFixture,
      ::testing::Test {

    using ContextFixture::SetUp;

    void SetUp() override {
        ClDeviceFixture::SetUp();
        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);
        CommandQueueHwFixture::SetUp(pClDevice, 0);
        executionEnvironment.initGmm();
        memoryManager = std::make_unique<MockMemoryManager>(false, true, executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get(), false);
        memoryManager->pageFaultManager.reset(new MockPageFaultManager);
        pPageFaultManager = static_cast<MockPageFaultManager *>(memoryManager->pageFaultManager.get());
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        ContextFixture::TearDown();
        ClDeviceFixture::TearDown();
    }

    cl_command_queue_properties properties;
    const HardwareInfo *pHwInfo = nullptr;
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get()};
    std::unique_ptr<MockMemoryManager> memoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    std::set<uint32_t> rootDeviceIndices{mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    MockPageFaultManager *pPageFaultManager;
};

HWTEST_F(CommandQueueHwSvmTest, givenSharedAllocationWhenInternalAllocationsMadeResidentThenTheyAreMigrated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableLocalMemory.set(1);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    auto allocationSize = 4096u;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, pCmdQ);
    EXPECT_NE(nullptr, ptr);
    auto allocation = svmManager->getSVMAlloc(ptr);
    auto gpuAllocation = allocation->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, allocation->cpuAllocation);
    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(InternalMemoryType::SHARED_UNIFIED_MEMORY, allocation->memoryType);
    EXPECT_EQ(allocationSize, allocation->size);

    EXPECT_EQ(alignUp(allocationSize, 64 * KB), gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(alignUp(allocationSize, MemoryConstants::pageSize2Mb), allocation->cpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(GraphicsAllocation::AllocationType::SVM_GPU, gpuAllocation->getAllocationType());
    EXPECT_EQ(GraphicsAllocation::AllocationType::SVM_CPU, allocation->cpuAllocation->getAllocationType());

    EXPECT_EQ(gpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);
    EXPECT_NE(allocation->cpuAllocation->getMemoryPool(), MemoryPool::LocalMemory);

    EXPECT_NE(nullptr, gpuAllocation->getUnderlyingBuffer());
    EXPECT_EQ(0, pPageFaultManager->transferToGpuCalled);
    EXPECT_EQ(0, pPageFaultManager->protectMemoryCalled);
    svmManager->makeInternalAllocationsResidentAndMigrateIfNeeded(mockRootDeviceIndex, InternalMemoryType::SHARED_UNIFIED_MEMORY, *pCmdQ->getDevice().getDefaultEngine().commandStreamReceiver, true);
    EXPECT_EQ(1, pPageFaultManager->transferToGpuCalled);
    EXPECT_EQ(1, pPageFaultManager->protectMemoryCalled);
    svmManager->freeSVMAlloc(ptr);
}
