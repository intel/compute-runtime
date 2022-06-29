/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
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

struct DrmCommandStreamMultiTileMemExecFixture {
    void SetUp() { // NOLINT(readability-identifier-naming)
        DebugManager.flags.CreateMultipleSubDevices.set(2u);
        DebugManager.flags.EnableImplicitScaling.set(1);
        DebugManager.flags.EnableForcePin.set(false);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&OSInterface::osEnableLocalMemory, true);

        executionEnvironment = new MockExecutionEnvironment();
        executionEnvironment->incRefInternal();
        executionEnvironment->initGmm();

        mock = new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0);

        memoryManager = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive,
                                             DebugManager.flags.EnableForcePin.get(),
                                             true,
                                             *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
        executionEnvironment->initializeMemoryManager();

        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useHwCsr = true;
        device.reset(MockDevice::create<MockDevice>(executionEnvironment, 0));
    }

    void TearDown() { // NOLINT(readability-identifier-naming)
        executionEnvironment->decRefInternal();
    }

    DebugManagerStateRestore dbgRestore;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
    std::unique_ptr<MockDevice> device;
    MockExecutionEnvironment *executionEnvironment = nullptr;
    DrmMockCustom *mock = nullptr;
    DrmMemoryManager *memoryManager = nullptr;
};

using DrmCommandStreamMultiTileMemExecTest = Test<DrmCommandStreamMultiTileMemExecFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmCommandStreamMultiTileMemExecTest, GivenDrmSupportsCompletionFenceAndVmBindWhenCallingCsrExecThenMultipleTagAllocationIsPassed) {
    auto *testCsr = new TestedDrmCommandStreamReceiver<FamilyType>(*executionEnvironment, 0, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(testCsr);
    EXPECT_EQ(2u, testCsr->activePartitions);

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

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{testCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    testCsr->makeResident(cmdBuffer);
    testCsr->makeResident(*allocation);
    testCsr->makeResident(*testCsr->getTagAllocation());

    testCsr->latestSentTaskCount = 2;
    testCsr->postSyncWriteOffset = 16;

    uint64_t expectedCompletionGpuAddress = testCsr->getTagAllocation()->getGpuAddress() + Drm::completionFenceOffset + testCsr->postSyncWriteOffset;

    int ret = testCsr->flushInternal(batchBuffer, testCsr->getResidencyAllocations());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedCompletionGpuAddress, bo.receivedCompletionGpuAddress);
    EXPECT_EQ(testCsr->latestSentTaskCount, bo.receivedCompletionValue);
    EXPECT_EQ(2u, bo.execCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmCommandStreamMultiTileMemExecTest, GivenDrmSupportsCompletionFenceAndVmBindWhenHandlingCompletionThenExpectMultipleWaitCalls) {
    EngineControl &defaultEngine = device->getDefaultEngine();
    EXPECT_EQ(2u, defaultEngine.commandStreamReceiver->getActivePartitions());

    uint32_t postSyncOffset = defaultEngine.commandStreamReceiver->getPostSyncWriteOffset();
    EXPECT_NE(0u, postSyncOffset);

    mock->completionFenceSupported = true;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, 1024, AllocationType::COMMAND_BUFFER});
    allocation->updateTaskCount(2, defaultEngine.osContext->getContextId());

    volatile uint32_t *completionAddress = defaultEngine.commandStreamReceiver->getTagAddress();
    completionAddress += (Drm::completionFenceOffset / sizeof(uint32_t));
    *completionAddress = 1;
    completionAddress += (postSyncOffset / sizeof(uint32_t));
    *completionAddress = 1;

    memoryManager->handleFenceCompletion(allocation);

    uint64_t expectedAddress = castToUint64(const_cast<uint32_t *>(defaultEngine.commandStreamReceiver->getTagAddress())) +
                               Drm::completionFenceOffset +
                               postSyncOffset;
    constexpr uint64_t expectedValue = 2;

    EXPECT_EQ(2u, mock->waitUserFenceCall.called);
    EXPECT_EQ(expectedAddress, mock->waitUserFenceCall.address);
    EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);

    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmCommandStreamMultiTileMemExecTest, GivenDrmSupportsCompletionFenceAndVmBindWhenHandlingCompletionAndOneContextIsReadyThenExpectOneWaitCall) {
    EngineControl &defaultEngine = device->getDefaultEngine();
    EXPECT_EQ(2u, defaultEngine.commandStreamReceiver->getActivePartitions());

    uint32_t postSyncOffset = defaultEngine.commandStreamReceiver->getPostSyncWriteOffset();
    EXPECT_NE(0u, postSyncOffset);

    mock->completionFenceSupported = true;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, 1024, AllocationType::COMMAND_BUFFER});
    allocation->updateTaskCount(2, defaultEngine.osContext->getContextId());

    volatile uint32_t *completionAddress = defaultEngine.commandStreamReceiver->getTagAddress();
    completionAddress += (Drm::completionFenceOffset / sizeof(uint32_t));
    *completionAddress = 2; //1st context is ready
    completionAddress += (postSyncOffset / sizeof(uint32_t));
    *completionAddress = 1;

    memoryManager->handleFenceCompletion(allocation);

    uint64_t expectedAddress = castToUint64(const_cast<uint32_t *>(defaultEngine.commandStreamReceiver->getTagAddress())) +
                               Drm::completionFenceOffset +
                               postSyncOffset;
    constexpr uint64_t expectedValue = 2;

    EXPECT_EQ(1u, mock->waitUserFenceCall.called);
    EXPECT_EQ(expectedAddress, mock->waitUserFenceCall.address);
    EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);

    memoryManager->freeGraphicsMemory(allocation);
}
