/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_interface.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/test/unit_test/mocks/linux/mock_drm_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/os_interface/linux/drm_command_stream_fixture.h"
#include "opencl/test/unit_test/os_interface/linux/drm_memory_manager_tests.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenAllocateInDevicePoolIsCalledThenNullptrAndStatusRetryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(drm);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(drm);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    DrmAllocation drmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);

    auto ptr = memoryManager.lockResourceInLocalMemoryImpl(drmAllocation.getBO());
    EXPECT_EQ(nullptr, ptr);

    memoryManager.unlockResourceInLocalMemoryImpl(drmAllocation.getBO());
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenFreeGraphicsMemoryIsCalledOnAllocationWithNullBufferObjectThenEarlyReturn) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(drm);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto drmAllocation = new DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_NE(nullptr, drmAllocation);

    memoryManager.freeGraphicsMemoryImpl(drmAllocation);
}

using DrmMemoryManagerWithLocalMemoryTest = Test<DrmMemoryManagerWithLocalMemoryFixture>;

TEST_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnAllocationInLocalMemoryThenReturnNullPtr) {
    DrmAllocation drmAllocation(rootDeviceIndex, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

using DrmMemoryManagerTest = Test<DrmMemoryManagerFixture>;

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationThenAllocationIsFilledWithCorrectData) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), GraphicsAllocation::AllocationType::BUFFER, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    auto ret = memoryManager->copyMemoryToAllocation(allocation, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), dataToCopy.data(), dataToCopy.size()));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    EXPECT_EQ(0 * GB, memoryManager->getLocalMemorySize(rootDeviceIndex));
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenMakeAllBuffersResidentSetWhenFlushThenDrmMemoryOperationHandlerIsLocked) {
    struct MockDrmMemoryOperationsHandler : public DrmMemoryOperationsHandler {
        using DrmMemoryOperationsHandler::mutex;
    };

    struct MockDrmCsr : public DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        void flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency) override {
            auto memoryOperationsInterface = static_cast<MockDrmMemoryOperationsHandler *>(this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface.get());
            EXPECT_FALSE(memoryOperationsInterface->mutex.try_lock());
        }
    };

    DebugManagerStateRestore restorer;
    DebugManager.flags.MakeAllBuffersResident.set(true);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};

    MockDrmCsr mockCsr(*executionEnvironment, rootDeviceIndex, gemCloseWorkerMode::gemCloseWorkerInactive);
    mockCsr.flush(batchBuffer, mockCsr.getResidencyAllocations());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenAllocInMemoryOperationsInterfaceWhenFlushThenAllocIsResident) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocation, 1));

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    const auto boRequirments = [&allocation](const auto &bo) {
        return (static_cast<int>(bo.handle) == static_cast<DrmAllocation *>(allocation)->getBO()->peekHandle() &&
                bo.offset == static_cast<DrmAllocation *>(allocation)->getBO()->peekAddress());
    };

    auto &residency = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->getExecStorage();
    EXPECT_TRUE(std::find_if(residency.begin(), residency.end(), boRequirments) != residency.end());
    EXPECT_EQ(residency.size(), 2u);
    residency.clear();

    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_TRUE(std::find_if(residency.begin(), residency.end(), boRequirments) != residency.end());
    EXPECT_EQ(residency.size(), 2u);
    residency.clear();

    csr->getResidencyAllocations().clear();
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->evict(device.get(), *allocation);

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_FALSE(std::find_if(residency.begin(), residency.end(), boRequirments) != residency.end());
    EXPECT_EQ(residency.size(), 1u);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

TEST(ResidencyTests, whenBuffersIsCreatedWithMakeResidentFlagThenItSuccessfulyCreates) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    ultHwConfig.forceOsAgnosticMemoryManager = false;
    DebugManagerStateRestore restorer;
    DebugManager.flags.MakeAllBuffersResident.set(true);

    initPlatform();
    auto device = platform()->getClDevice(0u);

    MockContext context(device, false);
    auto retValue = CL_SUCCESS;
    auto clBuffer = clCreateBuffer(&context, 0u, 4096u, nullptr, &retValue);
    ASSERT_EQ(retValue, CL_SUCCESS);
    clReleaseMemObject(clBuffer);
}