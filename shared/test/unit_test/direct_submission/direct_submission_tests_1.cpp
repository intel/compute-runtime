/*
 * Copyright (C) 2020-2024 Intel Corporation
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
#include "shared/test/unit_test/mocks/mock_direct_submission_diagnostic_collector.h"

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

HWTEST_F(DirectSubmissionTest, whenDebugCacheFlushDisabledSetThenExpectNoCpuCacheFlush) {
    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionDisableCpuCacheFlush.set(1);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.disableCpuCacheFlush);

    uintptr_t expectedPtrVal = 0;
    CpuIntrinsicsTests::lastClFlushedPtr = 0;
    void *ptr = reinterpret_cast<void *>(0xABCD00u);
    size_t size = 64;
    directSubmission.cpuCachelineFlush(ptr, size);
    EXPECT_EQ(expectedPtrVal, CpuIntrinsicsTests::lastClFlushedPtr);
}

HWTEST_F(DirectSubmissionTest, whenDebugCacheFlushDisabledNotSetThenExpectCpuCacheFlush) {
    if (!CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureClflush)) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionDisableCpuCacheFlush.set(0);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.disableCpuCacheFlush);

    uintptr_t expectedPtrVal = 0xABCD00u;
    CpuIntrinsicsTests::lastClFlushedPtr = 0;
    void *ptr = reinterpret_cast<void *>(expectedPtrVal);
    size_t size = 64;
    directSubmission.cpuCachelineFlush(ptr, size);
    EXPECT_EQ(expectedPtrVal, CpuIntrinsicsTests::lastClFlushedPtr);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionDisabledWhenStopThenRingIsNotStopped) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.stopDirectSubmission(false);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.directSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenStopThenRingIsNotStarted) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.directSubmission.reset(&directSubmission);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.stopDirectSubmission(false);
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
    csr.setupContext(*osContext);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.stopDirectSubmission(false);
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

    bool ret = directSubmission.initialize(true, false);
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
    csr.setupContext(*osContext);

    bool ret = directSubmission.initialize(true, false);
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
    bool ret = directSubmission.initialize(false, false);
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

    bool ret = directSubmission.initialize(true, false);
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
    bool ret = directSubmission.initialize(true, false);
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
    bool ret = directSubmission.initialize(true, false);
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
    EXPECT_TRUE(directSubmission.disableCpuCacheFlush);

    bool ret = directSubmission.initialize(true, false);
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

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_NE(nullptr, directSubmission.ringBuffers[0].ringBuffer);
    EXPECT_NE(nullptr, directSubmission.ringBuffers[1].ringBuffer);
    EXPECT_NE(nullptr, directSubmission.semaphores);

    EXPECT_EQ(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSwitchBuffersWhenCurrentIsPrimaryThenExpectNextSecondary) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    GraphicsAllocation *nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, nextRing);
    EXPECT_EQ(1u, directSubmission.currentRingBuffer);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSwitchBuffersWhenCurrentIsSecondaryThenExpectNextPrimary) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    GraphicsAllocation *nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, nextRing);
    EXPECT_EQ(1u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, nextRing);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionCurrentRingBuffersInUseWhenSwitchRingBufferThenAllocateNewInsteadOfWaiting) {
    auto mockMemoryOperations = std::make_unique<MockMemoryOperations>();
    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.reset(mockMemoryOperations.get());
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.isCompletedReturn = false;

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);
    EXPECT_EQ(2u, directSubmission.ringBuffers.size());

    auto nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(3u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[2].ringBuffer, nextRing);
    EXPECT_EQ(2u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[3].ringBuffer, nextRing);
    EXPECT_EQ(3u, directSubmission.currentRingBuffer);

    directSubmission.isCompletedReturn = true;

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, nextRing);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, nextRing);
    EXPECT_EQ(1u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, nextRing);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[1].ringBuffer, nextRing);
    EXPECT_EQ(1u, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(4u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[0].ringBuffer, nextRing);
    EXPECT_EQ(0u, directSubmission.currentRingBuffer);

    directSubmission.isCompletedReturn = false;

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(5u, directSubmission.ringBuffers.size());
    EXPECT_EQ(directSubmission.ringBuffers[4].ringBuffer, nextRing);
    EXPECT_EQ(4u, directSubmission.currentRingBuffer);

    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.release();
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionAllocateFailWhenRingIsStartedThenExpectRingNotStarted) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.disableCpuCacheFlush);

    directSubmission.allocateOsResourcesReturn = false;
    bool ret = directSubmission.initialize(true, false);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_EQ(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSubmitFailWhenRingIsStartedThenExpectRingNotStartedCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.submitReturn = false;
    bool ret = directSubmission.initialize(true, false);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_NE(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStopWhenStopRingIsCalledThenExpectStopCommandDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();
    uint32_t oldQueueCount = directSubmission.semaphoreData->queueWorkCount;

    directSubmission.stopRingBuffer(false);

    size_t expectedDispatchSize = alreadyDispatchedSize + directSubmission.getSizeEnd(false);
    EXPECT_LE(directSubmission.ringCommandStream.getUsed(), expectedDispatchSize);
    EXPECT_GE(directSubmission.ringCommandStream.getUsed() + MemoryConstants::cacheLineSize, expectedDispatchSize);
    EXPECT_EQ(oldQueueCount + 1, directSubmission.semaphoreData->queueWorkCount);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableMonitorFenceWhenStopRingIsCalledThenExpectStopCommandAndMonitorFenceDispatched) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    regularDirectSubmission.disableMonitorFence = false;
    size_t regularSizeEnd = regularDirectSubmission.getSizeEnd(false);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    directSubmission.ringStart = true;

    EXPECT_TRUE(ret);

    size_t tagUpdateSize = Dispatcher::getSizeMonitorFence(directSubmission.rootDeviceEnvironment);

    size_t disabledSizeEnd = directSubmission.getSizeEnd(false);
    EXPECT_EQ(disabledSizeEnd, regularSizeEnd + tagUpdateSize);

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    directSubmission.stopRingBuffer(false);
    size_t expectedDispatchSize = disabledSizeEnd;
    EXPECT_LE(directSubmission.ringCommandStream.getUsed(), expectedDispatchSize);
    EXPECT_GE(directSubmission.ringCommandStream.getUsed() + MemoryConstants::cacheLineSize, expectedDispatchSize);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_BATCH_BUFFER_END *bbEnd = hwParse.getCommand<MI_BATCH_BUFFER_END>();
    EXPECT_NE(nullptr, bbEnd);

    bool foundFenceUpdate = false;
    for (auto it = hwParse.pipeControlList.begin(); it != hwParse.pipeControlList.end(); it++) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        uint64_t data = pipeControl->getImmediateData();
        if ((directSubmission.tagAddressSetValue == NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl)) &&
            (directSubmission.tagValueSetValue == data)) {
            foundFenceUpdate = true;
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchSemaphoreThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchSemaphoreSection(1u);
    EXPECT_EQ(directSubmission.getSizeSemaphoreSection(false), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchStartSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchStartSection(1ull);
    EXPECT_EQ(directSubmission.getSizeStartSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchSwitchRingBufferSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchSwitchRingBufferSection(1ull);
    EXPECT_EQ(directSubmission.getSizeSwitchRingBufferSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchFlushSectionThenExpectCorrectSizeUsed) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);

    Dispatcher::dispatchCacheFlush(directSubmission.ringCommandStream, pDevice->getRootDeviceEnvironment(), 0ull);
    EXPECT_EQ(Dispatcher::getSizeCacheFlush(pDevice->getRootDeviceEnvironment()), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchTagUpdateSectionThenExpectCorrectSizeUsed) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDirectSubmissionHw<FamilyType, Dispatcher>
        directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    Dispatcher::dispatchMonitorFence(directSubmission.ringCommandStream, 0ull, 0ull, directSubmission.rootDeviceEnvironment, false, false, directSubmission.dcFlushRequired);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(Dispatcher::getSizeMonitorFence(directSubmission.rootDeviceEnvironment), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchEndingSectionThenExpectCorrectSizeUsed) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);

    Dispatcher::dispatchStopCommandBuffer(directSubmission.ringCommandStream);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(Dispatcher::getSizeStopCommandBuffer(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(0);
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t expectedSize = directSubmission.getSizeStartSection() +
                          Dispatcher::getSizeCacheFlush(directSubmission.rootDeviceEnvironment) +
                          directSubmission.getSizeSemaphoreSection(false) + directSubmission.getSizeNewResourceHandler();

    size_t actualSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(true));
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionEnableDebugBufferModeOneWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(0);
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.workloadMode = 1;

    size_t expectedSize = Dispatcher::getSizeStoreDwordCommand() +
                          Dispatcher::getSizeCacheFlush(directSubmission.rootDeviceEnvironment) +
                          directSubmission.getSizeSemaphoreSection(false) + directSubmission.getSizeNewResourceHandler();
    size_t actualSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false));
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionEnableDebugBufferModeTwoWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(0);
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.workloadMode = 2;
    size_t expectedSize = Dispatcher::getSizeCacheFlush(directSubmission.rootDeviceEnvironment) +
                          directSubmission.getSizeSemaphoreSection(false) + directSubmission.getSizeNewResourceHandler();
    size_t actualSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false));
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableCacheFlushWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableCacheFlush = true;
    size_t expectedSize = directSubmission.getSizeStartSection() +
                          directSubmission.getSizeSemaphoreSection(false) + directSubmission.getSizeNewResourceHandler();

    size_t actualSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false));
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableMonitorFenceWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(0);
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = true;
    size_t expectedSize = directSubmission.getSizeStartSection() +
                          Dispatcher::getSizeCacheFlush(directSubmission.rootDeviceEnvironment) +
                          directSubmission.getSizeSemaphoreSection(false) + directSubmission.getSizeNewResourceHandler();

    size_t actualSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false));
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenGetEndSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = false;

    size_t expectedSize = Dispatcher::getSizeStopCommandBuffer() +
                          Dispatcher::getSizeCacheFlush(directSubmission.rootDeviceEnvironment) +
                          (Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer()) +
                          MemoryConstants::cacheLineSize;
    size_t actualSize = directSubmission.getSizeEnd(false);
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenSettingAddressInReturnCommandThenVerifyCorrectlySet) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
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
    bool ret = directSubmission->initialize(false, false);
    EXPECT_TRUE(ret);

    GraphicsAllocation *nulledAllocation = directSubmission->ringBuffers[0u].ringBuffer;
    directSubmission->ringBuffers[0u].ringBuffer = nullptr;
    directSubmission.reset(nullptr);
    memoryManager->freeGraphicsMemory(nulledAllocation);

    directSubmission = std::make_unique<
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(*pDevice->getDefaultEngine().commandStreamReceiver);
    ret = directSubmission->initialize(false, false);
    EXPECT_TRUE(ret);

    nulledAllocation = directSubmission->ringBuffers[1u].ringBuffer;
    directSubmission->ringBuffers[1u].ringBuffer = nullptr;
    directSubmission.reset(nullptr);
    memoryManager->freeGraphicsMemory(nulledAllocation);

    directSubmission = std::make_unique<
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(*pDevice->getDefaultEngine().commandStreamReceiver);
    ret = directSubmission->initialize(false, false);
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

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDiagnosticNotAvailableWhenDiagnosticRegistryIsUsedThenDoNotEnableDiagnostic) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    if (NEO::directSubmissionDiagnosticAvailable) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionEnableDebugBuffer.set(1);
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    debugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_TRUE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(0u, directSubmission.workloadMode);
    EXPECT_EQ(nullptr, directSubmission.diagnostic.get());
    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(directSubmission.ringStart);
    EXPECT_EQ(0u, directSubmission.disabledDiagnosticCalled);
    EXPECT_EQ(0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(0u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDiagnosticNotAvailableWhenDiagnosticRegistryIsUsedThenDoNotEnableDiagnosticStartOnlyWithSemaphore) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    if (NEO::directSubmissionDiagnosticAvailable) {
        GTEST_SKIP();
    }
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(compilerProductHelper.isHeaplessModeEnabled());

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionEnableDebugBuffer.set(1);
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    debugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_TRUE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(0u, directSubmission.workloadMode);
    EXPECT_EQ(nullptr, directSubmission.diagnostic.get());
    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_EQ(1u, directSubmission.disabledDiagnosticCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
    size_t expectedSize = Dispatcher::getSizePreemption() +
                          directSubmission.getSizeSemaphoreSection(false);
    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
        expectedSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        expectedSize += RelaxedOrderingHelper::getSizeReturnPtrRegs<FamilyType>();
    }
    EXPECT_EQ(expectedSize, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDiagnosticAvailableWhenDiagnosticRegistryIsNotUsedThenDoNotEnableDiagnostic) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    if (!NEO::directSubmissionDiagnosticAvailable) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    debugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_TRUE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(0u, directSubmission.workloadMode);
    EXPECT_EQ(nullptr, directSubmission.diagnostic.get());
    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(directSubmission.ringStart);
    EXPECT_EQ(0u, directSubmission.disabledDiagnosticCalled);
    EXPECT_EQ(0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(0u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDiagnosticAvailableWhenDiagnosticRegistryUsedThenDoPerformDiagnosticRun) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename FamilyType::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    using WAIT_MODE = typename FamilyType::MI_SEMAPHORE_WAIT::WAIT_MODE;
    using Dispatcher = RenderDispatcher<FamilyType>;

    if (!NEO::directSubmissionDiagnosticAvailable) {
        GTEST_SKIP();
    }

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(compilerProductHelper.isHeaplessModeEnabled());

    uint32_t execCount = 5u;
    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionEnableDebugBuffer.set(1);
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    debugManager.flags.DirectSubmissionDisableMonitorFence.set(true);
    debugManager.flags.DirectSubmissionDiagnosticExecutionCount.set(static_cast<int32_t>(execCount));

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    uint32_t expectedSemaphoreValue = directSubmission.currentQueueWorkCount;
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_TRUE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(1u, directSubmission.workloadMode);
    ASSERT_NE(nullptr, directSubmission.diagnostic.get());
    uint32_t expectedExecCount = 5u;
    expectedSemaphoreValue += expectedExecCount;
    EXPECT_EQ(expectedExecCount, directSubmission.diagnostic->getExecutionsCount());
    size_t expectedSize = Dispatcher::getSizePreemption() +
                          directSubmission.getSizeSemaphoreSection(false) +
                          directSubmission.getDiagnosticModeSection();
    expectedSize += expectedExecCount * (directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false)) - directSubmission.getSizeNewResourceHandler());

    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
        expectedSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        expectedSize += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
    }

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_EQ(0u, directSubmission.disabledDiagnosticCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    // 1 - preamble, 1 - init time, 5 - exec logs
    EXPECT_EQ(7u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
    EXPECT_EQ(expectedSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(expectedSemaphoreValue, directSubmission.currentQueueWorkCount);

    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(0u, directSubmission.workloadMode);
    EXPECT_EQ(nullptr, directSubmission.diagnostic.get());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);

    GenCmdList storeDataCmdList = hwParse.getCommandsList<MI_STORE_DATA_IMM>();
    execCount += 1;
    ASSERT_EQ(execCount, storeDataCmdList.size());

    uint64_t expectedStoreAddress = directSubmission.semaphoreGpuVa;
    expectedStoreAddress += ptrDiff(directSubmission.workloadModeOneStoreAddress, directSubmission.semaphorePtr);

    uint32_t expectedData = 1u;
    for (auto &storeCmdData : storeDataCmdList) {
        MI_STORE_DATA_IMM *storeCmd = static_cast<MI_STORE_DATA_IMM *>(storeCmdData);
        auto storeData = storeCmd->getDataDword0();
        EXPECT_EQ(expectedData, storeData);
        expectedData++;
        EXPECT_EQ(expectedStoreAddress, storeCmd->getAddress());
    }

    size_t cmdOffset = 0;
    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
        cmdOffset = directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        cmdOffset += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
    }

    uint8_t *cmdBufferPosition = static_cast<uint8_t *>(directSubmission.ringCommandStream.getCpuBase()) + Dispatcher::getSizePreemption() + cmdOffset;
    MI_STORE_DATA_IMM *storeDataCmdAtPosition = genCmdCast<MI_STORE_DATA_IMM *>(cmdBufferPosition);
    ASSERT_NE(nullptr, storeDataCmdAtPosition);
    EXPECT_EQ(1u, storeDataCmdAtPosition->getDataDword0());
    EXPECT_EQ(expectedStoreAddress, storeDataCmdAtPosition->getAddress());

    cmdBufferPosition += sizeof(MI_STORE_DATA_IMM);

    auto &productHelper = pDevice->getProductHelper();
    if (productHelper.isPrefetcherDisablingInDirectSubmissionRequired()) {
        cmdBufferPosition += directSubmission.getSizeDisablePrefetcher();
    }
    MI_SEMAPHORE_WAIT *semaphoreWaitCmdAtPosition = genCmdCast<MI_SEMAPHORE_WAIT *>(cmdBufferPosition);
    ASSERT_NE(nullptr, semaphoreWaitCmdAtPosition);
    EXPECT_EQ(COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
              semaphoreWaitCmdAtPosition->getCompareOperation());
    EXPECT_EQ(1u, semaphoreWaitCmdAtPosition->getSemaphoreDataDword());
    EXPECT_EQ(directSubmission.semaphoreGpuVa, semaphoreWaitCmdAtPosition->getSemaphoreGraphicsAddress());
    EXPECT_EQ(WAIT_MODE::WAIT_MODE_POLLING_MODE, semaphoreWaitCmdAtPosition->getWaitMode());
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDiagnosticAvailableWhenDiagnosticRegistryModeTwoUsedThenDoPerformDiagnosticRunWithoutRoundtripMeasures) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using Dispatcher = RenderDispatcher<FamilyType>;

    if (!NEO::directSubmissionDiagnosticAvailable) {
        GTEST_SKIP();
    }

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(compilerProductHelper.isHeaplessModeEnabled());

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionEnableDebugBuffer.set(2);
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    debugManager.flags.DirectSubmissionDisableMonitorFence.set(true);
    debugManager.flags.DirectSubmissionDiagnosticExecutionCount.set(5);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    uint32_t expectedSemaphoreValue = directSubmission.currentQueueWorkCount;
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_TRUE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(2u, directSubmission.workloadMode);
    ASSERT_NE(nullptr, directSubmission.diagnostic.get());
    uint32_t expectedExecCount = 5u;
    expectedSemaphoreValue += expectedExecCount;
    EXPECT_EQ(expectedExecCount, directSubmission.diagnostic->getExecutionsCount());
    size_t expectedSize = Dispatcher::getSizePreemption() +
                          directSubmission.getSizeSemaphoreSection(false);
    size_t expectedDispatch = directSubmission.getSizeSemaphoreSection(false);
    EXPECT_EQ(expectedDispatch, directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false)) - directSubmission.getSizeNewResourceHandler());
    expectedSize += expectedExecCount * expectedDispatch;

    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
        expectedSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        expectedSize += RelaxedOrderingHelper::getSizeReturnPtrRegs<FamilyType>();
    }

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_EQ(0u, directSubmission.disabledDiagnosticCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    // 1 - preamble, 1 - init time, 0 exec logs in mode 2
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
    EXPECT_EQ(expectedSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(expectedSemaphoreValue, directSubmission.currentQueueWorkCount);

    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(0u, directSubmission.workloadMode);
    EXPECT_EQ(nullptr, directSubmission.diagnostic.get());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);

    GenCmdList storeDataCmdList = hwParse.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(0u, storeDataCmdList.size());
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDiagnosticAvailableWhenLoggingTimeStampsThenExpectStoredTimeStampsAvailable) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    if (!NEO::directSubmissionDiagnosticAvailable) {
        GTEST_SKIP();
    }

    uint32_t executions = 2;
    int32_t workloadMode = 1;
    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionEnableDebugBuffer.set(workloadMode);
    debugManager.flags.DirectSubmissionDiagnosticExecutionCount.set(static_cast<int32_t>(executions));

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_NE(nullptr, directSubmission.diagnostic.get());

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    // ctor: preamble 1 call
    EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);

    directSubmission.diagnostic = std::make_unique<MockDirectSubmissionDiagnosticsCollector>(
        executions,
        true,
        debugManager.flags.DirectSubmissionBufferPlacement.get(),
        debugManager.flags.DirectSubmissionSemaphorePlacement.get(),
        workloadMode,
        debugManager.flags.DirectSubmissionDisableCacheFlush.get(),
        debugManager.flags.DirectSubmissionDisableMonitorFence.get());
    EXPECT_EQ(2u, NEO::IoFunctions::mockFopenCalled);
    // dtor: 1 call general delta, 2 calls storing execution, ctor: preamble 1 call
    EXPECT_EQ(5u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);
    ASSERT_NE(nullptr, directSubmission.workloadModeOneStoreAddress);

    directSubmission.diagnostic->diagnosticModeOneWaitCollect(0, directSubmission.workloadModeOneStoreAddress, directSubmission.workloadModeOneExpectedValue);
    auto mockDiagnostic = reinterpret_cast<MockDirectSubmissionDiagnosticsCollector *>(directSubmission.diagnostic.get());

    EXPECT_NE(0ll, mockDiagnostic->executionList[0].totalTimeDiff);
    EXPECT_NE(0ll, mockDiagnostic->executionList[0].submitWaitTimeDiff);
    EXPECT_EQ(0ll, mockDiagnostic->executionList[0].dispatchSubmitTimeDiff);

    directSubmission.diagnostic->diagnosticModeOneWaitCollect(1, directSubmission.workloadModeOneStoreAddress, directSubmission.workloadModeOneExpectedValue);

    EXPECT_NE(0ll, mockDiagnostic->executionList[1].totalTimeDiff);
    EXPECT_NE(0ll, mockDiagnostic->executionList[1].submitWaitTimeDiff);
    EXPECT_EQ(0ll, mockDiagnostic->executionList[1].dispatchSubmitTimeDiff);

    // 1 call general delta, 2 calls storing execution
    uint32_t expectedVfprintfCall = NEO::IoFunctions::mockVfptrinfCalled + 1u + 2u;
    directSubmission.diagnostic.reset(nullptr);
    EXPECT_EQ(2u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(expectedVfprintfCall, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(2u, NEO::IoFunctions::mockFcloseCalled);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, DirectSubmissionTest,
            givenLegacyPlatformsWhenProgrammingPartitionRegisterThenExpectNoAction) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true, false);
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
