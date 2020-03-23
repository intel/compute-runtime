/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"
#include "shared/test/unit_test/mocks/mock_direct_submission_hw.h"

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/dispatch_flags_helper.h"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "test.h"

#include <atomic>
#include <memory>

using namespace NEO;

extern std::atomic<uintptr_t> lastClFlushedPtr;

struct DirectSubmissionFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();

        osContext.reset(OsContext::create(nullptr, 0u, 0u, aub_stream::ENGINE_RCS, PreemptionMode::ThreadGroup,
                                          false, false, false));
    }

    std::unique_ptr<OsContext> osContext;
};

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

struct DirectSubmissionDispatchBufferFixture : public DirectSubmissionFixture {
    void SetUp() {
        DirectSubmissionFixture::SetUp();
        MemoryManager *memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
        const AllocationProperties commandBufferProperties{pDevice->getRootDeviceIndex(),
                                                           true, 0x1000,
                                                           GraphicsAllocation::AllocationType::COMMAND_BUFFER,
                                                           false};
        commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);

        batchBuffer.endCmdPtr = &bbStart[0];
        batchBuffer.commandBufferAllocation = commandBuffer;
        batchBuffer.usedSize = 0x40;
    }

    void TearDown() {
        MemoryManager *memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
        memoryManager->freeGraphicsMemory(commandBuffer);

        DirectSubmissionFixture::TearDown();
    }

    BatchBuffer batchBuffer;
    uint8_t bbStart[64];
    GraphicsAllocation *commandBuffer;
};

using DirectSubmissionDispatchBufferTest = Test<DirectSubmissionDispatchBufferFixture>;

HWTEST_F(DirectSubmissionTest, whenDebugCacheFlushDisabledSetThenExpectNoCpuCacheFlush) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisableCpuCacheFlush.set(1);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());
    EXPECT_TRUE(directSubmission.disableCpuCacheFlush);

    uintptr_t expectedPtrVal = 0;
    lastClFlushedPtr = 0;
    void *ptr = reinterpret_cast<void *>(0xABCD00u);
    size_t size = 64;
    directSubmission.cpuCachelineFlush(ptr, size);
    EXPECT_EQ(expectedPtrVal, lastClFlushedPtr);
}

HWTEST_F(DirectSubmissionTest, whenDebugCacheFlushDisabledNotSetThenExpectCpuCacheFlush) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisableCpuCacheFlush.set(0);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());
    EXPECT_FALSE(directSubmission.disableCpuCacheFlush);

    uintptr_t expectedPtrVal = 0xABCD00u;
    lastClFlushedPtr = 0;
    void *ptr = reinterpret_cast<void *>(expectedPtrVal);
    size_t size = 64;
    directSubmission.cpuCachelineFlush(ptr, size);
    EXPECT_EQ(expectedPtrVal, lastClFlushedPtr);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionInitializedWhenRingIsStartedThenExpectAllocationsCreatedAndCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());
    EXPECT_FALSE(directSubmission.disableCpuCacheFlush);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);

    EXPECT_NE(nullptr, directSubmission.ringBuffer);
    EXPECT_NE(nullptr, directSubmission.ringBuffer2);
    EXPECT_NE(nullptr, directSubmission.semaphores);

    EXPECT_NE(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionInitializedWhenRingIsNotStartedThenExpectAllocationsCreatedAndCommandsNotDispatched) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_NE(nullptr, directSubmission.ringBuffer);
    EXPECT_NE(nullptr, directSubmission.ringBuffer2);
    EXPECT_NE(nullptr, directSubmission.semaphores);

    EXPECT_EQ(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSwitchBuffersWhenCurrentIsPrimaryThenExpectNextSecondary) {
    using RingBufferUse = typename MockDirectSubmissionHw<FamilyType>::RingBufferUse;
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(RingBufferUse::FirstBuffer, directSubmission.currentRingBuffer);

    GraphicsAllocation *nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(directSubmission.ringBuffer2, nextRing);
    EXPECT_EQ(RingBufferUse::SecondBuffer, directSubmission.currentRingBuffer);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSwitchBuffersWhenCurrentIsSecondaryThenExpectNextPrimary) {
    using RingBufferUse = typename MockDirectSubmissionHw<FamilyType>::RingBufferUse;
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(RingBufferUse::FirstBuffer, directSubmission.currentRingBuffer);

    GraphicsAllocation *nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(directSubmission.ringBuffer2, nextRing);
    EXPECT_EQ(RingBufferUse::SecondBuffer, directSubmission.currentRingBuffer);

    nextRing = directSubmission.switchRingBuffersAllocations();
    EXPECT_EQ(directSubmission.ringBuffer, nextRing);
    EXPECT_EQ(RingBufferUse::FirstBuffer, directSubmission.currentRingBuffer);
}
HWTEST_F(DirectSubmissionTest, givenDirectSubmissionAllocateFailWhenRingIsStartedThenExpectRingNotStarted) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());
    EXPECT_FALSE(directSubmission.disableCpuCacheFlush);

    directSubmission.allocateOsResourcesReturn = false;
    bool ret = directSubmission.initialize(true);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_EQ(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionSubmitFailWhenRingIsStartedThenExpectRingNotStartedCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    directSubmission.submitReturn = false;
    bool ret = directSubmission.initialize(true);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(directSubmission.ringStart);

    EXPECT_NE(0u, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStartWhenRingIsStartedThenExpectNoStartCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t usedSize = directSubmission.ringCommandStream.getUsed();

    ret = directSubmission.startRingBuffer();
    EXPECT_TRUE(ret);
    EXPECT_EQ(usedSize, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStartWhenRingIsNotStartedThenExpectStartCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    size_t usedSize = directSubmission.ringCommandStream.getUsed();

    ret = directSubmission.startRingBuffer();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_NE(usedSize, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStartWhenRingIsNotStartedSubmitFailThenExpectStartCommandsDispatchedRingNotStarted) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    size_t usedSize = directSubmission.ringCommandStream.getUsed();

    directSubmission.submitReturn = false;
    ret = directSubmission.startRingBuffer();
    EXPECT_FALSE(ret);
    EXPECT_FALSE(directSubmission.ringStart);
    EXPECT_NE(usedSize, directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStartWhenRingIsNotStartedAndSwitchBufferIsNeededThenExpectRingAllocationChangedStartCommandsDispatched) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    auto expectedRingBuffer = directSubmission.currentRingBuffer;
    GraphicsAllocation *oldRingBuffer = directSubmission.ringCommandStream.getGraphicsAllocation();

    directSubmission.ringCommandStream.getSpace(directSubmission.ringCommandStream.getAvailableSpace() - directSubmission.getSizeSemaphoreSection());

    ret = directSubmission.startRingBuffer();
    auto actualRingBuffer = directSubmission.currentRingBuffer;

    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.ringStart);
    EXPECT_NE(oldRingBuffer, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(directSubmission.getSizeSemaphoreSection(), directSubmission.ringCommandStream.getUsed());

    EXPECT_NE(expectedRingBuffer, actualRingBuffer);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionStopWhenStopRingIsCalledThenExpectStopCommandDispatched) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();
    uint32_t oldQueueCount = directSubmission.semaphoreData->QueueWorkCount;

    directSubmission.stopRingBuffer();

    size_t expectedDispatchSize = alreadyDispatchedSize + directSubmission.getSizeEnd();
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(oldQueueCount + 1, directSubmission.semaphoreData->QueueWorkCount);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableMonitorFenceWhenStopRingIsCalledThenExpectStopCommandAndMonitorFenceDispatched) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    MockDirectSubmissionHw<FamilyType> regularDirectSubmission(*pDevice,
                                                               std::make_unique<RenderDispatcher<FamilyType>>(),
                                                               *osContext.get());
    size_t regularSizeEnd = regularDirectSubmission.getSizeEnd();

    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();
    uint32_t oldQueueCount = directSubmission.semaphoreData->QueueWorkCount;

    size_t tagUpdateSize = directSubmission.getSizeTagUpdateSection();

    size_t disabledSizeEnd = directSubmission.getSizeEnd();
    EXPECT_EQ(disabledSizeEnd, regularSizeEnd + tagUpdateSize);

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    directSubmission.stopRingBuffer();
    size_t expectedDispatchSize = alreadyDispatchedSize + disabledSizeEnd;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(oldQueueCount + 1, directSubmission.semaphoreData->QueueWorkCount);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, alreadyDispatchedSize);
    hwParse.findHardwareCommands<FamilyType>();
    MI_BATCH_BUFFER_END *bbEnd = hwParse.getCommand<MI_BATCH_BUFFER_END>();
    EXPECT_NE(nullptr, bbEnd);

    bool foundFenceUpdate = false;
    for (auto it = hwParse.pipeControlList.begin(); it != hwParse.pipeControlList.end(); it++) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        uint32_t addressHigh = pipeControl->getAddressHigh();
        uint32_t address = pipeControl->getAddress();
        uint64_t actualAddress = (static_cast<uint64_t>(addressHigh) << 32ull) | address;
        uint64_t data = pipeControl->getImmediateData();
        if ((directSubmission.tagAddressSetValue == actualAddress) &&
            (directSubmission.tagValueSetValue == data)) {
            foundFenceUpdate = true;
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionDisableMonitorFenceWhenDispatchWorkloadCalledThenExpectStartWithoutMonitorFence) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MockDirectSubmissionHw<FamilyType> regularDirectSubmission(*pDevice,
                                                               std::make_unique<RenderDispatcher<FamilyType>>(),
                                                               *osContext.get());
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch();

    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();

    size_t tagUpdateSize = directSubmission.getSizeTagUpdateSection();

    size_t disabledSizeDispatch = directSubmission.getSizeDispatch();
    EXPECT_EQ(disabledSizeDispatch, (regularSizeDispatch - tagUpdateSize));

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    directSubmission.dispatchWorkloadSection(batchBuffer);
    size_t expectedDispatchSize = alreadyDispatchedSize + disabledSizeDispatch;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, alreadyDispatchedSize);
    hwParse.findHardwareCommands<FamilyType>();
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);

    bool foundFenceUpdate = false;
    for (auto it = hwParse.pipeControlList.begin(); it != hwParse.pipeControlList.end(); it++) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        uint32_t addressHigh = pipeControl->getAddressHigh();
        uint32_t address = pipeControl->getAddress();
        uint64_t actualAddress = (static_cast<uint64_t>(addressHigh) << 32ull) | address;
        uint64_t data = pipeControl->getImmediateData();
        if ((directSubmission.tagAddressSetValue == actualAddress) &&
            (directSubmission.tagValueSetValue == data)) {
            foundFenceUpdate = true;
            break;
        }
    }
    EXPECT_FALSE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionDisableCacheFlushWhenDispatchWorkloadCalledThenExpectStartWithoutCacheFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MockDirectSubmissionHw<FamilyType> regularDirectSubmission(*pDevice,
                                                               std::make_unique<RenderDispatcher<FamilyType>>(),
                                                               *osContext.get());
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch();

    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();

    size_t flushSize = directSubmission.getSizeFlushSection();

    size_t disabledSizeDispatch = directSubmission.getSizeDispatch();
    EXPECT_EQ(disabledSizeDispatch, (regularSizeDispatch - flushSize));

    directSubmission.dispatchWorkloadSection(batchBuffer);
    size_t expectedDispatchSize = alreadyDispatchedSize + disabledSizeDispatch;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, alreadyDispatchedSize);
    hwParse.findHardwareCommands<FamilyType>();
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);

    bool foundFlush = false;
    LinearStream parseDispatch;
    uint8_t buffer[256];
    parseDispatch.replaceBuffer(buffer, 256);
    directSubmission.cmdDispatcher->dispatchCacheFlush(parseDispatch, *directSubmission.hwInfo);
    auto expectedPipeControl = static_cast<PIPE_CONTROL *>(parseDispatch.getCpuBase());
    for (auto it = hwParse.pipeControlList.begin(); it != hwParse.pipeControlList.end(); it++) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        if (memcmp(expectedPipeControl, pipeControl, sizeof(PIPE_CONTROL)) == 0) {
            foundFlush = true;
            break;
        }
    }
    EXPECT_FALSE(foundFlush);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionDebugBufferModeOneWhenDispatchWorkloadCalledThenExpectNoStartAndLoadDataImm) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockDirectSubmissionHw<FamilyType> regularDirectSubmission(*pDevice,
                                                               std::make_unique<RenderDispatcher<FamilyType>>(),
                                                               *osContext.get());

    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch();

    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(1);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();

    size_t startSize = directSubmission.getSizeStartSection();
    size_t loadDataSize = directSubmission.getSizeStoraDataSection();

    size_t debugSizeDispatch = directSubmission.getSizeDispatch();
    EXPECT_EQ(debugSizeDispatch, (regularSizeDispatch - startSize + loadDataSize));

    directSubmission.currentQueueWorkCount = 0x40u;
    directSubmission.semaphoreGpuVa = 0xAFF0000;
    directSubmission.dispatchWorkloadSection(batchBuffer);
    size_t expectedDispatchSize = alreadyDispatchedSize + debugSizeDispatch;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, alreadyDispatchedSize);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(nullptr, bbStart);

    MI_STORE_DATA_IMM *loadData = hwParse.getCommand<MI_STORE_DATA_IMM>();
    ASSERT_NE(nullptr, loadData);
    EXPECT_EQ(0x40u, loadData->getDataDword0());
    uint64_t expectedGpuVa = directSubmission.semaphoreGpuVa;
    auto semaphore = static_cast<RingSemaphoreData *>(directSubmission.semaphorePtr);
    expectedGpuVa += ptrDiff(&semaphore->Reserved1Uint32, directSubmission.semaphorePtr);
    EXPECT_EQ(expectedGpuVa, loadData->getAddress());
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionDebugBufferModeTwoWhenDispatchWorkloadCalledThenExpectNoStartAndNoLoadDataImm) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockDirectSubmissionHw<FamilyType> regularDirectSubmission(*pDevice,
                                                               std::make_unique<RenderDispatcher<FamilyType>>(),
                                                               *osContext.get());
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch();

    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(2);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    size_t alreadyDispatchedSize = directSubmission.ringCommandStream.getUsed();

    size_t startSize = directSubmission.getSizeStartSection();

    size_t debugSizeDispatch = directSubmission.getSizeDispatch();
    EXPECT_EQ(debugSizeDispatch, (regularSizeDispatch - startSize));

    directSubmission.currentQueueWorkCount = 0x40u;
    directSubmission.dispatchWorkloadSection(batchBuffer);
    size_t expectedDispatchSize = alreadyDispatchedSize + debugSizeDispatch;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, alreadyDispatchedSize);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    EXPECT_EQ(nullptr, bbStart);

    MI_STORE_DATA_IMM *loadData = hwParse.getCommand<MI_STORE_DATA_IMM>();
    EXPECT_EQ(nullptr, loadData);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchSemaphoreThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchSemaphoreSection(1u);
    EXPECT_EQ(directSubmission.getSizeSemaphoreSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchStartSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchStartSection(1ull);
    EXPECT_EQ(directSubmission.getSizeStartSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchSwitchRingBufferSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchSwitchRingBufferSection(1ull);
    EXPECT_EQ(directSubmission.getSizeSwitchRingBufferSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchFlushSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchFlushSection();
    EXPECT_EQ(directSubmission.getSizeFlushSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchTagUpdateSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchTagUpdateSection(0ull, 0ull);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(directSubmission.getSizeTagUpdateSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenDispatchEndingSectionThenExpectCorrectSizeUsed) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    directSubmission.dispatchEndingSection();
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    EXPECT_EQ(directSubmission.getSizeEndingSection(), directSubmission.ringCommandStream.getUsed());
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    size_t expectedSize = directSubmission.getSizeStartSection() +
                          directSubmission.getSizeFlushSection() +
                          directSubmission.getSizeTagUpdateSection() +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionEnableDebugBufferModeOneWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(1);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());
    size_t expectedSize = directSubmission.getSizeStoraDataSection() +
                          directSubmission.getSizeFlushSection() +
                          directSubmission.getSizeTagUpdateSection() +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionEnableDebugBufferModeTwoWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionEnableDebugBuffer.set(2);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());
    size_t expectedSize = directSubmission.getSizeFlushSection() +
                          directSubmission.getSizeTagUpdateSection() +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableCacheFlushWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());
    size_t expectedSize = directSubmission.getSizeStartSection() +
                          directSubmission.getSizeTagUpdateSection() +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest,
         givenDirectSubmissionDisableMonitorFenceWhenGetDispatchSizeThenExpectCorrectSizeReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    size_t expectedSize = directSubmission.getSizeStartSection() +
                          directSubmission.getSizeFlushSection() +
                          directSubmission.getSizeSemaphoreSection();
    size_t actualSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenGetEndSizeThenExpectCorrectSizeReturned) {
    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    size_t expectedSize = directSubmission.getSizeEndingSection() +
                          directSubmission.getSizeFlushSection();
    size_t actualSize = directSubmission.getSizeEnd();
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionWhenSettingAddressInReturnCommandThenVerifyCorrectlySet) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    uint64_t returnAddress = 0x1A2BF000;
    void *space = directSubmission.ringCommandStream.getSpace(sizeof(MI_BATCH_BUFFER_START));
    directSubmission.setReturnAddress(space, returnAddress);
    MI_BATCH_BUFFER_START *bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(space);
    EXPECT_EQ(returnAddress, bbStart->getBatchBufferStartAddressGraphicsaddress472());
}

HWTEST_F(DirectSubmissionTest, whenDirectSubmissionInitializedThenExpectCreatedAllocationsFreed) {
    MemoryManager *memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();

    std::unique_ptr<MockDirectSubmissionHw<FamilyType>> directSubmission =
        std::make_unique<MockDirectSubmissionHw<FamilyType>>(*pDevice,
                                                             std::make_unique<RenderDispatcher<FamilyType>>(),
                                                             *osContext.get());
    bool ret = directSubmission->initialize(false);
    EXPECT_TRUE(ret);

    GraphicsAllocation *nulledAllocation = directSubmission->ringBuffer;
    directSubmission->ringBuffer = nullptr;
    directSubmission.reset(nullptr);
    memoryManager->freeGraphicsMemory(nulledAllocation);

    directSubmission =
        std::make_unique<MockDirectSubmissionHw<FamilyType>>(*pDevice,
                                                             std::make_unique<RenderDispatcher<FamilyType>>(),
                                                             *osContext.get());
    ret = directSubmission->initialize(false);
    EXPECT_TRUE(ret);

    nulledAllocation = directSubmission->ringBuffer2;
    directSubmission->ringBuffer2 = nullptr;
    directSubmission.reset(nullptr);
    memoryManager->freeGraphicsMemory(nulledAllocation);

    directSubmission =
        std::make_unique<MockDirectSubmissionHw<FamilyType>>(*pDevice,
                                                             std::make_unique<RenderDispatcher<FamilyType>>(),
                                                             *osContext.get());
    ret = directSubmission->initialize(false);
    EXPECT_TRUE(ret);
    nulledAllocation = directSubmission->semaphores;
    directSubmission->semaphores = nullptr;
    directSubmission.reset(nullptr);
    memoryManager->freeGraphicsMemory(nulledAllocation);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingStartAndSwitchBuffersWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferAndQueueCountIncrease) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = directSubmission.cmdDispatcher->getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection();
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(oldRingAllocation->getGpuAddress(), directSubmission.submitGpuAddress);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    size_t sizeUsed = directSubmission.ringCommandStream.getUsed();
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_EQ(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(1u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    EXPECT_EQ(sizeUsed + directSubmission.getSizeDispatch(), directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0u);
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingNotStartAndSwitchBuffersWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferQueueCountIncreaseAndSubmitToGpu) {
    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(0u, directSubmission.submitCount);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_EQ(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(1u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = directSubmission.getSizeDispatch();
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(oldRingAllocation->getGpuAddress(), directSubmission.submitGpuAddress);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    EXPECT_EQ(directSubmission.getSizeDispatch(), directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingStartWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferAndQueueCountIncrease) {
    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = directSubmission.cmdDispatcher->getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection();
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(oldRingAllocation->getGpuAddress(), directSubmission.submitGpuAddress);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    directSubmission.ringCommandStream.getSpace(directSubmission.ringCommandStream.getAvailableSpace() -
                                                directSubmission.getSizeSwitchRingBufferSection());

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_NE(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(1u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    EXPECT_EQ(directSubmission.getSizeDispatch(), directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingNotStartWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferQueueCountIncreaseAndSubmitToGpu) {
    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType> directSubmission(*pDevice,
                                                        std::make_unique<RenderDispatcher<FamilyType>>(),
                                                        *osContext.get());

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(0u, directSubmission.submitCount);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    directSubmission.ringCommandStream.getSpace(directSubmission.ringCommandStream.getAvailableSpace() -
                                                directSubmission.getSizeSwitchRingBufferSection());
    uint64_t submitGpuVa = oldRingAllocation->getGpuAddress() + directSubmission.ringCommandStream.getUsed();

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_NE(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(1u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = directSubmission.getSizeSwitchRingBufferSection();
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(submitGpuVa, directSubmission.submitGpuAddress);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    EXPECT_EQ(directSubmission.getSizeDispatch(), directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionTest, givenSuperBaseCsrWhenCheckingDirectSubmissionAvailableThenReturnFalse) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrSuperBaseCallDirectSubmissionAvailable = true;

    int32_t executionStamp = 0;
    std::unique_ptr<MockCsr<FamilyType>> mockCsr =
        std::make_unique<MockCsr<FamilyType>>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());

    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_FALSE(ret);
}

HWTEST_F(DirectSubmissionTest, givenBaseCsrWhenCheckingDirectSubmissionAvailableThenReturnFalse) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallDirectSubmissionAvailable = true;

    int32_t executionStamp = 0;
    std::unique_ptr<MockCsr<FamilyType>> mockCsr =
        std::make_unique<MockCsr<FamilyType>>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());

    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_FALSE(ret);
}

HWTEST_F(DirectSubmissionTest, givenDirectSubmissionAvailableWhenProgrammingEndingCommandThenUseBatchBufferStart) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    int32_t executionStamp = 0;
    std::unique_ptr<MockCsr<FamilyType>> mockCsr =
        std::make_unique<MockCsr<FamilyType>>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    mockCsr->directSubmissionAvailable = true;
    bool ret = mockCsr->isDirectSubmissionEnabled();
    EXPECT_TRUE(ret);

    void *location = nullptr;
    uint8_t buffer[128];
    mockCsr->commandStream.replaceBuffer(&buffer[0], 128u);
    mockCsr->programEndingCmd(mockCsr->commandStream, &location, ret);
    EXPECT_EQ(sizeof(MI_BATCH_BUFFER_START), mockCsr->commandStream.getUsed());

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.epilogueRequired = true;
    size_t expectedSize = sizeof(MI_BATCH_BUFFER_START) +
                          mockCsr->getCmdSizeForEpilogueCommands(dispatchFlags);
    expectedSize = alignUp(expectedSize, MemoryConstants::cacheLineSize);
    EXPECT_EQ(expectedSize, mockCsr->getCmdSizeForEpilogue(dispatchFlags));
}

HWTEST_F(DirectSubmissionTest, whenInitDirectSubmissionFailThenEngineIsNotCreated) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrFailInitDirectSubmission = true;
    auto device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo);
    EXPECT_EQ(nullptr, device);
}
