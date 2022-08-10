/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/windows/wddm_direct_submission.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/unit_test/mocks/windows/mock_wddm_direct_submission.h"

using namespace NEO;

template <PreemptionMode preemptionMode>
struct WddmDirectSubmissionFixture : public WddmFixture {
    void SetUp() override {
        VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        DebugManager.flags.ForcePreemptionMode.set(preemptionMode);

        WddmFixture::SetUp();

        wddm->wddmInterface.reset(new WddmMockInterface20(*wddm));
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.get());

        executionEnvironment->memoryManager.reset(new WddmMemoryManager{*executionEnvironment});
        device.reset(MockDevice::create<MockDevice>(executionEnvironment.get(), 0u));

        osContext = static_cast<OsContextWin *>(device->getDefaultEngine().osContext);

        wddmMockInterface->createMonitoredFence(*osContext);
    }

    DebugManagerStateRestore restorer;
    WddmMockInterface20 *wddmMockInterface;
    OsContextWin *osContext = nullptr;
    std::unique_ptr<MockDevice> device;
};

using WddmDirectSubmissionTest = WddmDirectSubmissionFixture<PreemptionMode::ThreadGroup>;

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenDirectIsInitializedAndStartedThenExpectProperCommandsDispatched) {
    std::unique_ptr<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>> wddmDirectSubmission =
        std::make_unique<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>>(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(1u, wddmDirectSubmission->commandBufferHeader->NeedsMidBatchPreEmptionSupport);

    bool ret = wddmDirectSubmission->initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(wddmDirectSubmission->ringStart);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffers[0].ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffers[1].ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->semaphores);

    EXPECT_EQ(1u, wddm->makeResidentResult.called);
    EXPECT_EQ(3u, wddm->makeResidentResult.handleCount);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);

    EXPECT_EQ(1u, wddm->submitResult.called);

    EXPECT_NE(0u, wddmDirectSubmission->ringCommandStream.getUsed());

    *wddmDirectSubmission->ringFence.cpuAddress = 1ull;
    wddmDirectSubmission->ringBuffers[wddmDirectSubmission->currentRingBuffer].completionFence = 2ull;

    wddmDirectSubmission.reset(nullptr);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(1u, wddmMockInterface->destroyMonitorFenceCalled);
}

using WddmDirectSubmissionNoPreemptionTest = WddmDirectSubmissionFixture<PreemptionMode::Disabled>;
HWTEST_F(WddmDirectSubmissionNoPreemptionTest, givenWddmWhenDirectIsInitializedAndNotStartedThenExpectNoCommandsDispatched) {
    std::unique_ptr<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>> wddmDirectSubmission =
        std::make_unique<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>>(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(0u, wddmDirectSubmission->commandBufferHeader->NeedsMidBatchPreEmptionSupport);

    bool ret = wddmDirectSubmission->initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(wddmDirectSubmission->ringStart);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffers[0].ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffers[1].ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->semaphores);

    EXPECT_EQ(1u, wddm->makeResidentResult.called);
    EXPECT_EQ(3u, wddm->makeResidentResult.handleCount);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);

    EXPECT_EQ(0u, wddm->submitResult.called);

    EXPECT_EQ(0u, wddmDirectSubmission->ringCommandStream.getUsed());

    wddmDirectSubmission.reset(nullptr);
    EXPECT_EQ(0u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(1u, wddmMockInterface->destroyMonitorFenceCalled);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSubmitingCmdBufferThenExpectPassWddmContextAndProperHeader) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(false, false);
    EXPECT_TRUE(ret);

    uint64_t gpuAddress = 0xFF00FF000;
    size_t size = 0xF0;
    ret = wddmDirectSubmission.submit(gpuAddress, size);
    EXPECT_TRUE(ret);
    EXPECT_EQ(1u, wddm->submitResult.called);
    EXPECT_EQ(gpuAddress, wddm->submitResult.commandBufferSubmitted);
    EXPECT_EQ(size, wddm->submitResult.size);
    EXPECT_EQ(wddmDirectSubmission.commandBufferHeader.get(), wddm->submitResult.commandHeaderSubmitted);
    EXPECT_EQ(&wddmDirectSubmission.ringFence, wddm->submitResult.submitArgs.monitorFence);
    EXPECT_EQ(osContext->getWddmContextHandle(), wddm->submitResult.submitArgs.contextHandle);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenAllocateOsResourcesThenExpectRingMonitorFenceCreatedAndAllocationsResident) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.allocateResources();
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);
    EXPECT_EQ(1u, wddm->makeResidentResult.called);
    EXPECT_EQ(3u, wddm->makeResidentResult.handleCount);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenAllocateOsResourcesFenceCreationFailsThenExpectRingMonitorFenceNotCreatedAndAllocationsNotResident) {
    MemoryManager *memoryManager = device->getExecutionEnvironment()->memoryManager.get();
    const auto allocationSize = MemoryConstants::pageSize;
    const AllocationProperties commandStreamAllocationProperties{device->getRootDeviceIndex(), allocationSize,
                                                                 AllocationType::RING_BUFFER, device->getDeviceBitfield()};
    GraphicsAllocation *ringBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    ASSERT_NE(nullptr, ringBuffer);

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    DirectSubmissionAllocations allocations;
    allocations.push_back(ringBuffer);

    wddmMockInterface->createMonitoredFenceCalledFail = true;

    bool ret = wddmDirectSubmission.allocateOsResources();
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);
    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    EXPECT_EQ(0u, wddm->makeResidentResult.handleCount);

    memoryManager->freeGraphicsMemory(ringBuffer);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenAllocateOsResourcesResidencyFailsThenExpectRingMonitorFenceCreatedAndAllocationsNotResident) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    wddm->callBaseMakeResident = false;
    wddm->makeResidentStatus = false;

    bool ret = wddmDirectSubmission.allocateResources();
    EXPECT_FALSE(ret);

    EXPECT_EQ(0u, wddmMockInterface->createMonitoredFenceCalled);
    //expect 2 makeResident calls, due to fail on 1st and then retry (which also fails)
    EXPECT_EQ(2u, wddm->makeResidentResult.called);
    EXPECT_EQ(3u, wddm->makeResidentResult.handleCount);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenGettingTagDataThenExpectContextMonitorFence) {
    uint64_t address = 0xFF00FF0000ull;
    uint64_t value = 0x12345678ull;
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();
    contextFence.gpuAddress = address;
    contextFence.currentFenceValue = value;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    TagData tagData;
    wddmDirectSubmission.getTagAddressValue(tagData);

    EXPECT_EQ(address, tagData.tagAddress);
    EXPECT_EQ(value, tagData.tagValue);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenHandleResidencyThenExpectWddmWaitOnPaginfFenceFromCpuCalled) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    wddmDirectSubmission.handleResidency();

    EXPECT_EQ(1u, wddm->waitOnPagingFenceFromCpuResult.called);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenHandlingRingBufferCompletionThenExpectWaitFromCpuWithCorrectFenceValue) {
    uint64_t address = 0xFF00FF0000ull;
    uint64_t value = 0x12345678ull;
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();
    contextFence.gpuAddress = address;
    contextFence.currentFenceValue = value;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    uint64_t completionValue = 0x12345679ull;
    wddmDirectSubmission.handleCompletionFence(completionValue, contextFence);

    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(completionValue, wddm->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(address, wddm->waitFromCpuResult.monitoredFence->gpuAddress);
    EXPECT_EQ(value, wddm->waitFromCpuResult.monitoredFence->currentFenceValue);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenCallIsCompleteThenProperValueIsReturned) {
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    *contextFence.cpuAddress = 0u;
    wddmDirectSubmission.ringBuffers[0].completionFence = 1u;
    EXPECT_FALSE(wddmDirectSubmission.isCompleted(0u));

    *contextFence.cpuAddress = 1u;
    EXPECT_TRUE(wddmDirectSubmission.isCompleted(0u));

    wddmDirectSubmission.ringBuffers[0].completionFence = 0u;
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferStartedThenExpectDispatchSwitchCommandsLinearStreamUpdated) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffers[0].ringBuffer->getGpuAddress() + usedSpace;

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers();
    EXPECT_EQ(expectedGpuVa, gpuVa);
    EXPECT_EQ(wddmDirectSubmission.ringBuffers[1].ringBuffer, wddmDirectSubmission.ringCommandStream.getGraphicsAllocation());

    LinearStream tmpCmdBuffer;
    tmpCmdBuffer.replaceBuffer(wddmDirectSubmission.ringBuffers[0].ringBuffer->getUnderlyingBuffer(),
                               wddmDirectSubmission.ringCommandStream.getMaxAvailableSpace());
    tmpCmdBuffer.getSpace(usedSpace + wddmDirectSubmission.getSizeSwitchRingBufferSection());
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(tmpCmdBuffer, usedSpace);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    auto gmmHelper = device->getGmmHelper();
    uint64_t actualGpuVa = gmmHelper->canonize(bbStart->getBatchBufferStartAddress());
    EXPECT_EQ(wddmDirectSubmission.ringBuffers[1].ringBuffer->getGpuAddress(), actualGpuVa);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferNotStartedThenExpectNoSwitchCommandsLinearStreamUpdated) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(false, false);
    EXPECT_TRUE(ret);

    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    EXPECT_EQ(0u, usedSpace);

    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffers[0].ringBuffer->getGpuAddress();

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers();
    EXPECT_EQ(expectedGpuVa, gpuVa);
    EXPECT_EQ(wddmDirectSubmission.ringBuffers[1].ringBuffer, wddmDirectSubmission.ringCommandStream.getGraphicsAllocation());

    LinearStream tmpCmdBuffer;
    tmpCmdBuffer.replaceBuffer(wddmDirectSubmission.ringBuffers[0].ringBuffer->getUnderlyingBuffer(),
                               wddmDirectSubmission.ringCommandStream.getMaxAvailableSpace());
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(tmpCmdBuffer, 0u);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(nullptr, bbStart);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferStartedAndWaitFenceUpdateThenExpectNewRingBufferAllocated) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    uint64_t expectedWaitFence = 0x10ull;
    wddmDirectSubmission.ringBuffers[1u].completionFence = expectedWaitFence;
    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffers[0].ringBuffer->getGpuAddress() + usedSpace;

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers();
    EXPECT_EQ(expectedGpuVa, gpuVa);
    EXPECT_EQ(wddmDirectSubmission.ringBuffers[2u].ringBuffer, wddmDirectSubmission.ringCommandStream.getGraphicsAllocation());

    LinearStream tmpCmdBuffer;
    tmpCmdBuffer.replaceBuffer(wddmDirectSubmission.ringBuffers[0].ringBuffer->getUnderlyingBuffer(),
                               wddmDirectSubmission.ringCommandStream.getMaxAvailableSpace());
    tmpCmdBuffer.getSpace(usedSpace + wddmDirectSubmission.getSizeSwitchRingBufferSection());
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(tmpCmdBuffer, usedSpace);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    auto gmmHelper = device->getGmmHelper();
    uint64_t actualGpuVa = gmmHelper->canonize(bbStart->getBatchBufferStartAddress());
    EXPECT_EQ(wddmDirectSubmission.ringBuffers[2u].ringBuffer->getGpuAddress(), actualGpuVa);

    EXPECT_EQ(0u, wddm->waitFromCpuResult.called);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferStartedAndWaitFenceUpdateThenExpectWaitCalled) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionMaxRingBuffers.set(2u);

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    uint64_t expectedWaitFence = 0x10ull;
    wddmDirectSubmission.ringBuffers[1u].completionFence = expectedWaitFence;
    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffers[0].ringBuffer->getGpuAddress() + usedSpace;

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers();
    EXPECT_EQ(expectedGpuVa, gpuVa);
    EXPECT_EQ(wddmDirectSubmission.ringBuffers.size(), 2u);
    EXPECT_EQ(wddmDirectSubmission.ringBuffers[1u].ringBuffer, wddmDirectSubmission.ringCommandStream.getGraphicsAllocation());

    LinearStream tmpCmdBuffer;
    tmpCmdBuffer.replaceBuffer(wddmDirectSubmission.ringBuffers[0].ringBuffer->getUnderlyingBuffer(),
                               wddmDirectSubmission.ringCommandStream.getMaxAvailableSpace());
    tmpCmdBuffer.getSpace(usedSpace + wddmDirectSubmission.getSizeSwitchRingBufferSection());
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(tmpCmdBuffer, usedSpace);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    auto gmmHelper = device->getGmmHelper();
    uint64_t actualGpuVa = gmmHelper->canonize(bbStart->getBatchBufferStartAddress());
    EXPECT_EQ(wddmDirectSubmission.ringBuffers[1u].ringBuffer->getGpuAddress(), actualGpuVa);

    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(expectedWaitFence, wddm->waitFromCpuResult.uint64ParamPassed);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenUpdatingTagValueThenExpectcompletionFenceUpdated) {
    uint64_t address = 0xFF00FF0000ull;
    uint64_t value = 0x12345678ull;
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();
    contextFence.gpuAddress = address;
    contextFence.currentFenceValue = value;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    uint64_t actualTagValue = wddmDirectSubmission.updateTagValue();
    EXPECT_EQ(value, actualTagValue);
    EXPECT_EQ(value + 1, contextFence.currentFenceValue);
    EXPECT_EQ(value, wddmDirectSubmission.ringBuffers[wddmDirectSubmission.currentRingBuffer].completionFence);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmResidencyEnabledWhenCreatingDestroyingThenSubmitterNotifiesResidencyLogger) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    DebugManager.flags.WddmResidencyLogger.set(1);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    wddm->createPagingFenceLogger();

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);

    std::unique_ptr<MockWddmDirectSubmission<FamilyType, Dispatcher>> wddmSubmission =
        std::make_unique<MockWddmDirectSubmission<FamilyType, Dispatcher>>(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);

    wddmSubmission.reset(nullptr);

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(3u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmResidencyEnabledWhenAllocatingResourcesThenSubmitterNotifiesResidencyLogger) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    DebugManager.flags.WddmResidencyLogger.set(1);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    wddm->callBaseMakeResident = true;
    wddm->createPagingFenceLogger();

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);

    bool ret = wddmDirectSubmission.allocateResources();
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(10u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmResidencyEnabledWhenHandleResidencyThenSubmitterNotifiesResidencyLogger) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    DebugManager.flags.WddmResidencyLogger.set(1);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddm->createPagingFenceLogger();

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);

    bool ret = wddmDirectSubmission.handleResidency();
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(5u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmResidencyEnabledWhenSubmitToGpuThenSubmitterNotifiesResidencyLogger) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    DebugManager.flags.WddmResidencyLogger.set(1);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddm->createPagingFenceLogger();

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);

    uint64_t gpuAddress = 0xF000;
    size_t size = 0xFF000;
    bool ret = wddmDirectSubmission.submit(gpuAddress, size);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}
