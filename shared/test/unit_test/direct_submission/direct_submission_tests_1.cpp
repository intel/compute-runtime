/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionDisabledWhenStopThenRingIsNotStopped) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.stopDirectSubmission(false, false);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.directSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenStopThenRingIsNotStarted) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.directSubmission.reset(&directSubmission);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.stopDirectSubmission(false, false);
    EXPECT_FALSE(directSubmission.ringStart);

    csr.directSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenBlitterDirectSubmissionWhenStopThenRingIsNotStarted) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = true;
    MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr.blitterDirectSubmission.reset(&directSubmission);
    csr.osContext = nullptr;
    csr.setupContext(*osContext);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.stopDirectSubmission(false, false);
    EXPECT_FALSE(directSubmission.ringStart);

    csr.blitterDirectSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenDeviceStopDirectSubmissionAndWaitForCompletionCalledThenCsrStopDirectSubmissionCalledBlocking) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = true;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.directSubmission.reset(&directSubmission);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);

    EXPECT_FALSE(csr.stopDirectSubmissionCalled);
    pDevice->stopDirectSubmissionAndWaitForCompletion();
    EXPECT_TRUE(csr.stopDirectSubmissionCalled);
    EXPECT_TRUE(csr.stopDirectSubmissionCalledBlocking);

    csr.directSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenDeviceStopDirectSubmissionAndWaitForCompletionCalledThenBcsStopDirectSubmissionCalledBlocking) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = true;

    MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr.blitterDirectSubmission.reset(&directSubmission);
    csr.callBaseStopDirectSubmission = false;
    csr.setupContext(*osContext);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);

    EXPECT_FALSE(csr.stopDirectSubmissionCalled);
    pDevice->stopDirectSubmissionAndWaitForCompletion();
    EXPECT_TRUE(csr.stopDirectSubmissionCalled);
    EXPECT_TRUE(csr.stopDirectSubmissionCalledBlocking);

    csr.blitterDirectSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenCsrWhenGetLastDirectSubmissionThrottleCalledThenDirectSubmissionLastSubmittedThrottleReturned) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = false;
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = false;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.directSubmission.reset(&directSubmission);
    csr.directSubmissionAvailable = true;

    directSubmission.lastSubmittedThrottle = QueueThrottle::LOW;
    EXPECT_EQ(QueueThrottle::LOW, csr.getLastDirectSubmissionThrottle());

    csr.directSubmissionAvailable = false;
    EXPECT_EQ(QueueThrottle::MEDIUM, csr.getLastDirectSubmissionThrottle());

    csr.directSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenBcsCsrWhenGetLastDirectSubmissionThrottleCalledThenDirectSubmissionLastSubmittedThrottleReturned) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = false;
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = false;

    MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr.blitterDirectSubmission.reset(&directSubmission);
    csr.setupContext(*osContext);
    csr.blitterDirectSubmissionAvailable = true;

    directSubmission.lastSubmittedThrottle = QueueThrottle::LOW;
    EXPECT_EQ(QueueThrottle::LOW, csr.getLastDirectSubmissionThrottle());

    csr.blitterDirectSubmissionAvailable = false;
    EXPECT_EQ(QueueThrottle::MEDIUM, csr.getLastDirectSubmissionThrottle());

    csr.blitterDirectSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStopRingBufferBlockingCalledAndRingBufferIsNotStartedThenEnsureRingCompletionCalled) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    EXPECT_FALSE(directSubmission.ringStart);
    EXPECT_EQ(0u, directSubmission.ensureRingCompletionCalled);
    directSubmission.stopRingBuffer(true);
    EXPECT_EQ(1u, directSubmission.ensureRingCompletionCalled);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenMakingResourcesResidentThenCorrectContextIsUsed) {
    auto mockMemoryOperations = std::make_unique<MockMemoryOperations>();

    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.reset(mockMemoryOperations.get());

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *alloc = directSubmission.ringCommandStream.getGraphicsAllocation();

    directSubmission.callBaseResident = true;

    DirectSubmissionAllocations allocs;
    allocs.push_back(alloc);

    directSubmission.makeResourcesResident(allocs);

    EXPECT_EQ(osContext->getContextId(), mockMemoryOperations->makeResidentContextId);

    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.release();
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWithoutCompletionFenceAllocationWhenAllocatingResourcesThenMakeResidentIsCalledForRingAndSemaphoreBuffers) {
    auto mockMemoryOperations = std::make_unique<MockMemoryOperations>();
    mockMemoryOperations->captureGfxAllocationsForMakeResident = true;
    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.reset(mockMemoryOperations.get());

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.callBaseResident = true;
    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_EQ(nullptr, directSubmission.completionFenceAllocation);

    size_t expectedAllocationsCnt = 3;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    if (gfxCoreHelper.isRelaxedOrderingSupported()) {
        expectedAllocationsCnt += 2;
    }

    EXPECT_EQ(1, mockMemoryOperations->makeResidentCalledCount);
    ASSERT_EQ(expectedAllocationsCnt, mockMemoryOperations->gfxAllocationsForMakeResident.size());
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, mockMemoryOperations->gfxAllocationsForMakeResident[0]);
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, mockMemoryOperations->gfxAllocationsForMakeResident[1]);
    EXPECT_EQ(directSubmission.semaphores, mockMemoryOperations->gfxAllocationsForMakeResident[2]);

    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.release();
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWithCompletionFenceAllocationWhenAllocatingResourcesThenMakeResidentIsCalledForRingAndSemaphoreBuffersAndCompletionFenceAllocation) {
    auto mockMemoryOperations = std::make_unique<MockMemoryOperations>();
    mockMemoryOperations->captureGfxAllocationsForMakeResident = true;

    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.reset(mockMemoryOperations.get());

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    MockGraphicsAllocation completionFenceAllocation{};

    directSubmission.completionFenceAllocation = &completionFenceAllocation;

    directSubmission.callBaseResident = true;
    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_EQ(&completionFenceAllocation, directSubmission.completionFenceAllocation);

    size_t expectedAllocationsCnt = 4;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    if (gfxCoreHelper.isRelaxedOrderingSupported()) {
        expectedAllocationsCnt += 2;
    }

    EXPECT_EQ(1, mockMemoryOperations->makeResidentCalledCount);
    ASSERT_EQ(expectedAllocationsCnt, mockMemoryOperations->gfxAllocationsForMakeResident.size());
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, mockMemoryOperations->gfxAllocationsForMakeResident[0]);
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, mockMemoryOperations->gfxAllocationsForMakeResident[1]);
    EXPECT_EQ(directSubmission.semaphores, mockMemoryOperations->gfxAllocationsForMakeResident[2]);
    EXPECT_EQ(directSubmission.completionFenceAllocation, mockMemoryOperations->gfxAllocationsForMakeResident[3]);

    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.release();
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionInitializedWhenRingIsStartedThenExpectAllocationsCreatedAndCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    EXPECT_NE(nullptr, directSubmission.ringBuffers[0].ringBuffer);
    EXPECT_NE(nullptr, directSubmission.ringBuffers[1].ringBuffer);
    EXPECT_NE(nullptr, directSubmission.semaphores);

    EXPECT_NE(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionInitializedWhenRingIsNotStartedThenExpectAllocationsCreatedAndCommandsNotDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.detectGpuHang);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_NE(nullptr, directSubmission.ringBuffers[0].ringBuffer);
    EXPECT_NE(nullptr, directSubmission.ringBuffers[1].ringBuffer);
    EXPECT_NE(nullptr, directSubmission.semaphores);

    EXPECT_EQ(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSwitchBuffersWhenCurrentIsPrimaryThenExpectNextSecondary) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    GraphicsAllocation *nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, nextRing);
    EXPECT_EQ(1u, directSubmission.currentRingBuffer);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSwitchBuffersWhenCurrentIsSecondaryThenExpectNextPrimary) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    GraphicsAllocation *nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, nextRing);
    EXPECT_EQ(1u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, nextRing);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionCurrentRingBuffersInUseWhenSwitchRingBufferThenAllocateNewInsteadOfWaiting) {
    auto mockMemoryOperations = std::make_unique<MockMemoryOperations>();
    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.reset(mockMemoryOperations.get());
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.isCompletedReturn = false;

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);
    EXPECT_EQ(2u, directSubmission.ringBuffers.size());

    auto nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(3u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[2].ringBuffer, nextRing);
    EXPECT_EQ(2u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[3].ringBuffer, nextRing);
    EXPECT_EQ(3u, directSubmission.currentRingBuffer);

    directSubmission.isCompletedReturn = true;

    nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, nextRing);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, nextRing);
    EXPECT_EQ(1u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, nextRing);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, nextRing);
    EXPECT_EQ(1u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, nextRing);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    directSubmission.isCompletedReturn = false;

    nextRing = directSubmission.switchRingBuffersAllocations(nullptr);
    EXPECT_EQ(5u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[4].ringBuffer, nextRing);
    EXPECT_EQ(4u, directSubmission.currentRingBuffer);

    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.release();
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionAllocateFailWhenRingIsStartedThenExpectRingNotStarted) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.allocateOsResourcesReturn = false;
    bool ret = directSubmission.initialize(true);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_EQ(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSubmitFailWhenRingIsStartedThenExpectRingNotStartedCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.submitReturn = false;
    bool ret = directSubmission.initialize(true);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_NE(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStopWhenStopRingIsCalledThenExpectStopCommandDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();
    uint32_t oldQueueCount = directSubmission.semaphoreData->queueWorkCount;

    directSubmission.stopRingBuffer(false);

    size_t expectedDispatchSize = alreadyDispatchedSize + directSubmission.getSizeEnd(false);
    EXPECT_LE(directSubmission.ringCommandStream.getUsed(), expectedDispatchSize);
    EXPECT_GE(directSubmission.ringCommandStream.getUsed() + MemoryConstants::cacheLineSize, expectedDispatchSize);
    EXPECT_EQ(oldQueueCount + 1, directSubmission.semaphoreData->queueWorkCount);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchSemaphoreThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchSemaphoreSection(1u);
    EXPECT_EQ(directSubmission.getSizeSemaphoreSection(false), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchStartSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchStartSection(1ull);
    EXPECT_EQ(directSubmission.getSizeStartSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchSwitchRingBufferSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchSwitchRingBufferSection(1ull);
    EXPECT_EQ(directSubmission.getSizeSwitchRingBufferSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchTagUpdateSectionThenExpectCorrectSizeUsed) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDirectSubmissionHw<FamilyType, Dispatcher>
        directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    Dispatcher::dispatchMonitorFence(directSubmission.ringCommandStream, 0ull, 0ull, directSubmission.rootDeviceEnvironment, false, directSubmission.dcFlushRequired, false);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(Dispatcher::getSizeMonitorFence(directSubmission.rootDeviceEnvironment), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchEndingSectionThenExpectCorrectSizeUsed) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    Dispatcher::dispatchStopCommandBuffer(directSubmission.ringCommandStream);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(Dispatcher::getSizeStopCommandBuffer(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t expectedSize = directSubmission.getSizeStartSection() +
                          directSubmission.getSizeSemaphoreSection(false) + directSubmission.getSizeNewResourceHandler();

    size_t actualSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(true));
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableCacheFlushWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    size_t expectedSize = directSubmission.getSizeStartSection() +
                          directSubmission.getSizeSemaphoreSection(false) + directSubmission.getSizeNewResourceHandler();

    size_t actualSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false));
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenGetEndSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t expectedSize = Dispatcher::getSizeStopCommandBuffer() +
                          (Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer()) +
                          MemoryConstants::cacheLineSize;
    size_t actualSize = directSubmission.getSizeEnd(false);
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenSettingAddressInReturnCommandThenVerifyCorrectlySet) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    uint64_t returnAddress = 0x1A2BF000;
    void *space = directSubmission.ringCommandStream.getSpace(sizeof(MI_BATCH_BUFFER_START));
    directSubmission.setReturnAddress(space, returnAddress);
    MI_BATCH_BUFFER_START *bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(space);
    EXPECT_EQ(returnAddress, bbStart->getBatchBufferStartAddress());
}

HWTEST_F(DirectSubmissionTest, whenDirectSubmissionInitializedThenExpectCreatedAllocationsFreed) {
    MemoryManager *memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();

    auto directSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(*pDevice->getDefaultEngine().commandStreamReceiver);
    bool ret = directSubmission->initialize(false);
    EXPECT_TRUE(ret);

    GraphicsAllocation *nulledAllocation = directSubmission->ringBuffers[0u].ringBuffer;
    directSubmission->ringBuffers[0u].ringBuffer = nullptr;
    directSubmission.reset(nullptr);
    memoryManager->freeGraphicsMemory(nulledAllocation);

    directSubmission = std::make_unique<
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(*pDevice->getDefaultEngine().commandStreamReceiver);
    ret = directSubmission->initialize(false);
    EXPECT_TRUE(ret);

    nulledAllocation = directSubmission->ringBuffers[1u].ringBuffer;
    directSubmission->ringBuffers[1u].ringBuffer = nullptr;
    directSubmission.reset(nullptr);
    memoryManager->freeGraphicsMemory(nulledAllocation);

    directSubmission = std::make_unique<
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(*pDevice->getDefaultEngine().commandStreamReceiver);
    ret = directSubmission->initialize(false);
    EXPECT_TRUE(ret);
    nulledAllocation = directSubmission->semaphores;
    directSubmission->semaphores = nullptr;
    directSubmission.reset(nullptr);
    memoryManager->freeGraphicsMemory(nulledAllocation);
}

HWTEST_F(DirectSubmissionTest, givenSuperBaseCsrWhenCheckingDirectSubmissionAvailableThenReturnFalse) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrSuperBaseCallDirectSubmissionAvailable = true;
    ultHwConfig.csrSuperBaseCallBlitterDirectSubmissionAvailable = true;

    int32_t executionStamp = 0;
    std::unique_ptr<MockCsr<FamilyType>> mockCsr =
        std::make_unique<MockCsr<FamilyType>>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_FALSE(ret);
    ret = mockCsr->isBlitterDirectSubmissionEnabled();
    EXPECT_FALSE(ret);
}

HWTEST_F(DirectSubmissionTest, givenBaseCsrWhenCheckingDirectSubmissionAvailableThenReturnFalse) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = true;

    int32_t executionStamp = 0;
    std::unique_ptr<MockCsr<FamilyType>> mockCsr =
        std::make_unique<MockCsr<FamilyType>>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_FALSE(ret);
    ret = mockCsr->isBlitterDirectSubmissionEnabled();
    EXPECT_FALSE(ret);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionAvailableWhenProgrammingEndingCommandThenUseBatchBufferStart) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    int32_t executionStamp = 0;

    std::unique_ptr<MockCsr<FamilyType>> mockCsr =
        std::make_unique<MockCsr<FamilyType>>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    mockCsr->setupContext(*osContext);

    mockCsr->directSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(*mockCsr);

    mockCsr->directSubmissionAvailable = true;
    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_TRUE(ret);

    MockGraphicsAllocation mockAllocation;

    void *location = nullptr;
    uint8_t buffer[128];
    mockCsr->commandStream.replaceBuffer(&buffer[0], 128u);
    mockCsr->commandStream.replaceGraphicsAllocation(&mockAllocation);
    mockCsr->programEndingCmd(mockCsr->commandStream, &location, ret, false, false);
    EXPECT_EQ(sizeof(MI_BATCH_BUFFER_START), mockCsr->commandStream.getUsed());

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.epilogueRequired = true;
    size_t expectedSize = sizeof(MI_BATCH_BUFFER_START) +
                          mockCsr->getCmdSizeForEpilogueCommands(dispatchFlags);
    expectedSize = alignUp(expectedSize, MemoryConstants::cacheLineSize);
    EXPECT_EQ(expectedSize, mockCsr->getCmdSizeForEpilogue(dispatchFlags));

    mockCsr->commandStream.replaceGraphicsAllocation(nullptr);
}

HWTEST_F(DirectSubmissionTest, givenDebugFlagSetWhenProgrammingEndingCommandThenUseCorrectBatchBufferStartValue) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManagerStateRestore restorer;
    MockGraphicsAllocation mockAllocation;
    int32_t executionStamp = 0;

    std::unique_ptr<MockCsr<FamilyType>> mockCsr = std::make_unique<MockCsr<FamilyType>>(executionStamp, *pDevice->executionEnvironment,
                                                                                         pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    mockCsr->setupContext(*osContext);

    mockCsr->directSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(*mockCsr);

    mockCsr->directSubmissionAvailable = true;
    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_TRUE(ret);

    void *location = nullptr;

    uint8_t buffer[256] = {};
    auto &cmdStream = mockCsr->commandStream;
    cmdStream.replaceBuffer(&buffer[0], 256);
    cmdStream.replaceGraphicsAllocation(&mockAllocation);

    for (int32_t value : {-1, 0, 1}) {
        debugManager.flags.BatchBufferStartPrepatchingWaEnabled.set(value);

        auto currectBbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(cmdStream.getSpace(0));
        uint64_t expectedGpuVa = cmdStream.getGraphicsAllocation()->getGpuAddress() + cmdStream.getUsed();

        mockCsr->programEndingCmd(cmdStream, &location, ret, false, false);
        EncodeNoop<FamilyType>::alignToCacheLine(cmdStream);

        if (value == 0) {
            EXPECT_EQ(0u, currectBbStartCmd->getBatchBufferStartAddress());
        } else {
            EXPECT_EQ(expectedGpuVa, currectBbStartCmd->getBatchBufferStartAddress());
        }
    }

    cmdStream.replaceGraphicsAllocation(nullptr);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, DirectSubmissionTest,
            givenLegacyPlatformsWhenProgrammingPartitionRegisterThenExpectNoAction) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t usedSize = directSubmission.ringCommandStream.getUsed();

    constexpr size_t expectedSize = 0;
    size_t estimatedSize = directSubmission.getSizePartitionRegisterConfigurationSection();
    EXPECT_EQ(expectedSize, estimatedSize);

    directSubmission.dispatchPartitionRegisterConfiguration();
    size_t usedSizeAfter = directSubmission.ringCommandStream.getUsed();
    EXPECT_EQ(expectedSize, usedSizeAfter - usedSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DirectSubmissionTest, givenDebugFlagSetWhenDispatchingPrefetcherThenSetCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForcePreParserEnabledForMiArbCheck.set(1);

    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using Dispatcher = BlitterDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    directSubmission.dispatchDisablePrefetcher(true);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_ARB_CHECK *arbCheck = hwParse.getCommand<MI_ARB_CHECK>();
    ASSERT_NE(nullptr, arbCheck);

    EXPECT_EQ(0u, arbCheck->getPreParserDisable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DirectSubmissionTest, givenDisablePrefetcherDebugFlagDisabledWhenDispatchingPrefetcherThenSetCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionDisablePrefetcher.set(0);

    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using Dispatcher = BlitterDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    directSubmission.dispatchDisablePrefetcher(true);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_ARB_CHECK *arbCheck = hwParse.getCommand<MI_ARB_CHECK>();
    EXPECT_EQ(nullptr, arbCheck);
}
