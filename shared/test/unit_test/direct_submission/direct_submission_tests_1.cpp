/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/direct_submission_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_direct_submission_diagnostic_collector.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/hw_test.h"

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

HWTEST_F(DirectSubmissionTest, whenDebugCacheFlushDisabledSetThenExpectNoCpuCacheFlush) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisableCpuCacheFlush.set(1);

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
    DebugManager.flags.DirectSubmissionDisableCpuCacheFlush.set(0);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.disableCpuCacheFlush);

    uintptr_t expectedPtrVal = 0xABCD00u;
    CpuIntrinsicsTests::lastClFlushedPtr = 0;
    void *ptr = reinterpret_cast<void *>(expectedPtrVal);
    size_t size = 64;
    directSubmission.cpuCachelineFlush(ptr, size);
    EXPECT_EQ(expectedPtrVal, CpuIntrinsicsTests::lastClFlushedPtr);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenStopThenRingIsNotStarted) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.directSubmission.reset(&directSubmission);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.stopDirectSubmission();
    EXPECT_FALSE(directSubmission.ringStart);

    csr.directSubmission.release();
}

HWTEST_F(DirectSubmissionTest, givenBlitterDirectSubmissionWhenStopThenRingIsNotStarted) {
    MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr.blitterDirectSubmission.reset(&directSubmission);
    csr.setupContext(*osContext);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    csr.stopDirectSubmission();
    EXPECT_FALSE(directSubmission.ringStart);

    csr.blitterDirectSubmission.release();
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

    EXPECT_EQ(1, mockMemoryOperations->makeResidentCalledCount);
    ASSERT_EQ(3u, mockMemoryOperations->gfxAllocationsForMakeResident.size());
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

    EXPECT_EQ(1, mockMemoryOperations->makeResidentCalledCount);
    ASSERT_EQ(4u, mockMemoryOperations->gfxAllocationsForMakeResident.size());
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

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStartWhenRingIsStartedThenExpectNoStartCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    size_t usedSize = directSubmission.ringCommandStream.getUsed();

    ret = directSubmission.startRingBuffer();
    EXPECT_TRUE(ret);
    EXPECT_EQ(usedSize, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStartWhenRingIsNotStartedThenExpectStartCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    size_t usedSize = directSubmission.ringCommandStream.getUsed();

    ret = directSubmission.startRingBuffer();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_NE(usedSize, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStartWhenRingIsNotStartedSubmitFailThenExpectStartCommandsDispatchedRingNotStarted) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    size_t usedSize = directSubmission.ringCommandStream.getUsed();

    directSubmission.submitReturn = false;
    ret = directSubmission.startRingBuffer();
    EXPECT_FALSE(ret);
    EXPECT_FALSE(directSubmission.ringStart);
    EXPECT_NE(usedSize, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStartWhenRingIsNotStartedAndSwitchBufferIsNeededThenExpectRingAllocationChangedStartCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    auto expectedRingBuffer = directSubmission.currentRingBuffer;
    GraphicsAllocation *oldRingBuffer = directSubmission.ringCommandStream.getGraphicsAllocation();

    auto requiredSize = directSubmission.getSizeSemaphoreSection();
    if (directSubmission.miMemFenceRequired) {
        requiredSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }

    directSubmission.ringCommandStream.getSpace(directSubmission.ringCommandStream.getAvailableSpace() - requiredSize);

    ret = directSubmission.startRingBuffer();
    auto actualRingBuffer = directSubmission.currentRingBuffer;

    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_NE(oldRingBuffer, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(requiredSize, directSubmission.ringCommandStream.getUsed());

    EXPECT_NE(expectedRingBuffer, actualRingBuffer);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStopWhenStopRingIsCalledThenExpectStopCommandDispatched) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();
    uint32_t oldQueueCount = directSubmission.semaphoreData->QueueWorkCount;

    directSubmission.stopRingBuffer();

    size_t expectedDispatchSize = alreadyDispatchedSize + directSubmission.getSizeEnd();
    EXPECT_LE(directSubmission.ringCommandStream.getUsed(), expectedDispatchSize);
    EXPECT_GE(directSubmission.ringCommandStream.getUsed() + MemoryConstants::cacheLineSize, expectedDispatchSize);
    EXPECT_EQ(oldQueueCount + 1, directSubmission.semaphoreData->QueueWorkCount);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableMonitorFenceWhenStopRingIsCalledThenExpectStopCommandAndMonitorFenceDispatched) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    size_t regularSizeEnd = regularDirectSubmission.getSizeEnd();

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    directSubmission.disableMonitorFence = true;
    directSubmission.ringStart = true;

    EXPECT_TRUE(ret);

    size_t tagUpdateSize = Dispatcher::getSizeMonitorFence(*directSubmission.hwInfo);

    size_t disabledSizeEnd = directSubmission.getSizeEnd();
    EXPECT_EQ(disabledSizeEnd, regularSizeEnd + tagUpdateSize);

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    directSubmission.stopRingBuffer();
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
    EXPECT_EQ(directSubmission.getSizeSemaphoreSection(), directSubmission.ringCommandStream.getUsed());
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

    Dispatcher::dispatchCacheFlush(directSubmission.ringCommandStream, *directSubmission.hwInfo, 0ull);
    EXPECT_EQ(Dispatcher::getSizeCacheFlush(*directSubmission.hwInfo), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchTagUpdateSectionThenExpectCorrectSizeUsed) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDirectSubmissionHw<FamilyType, Dispatcher>
        directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    Dispatcher::dispatchMonitorFence(directSubmission.ringCommandStream, 0ull, 0ull, *directSubmission.hwInfo, false, false);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(Dispatcher::getSizeMonitorFence(*directSubmission.hwInfo), directSubmission.ringCommandStream.getUsed());
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
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(0);
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t expectedSize = directSubmission.getSizeStartSection() +
                          Dispatcher::getSizeCacheFlush(*directSubmission.hwInfo) +
                          Dispatcher::getSizeMonitorFence(*directSubmission.hwInfo) +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionEnableDebugBufferModeOneWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(0);
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.workloadMode = 1;

    size_t expectedSize = Dispatcher::getSizeStoreDwordCommand() +
                          Dispatcher::getSizeCacheFlush(*directSubmission.hwInfo) +
                          Dispatcher::getSizeMonitorFence(*directSubmission.hwInfo) +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionEnableDebugBufferModeTwoWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(0);
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.workloadMode = 2;
    size_t expectedSize = Dispatcher::getSizeCacheFlush(*directSubmission.hwInfo) +
                          Dispatcher::getSizeMonitorFence(*directSubmission.hwInfo) +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableCacheFlushWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableCacheFlush = true;
    size_t expectedSize = directSubmission.getSizeStartSection() +
                          Dispatcher::getSizeMonitorFence(*directSubmission.hwInfo) +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableMonitorFenceWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(0);
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = true;
    size_t expectedSize = directSubmission.getSizeStartSection() +
                          Dispatcher::getSizeCacheFlush(*directSubmission.hwInfo) +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenGetEndSizeThenExpectCorrectSizeReturned) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t expectedSize = Dispatcher::getSizeStopCommandBuffer() +
                          Dispatcher::getSizeCacheFlush(*directSubmission.hwInfo) +
                          (Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer()) +
                          MemoryConstants::cacheLineSize;
    size_t actualSize = directSubmission.getSizeEnd();
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
    mockCsr->directSubmissionAvailable = true;
    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_TRUE(ret);

    MockGraphicsAllocation mockAllocation;

    void *location = nullptr;
    uint8_t buffer[128];
    mockCsr->commandStream.replaceBuffer(&buffer[0], 128u);
    mockCsr->commandStream.replaceGraphicsAllocation(&mockAllocation);
    auto &device = *pDevice;
    mockCsr->programEndingCmd(mockCsr->commandStream, device, &location, ret);
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
    mockCsr->directSubmissionAvailable = true;
    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_TRUE(ret);

    void *location = nullptr;

    uint8_t buffer[256] = {};
    auto &cmdStream = mockCsr->commandStream;
    cmdStream.replaceBuffer(&buffer[0], 256);
    cmdStream.replaceGraphicsAllocation(&mockAllocation);
    auto &device = *pDevice;

    for (int32_t value : {-1, 0, 1}) {
        DebugManager.flags.BatchBufferStartPrepatchingWaEnabled.set(value);

        auto currectBbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(cmdStream.getSpace(0));
        uint64_t expectedGpuVa = cmdStream.getGraphicsAllocation()->getGpuAddress() + cmdStream.getUsed();

        mockCsr->programEndingCmd(cmdStream, device, &location, ret);
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
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(1);
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_FALSE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_FALSE(directSubmission.disableMonitorFence);
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

    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(1);
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_FALSE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_FALSE(directSubmission.disableMonitorFence);
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
                          directSubmission.getSizeSemaphoreSection();
    if (directSubmission.miMemFenceRequired) {
        expectedSize += directSubmission.getSizeSystemMemoryFenceAddress();
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
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_FALSE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_FALSE(directSubmission.disableMonitorFence);
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

    uint32_t execCount = 5u;
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(1);
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(true);
    DebugManager.flags.DirectSubmissionDiagnosticExecutionCount.set(static_cast<int32_t>(execCount));

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    uint32_t expectedSemaphoreValue = directSubmission.currentQueueWorkCount;
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_FALSE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(1u, directSubmission.workloadMode);
    ASSERT_NE(nullptr, directSubmission.diagnostic.get());
    uint32_t expectedExecCount = 5u;
    expectedSemaphoreValue += expectedExecCount;
    EXPECT_EQ(expectedExecCount, directSubmission.diagnostic->getExecutionsCount());
    size_t expectedSize = Dispatcher::getSizePreemption() +
                          directSubmission.getSizeSemaphoreSection() +
                          directSubmission.getDiagnosticModeSection();
    expectedSize += expectedExecCount * directSubmission.getSizeDispatch();

    if (directSubmission.miMemFenceRequired) {
        expectedSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_EQ(0u, directSubmission.disabledDiagnosticCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    //1 - preamble, 1 - init time, 5 - exec logs
    EXPECT_EQ(7u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
    EXPECT_EQ(expectedSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(expectedSemaphoreValue, directSubmission.currentQueueWorkCount);

    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_FALSE(directSubmission.disableMonitorFence);
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

    size_t sysMemFenceOffset = 0;
    if (directSubmission.miMemFenceRequired) {
        sysMemFenceOffset = directSubmission.getSizeSystemMemoryFenceAddress();
    }

    uint8_t *cmdBufferPosition = static_cast<uint8_t *>(directSubmission.ringCommandStream.getCpuBase()) + Dispatcher::getSizePreemption() + sysMemFenceOffset;
    MI_STORE_DATA_IMM *storeDataCmdAtPosition = genCmdCast<MI_STORE_DATA_IMM *>(cmdBufferPosition);
    ASSERT_NE(nullptr, storeDataCmdAtPosition);
    EXPECT_EQ(1u, storeDataCmdAtPosition->getDataDword0());
    EXPECT_EQ(expectedStoreAddress, storeDataCmdAtPosition->getAddress());

    cmdBufferPosition += sizeof(MI_STORE_DATA_IMM);
    cmdBufferPosition += directSubmission.getSizeDisablePrefetcher();
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

    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(2);
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(true);
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(true);
    DebugManager.flags.DirectSubmissionDiagnosticExecutionCount.set(5);

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    uint32_t expectedSemaphoreValue = directSubmission.currentQueueWorkCount;
    EXPECT_TRUE(UllsDefaults::defaultDisableCacheFlush);
    EXPECT_FALSE(UllsDefaults::defaultDisableMonitorFence);
    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_TRUE(directSubmission.disableMonitorFence);
    EXPECT_EQ(2u, directSubmission.workloadMode);
    ASSERT_NE(nullptr, directSubmission.diagnostic.get());
    uint32_t expectedExecCount = 5u;
    expectedSemaphoreValue += expectedExecCount;
    EXPECT_EQ(expectedExecCount, directSubmission.diagnostic->getExecutionsCount());
    size_t expectedSize = Dispatcher::getSizePreemption() +
                          directSubmission.getSizeSemaphoreSection();
    size_t expectedDispatch = directSubmission.getSizeSemaphoreSection();
    EXPECT_EQ(expectedDispatch, directSubmission.getSizeDispatch());
    expectedSize += expectedExecCount * expectedDispatch;

    if (directSubmission.miMemFenceRequired) {
        expectedSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_EQ(0u, directSubmission.disabledDiagnosticCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    //1 - preamble, 1 - init time, 0 exec logs in mode 2
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
    EXPECT_EQ(expectedSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(expectedSemaphoreValue, directSubmission.currentQueueWorkCount);

    EXPECT_TRUE(directSubmission.disableCacheFlush);
    EXPECT_FALSE(directSubmission.disableMonitorFence);
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
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(workloadMode);
    DebugManager.flags.DirectSubmissionDiagnosticExecutionCount.set(static_cast<int32_t>(executions));

    NEO::IoFunctions::mockFopenCalled = 0u;
    NEO::IoFunctions::mockVfptrinfCalled = 0u;
    NEO::IoFunctions::mockFcloseCalled = 0u;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_NE(nullptr, directSubmission.diagnostic.get());

    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
    //ctor: preamble 1 call
    EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);

    directSubmission.diagnostic = std::make_unique<MockDirectSubmissionDiagnosticsCollector>(
        executions,
        true,
        DebugManager.flags.DirectSubmissionBufferPlacement.get(),
        DebugManager.flags.DirectSubmissionSemaphorePlacement.get(),
        workloadMode,
        DebugManager.flags.DirectSubmissionDisableCacheFlush.get(),
        DebugManager.flags.DirectSubmissionDisableMonitorFence.get());
    EXPECT_EQ(2u, NEO::IoFunctions::mockFopenCalled);
    //dtor: 1 call general delta, 2 calls storing execution, ctor: preamble 1 call
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

    //1 call general delta, 2 calls storing execution
    uint32_t expectedVfprintfCall = NEO::IoFunctions::mockVfptrinfCalled + 1u + 2u;
    directSubmission.diagnostic.reset(nullptr);
    EXPECT_EQ(2u, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(expectedVfprintfCall, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(2u, NEO::IoFunctions::mockFcloseCalled);
}

HWCMDTEST_F(IGFX_GEN8_CORE, DirectSubmissionTest,
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
    DebugManager.flags.ForcePreParserEnabledForMiArbCheck.set(1);

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
