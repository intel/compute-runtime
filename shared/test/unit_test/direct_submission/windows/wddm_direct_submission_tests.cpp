/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_os_context_win.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/direct_submission/direct_submission_controller_mock.h"
#include "shared/test/unit_test/mocks/windows/mock_wddm_direct_submission.h"

extern uint64_t cpuFence;

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> sfenceCounter;
extern std::atomic<uint32_t> mfenceCounter;
} // namespace CpuIntrinsicsTests

namespace NEO {

namespace SysCalls {
extern size_t timeBeginPeriodCalled;
extern MMRESULT timeBeginPeriodLastValue;
extern size_t timeEndPeriodCalled;
extern MMRESULT timeEndPeriodLastValue;
} // namespace SysCalls

template <PreemptionMode preemptionMode>
struct WddmDirectSubmissionFixture : public WddmFixture {
    void SetUp() override {
        VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        debugManager.flags.ForcePreemptionMode.set(preemptionMode);

        WddmFixture::SetUp();

        wddm->wddmInterface.reset(new WddmMockInterface20(*wddm));
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.get());

        executionEnvironment->memoryManager.reset(new WddmMemoryManager{*executionEnvironment});
        device.reset(MockDevice::create<MockDevice>(executionEnvironment.get(), 0u));

        osContext = static_cast<OsContextWin *>(device->getDefaultEngine().osContext);

        wddmMockInterface->createMonitoredFence(*osContext);
    }

    DebugManagerStateRestore restorer;

    WddmMockInterface20 *wddmMockInterface = nullptr;
    OsContextWin *osContext = nullptr;
    std::unique_ptr<MockDevice> device;
};

using WddmDirectSubmissionTest = WddmDirectSubmissionFixture<PreemptionMode::ThreadGroup>;

struct WddmDirectSubmissionWithMockGdiDllFixture : public WddmFixtureWithMockGdiDll {
    void setUp() {
        debugManager.flags.ForcePreemptionMode.set(PreemptionMode::ThreadGroup);

        WddmFixtureWithMockGdiDll::setUp();
        init();
        device.reset(MockDevice::create<MockDevice>(executionEnvironment.get(), 0u));
        osContextWin = static_cast<NEO::OsContextWin *>(device->getDefaultEngine().osContext);
        osContextWin->ensureContextInitialized(false);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<MockDevice> device;
    NEO::OsContextWin *osContextWin = nullptr;
};

using WddmDirectSubmissionWithMockGdiDllTest = Test<WddmDirectSubmissionWithMockGdiDllFixture>;

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenDirectIsInitializedAndStartedThenExpectProperCommandsDispatched) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    std::unique_ptr<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>> wddmDirectSubmission =
        std::make_unique<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>>(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission->globalFenceAllocation = nullptr;

    EXPECT_EQ(1u, wddmDirectSubmission->commandBufferHeader->NeedsMidBatchPreEmptionSupport);

    bool ret = wddmDirectSubmission->initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(wddmDirectSubmission->ringStart);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffers[0].ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffers[1].ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->semaphores);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t expectedAllocationsCnt = 4;
    if (gfxCoreHelper.isRelaxedOrderingSupported()) {
        expectedAllocationsCnt += 2;
    }
    EXPECT_EQ(expectedAllocationsCnt, wddm->makeResidentResult.handleCount);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);

    EXPECT_EQ(1u, wddm->submitResult.called);

    EXPECT_NE(0u, wddmDirectSubmission->ringCommandStream.getUsed());

    *wddmDirectSubmission->ringFence.cpuAddress = 1ull;
    wddmDirectSubmission->ringBuffers[wddmDirectSubmission->currentRingBuffer].completionFence = 2ull;

    wddmDirectSubmission.reset(nullptr);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(1u, wddmMockInterface->destroyMonitorFenceCalled);
}

struct WddmDirectSubmissionGlobalFenceTest : public WddmDirectSubmissionTest {
    void SetUp() override {
        debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(1);
        WddmDirectSubmissionTest::SetUp();
    }
    DebugManagerStateRestore restorer;
};

HWTEST_F(WddmDirectSubmissionGlobalFenceTest, givenWddmWhenDirectIsInitializedWithMiMemFenceSupportedThenMakeGlobalFenceResident) {
    std::unique_ptr<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>> wddmDirectSubmission =
        std::make_unique<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>>(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(1u, wddmDirectSubmission->commandBufferHeader->NeedsMidBatchPreEmptionSupport);

    bool ret = wddmDirectSubmission->initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(wddmDirectSubmission->ringStart);

    auto isFenceRequired = device->getGfxCoreHelper().isFenceAllocationRequired(device->getHardwareInfo(), device->getProductHelper());
    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto isHeaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo));
    if (isFenceRequired && !isHeaplessStateInit) {
        EXPECT_EQ(1u, wddm->makeResidentResult.handleCount);
        EXPECT_TRUE(device->getDefaultEngine().commandStreamReceiver->getGlobalFenceAllocation()->isExplicitlyMadeResident());
    }
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

    bool ret = wddmDirectSubmission->initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(wddmDirectSubmission->ringStart);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffers[0].ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffers[1].ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->semaphores);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t expectedAllocationsCnt = 4;
    if (gfxCoreHelper.isRelaxedOrderingSupported()) {
        expectedAllocationsCnt += 2;
    }
    EXPECT_EQ(expectedAllocationsCnt, wddm->makeResidentResult.handleCount);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);

    EXPECT_EQ(0u, wddm->submitResult.called);

    EXPECT_EQ(0u, wddmDirectSubmission->ringCommandStream.getUsed());

    wddmDirectSubmission.reset(nullptr);
    EXPECT_EQ(0u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(1u, wddmMockInterface->destroyMonitorFenceCalled);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSubmitingCmdBufferThenExpectPassWddmContextAndProperHeader) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(false);
    EXPECT_TRUE(ret);

    uint64_t gpuAddress = 0xFF00FF000;
    size_t size = 0xF0;
    ret = wddmDirectSubmission.submit(gpuAddress, size, nullptr);
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
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t expectedAllocationsCnt = 4;
    if (gfxCoreHelper.isRelaxedOrderingSupported()) {
        expectedAllocationsCnt += 2;
    }

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);
    EXPECT_EQ(expectedAllocationsCnt, wddm->makeResidentResult.handleCount);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenAllocateOsResourcesFenceCreationFailsThenExpectRingMonitorFenceNotCreatedAndAllocationsNotResident) {
    MemoryManager *memoryManager = device->getExecutionEnvironment()->memoryManager.get();
    const auto allocationSize = MemoryConstants::pageSize;
    const AllocationProperties commandStreamAllocationProperties{device->getRootDeviceIndex(), allocationSize,
                                                                 AllocationType::ringBuffer, device->getDeviceBitfield()};
    GraphicsAllocation *ringBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    ASSERT_NE(nullptr, ringBuffer);

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    DirectSubmissionAllocations allocations;
    allocations.push_back(ringBuffer);

    wddmMockInterface->createMonitoredFenceCalledFail = true;

    bool ret = wddmDirectSubmission.allocateOsResources();
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);

    memoryManager->freeGraphicsMemory(ringBuffer);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenAllocateOsResourcesThenRingBufferCompletionDataAndTagAddressAreSet) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    wddmDirectSubmission.allocateResources();
    EXPECT_EQ(wddmDirectSubmission.ringBufferEndCompletionTagData.tagAddress, wddmDirectSubmission.semaphoreGpuVa + offsetof(RingSemaphoreData, tagAllocation));
    EXPECT_EQ(wddmDirectSubmission.ringBufferEndCompletionTagData.tagValue, 0u);
    auto expectedTagAddress = reinterpret_cast<volatile TagAddressType *>(reinterpret_cast<uint8_t *>(wddmDirectSubmission.semaphorePtr) + offsetof(RingSemaphoreData, tagAllocation));
    EXPECT_EQ(wddmDirectSubmission.tagAddress, expectedTagAddress);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenAllocateOsResourcesResidencyFailsThenExpectRingMonitorFenceCreatedAndAllocationsNotResident) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    wddm->callBaseMakeResident = false;
    wddm->makeResidentStatus = false;

    bool ret = wddmDirectSubmission.allocateResources();
    EXPECT_FALSE(ret);
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t expectedAllocationsCnt = 4;
    if (gfxCoreHelper.isRelaxedOrderingSupported()) {
        expectedAllocationsCnt += 2;
    }

    EXPECT_EQ(0u, wddmMockInterface->createMonitoredFenceCalled);
    EXPECT_EQ(expectedAllocationsCnt, wddm->makeResidentResult.handleCount);
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
    auto expectedMakeResidentCalled = wddm->makeResidentResult.called + 1;

    wddmDirectSubmission.handleResidency();

    EXPECT_EQ(expectedMakeResidentCalled, wddm->waitOnPagingFenceFromCpuResult.called);
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
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    TagAddressType switchTag = 0u;

    wddmDirectSubmission.tagAddress = &switchTag;
    wddmDirectSubmission.ringBuffers[0].completionFenceForSwitch = 1u;
    EXPECT_FALSE(wddmDirectSubmission.isCompleted(0u));

    switchTag = 1u;
    EXPECT_TRUE(wddmDirectSubmission.isCompleted(0u));

    wddmDirectSubmission.ringBuffers[0].completionFenceForSwitch = 0u;
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferStartedThenExpectDispatchSwitchCommandsLinearStreamUpdated) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffers[0].ringBuffer->getGpuAddress() + usedSpace;

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers(nullptr);
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

    bool ret = wddmDirectSubmission.initialize(false);
    EXPECT_TRUE(ret);

    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    EXPECT_EQ(0u, usedSpace);

    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffers[0].ringBuffer->getGpuAddress();

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers(nullptr);
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

HWTEST_F(WddmDirectSubmissionTest, givenWddmDirectSubmissionWhenDispatchMonitorFenceAndNotifyKmdDisabledThenProgramPipeControlWithoutNotifyEnable) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(false);
    EXPECT_TRUE(ret);
    auto currOffset = wddmDirectSubmission.ringCommandStream.getUsed();
    wddmDirectSubmission.flushMonitorFence(false);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(wddmDirectSubmission.ringCommandStream, currOffset);
    auto pipeControls = hwParse.getCommandsList<PIPE_CONTROL>();
    bool isNotifyEnabledProgrammed = false;
    for (auto &pipeControl : pipeControls) {
        isNotifyEnabledProgrammed |= reinterpret_cast<PIPE_CONTROL *>(pipeControl)->getNotifyEnable();
    }
    EXPECT_FALSE(isNotifyEnabledProgrammed);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmDirectSubmissionWhenDispatchMonitorFenceAndNotifyKmdEnabledThenProgramPipeControlWithNotifyEnable) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(false);
    EXPECT_TRUE(ret);
    auto currOffset = wddmDirectSubmission.ringCommandStream.getUsed();
    wddmDirectSubmission.flushMonitorFence(true);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(wddmDirectSubmission.ringCommandStream, currOffset);
    auto pipeControls = hwParse.getCommandsList<PIPE_CONTROL>();
    bool isNotifyEnabledProgrammed = false;
    for (auto &pipeControl : pipeControls) {
        isNotifyEnabledProgrammed |= reinterpret_cast<PIPE_CONTROL *>(pipeControl)->getNotifyEnable();
    }
    EXPECT_TRUE(isNotifyEnabledProgrammed);
}

HWTEST_F(WddmDirectSubmissionTest, givenDirectSubmissionNewResourceTlbFlushWhenHandleNewResourcesSubmissionThenDispatchProperCommands) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionNewResourceTlbFlush.set(1);
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(false);
    EXPECT_TRUE(ret);

    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    EXPECT_EQ(0u, usedSpace);

    EXPECT_TRUE(wddmDirectSubmission.isNewResourceHandleNeeded());

    wddmDirectSubmission.handleNewResourcesSubmission();

    EXPECT_EQ(wddmDirectSubmission.ringCommandStream.getUsed(), usedSpace + wddmDirectSubmission.getSizeNewResourceHandler());
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferStartedAndWaitFenceUpdateThenExpectNewRingBufferAllocated) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    VariableBackup<uint64_t> cpuFenceBackup(&cpuFence, 0);

    bool ret = wddmDirectSubmission.initialize(true);
    EXPECT_TRUE(ret);
    uint64_t expectedWaitFence = 0x10ull;
    wddmDirectSubmission.ringBuffers[1u].completionFenceForSwitch = expectedWaitFence;
    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffers[0].ringBuffer->getGpuAddress() + usedSpace;

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers(nullptr);
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

HWTEST_F(WddmDirectSubmissionTest, givenWddmUllsWhenMaxRingBufferSizeReachedAndAllRingBuffersNotCompletedThenNextRingBufferSelected) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    VariableBackup<uint64_t> cpuFenceBackup(&cpuFence, 0);

    wddmDirectSubmission.initialize(true);
    uint64_t expectedWaitFence = 0x10ull;
    std::vector<std::unique_ptr<MockGraphicsAllocation>> mockAllocs;
    wddmDirectSubmission.maxRingBufferCount = static_cast<uint32_t>(wddmDirectSubmission.ringBuffers.size());

    auto ringBufferVectorSizeBefore = wddmDirectSubmission.ringBuffers.size();
    auto currentRingBufferBefore = wddmDirectSubmission.currentRingBuffer;

    for (auto &ringBuffer : wddmDirectSubmission.ringBuffers) {
        ringBuffer.completionFenceForSwitch = expectedWaitFence;
    }
    wddmDirectSubmission.switchRingBuffers(nullptr);
    EXPECT_EQ(ringBufferVectorSizeBefore, wddmDirectSubmission.ringBuffers.size());
    EXPECT_EQ((currentRingBufferBefore + 1) % wddmDirectSubmission.ringBuffers.size(), wddmDirectSubmission.currentRingBuffer);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferStartedAndWaitFenceUpdateThenExpectWaitNotCalled) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionMaxRingBuffers.set(2u);

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(true);
    EXPECT_TRUE(ret);
    uint64_t expectedWaitFence = 0x10ull;
    wddmDirectSubmission.ringBuffers[1u].completionFence = expectedWaitFence;
    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffers[0].ringBuffer->getGpuAddress() + usedSpace;

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers(nullptr);
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

    EXPECT_EQ(0u, wddm->waitFromCpuResult.called);
    EXPECT_NE(expectedWaitFence, wddm->waitFromCpuResult.uint64ParamPassed);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmDisableMonitorFenceAndStallingCmdsWhenUpdatingTagValueThenUpdateCompletionFence) {
    uint64_t address = 0xFF00FF0000ull;
    uint64_t value = 0x12345678ull;
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();
    contextFence.gpuAddress = address;
    contextFence.currentFenceValue = value;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(wddmDirectSubmission.allocateOsResources());

    uint64_t actualTagValue = wddmDirectSubmission.updateTagValue(wddmDirectSubmission.dispatchMonitorFenceRequired(true));
    EXPECT_EQ(value, actualTagValue);
    EXPECT_EQ(value + 1, contextFence.currentFenceValue);
    EXPECT_EQ(value, wddmDirectSubmission.ringBuffers[wddmDirectSubmission.currentRingBuffer].completionFence);
}

HWTEST_F(WddmDirectSubmissionWithMockGdiDllTest, givenNoMonitorFenceHangDetectedWhenUpdatingTagValueThenReturnUpdatedFenceCounter) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDetectGpuHang.set(1);

    VariableBackup<bool> backupMonitorFenceCreateSelector(getMonitorFenceCpuAddressSelectorFcn());

    MonitoredFence &contextFence = osContextWin->getResidencyController().getMonitoredFence();
    VariableBackup<volatile uint64_t> backupWddmMonitorFence(contextFence.cpuAddress);
    *contextFence.cpuAddress = 1;
    contextFence.currentFenceValue = 2u;

    *getMonitorFenceCpuAddressSelectorFcn() = true;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(wddmDirectSubmission.detectGpuHang);
    EXPECT_TRUE(wddmDirectSubmission.allocateOsResources());

    VariableBackup<volatile uint64_t> backupRingMonitorFence(wddmDirectSubmission.ringFence.cpuAddress);
    *wddmDirectSubmission.ringFence.cpuAddress = 1;

    uint64_t actualTagValue = wddmDirectSubmission.updateTagValue(true);
    EXPECT_EQ(2u, actualTagValue);
}

HWTEST_F(WddmDirectSubmissionWithMockGdiDllTest, givenWddmMonitorFenceHangDetectedWhenUpdatingTagValueThenReturnFail) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDetectGpuHang.set(1);

    VariableBackup<bool> backupMonitorFenceCreateSelector(getMonitorFenceCpuAddressSelectorFcn());

    MonitoredFence &contextFence = osContextWin->getResidencyController().getMonitoredFence();
    VariableBackup<volatile uint64_t> backupWddmMonitorFence(contextFence.cpuAddress);
    *contextFence.cpuAddress = std::numeric_limits<uint64_t>::max();

    *getMonitorFenceCpuAddressSelectorFcn() = true;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(wddmDirectSubmission.detectGpuHang);
    EXPECT_TRUE(wddmDirectSubmission.allocateOsResources());

    uint64_t actualTagValue = wddmDirectSubmission.updateTagValue(true);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), actualTagValue);
}

HWTEST_F(WddmDirectSubmissionWithMockGdiDllTest, givenRingMonitorFenceHangDetectedWhenUpdatingTagValueThenReturnFail) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDetectGpuHang.set(1);

    VariableBackup<bool> backupMonitorFenceCreateSelector(getMonitorFenceCpuAddressSelectorFcn());

    *getMonitorFenceCpuAddressSelectorFcn() = true;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(wddmDirectSubmission.detectGpuHang);
    EXPECT_TRUE(wddmDirectSubmission.allocateOsResources());

    VariableBackup<volatile uint64_t> backupRingMonitorFence(wddmDirectSubmission.ringFence.cpuAddress);
    *wddmDirectSubmission.ringFence.cpuAddress = std::numeric_limits<uint64_t>::max();

    uint64_t actualTagValue = wddmDirectSubmission.updateTagValue(true);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), actualTagValue);
}

HWTEST_F(WddmDirectSubmissionTest, givenDetectGpuFalseAndRequiredMonitorFenceWhenCallUpdateTagValueThenCurrentFenceValueIsReturned) {
    uint64_t value = 0x12345678ull;
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();
    contextFence.currentFenceValue = value;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.detectGpuHang = false;

    uint64_t actualTagValue = wddmDirectSubmission.updateTagValue(false);
    EXPECT_EQ(value, actualTagValue);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmDisableMonitorFenceWhenHandleStopRingBufferThenExpectCompletionFenceUpdated) {
    uint64_t value = 0x12345678ull;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.ringBufferEndCompletionTagData.tagValue = value;
    wddmDirectSubmission.handleStopRingBuffer();
    EXPECT_EQ(value + 1, wddmDirectSubmission.ringBufferEndCompletionTagData.tagValue);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmDisableMonitorFenceWhenHandleSwitchRingBufferThenExpectCompletionFenceUpdated) {
    uint64_t value = 0x12345678ull;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.ringBufferEndCompletionTagData.tagValue = value;
    wddmDirectSubmission.handleSwitchRingBuffers(nullptr);
    EXPECT_EQ(value + 1, wddmDirectSubmission.ringBuffers[wddmDirectSubmission.currentRingBuffer].completionFenceForSwitch);
    EXPECT_EQ(value + 1, wddmDirectSubmission.ringBufferEndCompletionTagData.tagValue);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmResidencyEnabledWhenCreatingDestroyingThenSubmitterNotifiesResidencyLogger) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.WddmResidencyLogger.set(1);

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
    debugManager.flags.WddmResidencyLogger.set(1);

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
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmResidencyEnabledWhenHandleResidencyThenSubmitterNotifiesResidencyLogger) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.WddmResidencyLogger.set(1);

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
    debugManager.flags.WddmResidencyLogger.set(1);

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

    bool ret = wddmDirectSubmission.submit(gpuAddress, size, nullptr);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(5u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}

HWTEST_F(WddmDirectSubmissionTest, givenMiMemFenceRequiredThenGpuVaForAdditionalSynchronizationWAIsSet) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    ASSERT_NE(nullptr, wddmDirectSubmission.completionFenceAllocation);
    if (wddmDirectSubmission.miMemFenceRequired) {
        EXPECT_EQ(wddmDirectSubmission.completionFenceAllocation->getGpuAddress() + 8u, wddmDirectSubmission.gpuVaForAdditionalSynchronizationWA);
    } else {
        EXPECT_EQ(0u, wddmDirectSubmission.gpuVaForAdditionalSynchronizationWA);
    }
}

HWTEST_F(WddmDirectSubmissionTest,
         givenRenderDirectSubmissionWithDisabledMonitorFenceWhenHasStallingCommandDispatchedThenDispatchNoPostSyncOperation) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using Dispatcher = RenderDispatcher<FamilyType>;

    BatchBuffer batchBuffer;
    GraphicsAllocation *clientCommandBuffer = nullptr;
    std::unique_ptr<LinearStream> clientStream;

    auto memoryManager = executionEnvironment->memoryManager.get();
    const AllocationProperties commandBufferProperties{device->getRootDeviceIndex(), 0x1000,
                                                       AllocationType::commandBuffer, device->getDeviceBitfield()};
    clientCommandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);
    ASSERT_NE(nullptr, clientCommandBuffer);

    clientStream = std::make_unique<LinearStream>(clientCommandBuffer);
    clientStream->getSpace(0x40);

    memset(clientStream->getCpuBase(), 0, 0x20);

    batchBuffer.endCmdPtr = ptrOffset(clientStream->getCpuBase(), 0x20);
    batchBuffer.commandBufferAllocation = clientCommandBuffer;
    batchBuffer.usedSize = 0x40;
    batchBuffer.taskStartAddress = clientCommandBuffer->getGpuAddress();
    batchBuffer.stream = clientStream.get();
    batchBuffer.hasStallingCmds = true;

    FlushStampTracker flushStamp(true);

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(true);
    EXPECT_TRUE(ret);

    size_t sizeUsedBefore = wddmDirectSubmission.ringCommandStream.getUsed();
    ret = wddmDirectSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(wddmDirectSubmission.ringCommandStream, sizeUsedBefore);
    hwParse.findHardwareCommands<FamilyType>();

    auto &monitorFence = osContext->getResidencyController().getMonitoredFence();

    bool foundFenceUpdate = false;
    for (auto it = hwParse.pipeControlList.begin(); it != hwParse.pipeControlList.end(); it++) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            uint64_t pipeControlPostSyncAddress = UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl);
            if (pipeControlPostSyncAddress == monitorFence.gpuAddress) {
                foundFenceUpdate = true;
                EXPECT_TRUE(pipeControl->getNotifyEnable());
                break;
            }
        }
    }
    EXPECT_FALSE(foundFenceUpdate);

    memoryManager->freeGraphicsMemory(clientCommandBuffer);
}

HWTEST_F(WddmDirectSubmissionTest,
         givenRenderDirectSubmissionWithDisabledMonitorFenceWhenMonitorFenceDispatchedThenDispatchPostSyncOperationWithoutNotifyFlag) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using Dispatcher = RenderDispatcher<FamilyType>;

    BatchBuffer batchBuffer;
    GraphicsAllocation *clientCommandBuffer = nullptr;
    std::unique_ptr<LinearStream> clientStream;

    auto memoryManager = executionEnvironment->memoryManager.get();
    const AllocationProperties commandBufferProperties{device->getRootDeviceIndex(), 0x1000,
                                                       AllocationType::commandBuffer, device->getDeviceBitfield()};
    clientCommandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);
    ASSERT_NE(nullptr, clientCommandBuffer);

    clientStream = std::make_unique<LinearStream>(clientCommandBuffer);
    clientStream->getSpace(0x40);

    memset(clientStream->getCpuBase(), 0, 0x20);

    batchBuffer.endCmdPtr = ptrOffset(clientStream->getCpuBase(), 0x20);
    batchBuffer.commandBufferAllocation = clientCommandBuffer;
    batchBuffer.usedSize = 0x40;
    batchBuffer.taskStartAddress = clientCommandBuffer->getGpuAddress();
    batchBuffer.stream = clientStream.get();
    batchBuffer.dispatchMonitorFence = true;

    FlushStampTracker flushStamp(true);

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = wddmDirectSubmission.initialize(true);
    EXPECT_TRUE(ret);

    size_t sizeUsedBefore = wddmDirectSubmission.ringCommandStream.getUsed();
    ret = wddmDirectSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(wddmDirectSubmission.ringCommandStream, sizeUsedBefore);
    hwParse.findHardwareCommands<FamilyType>();

    auto &monitorFence = osContext->getResidencyController().getMonitoredFence();

    bool foundFenceUpdate = false;
    for (auto it = hwParse.pipeControlList.begin(); it != hwParse.pipeControlList.end(); it++) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            uint64_t pipeControlPostSyncAddress = UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl);
            if (pipeControlPostSyncAddress == monitorFence.gpuAddress) {
                foundFenceUpdate = true;
                EXPECT_FALSE(pipeControl->getNotifyEnable());
                break;
            }
        }
    }
    EXPECT_TRUE(foundFenceUpdate);

    memoryManager->freeGraphicsMemory(clientCommandBuffer);
}

HWTEST_F(WddmDirectSubmissionTest,
         givenDisableMonitorFenceIsTrueWhenDispatchArgumentIsTrueThenDispatchMonitorFenceReturnsTrue) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_TRUE(wddmDirectSubmission.dispatchMonitorFenceRequired(true));
}

HWTEST_F(WddmDirectSubmissionTest,
         givenBatchBufferWithThrottleLowWhenCallDispatchCommandBufferThenStoreLastSubmittedThrottle) {

    using Dispatcher = RenderDispatcher<FamilyType>;

    BatchBuffer batchBuffer;
    GraphicsAllocation *clientCommandBuffer = nullptr;
    std::unique_ptr<LinearStream> clientStream;

    auto memoryManager = executionEnvironment->memoryManager.get();
    const AllocationProperties commandBufferProperties{device->getRootDeviceIndex(), 0x1000,
                                                       AllocationType::commandBuffer, device->getDeviceBitfield()};
    clientCommandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);
    ASSERT_NE(nullptr, clientCommandBuffer);

    clientStream = std::make_unique<LinearStream>(clientCommandBuffer);
    clientStream->getSpace(0x40);

    memset(clientStream->getCpuBase(), 0, 0x20);

    batchBuffer.endCmdPtr = ptrOffset(clientStream->getCpuBase(), 0x20);
    batchBuffer.commandBufferAllocation = clientCommandBuffer;
    batchBuffer.usedSize = 0x40;
    batchBuffer.taskStartAddress = clientCommandBuffer->getGpuAddress();
    batchBuffer.stream = clientStream.get();
    batchBuffer.throttle = QueueThrottle::LOW;

    FlushStampTracker flushStamp(true);

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    wddmDirectSubmission.initialize(true);

    bool ret = wddmDirectSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_EQ(QueueThrottle::LOW, wddmDirectSubmission.lastSubmittedThrottle);

    memoryManager->freeGraphicsMemory(clientCommandBuffer);
}

HWTEST_F(WddmDirectSubmissionTest, givenNullPtrResidencyControllerWhenUpdatingResidencyAfterSwitchRingThenReturnBeforeAccessingContextId) {

    using Dispatcher = RenderDispatcher<FamilyType>;
    auto mockOsContextWin = std::make_unique<MockOsContextWin>(*wddm, 0, 0, EngineDescriptorHelper::getDefaultDescriptor());

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.osContextWin = mockOsContextWin.get();
    wddmDirectSubmission.updateMonitorFenceValueForResidencyList(nullptr);
    EXPECT_EQ(mockOsContextWin->getResidencyControllerCalledTimes, 0u);
}

HWTEST_F(WddmDirectSubmissionTest, givenEmptyResidencyControllerWhenUpdatingResidencyAfterSwitchRingThenReturnAfterAccessingContextId) {

    using Dispatcher = RenderDispatcher<FamilyType>;
    auto mockOsContextWin = std::make_unique<MockOsContextWin>(*wddm, 0, 0, EngineDescriptorHelper::getDefaultDescriptor());

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.osContextWin = mockOsContextWin.get();
    ResidencyContainer container;
    wddmDirectSubmission.updateMonitorFenceValueForResidencyList(&container);
    EXPECT_EQ(mockOsContextWin->getResidencyControllerCalledTimes, 1u);
}

HWTEST_F(WddmDirectSubmissionTest, givenResidencyControllerWhenUpdatingResidencyAfterSwitchRingThenAllocationCallUpdateResidency) {

    using Dispatcher = RenderDispatcher<FamilyType>;
    auto mockOsContextWin = std::make_unique<MockOsContextWin>(*wddm, 0, 0, EngineDescriptorHelper::getDefaultDescriptor());

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.osContextWin = mockOsContextWin.get();
    ResidencyContainer container;
    NEO::MockGraphicsAllocation mockGa;
    container.push_back(&mockGa);
    wddmDirectSubmission.updateMonitorFenceValueForResidencyList(&container);
    EXPECT_EQ(mockGa.updateCompletionDataForAllocationAndFragmentsCalledtimes, 1u);
}

HWTEST_F(WddmDirectSubmissionTest, givenDirectSubmissionWhenSwitchingRingBuffersAndResidencyContainerIsNullThenUpdateResidencyNotCalled) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.handleSwitchRingBuffers(nullptr);
    EXPECT_EQ(wddmDirectSubmission.updateMonitorFenceValueForResidencyListCalled, 0u);
}

template <typename GfxFamily, typename Dispatcher>
struct MyMockWddmDirectSubmission : public MockWddmDirectSubmission<GfxFamily, Dispatcher> {
    using BaseClass = MockWddmDirectSubmission<GfxFamily, Dispatcher>;
    using BaseClass::MockWddmDirectSubmission;
    void updateMonitorFenceValueForResidencyList(ResidencyContainer *allocationsForResidency) override {
        lockInTesting = true;
        while (lockInTesting)
            ;
        BaseClass::updateMonitorFenceValueForResidencyList(allocationsForResidency);
    }
    std::atomic<bool> lockInTesting = false;
};

HWTEST_F(WddmDirectSubmissionTest, givenDirectSubmissionWhenSwitchingRingBuffersThenPrevRingIndexPassedForCompletionUpdate) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.currentRingBuffer = 0;
    wddmDirectSubmission.previousRingBuffer = 1;
    wddmDirectSubmission.handleSwitchRingBuffers(nullptr);
    EXPECT_EQ(wddmDirectSubmission.ringBufferForCompletionFence, wddmDirectSubmission.previousRingBuffer);
}

HWTEST_F(WddmDirectSubmissionTest, givenDirectSubmissionWhenUnblockPagingFenceSemaphoreThenCorrectValueAssigned) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    auto pagingFenceValueToWait = 30u;
    auto mockedPagingFence = 20u;

    MockWddmDirectSubmission<FamilyType, Dispatcher> wddmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    wddmDirectSubmission.initialize(false);
    EXPECT_EQ(0u, wddmDirectSubmission.semaphoreData->pagingFenceCounter);
    wddm->mockPagingFence = mockedPagingFence;

    wddmDirectSubmission.unblockPagingFenceSemaphore(pagingFenceValueToWait);
    EXPECT_GT(wddmDirectSubmission.semaphoreData->pagingFenceCounter, mockedPagingFence);
}

TEST(DirectSubmissionControllerWindowsTest, givenDirectSubmissionControllerWhenCallingSleepThenRequestHighResolutionTimers) {
    VariableBackup<size_t> timeBeginPeriodCalledBackup(&SysCalls::timeBeginPeriodCalled, 0u);
    VariableBackup<MMRESULT> timeBeginPeriodLastValueBackup(&SysCalls::timeBeginPeriodLastValue, 0u);
    VariableBackup<size_t> timeEndPeriodCalledBackup(&SysCalls::timeEndPeriodCalled, 0u);
    VariableBackup<MMRESULT> timeEndPeriodLastValueBackup(&SysCalls::timeEndPeriodLastValue, 0u);

    DirectSubmissionControllerMock controller;
    controller.callBaseSleepMethod = true;
    std::unique_lock<std::mutex> lock(controller.condVarMutex);
    controller.sleep(lock);
    EXPECT_TRUE(controller.sleepCalled);
    EXPECT_EQ(1u, SysCalls::timeBeginPeriodCalled);
    EXPECT_EQ(1u, SysCalls::timeEndPeriodCalled);
    EXPECT_EQ(1u, SysCalls::timeBeginPeriodLastValue);
    EXPECT_EQ(1u, SysCalls::timeEndPeriodLastValue);
}

HWTEST2_F(WddmDirectSubmissionTest, givenRelaxedOrderingSchedulerRequiredWhenAskingForCmdsSizeThenReturnCorrectValue, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockWddmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    size_t expectedBaseSemaphoreSectionSize = directSubmission.getSizePrefetchMitigation();
    if (directSubmission.isDisablePrefetcherRequired) {
        expectedBaseSemaphoreSectionSize += 2 * directSubmission.getSizeDisablePrefetcher();
    }

    directSubmission.relaxedOrderingEnabled = true;
    if (directSubmission.miMemFenceRequired) {
        expectedBaseSemaphoreSectionSize += MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(device->getRootDeviceEnvironment());
    }

    EXPECT_EQ(expectedBaseSemaphoreSectionSize + RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<FamilyType>::totalSize, directSubmission.getSizeSemaphoreSection(true));
    EXPECT_EQ(expectedBaseSemaphoreSectionSize + EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), directSubmission.getSizeSemaphoreSection(false));

    size_t expectedBaseEndSize = Dispatcher::getSizeStopCommandBuffer() +
                                 (Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer()) +
                                 MemoryConstants::cacheLineSize + 2 * Dispatcher::getSizeMonitorFence(device->getRootDeviceEnvironment());
    EXPECT_EQ(expectedBaseEndSize + directSubmission.getSizeDispatchRelaxedOrderingQueueStall(), directSubmission.getSizeEnd(true));
    EXPECT_EQ(expectedBaseEndSize, directSubmission.getSizeEnd(false));
}

HWTEST_F(WddmDirectSubmissionTest, givenDirectSubmissionControllerWhenRegisterCsrsThenTimeoutIsAdjusted) {
    auto csr = device->getDefaultEngine().commandStreamReceiver;

    DirectSubmissionControllerMock controller;
    uint64_t timeoutUs{5'000};
    EXPECT_EQ(static_cast<uint64_t>(controller.timeout.count()), timeoutUs);
    controller.registerDirectSubmission(csr);
    csr->getProductHelper().overrideDirectSubmissionTimeouts(timeoutUs, timeoutUs);
    EXPECT_EQ(static_cast<uint64_t>(controller.timeout.count()), timeoutUs);

    controller.unregisterDirectSubmission(csr);
}

} // namespace NEO
