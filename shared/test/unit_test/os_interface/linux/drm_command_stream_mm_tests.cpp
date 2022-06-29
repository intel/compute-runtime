/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/os_interface/linux/drm_buffer_object_fixture.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using DrmCommandStreamMMTest = ::testing::Test;

struct DrmCommandStreamMemExecTest : public DrmCommandStreamEnhancedTemplate<DrmMockCustom> {
    void SetUp() override {
        DrmCommandStreamEnhancedTemplate::SetUp();
    }
    void TearDown() override {
        DrmCommandStreamEnhancedTemplate::TearDown();
    }
};

HWTEST_F(DrmCommandStreamMMTest, GivenForcePinThenMemoryManagerCreatesPinBb) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(true);

    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);

    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->pinBBs[0]);
}

HWTEST_F(DrmCommandStreamMMTest, givenForcePinDisabledWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(false);

    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());

    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->pinBBs[0]);
}

HWTEST_F(DrmCommandStreamMMTest, givenExecutionEnvironmentWithMoreThanOneRootDeviceEnvWhenCreatingDrmMemoryManagerThenCreateAsManyPinBBs) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(2);

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initGmm();
        auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    }

    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        EXPECT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    }
}

HWTEST_TEMPLATED_F(DrmCommandStreamMemExecTest, GivenDrmSupportsVmBindAndCompletionFenceWhenCallingCsrExecThenTagAllocationIsPassed) {
    mock->completionFenceSupported = true;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedBufferObject bo(mock, 128);
    MockDrmAllocation cmdBuffer(AllocationType::COMMAND_BUFFER, MemoryPool::System4KBPages);
    cmdBuffer.bufferObjects[0] = &bo;
    uint8_t buff[128];

    LinearStream cs(&cmdBuffer, buff, 128);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(cmdBuffer);
    csr->makeResident(*allocation);
    csr->makeResident(*csr->getTagAllocation());

    uint64_t expectedCompletionGpuAddress = csr->getTagAllocation()->getGpuAddress() + Drm::completionFenceOffset;
    auto *testCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testCsr->latestSentTaskCount = 2;

    int ret = testCsr->exec(batchBuffer, 1, 2, 0);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedCompletionGpuAddress, bo.receivedCompletionGpuAddress);
    EXPECT_EQ(testCsr->latestSentTaskCount, bo.receivedCompletionValue);

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamMemExecTest, GivenDrmSupportsVmBindAndNotCompletionFenceWhenCallingCsrExecThenTagAllocationIsNotPassed) {
    mock->completionFenceSupported = false;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedBufferObject bo(mock, 128);
    MockDrmAllocation cmdBuffer(AllocationType::COMMAND_BUFFER, MemoryPool::System4KBPages);
    cmdBuffer.bufferObjects[0] = &bo;
    uint8_t buff[128];

    LinearStream cs(&cmdBuffer, buff, 128);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(cmdBuffer);
    csr->makeResident(*allocation);
    csr->makeResident(*csr->getTagAllocation());

    constexpr uint64_t expectedCompletionGpuAddress = 0;
    constexpr uint32_t expectedCompletionValue = 0;
    auto *testCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testCsr->latestSentTaskCount = 2;

    int ret = testCsr->exec(batchBuffer, 1, 2, 0);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedCompletionGpuAddress, bo.receivedCompletionGpuAddress);
    EXPECT_EQ(expectedCompletionValue, bo.receivedCompletionValue);

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamMemExecTest, GivenDrmSupportsCompletionFenceAndNotVmBindWhenCallingCsrExecThenTagAllocationIsNotPassed) {
    mock->completionFenceSupported = true;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    TestedBufferObject bo(mock, 128);
    MockDrmAllocation cmdBuffer(AllocationType::COMMAND_BUFFER, MemoryPool::System4KBPages);
    cmdBuffer.bufferObjects[0] = &bo;
    uint8_t buff[128];

    LinearStream cs(&cmdBuffer, buff, 128);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(cmdBuffer);
    csr->makeResident(*allocation);
    csr->makeResident(*csr->getTagAllocation());

    constexpr uint64_t expectedCompletionGpuAddress = 0;
    constexpr uint32_t expectedCompletionValue = 0;
    auto *testCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testCsr->latestSentTaskCount = 2;

    int ret = testCsr->exec(batchBuffer, 1, 2, 0);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedCompletionGpuAddress, bo.receivedCompletionGpuAddress);
    EXPECT_EQ(expectedCompletionValue, bo.receivedCompletionValue);

    mm->freeGraphicsMemory(allocation);
}
