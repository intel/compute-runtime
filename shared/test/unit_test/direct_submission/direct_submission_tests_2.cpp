/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/utilities/cpuintrinsics.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> sfenceCounter;
extern std::atomic<uint32_t> mfenceCounter;
} // namespace CpuIntrinsicsTests

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

using DirectSubmissionDispatchBufferTest = Test<DirectSubmissionDispatchBufferFixture>;

struct DirectSubmissionDispatchMiMemFenceTest : public DirectSubmissionDispatchBufferTest {
    void SetUp() override {
        DirectSubmissionDispatchBufferTest::SetUp();

        auto &productHelper = pDevice->getProductHelper();
        miMemFenceSupported = pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice ? false : productHelper.isAcquireGlobalFenceInDirectSubmissionRequired(pDevice->getHardwareInfo());

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo));
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
                    if (miMemFence->getFenceType() == MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_ACQUIRE_FENCE) {
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
    bool heaplessStateInit = false;
};

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenMiMemFenceSupportedWhenInitializingDirectSubmissionThenEnableMiMemFenceProgramming) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    uint32_t expectedSysMemFenceAddress = heaplessStateInit ? 0 : 1;

    EXPECT_EQ(miMemFenceSupported, directSubmission.miMemFenceRequired);
    EXPECT_EQ(heaplessStateInit && miMemFenceSupported, directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.initialize(true));

    EXPECT_EQ(directSubmission.globalFenceAllocation != nullptr, directSubmission.systemMemoryFenceAddressSet);

    validateFenceProgramming<FamilyType>(directSubmission, 1, expectedSysMemFenceAddress);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenMiMemFenceSupportedWhenDispatchingWithoutInitThenEnableMiMemFenceProgramming) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    FlushStampTracker flushStamp(true);

    uint32_t expectedSysMemFenceAddress = heaplessStateInit ? 0 : 1;

    EXPECT_EQ(miMemFenceSupported, directSubmission.miMemFenceRequired);
    EXPECT_EQ(heaplessStateInit && miMemFenceSupported, directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.initialize(false));

    EXPECT_EQ(heaplessStateInit && miMemFenceSupported, directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    validateFenceProgramming<FamilyType>(directSubmission, 1, expectedSysMemFenceAddress);

    EXPECT_EQ(directSubmission.globalFenceAllocation != nullptr, directSubmission.systemMemoryFenceAddressSet);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenMiMemFenceSupportedWhenSysMemFenceIsAlreadySentThenDontReprogram) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    FlushStampTracker flushStamp(true);

    EXPECT_EQ(miMemFenceSupported, directSubmission.miMemFenceRequired);
    directSubmission.systemMemoryFenceAddressSet = true;

    EXPECT_TRUE(directSubmission.initialize(false));

    EXPECT_TRUE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    validateFenceProgramming<FamilyType>(directSubmission, 1, 0);

    EXPECT_TRUE(directSubmission.systemMemoryFenceAddressSet);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenPciBarrierPtrSetWhenUnblockGpuThenWriteZero) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    uint32_t pciBarrierMock = 1;
    directSubmission.pciBarrierPtr = &pciBarrierMock;
    EXPECT_TRUE(directSubmission.initialize(false));

    directSubmission.unblockGpu();

    EXPECT_EQ(*directSubmission.pciBarrierPtr, 0u);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenDebugFlagSetToFalseWhenCreatingDirectSubmissionThenDontEnableMiMemFenceProgramming) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_FALSE(directSubmission.miMemFenceRequired);
    EXPECT_FALSE(directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.initialize(true));

    EXPECT_FALSE(directSubmission.miMemFenceRequired);
    EXPECT_EQ(directSubmission.systemMemoryFenceAddressSet, directSubmission.globalFenceAllocation != nullptr);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenNoGlobalFenceAllocationWhenInitializeThenDoNotProgramGlobalFence) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.globalFenceAllocation = nullptr;
    directSubmission.systemMemoryFenceAddressSet = false;

    EXPECT_TRUE(directSubmission.initialize(true));

    EXPECT_FALSE(directSubmission.systemMemoryFenceAddressSet);
}

HWTEST_F(DirectSubmissionDispatchMiMemFenceTest, givenDebugFlagSetToTrueWhenCreatingDirectSubmissionThenEnableMiMemFenceProgramming) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(1);

    if (heaplessStateInit || pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        GTEST_SKIP();
    }

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_TRUE(directSubmission.miMemFenceRequired);
    EXPECT_FALSE(directSubmission.systemMemoryFenceAddressSet);

    EXPECT_TRUE(directSubmission.initialize(true));

    EXPECT_EQ(directSubmission.systemMemoryFenceAddressSet, directSubmission.globalFenceAllocation != nullptr);
    EXPECT_TRUE(directSubmission.miMemFenceRequired);
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

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection(false) +
                        sizeof(MI_LOAD_REGISTER_IMM) +
                        sizeof(MI_LOAD_REGISTER_MEM);
    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
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
    EXPECT_EQ(1u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    EXPECT_EQ(directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false)) - directSubmission.getSizeNewResourceHandler(), directSubmission.ringCommandStream.getUsed());
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
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    FlushStampTracker flushStamp(true);
    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.copyCommandBufferIntoRing(batchBuffer));

    bool ret = directSubmission.initialize(true);
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
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenMetricsDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);
    pDevice->getExecutionEnvironment()->setMetricsEnabled(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemBatchBufferWithoutCommandBufferRelaxingAndForcedFlatRingDisabledDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    batchBuffer.disableFlatRingBuffer = true;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithoutCommandBufferRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    batchBuffer.commandBufferAllocation = nullptr;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDisabledDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(0);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    batchBuffer.hasRelaxedOrderingDependencies = true;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitSystemMemChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    batchBuffer.chainedBatchBuffer = reinterpret_cast<GraphicsAllocation *>(0x1234);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndSingleTileDirectSubmissionWhenSubmitLocalMemNotChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    static_cast<MemoryAllocation *>(batchBuffer.commandBufferAllocation)->overrideMemoryPool(MemoryPool::localMemory);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(directSubmission.copyCommandBufferIntoRing(batchBuffer));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDefaultDirectSubmissionFlatRingBufferAndMultiTileDirectSubmissionWhenSubmitSystemMemNotChainedBatchBufferWithoutRelaxingDependenciesThenNotCopyIntoRing) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
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
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(0);

    MockDirectSubmissionHw<FamilyType, Dispatcher> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    regularDirectSubmission.disableMonitorFence = false;
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch(false, false, regularDirectSubmission.dispatchMonitorFenceRequired(false));

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t tagUpdateSize = Dispatcher::getSizeMonitorFence(directSubmission.rootDeviceEnvironment);

    size_t disabledSizeDispatch = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false));
    EXPECT_EQ(disabledSizeDispatch, (regularSizeDispatch - tagUpdateSize));

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    directSubmission.dispatchWorkloadSection(batchBuffer, directSubmission.dispatchMonitorFenceRequired(batchBuffer.dispatchMonitorFence));
    size_t expectedDispatchSize = disabledSizeDispatch - directSubmission.getSizeNewResourceHandler();
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
    debugManager.flags.DirectSubmissionDisableCacheFlush.set(0);

    MockDirectSubmissionHw<FamilyType, Dispatcher> regularDirectSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    size_t regularSizeDispatch = regularDirectSubmission.getSizeDispatch(false, false, regularDirectSubmission.dispatchMonitorFenceRequired(false));

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.disableCacheFlush = true;
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    size_t flushSize = Dispatcher::getSizeCacheFlush(directSubmission.rootDeviceEnvironment);

    size_t disabledSizeDispatch = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false));
    EXPECT_EQ(disabledSizeDispatch, (regularSizeDispatch - flushSize));

    directSubmission.dispatchWorkloadSection(batchBuffer, directSubmission.dispatchMonitorFenceRequired(batchBuffer.dispatchMonitorFence));
    size_t expectedDispatchSize = disabledSizeDispatch - directSubmission.getSizeNewResourceHandler();
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
         givenDirectSubmissionRingStartAndSwitchBuffersWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferAndQueueCountIncrease) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection(false);
    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
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
    EXPECT_EQ(1u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    EXPECT_EQ(sizeUsed + directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false)) - directSubmission.getSizeNewResourceHandler(), directSubmission.ringCommandStream.getUsed());
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

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(0u, directSubmission.submitCount);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_EQ(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(0u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(oldRingAllocation->getGpuAddress(), directSubmission.submitGpuAddress);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    size_t submitSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false)) - directSubmission.getSizeNewResourceHandler();
    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        submitSize += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
    }
    EXPECT_EQ(submitSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingStartWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferAndQueueCountIncrease) {
    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection(false);
    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
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
    EXPECT_EQ(1u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    EXPECT_EQ(directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false)) - directSubmission.getSizeNewResourceHandler(), directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingNotStartWhenDispatchingCommandBufferThenExpectDispatchInCommandBufferQueueCountIncreaseAndSubmitToGpu) {
    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(0u, directSubmission.submitCount);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    directSubmission.ringCommandStream.getSpace(directSubmission.ringCommandStream.getAvailableSpace() -
                                                directSubmission.getSizeSwitchRingBufferSection());

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_NE(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(0u, directSubmission.semaphoreData->queueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    size_t submitSize = directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false)) - directSubmission.getSizeNewResourceHandler();
    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
        submitSize += directSubmission.getSizeSystemMemoryFenceAddress();
    }
    if (directSubmission.isRelaxedOrderingEnabled()) {
        submitSize += RelaxedOrderingHelper::getSizeRegistersInit<FamilyType>();
    }
    EXPECT_EQ(submitSize, directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDirectSubmissionPrintBuffersWhenInitializeAndDispatchBufferThenCommandBufferArePrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionPrintBuffers.set(true);

    FlushStampTracker flushStamp(true);
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    StreamCapture capture;
    capture.captureStdout();

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);

    std::string output = capture.getCapturedStdout();

    auto pos = output.find("Ring buffer 0");
    EXPECT_TRUE(pos != std::string::npos);
    pos = output.find("Ring buffer 1");
    EXPECT_TRUE(pos != std::string::npos);
    pos = output.find("Client buffer");
    EXPECT_TRUE(pos != std::string::npos);
    pos = output.find("Ring buffer for submission");
    EXPECT_TRUE(pos != std::string::npos);
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDirectSubmissionPrintSemaphoreWhenDispatchingThenPrintAllData) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionPrintSemaphoreUsage.set(1);

    {
        FlushStampTracker flushStamp(true);
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

        testing::internal::CaptureStdout();

        bool ret = directSubmission.initialize(false);
        EXPECT_TRUE(ret);
        ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
        EXPECT_TRUE(ret);
        directSubmission.unblockGpu();
    }

    std::string output = testing::internal::GetCapturedStdout();

    auto pos = output.find("DirectSubmission semaphore");
    EXPECT_TRUE(pos != std::string::npos);
    pos = output.find("unlocked with value:");
    EXPECT_TRUE(pos != std::string::npos);
    pos = output.find("programmed with value:");
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

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());

    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection(false) +
                        sizeof(MI_LOAD_REGISTER_IMM) +
                        sizeof(MI_LOAD_REGISTER_MEM);
    if (directSubmission.miMemFenceRequired && !heaplessStateInit) {
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

    uint32_t expectedOffset = NEO::ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset();
    EXPECT_EQ(expectedOffset, directSubmission.immWritePostSyncOffset);

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

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(directSubmission.partitionConfigSet);
    EXPECT_FALSE(directSubmission.ringStart);
    EXPECT_EQ(0x0u, directSubmission.ringCommandStream.getUsed());

    directSubmission.dispatchUllsState();
    EXPECT_TRUE(directSubmission.partitionConfigSet);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    uint32_t expectedOffset = NEO::ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset();

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
         givenRenderDirectSubmissionWhenDispatchWorkloadCalledWithMonitorFenceThenExpectPostSyncOperationWithoutNotifyFlag) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using Dispatcher = RenderDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = false;

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);

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
            EXPECT_FALSE(pipeControl->getNotifyEnable());
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenBlitterDirectSubmissionWhenDispatchWorkloadCalledWithMonitorFenceThenExpectPostSyncOperationWithoutNotifyFlag) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using Dispatcher = BlitterDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = false;

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);

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
            EXPECT_FALSE(miFlush->getNotifyEnable());
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenRenderDirectSubmissionWhenDispatchWorkloadCalledWithMonitorFenceAndNotifyEnableRequiredThenExpectPostSyncOperationWithNotifyFlag) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using Dispatcher = RenderDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = false;
    directSubmission.notifyKmdDuringMonitorFence = true;

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);

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

HWTEST_F(DirectSubmissionDispatchBufferTest, givenRingBufferRestartRequestWhenDispatchCommandBuffer) {
    FlushStampTracker flushStamp(true);
    MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_EQ(directSubmission.submitCount, 1u);

    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_EQ(directSubmission.submitCount, 1u);
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDebugFlagSetWhenDispatchingWorkloadThenProgramSfenceInstruction) {
    DebugManagerStateRestore restorer{};

    using Dispatcher = BlitterDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    for (int32_t debugFlag : {-1, 0, 1, 2}) {
        debugManager.flags.DirectSubmissionInsertSfenceInstructionPriorToSubmission.set(debugFlag);

        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        EXPECT_TRUE(directSubmission.initialize(true));

        auto initialSfenceCounterValue = CpuIntrinsicsTests::sfenceCounter.load();
        auto initialMfenceCounterValue = CpuIntrinsicsTests::mfenceCounter.load();

        EXPECT_TRUE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

        uint32_t expectedSfenceCount = (debugFlag == -1) ? 2 : static_cast<uint32_t>(debugFlag);
        uint32_t expectedMfenceCount = 0u;
        if (!pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice && !pDevice->getProductHelper().isAcquireGlobalFenceInDirectSubmissionRequired(pDevice->getHardwareInfo()) && expectedSfenceCount > 0u) {
            --expectedSfenceCount;
            ++expectedMfenceCount;
        }

        EXPECT_EQ(initialSfenceCounterValue + expectedSfenceCount, CpuIntrinsicsTests::sfenceCounter);
        EXPECT_EQ(initialMfenceCounterValue + expectedMfenceCount, CpuIntrinsicsTests::mfenceCounter);
    }
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDebugFlagSetWhenStoppingRingbufferThenProgramSfenceInstruction) {
    DebugManagerStateRestore restorer{};

    using Dispatcher = BlitterDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    for (int32_t debugFlag : {-1, 0, 1, 2}) {
        debugManager.flags.DirectSubmissionInsertSfenceInstructionPriorToSubmission.set(debugFlag);

        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        EXPECT_TRUE(directSubmission.initialize(true));

        auto initialSfenceCounterValue = CpuIntrinsicsTests::sfenceCounter.load();
        auto initialMfenceCounterValue = CpuIntrinsicsTests::mfenceCounter.load();

        EXPECT_TRUE(directSubmission.stopRingBuffer(false));

        uint32_t expectedSfenceCount = (debugFlag == -1) ? 2 : static_cast<uint32_t>(debugFlag);
        uint32_t expectedMfenceCount = 0u;
        if (!pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice && !directSubmission.pciBarrierPtr && !pDevice->getProductHelper().isAcquireGlobalFenceInDirectSubmissionRequired(pDevice->getHardwareInfo()) && expectedSfenceCount > 0u) {
            --expectedSfenceCount;
            ++expectedMfenceCount;
        }

        EXPECT_EQ(initialSfenceCounterValue + expectedSfenceCount, CpuIntrinsicsTests::sfenceCounter);
        EXPECT_EQ(initialMfenceCounterValue + expectedMfenceCount, CpuIntrinsicsTests::mfenceCounter);
    }
}

struct DirectSubmissionRelaxedOrderingTests : public DirectSubmissionDispatchBufferTest {
    void SetUp() override {
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        DirectSubmissionDispatchBufferTest::SetUp();
    }

    template <typename FamilyType>
    bool verifyDynamicSchedulerProgramming(LinearStream &cs, uint64_t schedulerAllocationGpuVa, uint64_t semaphoreGpuVa, uint32_t semaphoreValue, size_t offset, size_t &endOffset);

    template <typename FamilyType>
    bool verifyStaticSchedulerProgramming(GraphicsAllocation &schedulerAllocation, uint64_t deferredTaskListVa, uint64_t semaphoreGpuVa, uint32_t expectedQueueSizeLimit, uint32_t miMathMocs);

    template <typename FamilyType>
    bool verifyDummyBlt(typename FamilyType::XY_COLOR_BLT *cmd);

    DebugManagerStateRestore restore;
    FlushStampTracker flushStamp{true};
};

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyDummyBlt(typename FamilyType::XY_COLOR_BLT *cmd) {
    if (cmd->getDestinationX2CoordinateRight() == 1u && cmd->getDestinationY2CoordinateBottom() == 4u && cmd->getDestinationPitch() == static_cast<uint32_t>(MemoryConstants::pageSize)) {
        return true;
    }
    return false;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyStaticSchedulerProgramming(GraphicsAllocation &schedulerAllocation, uint64_t deferredTaskListVa, uint64_t semaphoreGpuVa, uint32_t expectedQueueSizeLimit, uint32_t miMathMocs) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_MATH = typename FamilyType::MI_MATH;

    uint64_t schedulerStartGpuAddress = schedulerAllocation.getGpuAddress();
    void *schedulerCmds = schedulerAllocation.getUnderlyingBuffer();

    auto startPtr = schedulerCmds;

    // 1. Init section
    auto miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(schedulerCmds);

    if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
        return false;
    }

    auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(++miPredicate);

    if (!RelaxedOrderingCommandsHelper::verifyLrr<FamilyType>(lrrCmd, RegisterOffsets::csGprR0, RegisterOffsets::csGprR9)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLrr<FamilyType>(++lrrCmd, RegisterOffsets::csGprR0 + 4, RegisterOffsets::csGprR9 + 4)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyConditionalDataRegBbStart<FamilyType>(++lrrCmd, 0, RegisterOffsets::csGprR1, 0, CompareOperation::equal, true)) {
        return false;
    }

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(lrrCmd, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart(false)));
    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR2, 0)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR2 + 4, 0)) {
        return false;
    }

    uint64_t removeTaskVa = schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::removeTaskSectionStart;

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR3, static_cast<uint32_t>(removeTaskVa & 0xFFFF'FFFFULL))) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR3 + 4, static_cast<uint32_t>(removeTaskVa >> 32))) {
        return false;
    }

    uint64_t walkersLoopConditionCheckVa = schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::tasksListLoopCheckSectionStart;

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR4, static_cast<uint32_t>(walkersLoopConditionCheckVa & 0xFFFF'FFFFULL))) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR4 + 4, static_cast<uint32_t>(walkersLoopConditionCheckVa >> 32))) {
        return false;
    }

    // 2. Dispatch task section (loop start)
    miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++lriCmd);

    if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
        return false;
    }

    lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++miPredicate);
    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR6, 8)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR6 + 4, 0)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR8, static_cast<uint32_t>(deferredTaskListVa & 0xFFFF'FFFFULL))) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR8 + 4, static_cast<uint32_t>(deferredTaskListVa >> 32))) {
        return false;
    }

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    if (miMathCmd->DW0.BitField.DwordLength != 9) {
        return false;
    }

    if constexpr (FamilyType::isUsingMiMathMocs) {
        if (miMathCmd->DW0.BitField.MemoryObjectControlState != miMathMocs) {
            return false;
        }
    }

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr2)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr6)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeShl, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeStore, AluRegisters::gpr7, AluRegisters::accu)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr7)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeStore, AluRegisters::gpr6, AluRegisters::accu)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoadind, AluRegisters::gpr0, AluRegisters::accu)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeFenceRd, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(++miAluCmd);
    if (!RelaxedOrderingCommandsHelper::verifyBbStart<FamilyType>(bbStart, 0, true, false)) {
        return false;
    }

    // 3. Remove task section
    miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++bbStart);
    if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
        return false;
    }

    miPredicate++;
    if (!RelaxedOrderingCommandsHelper::verifyIncrementOrDecrement<FamilyType>(miPredicate, AluRegisters::gpr1, false)) {
        return false;
    }

    auto cmds = ptrOffset(miPredicate, EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement());

    if (!RelaxedOrderingCommandsHelper::verifyIncrementOrDecrement<FamilyType>(cmds, AluRegisters::gpr2, false)) {
        return false;
    }

    lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(ptrOffset(cmds, EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement()));

    if (!RelaxedOrderingCommandsHelper::verifyLrr<FamilyType>(lrrCmd, RegisterOffsets::csGprR0, RegisterOffsets::csGprR9)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLrr<FamilyType>(++lrrCmd, RegisterOffsets::csGprR0 + 4, RegisterOffsets::csGprR9 + 4)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyConditionalDataRegBbStart<FamilyType>(++lrrCmd, 0, RegisterOffsets::csGprR1, 0, CompareOperation::equal, true)) {
        return false;
    }

    lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(lrrCmd, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart(false)));
    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR7, 8)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR7 + 4, 0)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR8, static_cast<uint32_t>(deferredTaskListVa & 0xFFFF'FFFFULL))) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR8 + 4, static_cast<uint32_t>(deferredTaskListVa >> 32))) {
        return false;
    }

    miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    if (miMathCmd->DW0.BitField.DwordLength != 13) {
        return false;
    }

    if constexpr (FamilyType::isUsingMiMathMocs) {
        if (miMathCmd->DW0.BitField.MemoryObjectControlState != miMathMocs) {
            return false;
        }
    }

    miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr1)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr7)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeShl, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeStore, AluRegisters::gpr7, AluRegisters::accu)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr7)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoadind, AluRegisters::gpr7, AluRegisters::accu)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeFenceRd, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr6)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad0, AluRegisters::srcb, AluRegisters::opcodeNone)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeStoreind, AluRegisters::accu, AluRegisters::gpr7)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeFenceWr, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    // 4. List loop check section

    miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++miAluCmd);
    if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
        return false;
    }

    miPredicate++;
    if (!RelaxedOrderingCommandsHelper::verifyIncrementOrDecrement<FamilyType>(miPredicate, AluRegisters::gpr2, true)) {
        return false;
    }

    cmds = ptrOffset(miPredicate, EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement());

    if (!RelaxedOrderingCommandsHelper::verifyConditionalRegRegBbStart<FamilyType>(cmds, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::loopStartSectionStart,
                                                                                   AluRegisters::gpr1, AluRegisters::gpr2, CompareOperation::notEqual, false)) {
        return false;
    }

    lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(cmds, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalRegRegBatchBufferStart()));

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR2, 0)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR2 + 4, 0)) {
        return false;
    }

    // 5. Drain request section

    auto arbCheck = reinterpret_cast<MI_ARB_CHECK *>(++lriCmd);
    if (memcmp(arbCheck, &FamilyType::cmdInitArbCheck, sizeof(MI_ARB_CHECK)) != 0) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyConditionalDataRegBbStart<FamilyType>(++arbCheck, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::loopStartSectionStart,
                                                                                    RegisterOffsets::csGprR1, expectedQueueSizeLimit, CompareOperation::greaterOrEqual, false)) {
        return false;
    }

    auto conditionalBbStartcmds = ptrOffset(arbCheck, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart(false));

    if (!RelaxedOrderingCommandsHelper::verifyConditionalDataRegBbStart<FamilyType>(conditionalBbStartcmds, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::loopStartSectionStart,
                                                                                    RegisterOffsets::csGprR5, 1, CompareOperation::equal, false)) {
        return false;
    }

    // 6. Scheduler loop check section
    lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(conditionalBbStartcmds, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart(false)));

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR10, static_cast<uint32_t>(RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<FamilyType>::semaphoreSectionSize))) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR10 + 4, 0)) {
        return false;
    }

    miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    if (miMathCmd->DW0.BitField.DwordLength != 3) {
        return false;
    }

    if constexpr (FamilyType::isUsingMiMathMocs) {
        if (miMathCmd->DW0.BitField.MemoryObjectControlState != miMathMocs) {
            return false;
        }
    }

    miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr9)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr10)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeStore, AluRegisters::gpr0, AluRegisters::accu)) {
        return false;
    }

    if (!RelaxedOrderingCommandsHelper::verifyConditionalRegMemBbStart<FamilyType>(++miAluCmd, 0, semaphoreGpuVa, RegisterOffsets::csGprR11, CompareOperation::greaterOrEqual, true)) {
        return false;
    }

    bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(ptrOffset(miAluCmd, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalRegMemBatchBufferStart()));
    if (!RelaxedOrderingCommandsHelper::verifyBbStart<FamilyType>(bbStart, schedulerStartGpuAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::loopStartSectionStart, false, false)) {
        return false;
    }

    auto endCmd = ++bbStart;

    EXPECT_EQ(RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::totalSize, ptrDiff(endCmd, startPtr));

    return true;
}

template <typename FamilyType>
bool DirectSubmissionRelaxedOrderingTests::verifyDynamicSchedulerProgramming(LinearStream &cs, uint64_t schedulerAllocationGpuVa, uint64_t semaphoreGpuVa, uint32_t semaphoreValue, size_t offset, size_t &endOffset) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cs, offset);
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;

    for (auto &it : hwParse.cmdList) {
        if (auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it)) {
            // 1. Init section

            uint64_t schedulerStartAddress = cs.getGraphicsAllocation()->getGpuAddress() + ptrDiff(lriCmd, cs.getCpuBase());

            uint64_t semaphoreSectionVa = schedulerStartAddress + RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<FamilyType>::semaphoreSectionStart;

            if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR11, semaphoreValue)) {
                continue;
            }

            if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR9, static_cast<uint32_t>(semaphoreSectionVa & 0xFFFF'FFFFULL))) {
                continue;
            }

            if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR9 + 4, static_cast<uint32_t>(semaphoreSectionVa >> 32))) {
                continue;
            }

            auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(++lriCmd);
            if (!RelaxedOrderingCommandsHelper::verifyBbStart<FamilyType>(bbStart, schedulerAllocationGpuVa, false, false)) {
                continue;
            }

            // 2. Semaphore section
            auto miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++bbStart);
            if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
                continue;
            }

            auto semaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(++miPredicate);
            if ((semaphore->getSemaphoreGraphicsAddress() != semaphoreGpuVa) ||
                (semaphore->getSemaphoreDataDword() != semaphoreValue) ||
                (semaphore->getCompareOperation() != MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD)) {
                continue;
            }

            // 3. End section

            miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++semaphore);
            if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
                continue;
            }

            lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++miPredicate);
            if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR5, 0)) {
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

    directSubmission.initialize(false);

    auto allocsIter = mockMemoryOperations->gfxAllocationsForMakeResident.rbegin();

    EXPECT_EQ(AllocationType::commandBuffer, directSubmission.relaxedOrderingSchedulerAllocation->getAllocationType());
    EXPECT_NE(nullptr, directSubmission.relaxedOrderingSchedulerAllocation);
    EXPECT_EQ(directSubmission.relaxedOrderingSchedulerAllocation, *allocsIter);

    allocsIter++;

    EXPECT_EQ(AllocationType::deferredTasksList, directSubmission.deferredTasksListAllocation->getAllocationType());
    EXPECT_NE(nullptr, directSubmission.deferredTasksListAllocation);
    EXPECT_EQ(directSubmission.deferredTasksListAllocation, *allocsIter);
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenDebugFlagSetWhenDispatchingStaticSchedulerThenOverrideQueueSizeLimit, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    debugManager.flags.DirectSubmissionRelaxedOrderingQueueSizeLimit.set(123);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true);

    EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    EXPECT_TRUE(verifyStaticSchedulerProgramming<FamilyType>(*directSubmission.relaxedOrderingSchedulerAllocation,
                                                             directSubmission.deferredTasksListAllocation->getGpuAddress(), directSubmission.semaphoreGpuVa, 123,
                                                             pDevice->getRootDeviceEnvironment().getGmmHelper()->getL3EnabledMOCS()));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenNewNumberOfClientsWhenDispatchingWorkThenIncraseQueueSize, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true);

    EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    EXPECT_EQ(RelaxedOrderingHelper::queueSizeMultiplier, directSubmission.currentRelaxedOrderingQueueSize);
    EXPECT_TRUE(verifyStaticSchedulerProgramming<FamilyType>(*directSubmission.relaxedOrderingSchedulerAllocation,
                                                             directSubmission.deferredTasksListAllocation->getGpuAddress(), directSubmission.semaphoreGpuVa, RelaxedOrderingHelper::queueSizeMultiplier,
                                                             pDevice->getRootDeviceEnvironment().getGmmHelper()->getL3EnabledMOCS()));

    const uint64_t expectedQueueSizeValueVa = directSubmission.relaxedOrderingSchedulerAllocation->getGpuAddress() +
                                              RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<FamilyType>::drainRequestSectionStart +
                                              EncodeMiArbCheck<FamilyType>::getCommandSize() +
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

    offset = directSubmission.ringCommandStream.getUsed();
    batchBuffer.numCsrClients = 5;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    EXPECT_TRUE((RelaxedOrderingHelper::queueSizeMultiplier * batchBuffer.numCsrClients) > RelaxedOrderingHelper::maxQueueSize);
    EXPECT_EQ(RelaxedOrderingHelper::maxQueueSize, directSubmission.currentRelaxedOrderingQueueSize);
    EXPECT_FALSE(findStaticSchedulerUpdate(directSubmission.ringCommandStream, offset, RelaxedOrderingHelper::queueSizeMultiplier * batchBuffer.numCsrClients));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenInitializingThenDispatchStaticScheduler, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(false);

        EXPECT_EQ(0u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true);

        EXPECT_EQ(1u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);
        EXPECT_TRUE(verifyStaticSchedulerProgramming<FamilyType>(*directSubmission.relaxedOrderingSchedulerAllocation,
                                                                 directSubmission.deferredTasksListAllocation->getGpuAddress(), directSubmission.semaphoreGpuVa, RelaxedOrderingHelper::queueSizeMultiplier,
                                                                 pDevice->getRootDeviceEnvironment().getGmmHelper()->getL3EnabledMOCS()));
    }

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(false);
        EXPECT_EQ(0u, directSubmission.dispatchStaticRelaxedOrderingSchedulerCalled);

        directSubmission.dispatchUllsState();

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
                if (RegisterOffsets::csGprR1 == lriCmd->getRegisterOffset()) {
                    EXPECT_EQ(0u, lriCmd->getDataDword());

                    lriCmd++;
                    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR1 + 4, 0)) {
                        continue;
                    }

                    lriCmd++;
                    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR5, 0)) {
                        continue;
                    }

                    lriCmd++;
                    if (!RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR5 + 4, 0)) {
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
        directSubmission.initialize(false);
        EXPECT_FALSE(verifyInitRegisters(directSubmission.ringCommandStream, 0));

        EXPECT_EQ(0u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
        EXPECT_FALSE(directSubmission.relaxedOrderingInitialized);
        EXPECT_EQ(nullptr, directSubmission.preinitializedTaskStoreSection.get());
        EXPECT_EQ(nullptr, directSubmission.preinitializedRelaxedOrderingScheduler.get());
    }

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true);
        EXPECT_TRUE(verifyInitRegisters(directSubmission.ringCommandStream, 0));

        EXPECT_EQ(1u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
        EXPECT_TRUE(directSubmission.relaxedOrderingInitialized);
        EXPECT_NE(nullptr, directSubmission.preinitializedTaskStoreSection.get());
        EXPECT_NE(nullptr, directSubmission.preinitializedRelaxedOrderingScheduler.get());

        size_t offset = directSubmission.ringCommandStream.getUsed();

        directSubmission.dispatchUllsState();
        EXPECT_FALSE(verifyInitRegisters(directSubmission.ringCommandStream, offset));

        EXPECT_EQ(1u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
    }

    {
        MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(false);
        EXPECT_EQ(0u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);

        directSubmission.dispatchUllsState();

        EXPECT_EQ(1u, directSubmission.preinitializeRelaxedOrderingSectionsCalled);
        EXPECT_TRUE(directSubmission.relaxedOrderingInitialized);
        EXPECT_NE(nullptr, directSubmission.preinitializedTaskStoreSection.get());
        EXPECT_NE(nullptr, directSubmission.preinitializedRelaxedOrderingScheduler.get());

        size_t offset = directSubmission.ringCommandStream.getUsed();
        directSubmission.dispatchUllsState();
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

    directSubmission.initialize(true);
    auto offset = directSubmission.ringCommandStream.getUsed() + directSubmission.getSizeStartSection() + RelaxedOrderingHelper::getSizeReturnPtrRegs<FamilyType>();

    batchBuffer.hasRelaxedOrderingDependencies = true;
    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    auto taskStoreSection = ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset);

    if constexpr (FamilyType::isUsingMiSetPredicate) {
        EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(taskStoreSection, MiPredicateType::disable));

        taskStoreSection = ptrOffset(taskStoreSection, sizeof(typename FamilyType::MI_SET_PREDICATE));
    }

    uint64_t deferredTasksVa = directSubmission.deferredTasksListAllocation->getGpuAddress();

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(taskStoreSection);

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR6, static_cast<uint32_t>(deferredTasksVa & 0xFFFF'FFFFULL)));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR6 + 4, static_cast<uint32_t>(deferredTasksVa >> 32)));

    EXPECT_NE(0u, batchBuffer.taskStartAddress);

    uint32_t taskStartAddressLow = static_cast<uint32_t>(batchBuffer.taskStartAddress & 0xFFFF'FFFFULL);
    EXPECT_NE(0u, taskStartAddressLow);
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR7, taskStartAddressLow));

    uint32_t taskStartHigh = static_cast<uint32_t>(batchBuffer.taskStartAddress >> 32);
    EXPECT_NE(0u, taskStartHigh);
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR7 + 4, taskStartHigh));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR8, 8));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR8 + 4, 0));

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    EXPECT_EQ(8u, miMathCmd->DW0.BitField.DwordLength);

    if constexpr (FamilyType::isUsingMiMathMocs) {
        EXPECT_EQ(miMathCmd->DW0.BitField.MemoryObjectControlState, pDevice->getRootDeviceEnvironment().getGmmHelper()->getL3EnabledMOCS());
    }

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr1));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeShl, AluRegisters::opcodeNone, AluRegisters::opcodeNone));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeStore, AluRegisters::gpr8, AluRegisters::accu));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr8));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr6));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeStoreind, AluRegisters::accu, AluRegisters::gpr7));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyAlu<FamilyType>(++miAluCmd, AluRegisters::opcodeFenceWr, AluRegisters::opcodeNone, AluRegisters::opcodeNone));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyIncrementOrDecrement<FamilyType>(++miAluCmd, AluRegisters::gpr1, true));
}

HWTEST_F(DirectSubmissionRelaxedOrderingTests, givenNotEnoughSpaceForTaskStoreSectionWhenDispatchingThenSwitchRingBuffers) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true);
    directSubmission.ringCommandStream.getUsed();

    auto sizeToConsume = directSubmission.ringCommandStream.getAvailableSpace() -
                         (directSubmission.getSizeDispatch(false, false, directSubmission.dispatchMonitorFenceRequired(false)) + directSubmission.getSizeEnd(false) + directSubmission.getSizeSwitchRingBufferSection());

    directSubmission.ringCommandStream.getSpace(sizeToConsume);

    auto oldAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

    EXPECT_NE(oldAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenDispatchingWorkThenDispatchScheduler, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    directSubmission.initialize(true);
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

    directSubmission.initialize(true);
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
                      EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart(false);
    uint64_t expectedJumpAddress = directSubmission.ringCommandStream.getGpuBase() + offset + jumpOffset;

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyConditionalDataRegBbStart<FamilyType>(startAddress, expectedJumpAddress, RegisterOffsets::csGprR1, 0, CompareOperation::equal, false));

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, offset + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart(false));
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;
    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;

    for (auto &it : hwParse.cmdList) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (lriCmd) {
            if (RegisterOffsets::csGprR5 == lriCmd->getRegisterOffset() && lriCmd->getDataDword() == 1) {
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

    directSubmission.initialize(true);
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
            if (RegisterOffsets::csGprR5 == lriCmd->getRegisterOffset() && lriCmd->getDataDword() == 1) {
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

    directSubmission.initialize(true);
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

    directSubmission.stopRingBuffer(false);

    auto startAddress = ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset);
    auto jumpOffset = directSubmission.getSizeSemaphoreSection(true) + sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM) +
                      EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart(false);
    uint64_t expectedJumpAddress = directSubmission.ringCommandStream.getGpuBase() + offset + jumpOffset;

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyConditionalDataRegBbStart<FamilyType>(startAddress, expectedJumpAddress, RegisterOffsets::csGprR1, 0, CompareOperation::equal, false));

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, offset + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart(false));
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;
    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;

    for (auto &it : hwParse.cmdList) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (lriCmd) {
            if (RegisterOffsets::csGprR5 == lriCmd->getRegisterOffset() && lriCmd->getDataDword() == 1) {
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

    directSubmission.initialize(true);
    size_t offset = directSubmission.ringCommandStream.getUsed();

    uint64_t staticSchedulerGpuAddress = directSubmission.relaxedOrderingSchedulerAllocation->getGpuAddress();
    uint64_t semaphoreGpuVa = directSubmission.semaphoreGpuVa;

    size_t endOffset = 0;

    EXPECT_FALSE(verifyDynamicSchedulerProgramming<FamilyType>(directSubmission.ringCommandStream, staticSchedulerGpuAddress, semaphoreGpuVa, directSubmission.currentQueueWorkCount, 0, endOffset));

    directSubmission.stopRingBuffer(false);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, offset);
    hwParse.findHardwareCommands<FamilyType>();

    bool success = false;
    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;

    for (auto &it : hwParse.cmdList) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (lriCmd) {
            if (RegisterOffsets::csGprR5 == lriCmd->getRegisterOffset() && lriCmd->getDataDword() == 1) {
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
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(-1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);
        EXPECT_EQ(gfxCoreHelper.isRelaxedOrderingSupported(), directSubmission.isRelaxedOrderingEnabled());
    }

    {
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(0);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_FALSE(directSubmission.isRelaxedOrderingEnabled());
    }

    {
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_TRUE(directSubmission.isRelaxedOrderingEnabled());
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenDebugFlagSetWhenCreatingBcsDispatcherThenEnableRelaxedOrdering, IsAtLeastXeHpcCore) {
    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->setupContext(*osContext);

    {
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(0);
        debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_FALSE(directSubmission.isRelaxedOrderingEnabled());
    }

    {
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(-1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_TRUE(directSubmission.isRelaxedOrderingEnabled());
    }

    {
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(0);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_FALSE(directSubmission.isRelaxedOrderingEnabled());
    }

    {
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);

        MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(*ultCsr);

        EXPECT_TRUE(directSubmission.isRelaxedOrderingEnabled());
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBcsRelaxedOrderingEnabledWhenProgrammingEndingCommandsThenSetReturnPtrs, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);

    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->setupContext(*osContext);
    ultCsr->blitterDirectSubmissionAvailable = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*ultCsr);
    ultCsr->blitterDirectSubmission.reset(directSubmission);

    auto &commandStream = ultCsr->getCS(0x100);

    auto endingPtr = commandStream.getSpace(0);

    ultCsr->programEndingCmd(commandStream, &endingPtr, true, true, true);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();
    auto lrrCmdIt = find<MI_LOAD_REGISTER_REG *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrrCmdIt);

    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR3);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR0);

    lrrCmd++;
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR3 + 4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR0 + 4);

    auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(++lrrCmd);
    EXPECT_EQ(1u, bbStartCmd->getIndirectAddressEnable());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBcsRelaxedOrderingDisabledWhenProgrammingEndingCommandsThenDontSetReturnPtrs, IsAtLeastXeHpcCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(0);

    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->setupContext(*osContext);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*ultCsr);
    ultCsr->blitterDirectSubmission.reset(directSubmission);

    auto &commandStream = ultCsr->getCS(0x100);
    auto endingPtr = commandStream.getSpace(0);

    ultCsr->programEndingCmd(commandStream, &endingPtr, true, false, true);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();
    auto bbStartCmdIt = find<MI_BATCH_BUFFER_START *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartCmdIt);

    ASSERT_NE(nullptr, bbStartCmd);
    EXPECT_EQ(0u, bbStartCmd->getIndirectAddressEnable());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenProgrammingEndingCmdsThenSetReturnRegisters, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->directSubmissionAvailable = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    directSubmission->initialize(true);

    ultCsr->directSubmission.reset(directSubmission);

    auto &commandStream = ultCsr->getCS(0x100);

    auto endingPtr = commandStream.getSpace(0);

    ultCsr->programEndingCmd(commandStream, &endingPtr, true, true, true);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();
    auto lrrCmdIt = find<MI_LOAD_REGISTER_REG *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrrCmdIt);

    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR3);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR0);

    lrrCmd++;
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR3 + 4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::csGprR0 + 4);

    auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(++lrrCmd);
    EXPECT_EQ(1u, bbStartCmd->getIndirectAddressEnable());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithoutRelaxedOrderingDependencieswhenProgrammingEndingCmdsThenDontSetReturnRegisters, IsAtLeastXeHpcCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    directSubmission->initialize(true);

    ultCsr->directSubmission.reset(directSubmission);

    auto &commandStream = ultCsr->getCS(0x100);

    auto endingPtr = commandStream.getSpace(0);

    ultCsr->programEndingCmd(commandStream, &endingPtr, true, false, false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();
    auto bbStartCmdIt = find<MI_BATCH_BUFFER_START *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartCmdIt);

    ASSERT_NE(nullptr, bbStartCmd);
    EXPECT_EQ(0u, bbStartCmd->getIndirectAddressEnable());
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, whenDispatchingWorkloadSectionThenProgramReturnPtrs, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true);

    auto offset = directSubmission.ringCommandStream.getUsed();

    auto originalBbStart = *reinterpret_cast<MI_BATCH_BUFFER_START *>(batchBuffer.endCmdPtr);

    batchBuffer.hasRelaxedOrderingDependencies = true;
    directSubmission.dispatchWorkloadSection(batchBuffer, directSubmission.dispatchMonitorFenceRequired(batchBuffer.dispatchMonitorFence));

    uint64_t returnPtr = directSubmission.ringCommandStream.getGpuBase() + offset + (4 * sizeof(MI_LOAD_REGISTER_IMM)) + directSubmission.getSizeStartSection();

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset));
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR4, static_cast<uint32_t>(returnPtr & 0xFFFF'FFFFULL)));
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR4 + 4, static_cast<uint32_t>(returnPtr >> 32)));

    uint64_t returnPtr2 = returnPtr + RelaxedOrderingHelper::getSizeTaskStoreSection<FamilyType>();
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR3, static_cast<uint32_t>(returnPtr2 & 0xFFFF'FFFFULL)));
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR3 + 4, static_cast<uint32_t>(returnPtr2 >> 32)));

    EXPECT_EQ(0, memcmp(&originalBbStart, batchBuffer.endCmdPtr, sizeof(MI_BATCH_BUFFER_START)));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithoutRelaxedOrderingDependencieswhenDispatchingWorkloadSectionThenDontProgramReturnPtrs, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true);

    auto offset = directSubmission.ringCommandStream.getUsed();

    batchBuffer.hasRelaxedOrderingDependencies = false;
    directSubmission.dispatchWorkloadSection(batchBuffer, directSubmission.dispatchMonitorFenceRequired(batchBuffer.dispatchMonitorFence));

    auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset));
    EXPECT_EQ(nullptr, lriCmd);
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithStallingCmdsAndDependenciesWhenDispatchingNextCmdBufferThenProgramSchedulerIfNeeded, IsAtLeastXeHpcCore) {
    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        directSubmission.stopRingBuffer(false);
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithNonStallingCmdsAndDependenciesWhenDispatchingNextCmdBufferThenProgramSchedulerIfNeeded, IsAtLeastXeHpcCore) {
    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = true;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        directSubmission.stopRingBuffer(false);
        EXPECT_EQ(2u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(1u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(1u, directSubmission.dispatchTaskStoreSectionCalled);
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithStallingCmdsAndWithoutDependenciesWhenDispatchingNextCmdBufferThenProgramSchedulerIfNeeded, IsAtLeastXeHpcCore) {
    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

        batchBuffer.hasStallingCmds = true;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        directSubmission.stopRingBuffer(false);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenBbWithNonStallingCmdsAndWithoutDependenciesWhenDispatchingNextCmdBufferThenProgramSchedulerIfNeeded, IsAtLeastXeHpcCore) {
    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

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
        directSubmission.initialize(true);

        batchBuffer.hasStallingCmds = false;
        batchBuffer.hasRelaxedOrderingDependencies = false;
        directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);

        directSubmission.stopRingBuffer(false);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingSchedulerSectionCalled);
        EXPECT_EQ(0u, directSubmission.dispatchRelaxedOrderingQueueStallCalled);
        EXPECT_EQ(0u, directSubmission.dispatchTaskStoreSectionCalled);
    }
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenSchedulerRequiredWhenDispatchingReturnPtrsThenAddOffset, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    directSubmission.initialize(true);

    uint64_t returnPtr = 0x800100123000;
    uint64_t returnPtr2 = returnPtr + RelaxedOrderingHelper::getSizeTaskStoreSection<FamilyType>();

    size_t offset = directSubmission.ringCommandStream.getUsed();

    directSubmission.dispatchRelaxedOrderingReturnPtrRegs(directSubmission.ringCommandStream, returnPtr);

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(directSubmission.ringCommandStream.getCpuBase(), offset));
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(lriCmd, RegisterOffsets::csGprR4, static_cast<uint32_t>(returnPtr & 0xFFFF'FFFFULL)));
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR4 + 4, static_cast<uint32_t>(returnPtr >> 32)));

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR3, static_cast<uint32_t>(returnPtr2 & 0xFFFF'FFFFULL)));
    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyLri<FamilyType>(++lriCmd, RegisterOffsets::csGprR3 + 4, static_cast<uint32_t>(returnPtr2 >> 32)));
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenReturnPtrsRequiredWhenAskingForDispatchSizeTheAddMmioSizes, IsAtLeastXeHpcCore) {
    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    size_t baseSize = directSubmission.getSizeDispatch(true, false, directSubmission.dispatchMonitorFenceRequired(false));
    size_t sizeWitfRetPtr = directSubmission.getSizeDispatch(true, true, directSubmission.dispatchMonitorFenceRequired(false));

    EXPECT_EQ(baseSize + RelaxedOrderingHelper::getSizeReturnPtrRegs<FamilyType>(), sizeWitfRetPtr);
}

HWTEST2_F(DirectSubmissionRelaxedOrderingTests, givenNumClientsWhenAskingIfRelaxedOrderingEnabledThenReturnCorrectValue, IsAtLeastXeHpcCore) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);

    int client1, client2, client3, client4;
    ultCsr->registerClient(&client1);

    auto directSubmission = new NEO::MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    EXPECT_EQ(1u, ultCsr->getNumClients());
    EXPECT_FALSE(NEO::RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*ultCsr, 1));

    ultCsr->registerClient(&client2);

    EXPECT_EQ(2u, ultCsr->getNumClients());
    EXPECT_TRUE(NEO::RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*ultCsr, 1));

    debugManager.flags.DirectSubmissionRelaxedOrderingMinNumberOfClients.set(4);

    EXPECT_EQ(2u, ultCsr->getNumClients());
    EXPECT_FALSE(NEO::RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*ultCsr, 1));

    ultCsr->registerClient(&client3);
    EXPECT_EQ(3u, ultCsr->getNumClients());
    EXPECT_FALSE(NEO::RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*ultCsr, 1));

    ultCsr->registerClient(&client4);
    EXPECT_EQ(4u, ultCsr->getNumClients());
    EXPECT_TRUE(NEO::RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*ultCsr, 1));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenNotStartedDirectSubmissionWhenStartingFailsDuringDispatchCommandBufferThenExpectReturnFalse) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.initialize(false));
    directSubmission.submitReturn = false;

    EXPECT_FALSE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));
    EXPECT_FALSE(directSubmission.ringStart);
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenNotStartedDirectSubmissionWhenUpdateMonitorFenceTagFailsDuringDispatchCommandBufferThenExpectReturnFalse) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.initialize(false));
    directSubmission.updateTagValueReturn = std::numeric_limits<uint64_t>::max();

    EXPECT_FALSE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));
}

HWTEST_F(DirectSubmissionDispatchBufferTest, givenDispatchBufferNotRequiresBlockingResidencyHandlingThenDontWaitForResidencyAndProgramSemaphore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using Dispatcher = RenderDispatcher<FamilyType>;
    FlushStampTracker flushStamp(true);
    HardwareParse hwParse;

    MockDirectSubmissionHw<FamilyType, Dispatcher> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(directSubmission.initialize(false));
    EXPECT_TRUE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));
    size_t sizeUsedBefore = directSubmission.ringCommandStream.getUsed();
    directSubmission.handleResidencyCount = 0u;
    batchBuffer.pagingFenceSemInfo.pagingFenceValue = 10u;
    batchBuffer.pagingFenceSemInfo.requiresBlockingResidencyHandling = false;
    EXPECT_TRUE(directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));
    EXPECT_EQ(0u, directSubmission.handleResidencyCount);

    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, sizeUsedBefore);
    hwParse.findHardwareCommands<FamilyType>();
    auto semaphoreIt = find<MI_SEMAPHORE_WAIT *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    auto semaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(*semaphoreIt);
    EXPECT_EQ(10u, semaphore->getSemaphoreDataDword());

    auto expectedGpuVa = directSubmission.semaphoreGpuVa + offsetof(RingSemaphoreData, pagingFenceCounter);
    EXPECT_EQ(expectedGpuVa, semaphore->getSemaphoreGraphicsAddress());
}

HWTEST_F(DirectSubmissionTest, givenCsrWhenUnblockPagingFenceSemaphoreCalledThenSemaphoreUnblocked) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    auto directSubmission = new NEO::MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    auto blitterDirectSubmission = new NEO::MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*ultCsr);

    ultCsr->directSubmission.reset(directSubmission);
    ultCsr->blitterDirectSubmission.reset(blitterDirectSubmission);

    ultCsr->unblockPagingFenceSemaphore(20u);
    EXPECT_EQ(0ull, directSubmission->pagingFenceValueToWait);
    EXPECT_EQ(0ull, blitterDirectSubmission->pagingFenceValueToWait);

    ultCsr->directSubmissionAvailable = true;
    ultCsr->blitterDirectSubmissionAvailable = true;

    ultCsr->unblockPagingFenceSemaphore(20u);
    EXPECT_EQ(20u, directSubmission->pagingFenceValueToWait);
    EXPECT_EQ(0ull, blitterDirectSubmission->pagingFenceValueToWait);

    directSubmission->pagingFenceValueToWait = 0u;

    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    ultCsr->setupContext(*osContext);
    ultCsr->unblockPagingFenceSemaphore(20u);
    EXPECT_EQ(0ull, directSubmission->pagingFenceValueToWait);
    EXPECT_EQ(20u, blitterDirectSubmission->pagingFenceValueToWait);
}
