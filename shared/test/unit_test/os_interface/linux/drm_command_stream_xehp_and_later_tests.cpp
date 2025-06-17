/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
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
    void setUp() {
        debugManager.flags.CreateMultipleSubDevices.set(2u);
        debugManager.flags.EnableImplicitScaling.set(1);
        debugManager.flags.EnableForcePin.set(false);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&OSInterface::osEnableLocalMemory, true);

        executionEnvironment = new MockExecutionEnvironment();
        executionEnvironment->incRefInternal();
        executionEnvironment->initGmm();

        mock = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]).release();
        mock->completionFenceSupported = true;
        mock->isVmBindAvailableCall.callParent = false;
        mock->isVmBindAvailableCall.returnValue = true;

        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0, false);

        memoryManager = new DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive,
                                             debugManager.flags.EnableForcePin.get(),
                                             true,
                                             *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());

        UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);
        UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->calculateMaxOsContextCount();

        executionEnvironment->initializeMemoryManager();

        VariableBackup<bool> backupSipData(&MockSipData::useMockSip, false);
        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useHwCsr = true;
        device.reset(MockDevice::create<MockDevice>(executionEnvironment, 0));
    }

    void tearDown() {
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

struct DrmCommandStreamMultiTileMemExecTestWithCsr : public DrmCommandStreamMultiTileMemExecTest {
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<TestedDrmCommandStreamReceiver<FamilyType>>();

        DrmCommandStreamMultiTileMemExecTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        DrmCommandStreamMultiTileMemExecTest::TearDown();
    }
};

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, DrmCommandStreamMultiTileMemExecTestWithCsr, GivenDrmSupportsCompletionFenceAndVmBindWhenCallingCsrExecThenMultipleTagAllocationIsPassed) {
    auto testCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(&device->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(2u, testCsr->activePartitions);

    TestedBufferObject bo(0, mock, 128);
    MockDrmAllocation cmdBuffer(0u, AllocationType::commandBuffer, MemoryPool::system4KBPages);
    cmdBuffer.bufferObjects[0] = &bo;
    uint8_t buff[128];

    LinearStream cs(&cmdBuffer, buff, 128);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{testCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    testCsr->makeResident(cmdBuffer);
    testCsr->makeResident(*allocation);
    testCsr->makeResident(*testCsr->getTagAllocation());

    testCsr->latestSentTaskCount = 2;
    testCsr->immWritePostSyncWriteOffset = 16;

    uint64_t expectedCompletionGpuAddress = testCsr->getTagAllocation()->getGpuAddress() + TagAllocationLayout::completionFenceOffset + testCsr->immWritePostSyncWriteOffset;

    SubmissionStatus ret = testCsr->flushInternal(batchBuffer, testCsr->getResidencyAllocations());
    EXPECT_EQ(SubmissionStatus::success, ret);

    EXPECT_EQ(expectedCompletionGpuAddress, bo.receivedCompletionGpuAddress);
    EXPECT_EQ(testCsr->latestSentTaskCount, bo.receivedCompletionValue);
    EXPECT_EQ(2u, bo.execCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmCommandStreamMultiTileMemExecTest, GivenDrmSupportsCompletionFenceAndVmBindWhenHandlingCompletionThenExpectMultipleWaitCalls) {
    EngineControl &defaultEngine = device->getDefaultEngine();
    EXPECT_EQ(2u, defaultEngine.commandStreamReceiver->getActivePartitions());

    uint32_t postSyncOffset = defaultEngine.commandStreamReceiver->getImmWritePostSyncWriteOffset();
    EXPECT_NE(0u, postSyncOffset);

    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto heapless = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    auto heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(heapless);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, 1024, AllocationType::commandBuffer});
    allocation->updateTaskCount(heaplessStateInit ? 3 : 2, defaultEngine.osContext->getContextId());

    volatile TagAddressType *completionAddress = defaultEngine.commandStreamReceiver->getTagAddress();
    completionAddress += (TagAllocationLayout::completionFenceOffset / sizeof(TagAddressType));
    *completionAddress = heaplessStateInit ? 2 : 1;

    completionAddress += (postSyncOffset / sizeof(TagAddressType));
    *completionAddress = heaplessStateInit ? 2 : 1;

    memoryManager->handleFenceCompletion(allocation);

    uint64_t expectedAddress = castToUint64(const_cast<TagAddressType *>(defaultEngine.commandStreamReceiver->getTagAddress())) +
                               TagAllocationLayout::completionFenceOffset +
                               postSyncOffset;
    uint64_t expectedValue = heaplessStateInit ? 3 : 2;

    EXPECT_EQ(2u, mock->waitUserFenceCall.called);
    EXPECT_EQ(expectedAddress, mock->waitUserFenceCall.address);
    EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);

    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmCommandStreamMultiTileMemExecTest, GivenDrmSupportsCompletionFenceAndVmBindWhenHandlingCompletionAndOneContextIsReadyThenExpectOneWaitCall) {
    EngineControl &defaultEngine = device->getDefaultEngine();
    EXPECT_EQ(2u, defaultEngine.commandStreamReceiver->getActivePartitions());

    uint32_t postSyncOffset = defaultEngine.commandStreamReceiver->getImmWritePostSyncWriteOffset();
    EXPECT_NE(0u, postSyncOffset);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, 1024, AllocationType::commandBuffer});
    allocation->updateTaskCount(2, defaultEngine.osContext->getContextId());

    volatile TagAddressType *completionAddress = defaultEngine.commandStreamReceiver->getTagAddress();
    completionAddress += (TagAllocationLayout::completionFenceOffset / sizeof(TagAddressType));
    *completionAddress = 2; // 1st context is ready
    completionAddress += (postSyncOffset / sizeof(TagAddressType));
    *completionAddress = 1;

    memoryManager->handleFenceCompletion(allocation);

    uint64_t expectedAddress = castToUint64(const_cast<TagAddressType *>(defaultEngine.commandStreamReceiver->getTagAddress())) +
                               TagAllocationLayout::completionFenceOffset +
                               postSyncOffset;
    constexpr uint64_t expectedValue = 2;

    EXPECT_EQ(1u, mock->waitUserFenceCall.called);
    EXPECT_EQ(expectedAddress, mock->waitUserFenceCall.address);
    EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);

    memoryManager->freeGraphicsMemory(allocation);
}
