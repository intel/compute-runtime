/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/windows/wddm_direct_submission.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/os_interface/windows/wddm_fixture.h"

struct WddmDirectSubmissionFixture : public WddmFixture {
    void SetUp() override {
        backupUlt = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        WddmFixture::SetUp();

        wddm->wddmInterface.reset(new WddmMockInterface20(*wddm));
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.get());

        osContext = std::make_unique<OsContextWin>(*wddm, 0u, 0u, aub_stream::ENGINE_RCS, PreemptionMode::ThreadGroup,
                                                   false, false, false);
        ultHwConfig.forceOsAgnosticMemoryManager = false;
        device.reset(MockDevice::create<MockDevice>(executionEnvironment, 0u));
        device->setPreemptionMode(PreemptionMode::ThreadGroup);
    }

    WddmMockInterface20 *wddmMockInterface;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<OsContextWin> osContext;

    std::unique_ptr<VariableBackup<UltHwConfig>> backupUlt;
};

template <typename GfxFamily, typename Dispatcher>
struct MockWddmDirectSubmission : public WddmDirectSubmission<GfxFamily, Dispatcher> {
    using BaseClass = WddmDirectSubmission<GfxFamily, Dispatcher>;
    using BaseClass::allocateOsResources;
    using BaseClass::commandBufferHeader;
    using BaseClass::completionRingBuffers;
    using BaseClass::currentRingBuffer;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getTagAddressValue;
    using BaseClass::handleCompletionRingBuffer;
    using BaseClass::handleResidency;
    using BaseClass::osContextWin;
    using BaseClass::ringBuffer;
    using BaseClass::ringBuffer2;
    using BaseClass::ringCommandStream;
    using BaseClass::ringFence;
    using BaseClass::ringStart;
    using BaseClass::semaphores;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::updateTagValue;
    using BaseClass::wddm;
    using BaseClass::WddmDirectSubmission;
    using typename BaseClass::RingBufferUse;
};

using WddmDirectSubmissionTest = WddmDirectSubmissionFixture;

using namespace NEO;

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenDirectIsInitializedAndStartedThenExpectProperCommandsDispatched) {
    std::unique_ptr<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>> wddmDirectSubmission =
        std::make_unique<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>>(*device.get(),
                                                                                             *osContext.get());

    EXPECT_EQ(1u, wddmDirectSubmission->commandBufferHeader->NeedsMidBatchPreEmptionSupport);

    bool ret = wddmDirectSubmission->initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(wddmDirectSubmission->ringStart);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffer2);
    EXPECT_NE(nullptr, wddmDirectSubmission->semaphores);

    EXPECT_EQ(1u, wddm->makeResidentResult.called);
    EXPECT_EQ(3u, wddm->makeResidentResult.handleCount);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);

    EXPECT_EQ(1u, wddm->submitResult.called);

    EXPECT_NE(0u, wddmDirectSubmission->ringCommandStream.getUsed());

    *wddmDirectSubmission->ringFence.cpuAddress = 1ull;
    wddmDirectSubmission->completionRingBuffers[wddmDirectSubmission->currentRingBuffer] = 2ull;

    wddmDirectSubmission.reset(nullptr);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(1u, wddmMockInterface->destroyMonitorFenceCalled);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenDirectIsInitializedAndNotStartedThenExpectNoCommandsDispatched) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    std::unique_ptr<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>> wddmDirectSubmission =
        std::make_unique<MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>>(*device.get(),
                                                                                             *osContext.get());

    EXPECT_EQ(0u, wddmDirectSubmission->commandBufferHeader->NeedsMidBatchPreEmptionSupport);

    bool ret = wddmDirectSubmission->initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(wddmDirectSubmission->ringStart);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffer);
    EXPECT_NE(nullptr, wddmDirectSubmission->ringBuffer2);
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
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    bool ret = wddmDirectSubmission.initialize(false);
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
    MemoryManager *memoryManager = device->getExecutionEnvironment()->memoryManager.get();
    const auto allocationSize = MemoryConstants::pageSize;
    const AllocationProperties commandStreamAllocationProperties{device->getRootDeviceIndex(),
                                                                 true, allocationSize,
                                                                 GraphicsAllocation::AllocationType::RING_BUFFER,
                                                                 false};
    GraphicsAllocation *ringBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    ASSERT_NE(nullptr, ringBuffer);

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    DirectSubmissionAllocations allocations;
    allocations.push_back(ringBuffer);

    bool ret = wddmDirectSubmission.allocateOsResources(allocations);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);
    EXPECT_EQ(1u, wddm->makeResidentResult.called);
    EXPECT_EQ(1u, wddm->makeResidentResult.handleCount);

    memoryManager->freeGraphicsMemory(ringBuffer);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenAllocateOsResourcesFenceCreationFailsThenExpectRingMonitorFenceNotCreatedAndAllocationsNotResident) {
    MemoryManager *memoryManager = device->getExecutionEnvironment()->memoryManager.get();
    const auto allocationSize = MemoryConstants::pageSize;
    const AllocationProperties commandStreamAllocationProperties{device->getRootDeviceIndex(),
                                                                 true, allocationSize,
                                                                 GraphicsAllocation::AllocationType::RING_BUFFER,
                                                                 false};
    GraphicsAllocation *ringBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    ASSERT_NE(nullptr, ringBuffer);

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    DirectSubmissionAllocations allocations;
    allocations.push_back(ringBuffer);

    wddmMockInterface->createMonitoredFenceCalledFail = true;

    bool ret = wddmDirectSubmission.allocateOsResources(allocations);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);
    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    EXPECT_EQ(0u, wddm->makeResidentResult.handleCount);

    memoryManager->freeGraphicsMemory(ringBuffer);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenAllocateOsResourcesResidencyFailsThenExpectRingMonitorFenceCreatedAndAllocationsNotResident) {
    MemoryManager *memoryManager = device->getExecutionEnvironment()->memoryManager.get();
    const auto allocationSize = MemoryConstants::pageSize;
    const AllocationProperties commandStreamAllocationProperties{device->getRootDeviceIndex(),
                                                                 true, allocationSize,
                                                                 GraphicsAllocation::AllocationType::RING_BUFFER,
                                                                 false};
    GraphicsAllocation *ringBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    ASSERT_NE(nullptr, ringBuffer);

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    DirectSubmissionAllocations allocations;
    allocations.push_back(ringBuffer);

    wddm->callBaseMakeResident = false;
    wddm->makeResidentStatus = false;

    bool ret = wddmDirectSubmission.allocateOsResources(allocations);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, wddmMockInterface->createMonitoredFenceCalled);
    //expect 2 makeResident calls, due to fail on 1st and then retry (which also fails)
    EXPECT_EQ(2u, wddm->makeResidentResult.called);
    EXPECT_EQ(1u, wddm->makeResidentResult.handleCount);

    memoryManager->freeGraphicsMemory(ringBuffer);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenGettingTagDataThenExpectContextMonitorFence) {
    uint64_t address = 0xFF00FF0000ull;
    uint64_t value = 0x12345678ull;
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();
    contextFence.gpuAddress = address;
    contextFence.currentFenceValue = value;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    TagData tagData;
    wddmDirectSubmission.getTagAddressValue(tagData);

    EXPECT_EQ(address, tagData.tagAddress);
    EXPECT_EQ(value, tagData.tagValue);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenHandleResidencyThenExpectWddmWaitOnPaginfFenceFromCpuCalled) {
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    wddmDirectSubmission.handleResidency();

    EXPECT_EQ(1u, wddm->waitOnPagingFenceFromCpuResult.called);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenHandlingRingBufferCompletionThenExpectWaitFromCpuWithCorrectFenceValue) {
    uint64_t address = 0xFF00FF0000ull;
    uint64_t value = 0x12345678ull;
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();
    contextFence.gpuAddress = address;
    contextFence.currentFenceValue = value;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    uint64_t completionValue = 0x12345679ull;
    wddmDirectSubmission.handleCompletionRingBuffer(completionValue, contextFence);

    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(completionValue, wddm->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(address, wddm->waitFromCpuResult.monitoredFence->gpuAddress);
    EXPECT_EQ(value, wddm->waitFromCpuResult.monitoredFence->currentFenceValue);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferStartedThenExpectDispatchSwitchCommandsLinearStreamUpdated) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    bool ret = wddmDirectSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffer->getGpuAddress() + usedSpace;

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers();
    EXPECT_EQ(expectedGpuVa, gpuVa);
    EXPECT_EQ(wddmDirectSubmission.ringBuffer2, wddmDirectSubmission.ringCommandStream.getGraphicsAllocation());

    LinearStream tmpCmdBuffer;
    tmpCmdBuffer.replaceBuffer(wddmDirectSubmission.ringBuffer->getUnderlyingBuffer(),
                               wddmDirectSubmission.ringCommandStream.getMaxAvailableSpace());
    tmpCmdBuffer.getSpace(usedSpace + wddmDirectSubmission.getSizeSwitchRingBufferSection());
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(tmpCmdBuffer, usedSpace);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    uint64_t actualGpuVa = GmmHelper::canonize(bbStart->getBatchBufferStartAddressGraphicsaddress472());
    EXPECT_EQ(wddmDirectSubmission.ringBuffer2->getGpuAddress(), actualGpuVa);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferNotStartedThenExpectNoSwitchCommandsLinearStreamUpdated) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    bool ret = wddmDirectSubmission.initialize(false);
    EXPECT_TRUE(ret);

    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    EXPECT_EQ(0u, usedSpace);

    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffer->getGpuAddress();

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers();
    EXPECT_EQ(expectedGpuVa, gpuVa);
    EXPECT_EQ(wddmDirectSubmission.ringBuffer2, wddmDirectSubmission.ringCommandStream.getGraphicsAllocation());

    LinearStream tmpCmdBuffer;
    tmpCmdBuffer.replaceBuffer(wddmDirectSubmission.ringBuffer->getUnderlyingBuffer(),
                               wddmDirectSubmission.ringCommandStream.getMaxAvailableSpace());
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(tmpCmdBuffer, 0u);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(nullptr, bbStart);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenSwitchingRingBufferStartedAndWaitFenceUpdateThenExpectWaitCalled) {
    using RingBufferUse = typename MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>::RingBufferUse;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    bool ret = wddmDirectSubmission.initialize(true);
    EXPECT_TRUE(ret);
    uint64_t expectedWaitFence = 0x10ull;
    wddmDirectSubmission.completionRingBuffers[RingBufferUse::SecondBuffer] = expectedWaitFence;
    size_t usedSpace = wddmDirectSubmission.ringCommandStream.getUsed();
    uint64_t expectedGpuVa = wddmDirectSubmission.ringBuffer->getGpuAddress() + usedSpace;

    uint64_t gpuVa = wddmDirectSubmission.switchRingBuffers();
    EXPECT_EQ(expectedGpuVa, gpuVa);
    EXPECT_EQ(wddmDirectSubmission.ringBuffer2, wddmDirectSubmission.ringCommandStream.getGraphicsAllocation());

    LinearStream tmpCmdBuffer;
    tmpCmdBuffer.replaceBuffer(wddmDirectSubmission.ringBuffer->getUnderlyingBuffer(),
                               wddmDirectSubmission.ringCommandStream.getMaxAvailableSpace());
    tmpCmdBuffer.getSpace(usedSpace + wddmDirectSubmission.getSizeSwitchRingBufferSection());
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(tmpCmdBuffer, usedSpace);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    uint64_t actualGpuVa = GmmHelper::canonize(bbStart->getBatchBufferStartAddressGraphicsaddress472());
    EXPECT_EQ(wddmDirectSubmission.ringBuffer2->getGpuAddress(), actualGpuVa);

    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(expectedWaitFence, wddm->waitFromCpuResult.uint64ParamPassed);
}

HWTEST_F(WddmDirectSubmissionTest, givenWddmWhenUpdatingTagValueThenExpectCompletionRingBufferUpdated) {
    uint64_t address = 0xFF00FF0000ull;
    uint64_t value = 0x12345678ull;
    MonitoredFence &contextFence = osContext->getResidencyController().getMonitoredFence();
    contextFence.gpuAddress = address;
    contextFence.currentFenceValue = value;

    MockWddmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> wddmDirectSubmission(*device.get(),
                                                                                            *osContext.get());

    uint64_t actualTagValue = wddmDirectSubmission.updateTagValue();
    EXPECT_EQ(value, actualTagValue);
    EXPECT_EQ(value + 1, contextFence.currentFenceValue);
    EXPECT_EQ(value, wddmDirectSubmission.completionRingBuffers[wddmDirectSubmission.currentRingBuffer]);
}
