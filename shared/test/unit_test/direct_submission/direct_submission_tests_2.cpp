/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/utilities/cpuintrinsics.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"
#include "shared/test/unit_test/mocks/mock_direct_submission_diagnostic_collector.h"

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> sfenceCounter;
} // namespace CpuIntrinsicsTests

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

using DirectSubmissionDispatchBufferTest = Test<DirectSubmissionDispatchBufferFixture>;

struct DirectSubmissionDispatchMiMemFenceTest : public DirectSubmissionDispatchBufferTest {
    void SetUp() override {
        DirectSubmissionDispatchBufferTest::SetUp();

        auto &productHelper = pDevice->getProductHelper();
        miMemFenceSupported = productHelper.isGlobalFenceInDirectSubmissionRequired(pDevice->getHardwareInfo());
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
                        directSubmission.getSizeSemaphoreSection(false) +
                        sizeof(MI_LOAD_REGISTER_IMM) +
                        sizeof(MI_LOAD_REGISTER_MEM);
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        submitSize += RelaxedOrderingHelper::getSizeReturnPtrRegs<FamilyType>();
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

    EXPECT_EQ(directSubmission.getSizeDispatch(false, false), directSubmission.ringCommandStream.getUsed());
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

HWTEST_F(DirectSubmissionDispatchBufferTest, givenCopyCommandBufferIntoRingWhenDispatchCommandBufferThenCopyTaskStream) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    FlushStampTracker flushStamp(true);
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.copyCommandBufferIntoRing(batchBuffer));

    bool ret = directSubmission.initialize(true, false);
    EXPECT_TRUE(ret);

    size_t sizeUsed = directSubmission.ringCommandStream.getUsed();
    batchBuffer.endCmdPtr = batchBuffer.stream->getCpuBase();
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, sizeUsed);
    auto semaphoreIt = find<MI_SEMAPHORE_WAIT *>(hwParse.cmdList.begin(), hwParse.cmdList.end());

    MI_BATCH_BUFFER_START *bbStart = hwParse.getCommand<MI_BATCH_BUFFER_START>(hwParse.cmdList.begin(), semaphoreIt);
    EXPECT_EQ(nullptr, bbStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithoutRelaxingDependenciesThenCopyIntoRing) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithoutCommandBufferRelaxingDependenciesThenNotCopyIntoRing) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    batchBuffer.commandBufferAllocation = nullptr;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDisabledDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionFlatRingBuffer.set(0);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    batchBuffer.hasRelaxedOrderingDependencies = true;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    batchBuffer.chainedBatchBuffer = reinterpret_cast<GraphicsAllocation *>(0x1234);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitLocalMemNotChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    static_cast<MemoryAllocation *>(batchBuffer.commandBufferAllocation)->overrideMemoryPool(MemoryPool::LocalMemory);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndMultiTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, 0b11)));
    pDevice->getDefaultEngine().commandStreamReceiver->setupContext(*osContext.get());

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionDisableMonitorFenceWhenDispatchWorkloadCalledThenExpectStartWithoutMonitorFence) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionDisableCacheFlush.set(0);

    MockDirectSubmissionHw<FamilyType, Dispatcher> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch(false, false);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = true;

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t tagUpdateSize = Dispatcher::getSizeMonitorFence(directSubmission.rootDeviceEnvironment);

    size_t disabledSizeDispatch = directSubmission.getSizeDispatch(false, false);
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
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch(false, false);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.disableCacheFlush = true;
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t flushSize = Dispatcher::getSizeCacheFlush(*directSubmission.hwInfo);

    size_t disabledSizeDispatch = directSubmission.getSizeDispatch(false, false);
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
    RenderDispatcher<FamilyType>::dispatchCacheFlush(parseDispatch, pDevice->getRootDeviceEnvironment(), 0ull);
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

    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch(false, false);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.workloadMode = 1;
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t startSize = directSubmission.getSizeStartSection();
    size_t storeDataSize = Dispatcher::getSizeStoreDwordCommand();

    size_t debugSizeDispatch = directSubmission.getSizeDispatch(false, false);
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
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch(false, false);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.workloadMode = 2;
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t startSize = directSubmission.getSizeStartSection();

    size_t debugSizeDispatch = directSubmission.getSizeDispatch(false, false);
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
                        directSubmission.getSizeSemaphoreSection(false);
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        submitSize += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
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

    EXPECT_EQ(sizeUsed + directSubmission.getSizeDispatch(false, false), directSubmission.ringCommandStream.getUsed());
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
    size_t submitSize = directSubmission.getSizeSemaphoreSection(false);
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        submitSize += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
    }
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(oldRingAllocation->getGpuAddress(), directSubmission.submitGpuAddress);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    size_t dispatchSize = submitSize + directSubmission.getSizeDispatch(false, false);

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
                        directSubmission.getSizeSemaphoreSection(false);
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        submitSize += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
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

    EXPECT_EQ(directSubmission.getSizeDispatch(false, false), directSubmission.ringCommandStream.getUsed());
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
    size_t submitSize = directSubmission.getSizeSemaphoreSection(false);
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        submitSize += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
    }
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    size_t dispatchSize = submitSize + directSubmission.getSizeDispatch(false, false);

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
                        directSubmission.getSizeSemaphoreSection(false) +
                        sizeof(MI_LOAD_REGISTER_IMM) +
                        sizeof(MI_LOAD_REGISTER_MEM);
    if (directSubmission.miMemFenceRequired) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }

    size_t expectedAllocationsCount = 4;

    if (directSubmission.isRelaxedOrderingEnabled()) {
        expectedAllocationsCount += 2;

        submitSize += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
    }
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);
    EXPECT_EQ(expectedAllocationsCount, directSubmission.makeResourcesResidentVectorSize);

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

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDebugFlagSetWhenStoppingRingbufferThenProgramSfenceInstruction) {
    DebugManagerStateRestore restorer{};

    using Dispatcher = BlitterDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    for (int32_t debugFlag : {-1, 0, 1, 2}) {
        DebugManager.flags.DirectSubmissionInsertSfenceInstructionPriorToSubmission.set(debugFlag);

        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        EXPECT_TRUE(directSubmission.initialize(true, true));

        auto initialCounterValue = CpuIntrinsicsTests::sfenceCounter.load();

        EXPECT_TRUE(directSubmission.stopRingBuffer());

        uint32_t expectedCount = (debugFlag == -1) ? 2 : static_cast<uint32_t>(debugFlag);

        EXPECT_EQ(initialCounterValue + expectedCount, CpuIntrinsicsTests::sfenceCounter);
    }
}

struct DirectSubmissionRelaxedOrderingTests : public DirectSubmissionDispatchBufferTest {
    void SetUp() override {
        DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        DirectSubmissionDispatchBufferTest::SetUp();
    }

    template <typename FamilyType>
    bool verifyDynamicSchedulerProgramming(LinearStream &cs, uint64_t schedulerAllocationGpuVa, uint64_t semaphoreGpuVa, uint32_t semaphoreValue, size_t offset, size_t &endOffset);

    template <typename FamilyType>
    bool verifyStaticSchedulerProgramming(GraphicsAllocation &schedulerAllocation, uint64_t deferredTaskListVa, uint32_t expectedQueueSizeLimit);

    template <typename FamilyType>
    bool verifyMiPredicate(void *miPredicateCmd, MiPredicateType predicateType);

    template <typename FamilyType>
    bool verifyAlu(typename FamilyType::MI_MATH_ALU_INST_INLINE *miAluCmd, AluRegisters opcode, AluRegisters operand1, AluRegisters operand2);

    template <typename FamilyType>
    bool verifyLri(typename FamilyType::MI_LOAD_REGISTER_IMM *lriCmd, uint32_t registerOffset, uint32_t data);

    template <typename FamilyType>
    bool verifyLrr(typename FamilyType::MI_LOAD_REGISTER_REG *lrrCmd, uint32_t dstOffset, uint32_t srcOffset);

    template <typename FamilyType>
    bool verifyIncrementOrDecrement(void *cmds, AluRegisters aluRegister, bool increment);

    template <typename FamilyType>
    bool verifyConditionalDataRegBbStart(void *cmd, uint64_t startAddress, uint32_t compareReg, uint32_t compareData, CompareOperation compareOperation, bool indirect);

    template <typename FamilyType>
    bool verifyConditionalDataMemBbStart(void *cmd, uint64_t startAddress, uint64_t compareAddress, uint32_t compareData, CompareOperation compareOperation, bool indirect);

    template <typename FamilyType>
    bool verifyConditionalRegRegBbStart(void *cmd, uint64_t startAddress, AluRegisters compareReg0, AluRegisters compareReg1, CompareOperation compareOperation, bool indirect);

    template <typename FamilyType>
    bool verifyBaseConditionalBbStart(void *cmd, CompareOperation compareOperation, uint64_t startAddress, bool indirect, AluRegisters regA, AluRegisters regB);

    template <typename FamilyType>
    bool verifyBbStart(typename FamilyType::MI_BATCH_BUFFER_START *cmd, uint64_t startAddress, bool indirect, bool predicate);

    DebugManagerStateRestore restore;
    FlushStampTracker flushStamp{true};
};

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyMiPredicate(void *miPredicateCmd, MiPredicateType predicateType) {
    if constexpr (FamilyType::isUsingMiSetPredicate) {
        using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
        using PREDICATE_ENABLE = typename MI_SET_PREDICATE::PREDICATE_ENABLE;

        auto miSetPredicate = reinterpret_cast<MI_SET_PREDICATE *>(miPredicateCmd);
        if (static_cast<PREDICATE_ENABLE>(predicateType) != miSetPredicate->getPredicateEnable()) {
            return false;
        }

        return true;
    }
    return false;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyLri(typename FamilyType::MI_LOAD_REGISTER_IMM *lriCmd, uint32_t registerOffset, uint32_t data) {
    if ((lriCmd->getRegisterOffset() != registerOffset) || (lriCmd->getDataDword() != data)) {
        return false;
    }

    return true;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyLrr(typename FamilyType::MI_LOAD_REGISTER_REG *lrrCmd, uint32_t dstOffset, uint32_t srcOffset) {
    if ((dstOffset != lrrCmd->getDestinationRegisterAddress()) || (srcOffset != lrrCmd->getSourceRegisterAddress())) {
        return false;
    }
    return true;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyIncrementOrDecrement(void *cmds, AluRegisters aluRegister, bool increment) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename FamilyType::MI_MATH;

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(cmds);
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R7, 1)) {
        return false;
    }

    lriCmd++;
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R7 + 4, 0)) {
        return false;
    }

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    if (miMathCmd->DW0.BitField.DwordLength != 3) {
        return false;
    }

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, aluRegister)) {
        return false;
    }

    miAluCmd++;
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_7)) {
        return false;
    }

    miAluCmd++;

    if (increment && !verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_ADD, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!increment && !verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_SUB, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    miAluCmd++;
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_STORE, aluRegister, AluRegisters::R_ACCU)) {
        return false;
    }

    return true;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyAlu(typename FamilyType::MI_MATH_ALU_INST_INLINE *miAluCmd, AluRegisters opcode, AluRegisters operand1, AluRegisters operand2) {
    if ((static_cast<uint32_t>(opcode) != miAluCmd->DW0.BitField.ALUOpcode) ||
        (static_cast<uint32_t>(operand1) != miAluCmd->DW0.BitField.Operand1) ||
        (static_cast<uint32_t>(operand2) != miAluCmd->DW0.BitField.Operand2)) {
        return false;
    }

    return true;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyBbStart(typename FamilyType::MI_BATCH_BUFFER_START *bbStartCmd, uint64_t startAddress, bool indirect, bool predicate) {
    if constexpr (FamilyType::isUsingMiSetPredicate) {
        if ((predicate != !!bbStartCmd->getPredicationEnable()) ||
            (indirect != !!bbStartCmd->getIndirectAddressEnable())) {
            return false;
        }
    }

    if (!indirect && startAddress != bbStartCmd->getBatchBufferStartAddress()) {
        return false;
    }

    return true;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyBaseConditionalBbStart(void *cmd, CompareOperation compareOperation, uint64_t startAddress, bool indirect, AluRegisters regA, AluRegisters regB) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename FamilyType::MI_MATH;

    auto miMathCmd = reinterpret_cast<MI_MATH *>(cmd);
    if (miMathCmd->DW0.BitField.DwordLength != 3) {
        return false;
    }

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, regA)) {
        return false;
    }

    miAluCmd++;
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, regB)) {
        return false;
    }

    miAluCmd++;
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_SUB, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    miAluCmd++;

    if (compareOperation == CompareOperation::Equal || compareOperation == CompareOperation::NotEqual) {
        if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_STORE, AluRegisters::R_7, AluRegisters::R_ZF)) {
            return false;
        }
    } else {
        if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_STORE, AluRegisters::R_7, AluRegisters::R_CF)) {
            return false;
        }
    }

    auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(++miAluCmd);
    if (!verifyLrr<FamilyType>(lrrCmd, CS_PREDICATE_RESULT_2, CS_GPR_R7)) {
        return false;
    }

    auto predicateCmd = reinterpret_cast<MI_SET_PREDICATE *>(++lrrCmd);
    if (compareOperation == CompareOperation::Equal) {
        if (!verifyMiPredicate<FamilyType>(predicateCmd, MiPredicateType::NoopOnResult2Clear)) {
            return false;
        }
    } else {
        if (!verifyMiPredicate<FamilyType>(predicateCmd, MiPredicateType::NoopOnResult2Set)) {
            return false;
        }
    }

    auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(++predicateCmd);
    if (!verifyBbStart<FamilyType>(bbStartCmd, startAddress, indirect, true)) {
        return false;
    }

    predicateCmd = reinterpret_cast<MI_SET_PREDICATE *>(++bbStartCmd);
    if (!verifyMiPredicate<FamilyType>(predicateCmd, MiPredicateType::Disable)) {
        return false;
    }

    return true;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyConditionalRegRegBbStart(void *cmd, uint64_t startAddress, AluRegisters compareReg0, AluRegisters compareReg1, CompareOperation compareOperation, bool indirect) {
    return verifyBaseConditionalBbStart<FamilyType>(cmd, compareOperation, startAddress, indirect, compareReg0, compareReg1);
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyConditionalDataMemBbStart(void *cmd, uint64_t startAddress, uint64_t compareAddress, uint32_t compareData, CompareOperation compareOperation, bool indirect) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto lrmCmd = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(cmd);
    if ((lrmCmd->getRegisterAddress() != CS_GPR_R7) || (lrmCmd->getMemoryAddress() != compareAddress)) {
        return false;
    }

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++lrmCmd);
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R7 + 4, 0)) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R8, compareData)) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R8 + 4, 0)) {
        return false;
    }

    return verifyBaseConditionalBbStart<FamilyType>(++lriCmd, compareOperation, startAddress, indirect, AluRegisters::R_7, AluRegisters::R_8);
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyConditionalDataRegBbStart(void *cmds, uint64_t startAddress, uint32_t compareReg, uint32_t compareData,
                                                                           CompareOperation compareOperation, bool indirect) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(cmds);
    if (!verifyLrr<FamilyType>(lrrCmd, CS_GPR_R7, compareReg)) {
        return false;
    }

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++lrrCmd);
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R7 + 4, 0)) {
        return false;
    }

    lriCmd++;
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R8, compareData)) {
        return false;
    }

    lriCmd++;
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R8 + 4, 0)) {
        return false;
    }

    return verifyBaseConditionalBbStart<FamilyType>(++lriCmd, compareOperation, startAddress, indirect, AluRegisters::R_7, AluRegisters::R_8);
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyStaticSchedulerProgramming(GraphicsAllocation &schedulerAllocation, uint64_t deferredTaskListVa, uint32_t expectedQueueSizeLimit) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_MATH = typename FamilyType::MI_MATH;

    uint64_t schedulerStartGpuAddress = schedulerAllocation.getGpuAddress();
    void *schedulerCmds = schedulerAllocation.getUnderlyingBuffer();

    // 1. Init section
    auto miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(schedulerCmds);

    if (!verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::Disable)) {
        return false;
    }

    miPredicate++;
    if (!verifyConditionalDataRegBbStart<FamilyType>(miPredicate, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::semaphoreSectionJumpStart,
                                                     CS_GPR_R1, 0, CompareOperation::Equal, false)) {
        return false;
    }

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(miPredicate, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart()));
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R2, 0)) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R2 + 4, 0)) {
        return false;
    }

    uint64_t removeTaskVa = schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::removeTaskSectionStart;

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R3, static_cast<uint32_t>(removeTaskVa & 0xFFFF'FFFFULL))) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R3 + 4, static_cast<uint32_t>(removeTaskVa >> 32))) {
        return false;
    }

    uint64_t walkersLoopConditionCheckVa = schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::tasksListLoopCheckSectionStart;

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R4, static_cast<uint32_t>(walkersLoopConditionCheckVa & 0xFFFF'FFFFULL))) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R4 + 4, static_cast<uint32_t>(walkersLoopConditionCheckVa >> 32))) {
        return false;
    }

    // 2. Dispatch task section (loop start)
    miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++lriCmd);

    if (!verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::Disable)) {
        return false;
    }

    lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++miPredicate);
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R6, 8)) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R6 + 4, 0)) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R8, static_cast<uint32_t>(deferredTaskListVa & 0xFFFF'FFFFULL))) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R8 + 4, static_cast<uint32_t>(deferredTaskListVa >> 32))) {
        return false;
    }

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    if (miMathCmd->DW0.BitField.DwordLength != 9) {
        return false;
    }

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_2)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_6)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_SHL, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_STORE, AluRegisters::R_7, AluRegisters::R_ACCU)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_7)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_8)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_ADD, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_STORE, AluRegisters::R_6, AluRegisters::R_ACCU)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOADIND, AluRegisters::R_0, AluRegisters::R_ACCU)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_FENCE_RD, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(++miAluCmd);
    if (!verifyBbStart<FamilyType>(bbStart, 0, true, false)) {
        return false;
    }

    // 3. Remove task section
    miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++bbStart);
    if (!verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::Disable)) {
        return false;
    }

    miPredicate++;
    if (!verifyIncrementOrDecrement<FamilyType>(miPredicate, AluRegisters::R_1, false)) {
        return false;
    }

    auto cmds = ptrOffset(miPredicate, EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement());

    if (!verifyIncrementOrDecrement<FamilyType>(cmds, AluRegisters::R_2, false)) {
        return false;
    }

    cmds = ptrOffset(cmds, EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement());

    if (!verifyConditionalDataRegBbStart<FamilyType>(cmds, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::semaphoreSectionJumpStart,
                                                     CS_GPR_R1, 0, CompareOperation::Equal, false)) {
        return false;
    }

    lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(cmds, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart()));
    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R7, 8)) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R7 + 4, 0)) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R8, static_cast<uint32_t>(deferredTaskListVa & 0xFFFF'FFFFULL))) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R8 + 4, static_cast<uint32_t>(deferredTaskListVa >> 32))) {
        return false;
    }

    miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    if (miMathCmd->DW0.BitField.DwordLength != 13) {
        return false;
    }

    miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_1)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_7)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_SHL, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_STORE, AluRegisters::R_7, AluRegisters::R_ACCU)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_7)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_8)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_ADD, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOADIND, AluRegisters::R_7, AluRegisters::R_ACCU)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_FENCE_RD, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_6)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD0, AluRegisters::R_SRCB, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_ADD, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_STOREIND, AluRegisters::R_ACCU, AluRegisters::R_7)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_FENCE_WR, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    // 4. List loop check section

    miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++miAluCmd);
    if (!verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::Disable)) {
        return false;
    }

    miPredicate++;
    if (!verifyIncrementOrDecrement<FamilyType>(miPredicate, AluRegisters::R_2, true)) {
        return false;
    }

    cmds = ptrOffset(miPredicate, EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement());

    if (!verifyConditionalRegRegBbStart<FamilyType>(cmds, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::loopStartSectionStart,
                                                    AluRegisters::R_1, AluRegisters::R_2, CompareOperation::NotEqual, false)) {
        return false;
    }

    lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(cmds, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalRegRegBatchBufferStart()));

    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R2, 0)) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R2 + 4, 0)) {
        return false;
    }

    // 5. Drain request section
    auto arbCheck = reinterpret_cast<MI_ARB_CHECK *>(++lriCmd);
    if (memcmp(arbCheck, &FamilyType::cmdInitArbCheck, sizeof(MI_ARB_CHECK)) != 0) {
        return false;
    }

    if (!verifyConditionalDataRegBbStart<FamilyType>(++arbCheck, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::loopStartSectionStart,
                                                     CS_GPR_R1, expectedQueueSizeLimit, CompareOperation::GreaterOrEqual, false)) {
        return false;
    }

    auto conditionalBbStartcmds = ptrOffset(arbCheck, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart());

    if (!verifyConditionalDataRegBbStart<FamilyType>(conditionalBbStartcmds, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::loopStartSectionStart,
                                                     CS_GPR_R5, 1, CompareOperation::Equal, false)) {
        return false;
    }

    // 6. Jump to scheduler loop check section (dynamic scheduler)
    auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(ptrOffset(conditionalBbStartcmds, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart()));

    if (!verifyLrr<FamilyType>(lrrCmd, CS_GPR_R0, CS_GPR_R9)) {
        return false;
    }

    if (!verifyLrr<FamilyType>(++lrrCmd, CS_GPR_R0 + 4, CS_GPR_R9 + 4)) {
        return false;
    }

    bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(++lrrCmd);
    if (!verifyBbStart<FamilyType>(bbStart, 0, true, false)) {
        return false;
    }

    // 7. Jump to Semaphore section (dynamic scheduler)
    miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++bbStart);

    if (!verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::Disable)) {
        return false;
    }

    lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++miPredicate);

    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R10, static_cast<uint32_t>(RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<FamilyType>::schedulerLoopCheckSectionSize))) {
        return false;
    }

    if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R10 + 4, 0)) {
        return false;
    }

    miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    if (miMathCmd->DW0.BitField.DwordLength != 3) {
        return false;
    }

    miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    if (!verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_9)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_10)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_ADD, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE)) {
        return false;
    }

    if (!verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_STORE, AluRegisters::R_0, AluRegisters::R_ACCU)) {
        return false;
    }

    bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(++miAluCmd);
    if (!verifyBbStart<FamilyType>(bbStart, 0, true, false)) {
        return false;
    }

    return true;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyDynamicSchedulerProgramming(LinearStream &cs, uint64_t schedulerAllocationGpuVa, uint64_t semaphoreGpuVa, uint32_t semaphoreValue, size_t offset, size_t &endOffset) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cs, offset);
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;

    for (auto &it : hwParse.cmdList) {
        if (auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it)) {
            // 1. Init section

            uint64_t schedulerStartAddress = cs.getGraphicsAllocation()->getGpuAddress() + ptrDiff(lriCmd, cs.getCpuBase());

            uint64_t schedulerLoopCheckVa = schedulerStartAddress + RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<FamilyType>::schedulerLoopCheckSectionStart;

            if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R9, static_cast<uint32_t>(schedulerLoopCheckVa & 0xFFFF'FFFFULL))) {
                continue;
            }

            if (!verifyLri<FamilyType>(++lriCmd, CS_GPR_R9 + 4, static_cast<uint32_t>(schedulerLoopCheckVa >> 32))) {
                continue;
            }

            auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(++lriCmd);
            if (!verifyBbStart<FamilyType>(bbStart, schedulerAllocationGpuVa, false, false)) {
                continue;
            }

            // 2. Scheduler loop check section

            bbStart++;

            if (!verifyConditionalDataMemBbStart<FamilyType>(bbStart, schedulerStartAddress + RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<FamilyType>::endSectionStart,
                                                             semaphoreGpuVa, semaphoreValue, CompareOperation::GreaterOrEqual, false)) {
                continue;
            }

            bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(ptrOffset(bbStart, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart()));
            if (!verifyBbStart<FamilyType>(bbStart, schedulerAllocationGpuVa + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::loopStartSectionStart, false, false)) {
                continue;
            }

            // 3. Semaphore section
            auto miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++bbStart);
            if (!verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::Disable)) {
                continue;
            }

            auto semaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(++miPredicate);
            if ((semaphore->getSemaphoreGraphicsAddress() != semaphoreGpuVa) ||
                (semaphore->getSemaphoreDataDword() != semaphoreValue) ||
                (semaphore->getCompareOperation() != MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD)) {
                continue;
            }

            // 4. End section

            miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++semaphore);
            if (!verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::Disable)) {
                continue;
            }

            lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++miPredicate);
            if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R5, 0)) {
                continue;
            }
            lriCmd++;
            endOffset = ptrDiff(lriCmd, cs.getCpuBase());

            success = true;
            break;
        }
    }

    return success;
}

HWTEST_F(DirectSubmissionRelaxedOrderingTests, whenAllocatingResourcesThenCreateDeferredTasksAndSchedulerAllocation) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    auto mockMemoryOperations = new MockMemoryOperations();
    mockMemoryOperations->captureGfxAllocationsForMakeResident = true;

    pDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.reset(mockMemoryOperations);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.callBaseResident = true;

    directSubmission.initialize(false, false);

    auto allocsIter = mockMemoryOperations->gfxAllocationsForMakeResident.rbegin();

    EXPECT_EQ(AllocationType::COMMAND_BUFFER, directSubmission.relaxedOrderingSchedulerAllocation->getAllocationType());
    EXPECT_NE(nullptr, directSubmission.relaxedOrderingSchedulerAllocation);
    EXPECT_EQ(directSubmission.relaxedOrderingSchedulerAllocation, *allocsIter);

    allocsIter++;

    EXPECT_EQ(AllocationType::DEFERRED_TASKS_LIST, directSubmission.deferredTasksListAllocation->getAllocationType());
    EXPECT_NE(nullptr, directSubmission.deferredTasksListAllocation);
    EXPECT_EQ(directSubmission.deferredTasksListAllocation, *allocsIter);
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenDebugFlagSetWhenDispatchingStaticSchedulerThenOverrideQueueSizeLimit, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManager.flags.DirectSubmissionRelaxedOrderingQueueSizeLimit.set(123);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true, false);

    EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    EXPECT_TRUE(verifyStaticSchedulerProgramming<FamilyType>(*directSubmission.relaxedOrderingSchedulerAllocation,
                                                             directSubmission.deferredTasksListAllocation->getGpuAddress(), 123));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenNewNumberOfClientsWhenDispatchingWorkThenIncraseQueueSize, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true, false);

    EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    EXPECT_EQ(RelaxedOrderingHelper::queueSizeMultiplier, directSubmission.currentRelaxedOrderingQueueSize);
    EXPECT_TRUE(verifyStaticSchedulerProgramming<FamilyType>(*directSubmission.relaxedOrderingSchedulerAllocation,
                                                             directSubmission.deferredTasksListAllocation->getGpuAddress(), RelaxedOrderingHelper::queueSizeMultiplier));

    const uint64_t expectedQueueSizeValueVa = directSubmission.relaxedOrderingSchedulerAllocation->getGpuAddress() +
                                              RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::drainRequestSectionStart +
                                              sizeof(typename FamilyType::MI_ARB_CHECK) +
                                              RelaxedOrderingHelper::getQueueSizeLimitValueOffset<FamilyType>();

    auto findStaticSchedulerUpdate = [&](LinearStream &cs, size_t offset, uint32_t expectedQueueSize) {
        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(cs, offset);
        hwParse.findHardwareCommands<FamilyType>();

        bool success = false;

        for (auto &it : hwParse.cmdList) {
            if (auto sriCmd = genCmdCast<MI_STORE_DATA_IMM *>(it)) {
                if (expectedQueueSizeValueVa == sriCmd->getAddress() && expectedQueueSize == sriCmd->getDataDword0()) {
                    success = true;
                    break;
                }
            }
        }

        return success;
    };

    auto offset = directSubmission.ringCommandStream.getUsed();

    batchBuffer.hasRelaxedOrderingDependencies = false;
    batchBuffer.numCsrClients = 2;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    EXPECT_EQ(RelaxedOrderingHelper::queueSizeMultiplier, directSubmission.currentRelaxedOrderingQueueSize);
    EXPECT_FALSE(findStaticSchedulerUpdate(directSubmission.ringCommandStream, offset, RelaxedOrderingHelper::queueSizeMultiplier));

    offset = directSubmission.ringCommandStream.getUsed();
    batchBuffer.hasRelaxedOrderingDependencies = true;
    batchBuffer.numCsrClients = 2;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    EXPECT_EQ(RelaxedOrderingHelper::queueSizeMultiplier * batchBuffer.numCsrClients, directSubmission.currentRelaxedOrderingQueueSize);
    EXPECT_TRUE(findStaticSchedulerUpdate(directSubmission.ringCommandStream, offset, RelaxedOrderingHelper::queueSizeMultiplier * batchBuffer.numCsrClients));

    offset = directSubmission.ringCommandStream.getUsed();
    batchBuffer.numCsrClients = 4;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    EXPECT_EQ(RelaxedOrderingHelper::queueSizeMultiplier * batchBuffer.numCsrClients, directSubmission.currentRelaxedOrderingQueueSize);
    EXPECT_TRUE(findStaticSchedulerUpdate(directSubmission.ringCommandStream, offset, RelaxedOrderingHelper::queueSizeMultiplier * batchBuffer.numCsrClients));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenInitializingThenDispatchStaticScheduler, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(false, false);

        EXPECT_EQ(0u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
        EXPECT_TRUE(verifyStaticSchedulerProgramming<FamilyType>(*directSubmission.relaxedOrderingSchedulerAllocation,
                                                                 directSubmission.deferredTasksListAllocation->getGpuAddress(), RelaxedOrderingHelper::queueSizeMultiplier));
    }

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(false, false);
        EXPECT_EQ(0u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);

        directSubmission.startRingBuffer();

        EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);

        directSubmission.startRingBuffer();
        EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);

        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    }
}

HWTEST_F(DirectSubmissionRelaxedOrderingTests, whenInitializingThenPreinitializeTaskStoreSectionAndStaticSchedulerAndInitRegs) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto verifyInitRegisters = [&](LinearStream &cs, size_t offset) {
        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(cs, offset);
        hwParse.findHardwareCommands<FamilyType>();

        bool success = false;

        for (auto &it : hwParse.cmdList) {
            if (auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it)) {
                if (CS_GPR_R1 == lriCmd->getRegisterOffset()) {
                    EXPECT_EQ(0u, lriCmd->getDataDword());

                    lriCmd++;
                    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R1 + 4, 0)) {
                        continue;
                    }

                    lriCmd++;
                    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R5, 0)) {
                        continue;
                    }

                    lriCmd++;
                    if (!verifyLri<FamilyType>(lriCmd, CS_GPR_R5 + 4, 0)) {
                        continue;
                    }

                    success = true;
                    break;
                }
            }
        }

        return success;
    };

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(false, false);
        EXPECT_FALSE(verifyInitRegisters(directSubmission.ringCommandStream, 0));

        EXPECT_EQ(0u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
        EXPECT_FALSE(directSubmission.relaxedOrderingInitialized);
        EXPECT_EQ(nullptr, directSubmission.preinitializedTaskStoreSection.get());
        EXPECT_EQ(nullptr, directSubmission.preinitializedRelaxedOrderingScheduler.get());
    }

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);
        EXPECT_TRUE(verifyInitRegisters(directSubmission.ringCommandStream, 0));

        EXPECT_EQ(1u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
        EXPECT_TRUE(directSubmission.relaxedOrderingInitialized);
        EXPECT_NE(nullptr, directSubmission.preinitializedTaskStoreSection.get());
        EXPECT_NE(nullptr, directSubmission.preinitializedRelaxedOrderingScheduler.get());

        size_t offset = directSubmission.ringCommandStream.getUsed();

        directSubmission.startRingBuffer();
        EXPECT_FALSE(verifyInitRegisters(directSubmission.ringCommandStream, offset));

        EXPECT_EQ(1u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(false, false);
        EXPECT_EQ(0u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);

        directSubmission.startRingBuffer();

        EXPECT_EQ(1u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
        EXPECT_TRUE(directSubmission.relaxedOrderingInitialized);
        EXPECT_NE(nullptr, directSubmission.preinitializedTaskStoreSection.get());
        EXPECT_NE(nullptr, directSubmission.preinitializedRelaxedOrderingScheduler.get());

        size_t offset = directSubmission.ringCommandStream.getUsed();
        directSubmission.startRingBuffer();
        EXPECT_FALSE(verifyInitRegisters(directSubmission.ringCommandStream, offset));
        EXPECT_EQ(1u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
    }
}

HWTEST_F(DirectSubmissionRelaxedOrderingTests, whenDispatchingWorkThenDispatchTaskStoreSection) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename FamilyType::MI_MATH;
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true, false);
    auto offset = directSubmission.ringCommandStream.getUsed() + directSubmission.getSizeStartSection() + RelaxedOrderingHelper::getSizeReturnPtrRegs<FamilyType>();

    batchBuffer.hasRelaxedOrderingDependencies = true;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    auto taskStoreSection = ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset);

    if constexpr (FamilyType::isUsingMiSetPredicate) {
        EXPECT_TRUE(verifyMiPredicate<FamilyType>(taskStoreSection, MiPredicateType::Disable));

        taskStoreSection = ptrOffset(taskStoreSection, sizeof(typename FamilyType::MI_SET_PREDICATE));
    }

    uint64_t deferredTasksVa = directSubmission.deferredTasksListAllocation->getGpuAddress();

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(taskStoreSection);

    EXPECT_TRUE(verifyLri<FamilyType>(lriCmd, CS_GPR_R6, static_cast<uint32_t>(deferredTasksVa & 0xFFFF'FFFFULL)));

    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R6 + 4, static_cast<uint32_t>(deferredTasksVa >> 32)));

    EXPECT_NE(0u, batchBuffer.taskStartAddress);

    uint32_t taskStartAddressLow = static_cast<uint32_t>(batchBuffer.taskStartAddress & 0xFFFF'FFFFULL);
    EXPECT_NE(0u, taskStartAddressLow);
    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R7, taskStartAddressLow));

    uint32_t taskStartHigh = static_cast<uint32_t>(batchBuffer.taskStartAddress >> 32);
    EXPECT_NE(0u, taskStartHigh);
    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R7 + 4, taskStartHigh));

    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R8, 8));

    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R8 + 4, 0));

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    EXPECT_EQ(8u, miMathCmd->DW0.BitField.DwordLength);

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    EXPECT_TRUE(verifyAlu<FamilyType>(miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_1));

    EXPECT_TRUE(verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_8));

    EXPECT_TRUE(verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_SHL, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE));

    EXPECT_TRUE(verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_STORE, AluRegisters::R_8, AluRegisters::R_ACCU));

    EXPECT_TRUE(verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_8));

    EXPECT_TRUE(verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_6));

    EXPECT_TRUE(verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_ADD, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE));

    EXPECT_TRUE(verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_STOREIND, AluRegisters::R_ACCU, AluRegisters::R_7));

    EXPECT_TRUE(verifyAlu<FamilyType>(++miAluCmd, AluRegisters::OPCODE_FENCE_WR, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE));

    EXPECT_TRUE(verifyIncrementOrDecrement<FamilyType>(++miAluCmd, AluRegisters::R_1, true));
}

HWTEST_F(DirectSubmissionRelaxedOrderingTests, givenNotEnoughSpaceForTaskStoreSectionWhenDispatchingThenSwitchRingBuffers) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true, false);
    directSubmission.ringCommandStream.getUsed();

    auto sizeToConsume = directSubmission.ringCommandStream.getAvailableSpace() -
                         (directSubmission.getSizeDispatch(false, false) + directSubmission.getSizeEnd(false) + directSubmission.getSizeSwitchRingBufferSection());

    directSubmission.ringCommandStream.getSpace(sizeToConsume);

    auto oldAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_NE(oldAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenDispatchingWorkThenDispatchScheduler, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true, false);
    auto offset = directSubmission.ringCommandStream.getUsed();

    uint64_t staticSchedulerGpuAddress = directSubmission.relaxedOrderingSchedulerAllocation->getGpuAddress();
    uint64_t semaphoreGpuVa = directSubmission.semaphoreGpuVa;

    size_t endOffset = 0;

    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, 0, endOffset));

    batchBuffer.hasRelaxedOrderingDependencies = true;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_TRUE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, offset, endOffset));

    offset = directSubmission.ringCommandStream.getUsed();

    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_TRUE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, offset, endOffset));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithStallingCmdsWhenDispatchingThenProgramSchedulerWithR5, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true, false);
    size_t offset = directSubmission.ringCommandStream.getUsed();

    uint64_t staticSchedulerGpuAddress = directSubmission.relaxedOrderingSchedulerAllocation->getGpuAddress();
    uint64_t semaphoreGpuVa = directSubmission.semaphoreGpuVa;

    size_t endOffset = 0;

    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, 0, endOffset));

    batchBuffer.hasStallingCmds = false;
    batchBuffer.hasRelaxedOrderingDependencies = true;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_TRUE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, offset, endOffset));
    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, endOffset, endOffset));

    offset = directSubmission.ringCommandStream.getUsed();

    batchBuffer.hasStallingCmds = true;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    auto startAddress = ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset);
    auto jumpOffset = directSubmission.getSizeSemaphoreSection(true) + sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM) +
                      EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart();
    uint64_t expectedJumpAddress = directSubmission.ringCommandStream.getGpuBase() + offset + jumpOffset;

    EXPECT_TRUE(verifyConditionalDataRegBbStart<FamilyType>(startAddress, expectedJumpAddress, CS_GPR_R1, 0, CompareOperation::Equal, false));

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, offset + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart());
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;
    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;

    for (auto &it : hwParse.cmdList) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (lriCmd) {
            if (CS_GPR_R5 == lriCmd->getRegisterOffset() && lriCmd->getDataDword() == 1) {
                success = true;
                break;
            }
        }
    }

    ASSERT_TRUE(success);
    offset = ptrDiff(++lriCmd, directSubmission.ringCommandStream.getCpuBase());
    EXPECT_TRUE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount - 1, offset, endOffset));

    EXPECT_TRUE(endOffset > offset);

    EXPECT_TRUE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, endOffset, endOffset));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenFirstBbWithStallingCmdsWhenDispatchingThenDontProgramSchedulerWithR5, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true, false);
    size_t offset = directSubmission.ringCommandStream.getUsed();

    uint64_t staticSchedulerGpuAddress = directSubmission.relaxedOrderingSchedulerAllocation->getGpuAddress();
    uint64_t semaphoreGpuVa = directSubmission.semaphoreGpuVa;

    size_t endOffset = 0;

    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, 0, endOffset));

    batchBuffer.hasStallingCmds = true;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, offset);
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;
    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;

    for (auto &it : hwParse.cmdList) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (lriCmd) {
            if (CS_GPR_R5 == lriCmd->getRegisterOffset() && lriCmd->getDataDword() == 1) {
                success = true;
                break;
            }
        }
    }

    EXPECT_FALSE(success);
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenStoppingRingThenProgramSchedulerWithR5, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true, false);
    size_t offset = directSubmission.ringCommandStream.getUsed();

    uint64_t staticSchedulerGpuAddress = directSubmission.relaxedOrderingSchedulerAllocation->getGpuAddress();
    uint64_t semaphoreGpuVa = directSubmission.semaphoreGpuVa;

    size_t endOffset = 0;

    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, 0, endOffset));

    batchBuffer.hasStallingCmds = false;
    batchBuffer.hasRelaxedOrderingDependencies = true;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_TRUE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, offset, endOffset));
    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, endOffset, endOffset));

    offset = directSubmission.ringCommandStream.getUsed();

    directSubmission.stopRingBuffer();

    auto startAddress = ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset);
    auto jumpOffset = directSubmission.getSizeSemaphoreSection(true) + sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM) +
                      EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart();
    uint64_t expectedJumpAddress = directSubmission.ringCommandStream.getGpuBase() + offset + jumpOffset;

    EXPECT_TRUE(verifyConditionalDataRegBbStart<FamilyType>(startAddress, expectedJumpAddress, CS_GPR_R1, 0, CompareOperation::Equal, false));

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, offset + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart());
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;
    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;

    for (auto &it : hwParse.cmdList) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (lriCmd) {
            if (CS_GPR_R5 == lriCmd->getRegisterOffset() && lriCmd->getDataDword() == 1) {
                success = true;
                break;
            }
        }
    }

    ASSERT_TRUE(success);
    offset = ptrDiff(lriCmd, directSubmission.ringCommandStream.getCpuBase());
    EXPECT_TRUE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, offset, endOffset));

    EXPECT_TRUE(endOffset > offset);

    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, endOffset, endOffset));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, WhenStoppingRingWithoutSubmissionThenDontProgramSchedulerWithR5, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true, false);
    size_t offset = directSubmission.ringCommandStream.getUsed();

    uint64_t staticSchedulerGpuAddress = directSubmission.relaxedOrderingSchedulerAllocation->getGpuAddress();
    uint64_t semaphoreGpuVa = directSubmission.semaphoreGpuVa;

    size_t endOffset = 0;

    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, 0, endOffset));

    directSubmission.stopRingBuffer();

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, offset);
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;
    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;

    for (auto &it : hwParse.cmdList) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (lriCmd) {
            if (CS_GPR_R5 == lriCmd->getRegisterOffset() && lriCmd->getDataDword() == 1) {
                success = true;
                break;
            }
        }
    }

    EXPECT_FALSE(success);
}

HWTEST_F(DirectSubmissionRelaxedOrderingTests, givenDebugFlagSetWhenAskingForRelaxedOrderingSupportThenEnable) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    {
        DebugManager.flags.DirectSubmissionRelaxedOrdering.set(-1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);
        EXPECT_EQ(gfxCoreHelper.isRelaxedOrderingSupported(), directSubmission.isRelaxedOrderingEnabled());
    }

    {
        DebugManager.flags.DirectSubmissionRelaxedOrdering.set(0);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_FALSE(directSubmission.isRelaxedOrderingEnabled());
    }

    {
        DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_TRUE(directSubmission.isRelaxedOrderingEnabled());
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenDebugFlagSetWhenCreatingBcsDispatcherThenEnableRelaxedOrdering, IsAtLeastXeHpcCore) {
    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->setupContext(*osContext);

    {
        DebugManager.flags.DirectSubmissionRelaxedOrdering.set(0);
        DebugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_FALSE(directSubmission.isRelaxedOrderingEnabled());
    }

    {
        DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        DebugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(-1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_TRUE(directSubmission.isRelaxedOrderingEnabled());
    }

    {
        DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        DebugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(0);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_FALSE(directSubmission.isRelaxedOrderingEnabled());
    }

    {
        DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        DebugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_TRUE(directSubmission.isRelaxedOrderingEnabled());
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBcsRelaxedOrderingEnabledWhenProgrammingEndingCommandsThenSetReturnPtrs, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);

    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->setupContext(*osContext);
    ultCsr->blitterDirectSubmissionAvailable = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*ultCsr);
    ultCsr->blitterDirectSubmission.reset(directSubmission);

    auto &commandStream = ultCsr->getCS(0x100);

    auto endingPtr = commandStream.getSpace(0);

    ultCsr->programEndingCmd(commandStream, &endingPtr, true, true, false);

    auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(commandStream.getCpuBase());
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), CS_GPR_R3);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), CS_GPR_R0);

    lrrCmd++;
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), CS_GPR_R3 + 4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), CS_GPR_R0 + 4);

    auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(++lrrCmd);
    EXPECT_EQ(1u, bbStartCmd->getIndirectAddressEnable());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBcsRelaxedOrderingDisabledWhenProgrammingEndingCommandsThenDontSetReturnPtrs, IsAtLeastXeHpcCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(0);

    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->setupContext(*osContext);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*ultCsr);
    ultCsr->blitterDirectSubmission.reset(directSubmission);

    auto &commandStream = ultCsr->getCS(0x100);

    auto endingPtr = commandStream.getSpace(0);

    ultCsr->programEndingCmd(commandStream, &endingPtr, true, false, false);

    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(commandStream.getCpuBase());
    ASSERT_NE(nullptr, bbStartCmd);
    EXPECT_EQ(0u, bbStartCmd->getIndirectAddressEnable());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenProgrammingEndingCmdsThenSetReturnRegisters, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->directSubmissionAvailable = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    directSubmission->initialize(true, false);

    ultCsr->directSubmission.reset(directSubmission);

    auto &commandStream = ultCsr->getCS(0x100);

    auto endingPtr = commandStream.getSpace(0);

    ultCsr->programEndingCmd(commandStream, &endingPtr, true, true, false);

    auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(commandStream.getCpuBase());
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), CS_GPR_R3);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), CS_GPR_R0);

    lrrCmd++;
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), CS_GPR_R3 + 4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), CS_GPR_R0 + 4);

    auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(++lrrCmd);
    EXPECT_EQ(1u, bbStartCmd->getIndirectAddressEnable());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithoutRelaxedOrderingDependencieswhenProgrammingEndingCmdsThenDontSetReturnRegisters, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    directSubmission->initialize(true, false);

    ultCsr->directSubmission.reset(directSubmission);

    auto &commandStream = ultCsr->getCS(0x100);

    auto endingPtr = commandStream.getSpace(0);

    ultCsr->programEndingCmd(commandStream, &endingPtr, true, false, false);

    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(commandStream.getCpuBase());
    ASSERT_NE(nullptr, bbStartCmd);
    EXPECT_EQ(0u, bbStartCmd->getIndirectAddressEnable());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenDispatchingWorkloadSectionThenProgramReturnPtrs, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true, false);

    auto offset = directSubmission.ringCommandStream.getUsed();

    auto originalBbStart = *reinterpret_cast<MI_BATCH_BUFFER_START *>(batchBuffer.endCmdPtr);

    batchBuffer.hasRelaxedOrderingDependencies = true;
    directSubmission.dispatchWorkloadSection(batchBuffer);

    uint64_t returnPtr = directSubmission.ringCommandStream.getGpuBase() + offset + (4 * sizeof(MI_LOAD_REGISTER_IMM)) + directSubmission.getSizeStartSection();

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset));
    EXPECT_TRUE(verifyLri<FamilyType>(lriCmd, CS_GPR_R4, static_cast<uint32_t>(returnPtr & 0xFFFF'FFFFULL)));
    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R4 + 4, static_cast<uint32_t>(returnPtr >> 32)));

    uint64_t returnPtr2 = returnPtr + RelaxedOrderingHelper::getSizeTaskStoreSection<FamilyType>();
    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R3, static_cast<uint32_t>(returnPtr2 & 0xFFFF'FFFFULL)));
    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R3 + 4, static_cast<uint32_t>(returnPtr2 >> 32)));

    EXPECT_EQ(0, memcmp(&originalBbStart, batchBuffer.endCmdPtr, sizeof(MI_BATCH_BUFFER_START)));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithoutRelaxedOrderingDependencieswhenDispatchingWorkloadSectionThenDontProgramReturnPtrs, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true, false);

    auto offset = directSubmission.ringCommandStream.getUsed();

    batchBuffer.hasRelaxedOrderingDependencies = false;
    directSubmission.dispatchWorkloadSection(batchBuffer);

    auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset));
    EXPECT_EQ(nullptr, lriCmd);
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithStallingCmdsAndDependenciesWhenDispatchingNextCmdBufferThenProgramSchedulerIfNeeded, IsAtLeastXeHpcCore) {
    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(2u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(3u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(2u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        directSubmission.stopRingBuffer();
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithNonStallingCmdsAndDependenciesWhenDispatchingNextCmdBufferThenProgramSchedulerIfNeeded, IsAtLeastXeHpcCore) {
    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(3u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(2u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        directSubmission.stopRingBuffer();
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithStallingCmdsAndWithoutDependenciesWhenDispatchingNextCmdBufferThenProgramSchedulerIfNeeded, IsAtLeastXeHpcCore) {
    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        directSubmission.stopRingBuffer();
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithNonStallingCmdsAndWithoutDependenciesWhenDispatchingNextCmdBufferThenProgramSchedulerIfNeeded, IsAtLeastXeHpcCore) {
    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true, false);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        directSubmission.stopRingBuffer();
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenRelaxedOrderingSchedulerRequiredWhenAskingForCmdsSizeThenReturnCorrectValue, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t expectedBaseSemaphoreSectionSize = directSubmission.getSizePrefetchMitigation();
    if (directSubmission.isDisablePrefetcherRequired) {
        expectedBaseSemaphoreSectionSize += 2 * directSubmission.getSizeDisablePrefetcher();
    }

    if (directSubmission.miMemFenceRequired) {
        expectedBaseSemaphoreSectionSize += MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(pDevice->getRootDeviceEnvironment());
    }

    EXPECT_EQ(expectedBaseSemaphoreSectionSize + RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<FamilyType>::totalSize, directSubmission.getSizeSemaphoreSection(true));
    EXPECT_EQ(expectedBaseSemaphoreSectionSize + EncodeSempahore<FamilyType>::getSizeMiSemaphoreWait(), directSubmission.getSizeSemaphoreSection(false));

    size_t expectedBaseEndSize = Dispatcher::getSizeStopCommandBuffer() +
                                 Dispatcher::getSizeCacheFlush(pDevice->getHardwareInfo()) +
                                 (Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer()) +
                                 MemoryConstants::cacheLineSize;
    if (directSubmission.disableMonitorFence) {
        expectedBaseEndSize += Dispatcher::getSizeMonitorFence(pDevice->getRootDeviceEnvironment());
    }

    EXPECT_EQ(expectedBaseEndSize + directSubmission.getSizeDispatchRelaxedOrderingQueueStall(), directSubmission.getSizeEnd(true));
    EXPECT_EQ(expectedBaseEndSize, directSubmission.getSizeEnd(false));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenSchedulerRequiredWhenDispatchingReturnPtrsThenAddOffset, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true, false);

    uint64_t returnPtr = 0x800100123000;
    uint64_t returnPtr2 = returnPtr + RelaxedOrderingHelper::getSizeTaskStoreSection<FamilyType>();

    size_t offset = directSubmission.ringCommandStream.getUsed();

    directSubmission.dispatchRelaxedOrderingReturnPtrRegs(directSubmission.ringCommandStream, returnPtr);

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset));
    EXPECT_TRUE(verifyLri<FamilyType>(lriCmd, CS_GPR_R4, static_cast<uint32_t>(returnPtr & 0xFFFF'FFFFULL)));
    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R4 + 4, static_cast<uint32_t>(returnPtr >> 32)));

    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R3, static_cast<uint32_t>(returnPtr2 & 0xFFFF'FFFFULL)));
    EXPECT_TRUE(verifyLri<FamilyType>(++lriCmd, CS_GPR_R3 + 4, static_cast<uint32_t>(returnPtr2 >> 32)));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenReturnPtrsRequiredWhenAskingForDispatchSizeTheAddMmioSizes, IsAtLeastXeHpcCore) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t baseSize = directSubmission.getSizeDispatch(true, false);
    size_t sizeWitfRetPtr = directSubmission.getSizeDispatch(true, true);

    EXPECT_EQ(baseSize + RelaxedOrderingHelper::getSizeReturnPtrRegs<FamilyType>(), sizeWitfRetPtr);
}