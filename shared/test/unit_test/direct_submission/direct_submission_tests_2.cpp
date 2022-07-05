/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/utilities/cpuintrinsics.h"
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

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> sfenceCounter;
} // namespace CpuIntrinsicsTests

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

using DirectSubmissionDispatchBufferTest = Test<DirectSubmissionDispatchBufferFixture>;

struct DirectSubmissionDispatchMiMemFenceTest : public DirectSubmissionDispatchBufferTest {
    void SetUp() override {
        DirectSubmissionDispatchBufferTest::SetUp();

        auto hwInfoConfig = HwInfoConfig::get(pDevice->getHardwareInfo().platform.eProductFamily);
        miMemFenceSupported = hwInfoConfig->isGlobalFenceInDirectSubmissionRequired(pDevice->getHardwareInfo());
    }

    template <typename FamilyType>
    void validateFenceProgramming(MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> &directSubmission, uint32_t expectedFenceCount, uint32_t expectedSysMemFenceCount) {
        int32_t id = 0;
        int32_t systemMemoryFenceId = -1;
        uint32_t fenceCount = 0;
        uint32_t sysMemFenceCount = 0;

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
        hwParse.findHardwareCommands<FamilyType>();

        if constexpr (FamilyType::isUsingMiMemFence) {
            using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
            using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

            for (auto &it : hwParse.cmdList) {
                if (auto sysFenceAddress = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(it)) {
                    EXPECT_EQ(-1, systemMemoryFenceId);
                    systemMemoryFenceId = id;
                    sysMemFenceCount++;

                    EXPECT_NE(0u, sysFenceAddress->getSystemMemoryFenceAddress());
                } else if (auto miMemFence = genCmdCast<MI_MEM_FENCE *>(it)) {
                    if (miMemFence->getFenceType() == MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_ACQUIRE) {
                        EXPECT_TRUE(id > systemMemoryFenceId);

                        fenceCount++;
                    }
                }

                id++;
            }
        } else if (miMemFenceSupported) {
            using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
            expectedSysMemFenceCount = 0u;
            for (auto &it : hwParse.cmdList) {
                if (auto sysFenceAddress = genCmdCast<MI_SEMAPHORE_WAIT *>(it)) {
                    fenceCount++;
                }

                id++;
            }
            fenceCount /= 2;
        }

        if (miMemFenceSupported) {
            if (expectedSysMemFenceCount > 0) {
                EXPECT_NE(-1, systemMemoryFenceId);
            } else {
                EXPECT_EQ(-1, systemMemoryFenceId);
            }
            EXPECT_EQ(expectedFenceCount, fenceCount);
            EXPECT_EQ(expectedSysMemFenceCount, sysMemFenceCount);
        } else {
            EXPECT_EQ(-1, systemMemoryFenceId);
            EXPECT_EQ(0u, fenceCount);
            EXPECT_EQ(0u, sysMemFenceCount);
        }
    }

    bool miMemFenceSupported = false;
};

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenMiMemFenceSupportedWhenInitializingDirectSubmissionThenEnableMiMemFenceProgramming) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(miMemFenceSupported, directSubmission.miMemFenceRequired);
    EXPECT_FALSE(directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.initialize(true, false));

    EXPECT_EQ(miMemFenceSupported, directSubmission.systemMemoryFenceAddressSet);

    validateFenceProgramming<FamilyType>(directSubmission, 1, 1);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenMiMemFenceSupportedWhenDispatchingWithoutInitThenEnableMiMemFenceProgramming) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    FlushStampTracker flushStamp(true);

    EXPECT_EQ(miMemFenceSupported, directSubmission.miMemFenceRequired);
    EXPECT_FALSE(directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.initialize(false, false));

    EXPECT_FALSE(directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    validateFenceProgramming<FamilyType>(directSubmission, 2, 1);

    EXPECT_EQ(miMemFenceSupported, directSubmission.systemMemoryFenceAddressSet);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenMiMemFenceSupportedWhenSysMemFenceIsAlreadySentThenDontReprogram) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    FlushStampTracker flushStamp(true);

    EXPECT_EQ(miMemFenceSupported, directSubmission.miMemFenceRequired);
    directSubmission.systemMemoryFenceAddressSet = true;

    EXPECT_TRUE(directSubmission.initialize(false, false));

    EXPECT_TRUE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    validateFenceProgramming<FamilyType>(directSubmission, 2, 0);

    EXPECT_TRUE(directSubmission.systemMemoryFenceAddressSet);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenDebugFlagSetWhenCreatingDirectSubmissionThenDontEnableMiMemFenceProgramming) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_FALSE(directSubmission.miMemFenceRequired);
    EXPECT_FALSE(directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.initialize(true, false));

    EXPECT_FALSE(directSubmission.systemMemoryFenceAddressSet);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DirectSubmissionDispatchBufferTest,
            givenDirectSubmissionInPartitionModeWhenDispatchingCommandBufferThenExpectDispatchPartitionedPipeControlInCommandBuffer) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    FlushStampTracker flushStamp(true);

    pDevice->rootCsrCreated = true;
    pDevice->numSubDevices = 2;

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*pDevice);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    directSubmission.partitionConfigSet = false;
    directSubmission.disableMonitorFence = false;
    directSubmission.partitionedMode = true;
    directSubmission.workPartitionAllocation = ultCsr->getWorkPartitionAllocation();

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection() +
                        sizeof(MI_LOAD_REGISTER_IMM) +
                        sizeof(MI_LOAD_REGISTER_MEM);
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(oldRingAllocation->getGpuAddress(), directSubmission.submitGpuAddress);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    directSubmission.ringCommandStream.getSpace(directSubmission.ringCommandStream.getAvailableSpace() -
                                                directSubmission.getSizeSwitchRingBufferSection());

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_NE(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(1u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    EXPECT_EQ(directSubmission.getSizeDispatch(), directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    bool foundFenceUpdate = false;
    for (auto &it : hwParse.pipeControlList) {
        PIPE_CONTROL *pipeControl = reinterpret_cast<PIPE_CONTROL *>(it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            foundFenceUpdate = true;
            EXPECT_EQ(directSubmission.tagAddressSetValue, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
            uint64_t data = pipeControl->getImmediateData();
            EXPECT_EQ(directSubmission.tagValueSetValue, data);
            EXPECT_TRUE(pipeControl->getWorkloadPartitionIdOffsetEnable());
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionDisableMonitorFenceWhenDispatchWorkloadCalledThenExpectStartWithoutMonitorFence) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(0);

    MockDirectSubmissionHw<FamilyType, Dispatcher> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch();

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = true;

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t tagUpdateSize = Dispatcher::getSizeMonitorFence(*directSubmission.hwInfo);

    size_t disabledSizeDispatch = directSubmission.getSizeDispatch();
    EXPECT_EQ(disabledSizeDispatch, (regularSizeDispatch - tagUpdateSize));

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    directSubmission.dispatchWorkloadSection(batchBuffer);
    size_t expectedDispatchSize = disabledSizeDispatch;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);

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
    EXPECT_FALSE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionDisableCacheFlushWhenDispatchWorkloadCalledThenExpectStartWithoutCacheFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(0);

    MockDirectSubmissionHw<FamilyType, Dispatcher> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch();

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.disableCacheFlush = true;
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t flushSize = Dispatcher::getSizeCacheFlush(*directSubmission.hwInfo);

    size_t disabledSizeDispatch = directSubmission.getSizeDispatch();
    EXPECT_EQ(disabledSizeDispatch, (regularSizeDispatch - flushSize));

    directSubmission.dispatchWorkloadSection(batchBuffer);
    size_t expectedDispatchSize = disabledSizeDispatch;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);

    bool foundFlush = false;
    LinearStream parseDispatch;
    uint8_t buffer[256];
    parseDispatch.replaceBuffer(buffer, 256);
    RenderDispatcher<FamilyType>::dispatchCacheFlush(parseDispatch, *directSubmission.hwInfo, 0ull);
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
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch();

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.workloadMode = 1;
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t startSize = directSubmission.getSizeStartSection();
    size_t storeDataSize = Dispatcher::getSizeStoreDwordCommand();

    size_t debugSizeDispatch = directSubmission.getSizeDispatch();
    EXPECT_EQ(debugSizeDispatch, (regularSizeDispatch - startSize + storeDataSize));

    directSubmission.workloadModeOneExpectedValue = 0x40u;
    directSubmission.semaphoreGpuVa = 0xAFF0000;
    directSubmission.dispatchWorkloadSection(batchBuffer);
    size_t expectedDispatchSize = debugSizeDispatch;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);

    if (directSubmission.getSizePrefetchMitigation() == sizeof(MI_BATCH_BUFFER_START)) {
        EXPECT_EQ(1u, hwParse.getCommandCount<MI_BATCH_BUFFER_START>());
    } else {
        EXPECT_EQ(0u, hwParse.getCommandCount<MI_BATCH_BUFFER_START>());
    }

    MI_STORE_DATA_IMM *storeData = hwParse.getCommand<MI_STORE_DATA_IMM>();
    ASSERT_NE(nullptr, storeData);
    EXPECT_EQ(0x40u + 1u, storeData->getDataDword0());
    uint64_t expectedGpuVa = directSubmission.semaphoreGpuVa;
    auto semaphore = static_cast<RingSemaphoreData *>(directSubmission.semaphorePtr);
    expectedGpuVa += ptrDiff(&semaphore->DiagnosticModeCounter, directSubmission.semaphorePtr);
    EXPECT_EQ(expectedGpuVa, storeData->getAddress());
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionDebugBufferModeTwoWhenDispatchWorkloadCalledThenExpectNoStartAndNoLoadDataImm) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch();

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.workloadMode = 2;
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t startSize = directSubmission.getSizeStartSection();

    size_t debugSizeDispatch = directSubmission.getSizeDispatch();
    EXPECT_EQ(debugSizeDispatch, (regularSizeDispatch - startSize));

    directSubmission.currentQueueWorkCount = 0x40u;
    directSubmission.dispatchWorkloadSection(batchBuffer);
    size_t expectedDispatchSize = debugSizeDispatch;
    EXPECT_EQ(expectedDispatchSize, directSubmission.ringCommandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);

    if (directSubmission.getSizePrefetchMitigation() == sizeof(MI_BATCH_BUFFER_START)) {
        EXPECT_EQ(1u, hwParse.getCommandCount<MI_BATCH_BUFFER_START>());
    } else {
        EXPECT_EQ(0u, hwParse.getCommandCount<MI_BATCH_BUFFER_START>());
    }

    MI_STORE_DATA_IMM *storeData = hwParse.getCommand<MI_STORE_DATA_IMM>();
    EXPECT_EQ(nullptr, storeData);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingStartAndSwitchBuffersWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferAndQueueCountIncrease) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection();
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
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

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
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
    EXPECT_EQ(2u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(3u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = directSubmission.getSizeSemaphoreSection();
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(oldRingAllocation->getGpuAddress(), directSubmission.submitGpuAddress);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    size_t dispatchSize = submitSize + directSubmission.getSizeDispatch();

    EXPECT_EQ(dispatchSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingStartWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferAndQueueCountIncrease) {
    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection();
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
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

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(0u, directSubmission.submitCount);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    directSubmission.ringCommandStream.getSpace(directSubmission.ringCommandStream.getAvailableSpace() -
                                                directSubmission.getSizeSwitchRingBufferSection());

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_NE(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(2u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(3u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = directSubmission.getSizeSemaphoreSection();
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    size_t dispatchSize = submitSize + directSubmission.getSizeDispatch();

    EXPECT_EQ(dispatchSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDirectSubmissionPrintBuffersWhenInitializeAndDispatchBufferThenCommandBufferArePrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionPrintBuffers.set(true);

    FlushStampTracker flushStamp(true);
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    testing::internal::CaptureStdout();

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);

    std::string output = testing::internal::GetCapturedStdout();

    auto pos = output.find("Ring buffer 0");
    EXPECT_TRUE(pos != std::string::npos);
    pos = output.find("Ring buffer 1");
    EXPECT_TRUE(pos != std::string::npos);
    pos = output.find("Client buffer");
    EXPECT_TRUE(pos != std::string::npos);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DirectSubmissionDispatchBufferTest,
            givenDirectSubmissionRingStartWhenMultiTileSupportedThenExpectMultiTileConfigSetAndWorkPartitionResident) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    pDevice->rootCsrCreated = true;
    pDevice->numSubDevices = 2;

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*pDevice);

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    directSubmission.activeTiles = 2;
    directSubmission.partitionedMode = true;
    directSubmission.partitionConfigSet = false;
    directSubmission.workPartitionAllocation = ultCsr->getWorkPartitionAllocation();

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());

    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection() +
                        sizeof(MI_LOAD_REGISTER_IMM) +
                        sizeof(MI_LOAD_REGISTER_MEM);
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);
    EXPECT_EQ(4u, directSubmission.makeResourcesResidentVectorSize);

    uint32_t expectedOffset = NEO::ImplicitScalingDispatch<FamilyType>::getPostSyncOffset();
    EXPECT_EQ(expectedOffset, directSubmission.postSyncOffset);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    ASSERT_NE(hwParse.lriList.end(), hwParse.lriList.begin());
    bool partitionRegisterFound = false;
    for (auto &it : hwParse.lriList) {
        auto loadRegisterImm = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(it);
        if (loadRegisterImm->getRegisterOffset() == 0x23B4u) {
            EXPECT_EQ(expectedOffset, loadRegisterImm->getDataDword());
            partitionRegisterFound = true;
        }
    }
    EXPECT_TRUE(partitionRegisterFound);

    auto loadRegisterMemItor = find<MI_LOAD_REGISTER_MEM *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), loadRegisterMemItor);
    auto loadRegisterMem = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(*loadRegisterMemItor);
    EXPECT_EQ(0x221Cu, loadRegisterMem->getRegisterAddress());
    uint64_t gpuAddress = ultCsr->getWorkPartitionAllocation()->getGpuAddress();
    EXPECT_EQ(gpuAddress, loadRegisterMem->getMemoryAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DirectSubmissionDispatchBufferTest,
            givenDirectSubmissionRingNotStartOnInitWhenMultiTileSupportedThenExpectMultiTileConfigSetDuringExplicitRingStart) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    pDevice->rootCsrCreated = true;
    pDevice->numSubDevices = 2;

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*pDevice);

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    directSubmission.activeTiles = 2;
    directSubmission.partitionedMode = true;
    directSubmission.partitionConfigSet = false;
    directSubmission.workPartitionAllocation = ultCsr->getWorkPartitionAllocation();

    bool ret = directSubmission.initialize(false, false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(directSubmission.partitionConfigSet);
    EXPECT_FALSE(directSubmission.ringStart);
    EXPECT_EQ(0x0u, directSubmission.ringCommandStream.getUsed());

    ret = directSubmission.startRingBuffer();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    EXPECT_TRUE(directSubmission.ringStart);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    uint32_t expectedOffset = NEO::ImplicitScalingDispatch<FamilyType>::getPostSyncOffset();

    ASSERT_NE(hwParse.lriList.end(), hwParse.lriList.begin());
    bool partitionRegisterFound = false;
    for (auto &it : hwParse.lriList) {
        auto loadRegisterImm = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(it);
        if (loadRegisterImm->getRegisterOffset() == 0x23B4u) {
            EXPECT_EQ(expectedOffset, loadRegisterImm->getDataDword());
            partitionRegisterFound = true;
        }
    }
    EXPECT_TRUE(partitionRegisterFound);

    auto loadRegisterMemItor = find<MI_LOAD_REGISTER_MEM *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), loadRegisterMemItor);
    auto loadRegisterMem = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(*loadRegisterMemItor);
    EXPECT_EQ(0x221Cu, loadRegisterMem->getRegisterAddress());
    uint64_t gpuAddress = ultCsr->getWorkPartitionAllocation()->getGpuAddress();
    EXPECT_EQ(gpuAddress, loadRegisterMem->getMemoryAddress());
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenRenderDirectSubmissionUsingNotifyEnabledWhenDispatchWorkloadCalledWithMonitorFenceThenExpectPostSyncOperationWithNotifyFlag) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using Dispatcher = RenderDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = false;

    bool ret = directSubmission.initialize(true, true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.useNotifyForPostSync);

    size_t sizeUsedBefore = directSubmission.ringCommandStream.getUsed();
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, sizeUsedBefore);
    hwParse.findHardwareCommands<FamilyType>();

    bool foundFenceUpdate = false;
    for (auto it = hwParse.pipeControlList.begin(); it != hwParse.pipeControlList.end(); it++) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            foundFenceUpdate = true;
            EXPECT_TRUE(pipeControl->getNotifyEnable());
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenBlitterDirectSubmissionUsingNotifyEnabledWhenDispatchWorkloadCalledWithMonitorFenceThenExpectPostSyncOperationWithNotifyFlag) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using Dispatcher = BlitterDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = false;

    bool ret = directSubmission.initialize(true, true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.useNotifyForPostSync);

    size_t sizeUsedBefore = directSubmission.ringCommandStream.getUsed();
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, sizeUsedBefore);
    hwParse.findHardwareCommands<FamilyType>();
    auto miFlushList = hwParse.getCommandsList<MI_FLUSH_DW>();

    bool foundFenceUpdate = false;
    for (auto it = miFlushList.begin(); it != miFlushList.end(); it++) {
        auto miFlush = genCmdCast<MI_FLUSH_DW *>(*it);
        if (miFlush->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) {
            foundFenceUpdate = true;
            EXPECT_TRUE(miFlush->getNotifyEnable());
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenReadBackCommandBufferDebugFlagEnabledWhenDispatchWorkloadCalledThenFirstDwordOfCommandBufferIsRead) {

    DebugManagerStateRestore restorer{};

    DebugManager.flags.DirectSubmissionReadBackCommandBuffer.set(1);
    using Dispatcher = BlitterDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    bool ret = directSubmission.initialize(true, true);
    EXPECT_TRUE(ret);

    uint32_t expectedValue = 0xDEADBEEF;
    directSubmission.reserved = 0u;
    reinterpret_cast<uint32_t *>(commandBuffer->getUnderlyingBuffer())[0] = expectedValue;

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);

    EXPECT_EQ(expectedValue, directSubmission.reserved);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenReadBackRingBufferDebugFlagEnabledWhenDispatchWorkloadCalledThenFirstDwordOfRingBufferIsRead) {

    DebugManagerStateRestore restorer{};

    DebugManager.flags.DirectSubmissionReadBackRingBuffer.set(1);
    using Dispatcher = BlitterDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    bool ret = directSubmission.initialize(true, true);
    EXPECT_TRUE(ret);

    directSubmission.reserved = 0u;

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    auto expectedValue = reinterpret_cast<uint32_t *>(directSubmission.ringCommandStream.getSpace(0))[0];
    EXPECT_EQ(expectedValue, directSubmission.reserved);
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenRingBufferRestartRequestWhenDispatchCommandBuffer) {
    FlushStampTracker flushStamp(true);
    MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    bool ret = directSubmission.initialize(true, true);
    EXPECT_TRUE(ret);
    EXPECT_EQ(directSubmission.submitCount, 1u);

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_EQ(directSubmission.submitCount, 1u);

    batchBuffer.ringBufferRestartRequest = true;
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_EQ(directSubmission.submitCount, 2u);
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDebugFlagSetWhenDispatchingWorkloadThenProgramSfenceInstruction) {
    DebugManagerStateRestore restorer{};

    using Dispatcher = BlitterDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    for (int32_t debugFlag : {-1, 0, 1, 2}) {
        DebugManager.flags.DirectSubmissionInsertSfenceInstructionPriorToSubmission.set(debugFlag);

        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        EXPECT_TRUE(directSubmission.initialize(true, true));

        auto initialCounterValue = CpuIntrinsicsTests::sfenceCounter.load();

        EXPECT_TRUE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

        uint32_t expectedCount = (debugFlag == -1) ? 2 : static_cast<uint32_t>(debugFlag);

        EXPECT_EQ(initialCounterValue + expectedCount, CpuIntrinsicsTests::sfenceCounter);
    }
}
