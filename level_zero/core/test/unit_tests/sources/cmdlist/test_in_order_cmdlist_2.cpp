/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/source/utilities/staging_buffer_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/api/internal/l0_graph.h"
#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_graph.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {
namespace ult {
using CopyOffloadInOrderTests = CopyOffloadInOrderFixture;

HWTEST2_F(CopyOffloadInOrderTests, givenImmCmdListWhenGraphIsInstantiatedThenGraphCmdListsInheritCopyOffload, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);
    debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(0);

    const bool copyOffloadSupported = !device->getGfxCoreHelper().crossEngineCacheFlushRequired();

    for (bool withOffload : {false, true}) {
        const bool expectedOffload = withOffload && copyOffloadSupported;
        ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER | (withOffload ? ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT : 0);
        queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        ze_command_list_handle_t cmdListHandle = nullptr;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &cmdListHandle));
        auto immCmdList = CommandList::fromHandle(cmdListHandle);
        ASSERT_EQ(expectedOffload, immCmdList->isCopyOffloadEnabled());
        EXPECT_EQ(expectedOffload, NEO::isValueSet(immCmdList->getCmdListFlags(), ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT));

        {
            MockGraph graph(context, true, false);
            ze_graph_handle_t graphHandle = graph.toHandle();
            ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, nullptr));
            ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, &copyData1, &copyData2, sizeof(copyData1), nullptr, 0, nullptr));
            ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(cmdListHandle, &graphHandle, nullptr));

            GraphInstatiateSettings settings{nullptr, false};
            MockExecutableGraph execGraph;
            ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(graph, settings));
            ASSERT_NE(0u, execGraph.myCommandLists.size());
            for (auto &graphCmdList : execGraph.myCommandLists) {
                EXPECT_FALSE(graphCmdList->isImmediateType());
                EXPECT_EQ(expectedOffload, graphCmdList->isCopyOffloadEnabled());
            }
        }

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(cmdListHandle));
    }
}

template <typename FamilyType>
static void verifyInOrderCounterUpdate(WhiteBox<L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> &list, NEO::GmmHelper &gmmHelper, size_t offset, bool atomicSignalling) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto *cmdStream = list.getCmdContainer().getCommandStream();
    auto inOrderInfo = list.getInOrderExecInfo();
    EXPECT_EQ(1u, inOrderInfo->getCounterValue());

    GenCmdList commands;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
    const auto counterAddress = inOrderInfo->getBaseDeviceAddress() + inOrderInfo->getAllocationOffset();
    auto atomics = findAll<MI_ATOMIC *>(commands.begin(), commands.end());
    ASSERT_EQ(atomicSignalling ? 1u : 0u, atomics.size());
    if (atomicSignalling) {
        auto *atomic = genCmdCast<MI_ATOMIC *>(*atomics[0]);
        EXPECT_EQ(counterAddress, gmmHelper.canonize(atomic->getMemoryAddress()));
        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, atomic->getAtomicOpcode());
        EXPECT_EQ(MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD, atomic->getDataSize());
        EXPECT_EQ(1u, atomic->getOperand1DataDword0());
        EXPECT_EQ(0u, atomic->getOperand1DataDword1());
    } else {
        auto stores = findAll<MI_STORE_DATA_IMM *>(commands.begin(), commands.end());
        ASSERT_EQ(inOrderInfo->isHostStorageDuplicated() ? 2u : 1u, stores.size());
        auto *store = genCmdCast<MI_STORE_DATA_IMM *>(*stores[0]);
        EXPECT_EQ(counterAddress, gmmHelper.canonize(store->getAddress()));
        EXPECT_EQ(list.isQwordInOrderCounter(), store->getStoreQword());
        EXPECT_EQ(1u, store->getDataDword0());
        EXPECT_EQ(0u, store->getDataDword1());
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonZeroSizeNonVmmCopyWithSingleStreamOffloadWhenAppendingThenInOrderCounterIsUpdated, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);
    debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(2);
    auto source = allocDeviceMem(MemoryConstants::pageSize64k);
    auto destination = allocDeviceMem(MemoryConstants::pageSize64k);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);
    for (bool atomicSignalling : {false, true}) {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(atomicSignalling);
        auto list = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
        list->useAdditionalBlitProperties = false;
        ASSERT_TRUE(list->isInOrderExecutionEnabled());
        ASSERT_TRUE(list->isCopyOffloadEnabled());
        ASSERT_NE(CopyOffloadModes::dualStream, list->getCopyOffloadModeForOperation(true));
        CmdListMemoryCopyParams copyParams = {};
        auto *cmdStream = list->getCmdContainer().getCommandStream();
        const auto offset = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, list->appendMemoryCopy(destination, source, 1, nullptr, 0, nullptr, copyParams));
        EXPECT_TRUE(copyParams.copyOffloadAllowed);
        verifyInOrderCounterUpdate<FamilyType>(*list, *device->getNEODevice()->getGmmHelper(), offset, atomicSignalling);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(source));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(destination));
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonZeroSizeCopyRegionWithSingleStreamOffloadWhenAppendingThenInOrderCounterIsUpdated, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);
    debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(2);
    auto source = allocDeviceMem(MemoryConstants::pageSize64k);
    auto destination = allocDeviceMem(MemoryConstants::pageSize64k);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);
    ze_copy_region_t region = {0, 0, 0, 16, 2, 1};
    for (bool atomicSignalling : {false, true}) {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(atomicSignalling);
        auto list = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
        list->useAdditionalBlitProperties = false;
        ASSERT_TRUE(list->isInOrderExecutionEnabled());
        ASSERT_TRUE(list->isCopyOffloadEnabled());
        ASSERT_EQ(nonDualStreamMode, list->getCopyOffloadModeForOperation(true));
        CmdListMemoryCopyParams copyParams = {};
        auto *cmdStream = list->getCmdContainer().getCommandStream();
        const auto offset = cmdStream->getUsed();
        ASSERT_EQ(ZE_RESULT_SUCCESS, list->appendMemoryCopyRegion(destination, &region, 16, 32, source, &region, 16, 32, nullptr, 0, nullptr, copyParams));
        ASSERT_TRUE(copyParams.copyOffloadAllowed);
        verifyInOrderCounterUpdate<FamilyType>(*list, *device->getNEODevice()->getGmmHelper(), offset, atomicSignalling);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(source));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(destination));
}

HWTEST_F(CopyOffloadInOrderTests, givenVariousParametersWhenAskingForCopyOffloadThenReturnCorrectValue) {
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    MockGraphicsAllocation hostAlloc;
    hostAlloc.overrideMemoryPool(NEO::MemoryPool::system64KBPages);

    MockGraphicsAllocation deviceAlloc;
    deviceAlloc.overrideMemoryPool(NEO::MemoryPool::localMemory);

    std::array<GraphicsAllocation *, 3> allocations = {&hostAlloc, &deviceAlloc, nullptr};

    for (auto srcAlloc : allocations) {
        for (auto dstAlloc : allocations) {
            for (bool remoteCopy : {true, false}) {
                for (bool localToLocalAllowed : {true, false}) {
                    bool expected = false;

                    if (remoteCopy || srcAlloc == nullptr || dstAlloc == nullptr) {
                        expected = true;
                    } else if (srcAlloc->isAllocatedInLocalMemoryPool() && dstAlloc->isAllocatedInLocalMemoryPool()) {
                        expected = localToLocalAllowed;
                    } else {
                        expected = true;
                    }

                    EXPECT_EQ(expected, immCmdList->isCopyOffloadAllowed(srcAlloc, dstAlloc, remoteCopy, localToLocalAllowed, 0));
                }
            }
        }
    }
}

HWTEST_F(CopyOffloadInOrderTests, givenLocalToLocalAllowedWhenCheckingCopyOffloadThenBcsAllowed) {
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    MockGraphicsAllocation srcAlloc;
    srcAlloc.overrideMemoryPool(NEO::MemoryPool::localMemory);

    MockGraphicsAllocation dstAlloc;
    dstAlloc.overrideMemoryPool(NEO::MemoryPool::localMemory);

    EXPECT_TRUE(immCmdList->isCopyOffloadAllowed(&srcAlloc, &dstAlloc, false, true, 0));
}

HWTEST_F(CopyOffloadInOrderTests, givenLocalToLocalNotAllowedWhenCheckingCopyOffloadThenBcsBlocked) {
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    MockGraphicsAllocation srcAlloc;
    srcAlloc.overrideMemoryPool(NEO::MemoryPool::localMemory);

    MockGraphicsAllocation dstAlloc;
    dstAlloc.overrideMemoryPool(NEO::MemoryPool::localMemory);

    EXPECT_FALSE(immCmdList->isCopyOffloadAllowed(&srcAlloc, &dstAlloc, false, false, 0));
}

HWTEST_F(CopyOffloadInOrderTests, givenRemoteCopyWhenCheckingCopyOffloadThenLocalToLocalFlagIgnored) {
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    MockGraphicsAllocation srcAlloc;
    srcAlloc.overrideMemoryPool(NEO::MemoryPool::localMemory);

    MockGraphicsAllocation dstAlloc;
    dstAlloc.overrideMemoryPool(NEO::MemoryPool::localMemory);

    EXPECT_TRUE(immCmdList->isCopyOffloadAllowed(&srcAlloc, &dstAlloc, true, false, 0));
    EXPECT_TRUE(immCmdList->isCopyOffloadAllowed(&srcAlloc, &dstAlloc, true, true, 0));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CopyOffloadInOrderTests, givenCmdsChainingWhenDispatchingCopyOffloadThenDontSkipImplictDependency) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    if (immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    auto offset = cmdStream->getUsed();

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16 * MemoryConstants::kiloByte, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto findSemaphores = [&](size_t expectedNumSemaphores) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        if (cmds.size() != expectedNumSemaphores) {
            EXPECT_EQ(expectedNumSemaphores + 1, cmds.size());
        }
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    findSemaphores(1); // chaining
    EXPECT_TRUE(immCmdList->latestOperationRequiredNonWalkerInOrderCmdsChaining || !immCmdList->isHeaplessModeEnabled());

    offset = cmdStream->getUsed();
    uint32_t copyData = 0;

    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);

    findSemaphores(1); // implicit dependency

    context->freeMem(alloc);
}

HWTEST_F(CopyOffloadInOrderTests, givenCopyOffloadWhenAskingForOperationModeThenReturnCorrectValue) {
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    EXPECT_NE(CopyOffloadModes::disabled, immCmdList->copyOffloadMode);

    EXPECT_TRUE(immCmdList->isCopyOffloadEnabled());
    EXPECT_EQ(immCmdList->copyOffloadMode, immCmdList->getCopyOffloadModeForOperation(true));
    EXPECT_EQ(CopyOffloadModes::disabled, immCmdList->getCopyOffloadModeForOperation(false));

    if (immCmdList->copyOffloadMode == CopyOffloadModes::dualStream) {
        EXPECT_EQ(immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex], immCmdList->getCmdQImmediate(immCmdList->copyOffloadMode));
    } else {
        EXPECT_EQ(immCmdList->cmdQImmediate, immCmdList->getCmdQImmediate(immCmdList->copyOffloadMode));
    }

    immCmdList->copyOffloadMode = CopyOffloadModes::disabled;
    EXPECT_FALSE(immCmdList->isCopyOffloadEnabled());
    EXPECT_EQ(CopyOffloadModes::disabled, immCmdList->getCopyOffloadModeForOperation(true));
    EXPECT_EQ(CopyOffloadModes::disabled, immCmdList->getCopyOffloadModeForOperation(false));

    immCmdList->copyOffloadMode = nonDualStreamMode;
    EXPECT_TRUE(immCmdList->isCopyOffloadEnabled());
    EXPECT_EQ(nonDualStreamMode, immCmdList->getCopyOffloadModeForOperation(true));
    EXPECT_EQ(CopyOffloadModes::disabled, immCmdList->getCopyOffloadModeForOperation(false));
    EXPECT_EQ(immCmdList->cmdQImmediate, immCmdList->getCmdQImmediate(immCmdList->copyOffloadMode));
}

HWTEST_F(CopyOffloadInOrderTests, givenDebugFlagSetWhenCreatingCmdListThenOverrideOffloadMode) {
    debugManager.flags.OverrideCopyOffloadMode.set(1234);
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    EXPECT_NE(0x1234u, immCmdList->copyOffloadMode);
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonDualStreamModeWhenSubmittedThenUseDefaultQueue, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = true;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    EXPECT_TRUE(immCmdList->isCopyOffloadEnabled());
    EXPECT_EQ(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto taskCount = csr->taskCount.load();

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle, 0, nullptr, copyParams);
    EXPECT_EQ(taskCount + 1, csr->taskCount.load());

    EXPECT_FALSE(immCmdList->latestFlushIsDualCopyOffload);
}

HWTEST2_F(CopyOffloadInOrderTests, givenStagingCopyEnabledWhenCopyCalledThenOffloadOnlyIfPreferred, IsAtLeastXeCore) {
    using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;

    struct MyCmdList : public BaseClass {
        ze_result_t appendStagingMemoryCopy(const CpuMemCopyInfo &cpuMemCopyInfo, ze_event_handle_t hSignalEvent, CmdListMemoryCopyParams &memoryCopyParams) override {
            stagingCopyCalled = true;
            return BaseClass::appendStagingMemoryCopy(cpuMemCopyInfo, hSignalEvent, memoryCopyParams);
        }

        bool stagingCopyCalled = false;
    };

    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(-1);

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyCmdList>(true);

    immCmdList->useAdditionalBlitProperties = false;

    EXPECT_TRUE(immCmdList->isCopyOffloadEnabled());
    EXPECT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto mainTaskCount = mainCsr->taskCount.load();
    auto copyTaskCount = copyQueueCsr->taskCount.load();

    auto usmDevice = allocDeviceMem(1);
    immCmdList->appendMemoryCopy(usmDevice, &copyData2, 1, nullptr, 0, nullptr, copyParams);

    EXPECT_TRUE(immCmdList->stagingCopyCalled);

    if (device->getProductHelper().blitEnqueuePreferred(false)) {
        EXPECT_EQ(mainTaskCount, mainCsr->taskCount.load());
        EXPECT_EQ(copyTaskCount + 1, copyQueueCsr->taskCount.load());
    } else {
        EXPECT_EQ(mainTaskCount + 1, mainCsr->taskCount.load());
        EXPECT_EQ(copyTaskCount, copyQueueCsr->taskCount.load());
    }

    context->freeMem(usmDevice);
}

HWTEST2_F(CopyOffloadInOrderTests, givenDualStreamCopyOffloadWhenHostSynchronizeThenWaitOnBothMainAndCopyCsrTaskCounts, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->forceDisableInOrderWaits();

    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    mainCsr->callBaseWaitForCompletionWithTimeout = false;
    copyCsr->callBaseWaitForCompletionWithTimeout = false;
    mainCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    copyCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;

    immCmdList->cmdQImmediate->setTaskCount(10u);
    immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->setTaskCount(20u);
    immCmdList->latestFlushIsDualCopyOffload = true;

    mainCsr->waitForCompletionWithTimeoutTaskCountCalled.store(0u);
    copyCsr->waitForCompletionWithTimeoutTaskCountCalled.store(0u);
    auto initialFlushedTaskCountMainCsr = mainCsr->peekLatestFlushedTaskCount();

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, false));

    EXPECT_EQ(1u, mainCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(1u, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    EXPECT_EQ(10u, mainCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(20u, copyCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    EXPECT_EQ(initialFlushedTaskCountMainCsr + 1, mainCsr->peekLatestFlushedTaskCount());
}

HWTEST2_F(CopyOffloadInOrderTests, givenDualStreamCopyOffloadWhenAppendingCopyThenInOrderCounterIsResidentOnCopyCsr, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

    if (!device->getProductHelper().blitEnqueuePreferred(false)) {
        GTEST_SKIP();
    }

    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));
    copyCsr->storeMakeResidentAllocations = true;

    auto copyTaskCountBefore = copyCsr->taskCount.load();

    auto usmDevice = allocDeviceMem(1);
    immCmdList->appendMemoryCopy(usmDevice, &copyData2, 1, nullptr, 0, nullptr, copyParams);

    ASSERT_LT(copyTaskCountBefore, copyCsr->taskCount.load());

    auto deviceCounterAllocation = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
    ASSERT_NE(nullptr, deviceCounterAllocation);
    EXPECT_NE(0u, copyCsr->makeResidentAllocations[deviceCounterAllocation]);

    if (auto *hostCounterAllocation = immCmdList->inOrderExecInfo->getHostCounterAllocation()) {
        EXPECT_NE(0u, copyCsr->makeResidentAllocations[hostCounterAllocation]);
    }

    context->freeMem(usmDevice);
}

HWTEST2_F(CopyOffloadInOrderTests, givenPendingCopyOffloadWhenMainCsrCompletesThenCommandBufferIsNotReused, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(0);
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));
    const auto mainContextId = mainCsr->getOsContext().getContextId();
    const auto copyContextId = copyCsr->getOsContext().getContextId();
    ASSERT_NE(mainContextId, copyContextId);

    *mainCsr->getTagAddress() = 0;
    *mainCsr->getUcTagAddress() = 0;
    *copyCsr->getTagAddress() = 0;
    *copyCsr->getUcTagAddress() = 0;

    auto &container = immCmdList->commandContainer;
    auto commandStream = container.getCommandStream();
    auto originalAllocation = commandStream->getGraphicsAllocation();
    auto usmDevice = allocDeviceMem(sizeof(copyData2));
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendMemoryCopy(usmDevice, &copyData2, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    ASSERT_TRUE(originalAllocation->isUsedByOsContext(copyContextId));
    const auto copyTaskCount = originalAllocation->getTaskCount(copyContextId);
    ASSERT_GT(copyTaskCount, *copyCsr->getTagAddress());
    ASSERT_GT(copyTaskCount, *copyCsr->getUcTagAddress());

    commandStream->getSpace(commandStream->getAvailableSpace());
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendMemoryCopy(usmDevice, &copyData2, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    ASSERT_NE(originalAllocation, container.getCommandStream()->getGraphicsAllocation());
    ASSERT_TRUE(originalAllocation->isUsedByOsContext(mainContextId));
    ASSERT_EQ(copyTaskCount, originalAllocation->getTaskCount(copyContextId));
    EXPECT_EQ(nullptr, container.reuseExistingCmdBuffer());

    const auto mainTaskCount = originalAllocation->getTaskCount(mainContextId);
    ASSERT_GT(mainTaskCount, 0u);
    *mainCsr->getTagAddress() = mainTaskCount;
    *mainCsr->getUcTagAddress() = mainTaskCount;

    commandStream = container.getCommandStream();
    commandStream->getSpace(commandStream->getAvailableSpace());
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendMemoryCopy(usmDevice, &copyData2, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    EXPECT_NE(originalAllocation, container.getCommandStream()->getGraphicsAllocation());
    EXPECT_GT(copyTaskCount, *copyCsr->getTagAddress());
    EXPECT_GT(copyTaskCount, *copyCsr->getUcTagAddress());

    *mainCsr->getTagAddress() = mainCsr->peekTaskCount();
    *copyCsr->getTagAddress() = copyCsr->peekTaskCount();
    commandStream = container.getCommandStream();
    commandStream->getSpace(commandStream->getAvailableSpace());
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendMemoryCopy(usmDevice, &copyData2, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    EXPECT_EQ(originalAllocation, container.getCommandStream()->getGraphicsAllocation());

    *mainCsr->getTagAddress() = mainCsr->peekTaskCount();
    *mainCsr->getUcTagAddress() = mainCsr->peekTaskCount();
    *copyCsr->getTagAddress() = copyCsr->peekTaskCount();
    *copyCsr->getUcTagAddress() = copyCsr->peekTaskCount();
    context->freeMem(usmDevice);
}

HWTEST2_F(CopyOffloadInOrderTests, givenAsyncCopyInRetiredCommandBufferWhenNextCopyIsFlushedThenCopyTagIsUpdatedAndBufferIsReused, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(0);
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);

    auto immediate = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immediate->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immediate->getCsr(true));
    VariableBackup<volatile TagAddressType> mainTag(mainCsr->getTagAddress(), 0);
    VariableBackup<volatile TagAddressType> mainUcTag(mainCsr->getUcTagAddress(), 0);
    VariableBackup<volatile TagAddressType> copyTag(copyCsr->getTagAddress(), 0);
    VariableBackup<volatile TagAddressType> copyUcTag(copyCsr->getUcTagAddress(), 0);

    const auto src = allocHostMem(sizeof(copyData2));
    const auto dst = allocDeviceMem(sizeof(copyData2));
    auto &container = immediate->commandContainer;
    const auto stream = container.getCommandStream();
    const auto flushedTaskCount = copyCsr->peekLatestFlushedTaskCount();
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    ASSERT_TRUE(stream->getGraphicsAllocation()->isUsedByOsContext(copyCsr->getOsContext().getContextId()));
    ASSERT_FALSE(copyParams.taskCountUpdateRequired);
    EXPECT_EQ(flushedTaskCount, copyCsr->peekLatestFlushedTaskCount());

    const auto retired = stream->getGraphicsAllocation();
    stream->getSpace(stream->getAvailableSpace());
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));

    EXPECT_NE(retired, stream->getGraphicsAllocation());
    EXPECT_FALSE(immediate->copyOffloadTagUpdateRequired[DualStreamCopyOffload::copyOffloadReadIndex]);
    EXPECT_EQ(copyCsr->peekTaskCount(), copyCsr->peekLatestFlushedTaskCount());

    *mainCsr->getTagAddress() = mainCsr->peekTaskCount();
    EXPECT_EQ(nullptr, container.reuseExistingCmdBuffer());
    *copyCsr->getTagAddress() = copyCsr->peekTaskCount();
    EXPECT_EQ(retired, container.reuseExistingCmdBuffer());

    context->freeMem(src);
    context->freeMem(dst);
}

HWTEST2_F(CopyOffloadInOrderTests, givenAsyncCopyInRetiredCommandBufferWhenFlushOfNextCopyFailsThenRetriedFlushUpdatesCopyTag, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);

    auto immediate = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immediate->getCsr(true));

    const auto src = allocHostMem(sizeof(copyData2));
    const auto dst = allocDeviceMem(sizeof(copyData2));
    const auto stream = immediate->commandContainer.getCommandStream();
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    ASSERT_TRUE(stream->getGraphicsAllocation()->isUsedByOsContext(copyCsr->getOsContext().getContextId()));

    stream->getSpace(stream->getAvailableSpace());
    copyCsr->flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    copyCsr->flushReturnValue.reset();
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));

    EXPECT_EQ(copyCsr->peekTaskCount(), copyCsr->peekLatestFlushedTaskCount());

    context->freeMem(src);
    context->freeMem(dst);
}

HWTEST2_F(CopyOffloadInOrderTests, givenUcResourceTagUpdateRequiredWhenCopyFlushFailsThenRetriedFlushProgramsUcTagUpdate, IsAtLeastXeCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);

    auto immediate = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immediate->getCsr(true));

    const auto src = allocHostMem(sizeof(copyData2));
    const auto dst = allocDeviceMem(sizeof(copyData2));
    const auto stream = immediate->commandContainer.getCommandStream();
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));

    const auto submissionStart = stream->getUsed();
    copyCsr->ucResourceRequiresTagUpdate = true;
    copyCsr->flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    copyCsr->flushReturnValue.reset();
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(stream->getCpuBase(), submissionStart), stream->getUsed() - submissionStart));
    auto flushes = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    const auto ucTagAddress = copyCsr->getUcTagGPUAddress();
    EXPECT_EQ(1, std::count_if(flushes.begin(), flushes.end(), [ucTagAddress](auto it) { return genCmdCast<MI_FLUSH_DW *>(*it)->getDestinationAddress() == ucTagAddress; }));

    context->freeMem(src);
    context->freeMem(dst);
}

HWTEST2_F(CopyOffloadInOrderTests, givenRetiredCommandBufferNotUsedByCopyEngineWhenNextCopyIsFlushedThenCopyTagIsNotUpdated, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);

    auto immediate = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immediate->getCsr(true));

    const auto src = allocHostMem(sizeof(copyData2));
    const auto dst = allocDeviceMem(sizeof(copyData2));
    const auto stream = immediate->commandContainer.getCommandStream();
    const auto retired = stream->getGraphicsAllocation();
    ASSERT_FALSE(retired->isUsedByOsContext(copyCsr->getOsContext().getContextId()));
    const auto flushedTaskCount = copyCsr->peekLatestFlushedTaskCount();

    stream->getSpace(stream->getAvailableSpace());
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    ASSERT_FALSE(copyParams.taskCountUpdateRequired);

    EXPECT_NE(retired, stream->getGraphicsAllocation());
    EXPECT_FALSE(immediate->copyOffloadTagUpdateRequired[DualStreamCopyOffload::copyOffloadReadIndex]);
    EXPECT_EQ(flushedTaskCount, copyCsr->peekLatestFlushedTaskCount());

    context->freeMem(src);
    context->freeMem(dst);
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyTagAlreadyFlushedForRetiredCommandBufferWhenNextCopyIsFlushedThenCopyTagIsNotUpdated, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);

    auto immediate = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immediate->getCsr(true));

    const auto src = allocHostMem(sizeof(copyData2));
    const auto dst = allocDeviceMem(sizeof(copyData2));
    const auto stream = immediate->commandContainer.getCommandStream();
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));
    ASSERT_FALSE(copyParams.taskCountUpdateRequired);
    const auto retired = stream->getGraphicsAllocation();
    ASSERT_TRUE(retired->isUsedByOsContext(copyCsr->getOsContext().getContextId()));
    copyCsr->setLatestFlushedTaskCount(copyCsr->peekTaskCount());
    const auto flushedTaskCount = copyCsr->peekLatestFlushedTaskCount();

    stream->getSpace(stream->getAvailableSpace());
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediate->appendMemoryCopy(dst, src, sizeof(copyData2), nullptr, 0, nullptr, copyParams));

    EXPECT_NE(retired, stream->getGraphicsAllocation());
    EXPECT_FALSE(immediate->copyOffloadTagUpdateRequired[DualStreamCopyOffload::copyOffloadReadIndex]);
    EXPECT_EQ(flushedTaskCount, copyCsr->peekLatestFlushedTaskCount());

    context->freeMem(src);
    context->freeMem(dst);
}

HWTEST2_F(CopyOffloadInOrderTests, givenDualStreamCopyOffloadWhenCopyEngineNotReadyThenDoNotWaitOnCompute, IsAtLeastXeCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->forceDisableInOrderWaits();

    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    mainCsr->callBaseWaitForCompletionWithTimeout = false;
    copyCsr->callBaseWaitForCompletionWithTimeout = false;
    mainCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    copyCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::notReady;

    immCmdList->cmdQImmediate->setTaskCount(10u);
    immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->setTaskCount(20u);
    immCmdList->latestFlushIsDualCopyOffload = true;

    mainCsr->waitForCompletionWithTimeoutTaskCountCalled.store(0u);
    copyCsr->waitForCompletionWithTimeoutTaskCountCalled.store(0u);

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(1000, false));

    EXPECT_EQ(0u, mainCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(1u, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    EXPECT_EQ(0u, mainCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(20u, copyCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderImmediateCmdListWithCopyOffloadHintWhenCreatingThenEnableCopyOffload, IsAtLeastXe3pCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

    ASSERT_TRUE(device->getL0GfxCoreHelper().isCopyOffloadForOutOfOrderImmediateCmdListSupported());

    ze_command_list_handle_t cmdListHandle;

    zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadDesc = {ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES};
    copyOffloadDesc.copyOffloadEnabled = true;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.pNext = &copyOffloadDesc;

    auto crossEngineCacheFlushRequired = device->getGfxCoreHelper().crossEngineCacheFlushRequired();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
    auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));

    EXPECT_FALSE(cmdList->isInOrderExecutionEnabled());

    if (crossEngineCacheFlushRequired) {
        EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    } else if (device->getProductHelper().useAdditionalBlitProperties()) {
        EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    } else {
        EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

        auto copyQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        EXPECT_TRUE(copyQueue->peekIsCopyOnlyCommandQueue());
        EXPECT_TRUE(NEO::EngineHelpers::isBcs(copyQueue->getCsr()->getOsContext().getEngineType()));
    }

    zeCommandListDestroy(cmdListHandle);
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenAppendingCopyThenUseCopyQueue, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    EXPECT_FALSE(immCmdList->isInOrderExecutionEnabled());

    if (!device->getProductHelper().blitEnqueuePreferred(false)) {
        GTEST_SKIP();
    }

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto mainTaskCount = mainCsr->taskCount.load();
    auto copyTaskCount = copyCsr->taskCount.load();

    auto usmDevice = allocDeviceMem(1);
    immCmdList->appendMemoryCopy(usmDevice, &copyData2, 1, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(mainTaskCount, mainCsr->taskCount.load());
    EXPECT_EQ(copyTaskCount + 1, copyCsr->taskCount.load());

    context->freeMem(usmDevice);
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenHostSynchronizeThenWaitOnBothCsrTaskCountsEvenWhenLastFlushWasCompute, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    mainCsr->callBaseWaitForCompletionWithTimeout = false;
    copyCsr->callBaseWaitForCompletionWithTimeout = false;
    mainCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    copyCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;

    immCmdList->cmdQImmediate->setTaskCount(10u);
    immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->setTaskCount(20u);
    immCmdList->latestFlushIsDualCopyOffload = false;

    mainCsr->waitForCompletionWithTimeoutTaskCountCalled.store(0u);
    copyCsr->waitForCompletionWithTimeoutTaskCountCalled.store(0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, false));

    EXPECT_EQ(1u, mainCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(1u, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    EXPECT_EQ(10u, mainCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(20u, copyCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenSynchronizingAgainThenOnlyUnchangedTaskCountsAreSkipped, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));
    mainCsr->callBaseWaitForCompletionWithTimeout = false;
    copyCsr->callBaseWaitForCompletionWithTimeout = false;
    mainCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    copyCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    immCmdList->cmdQImmediate->setTaskCount(10u);
    immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->setTaskCount(20u);
    immCmdList->latestFlushIsDualCopyOffload = false;

    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0));
    const auto mainWaitsBefore = mainCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    const auto copyWaitsBefore = copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0));
    EXPECT_EQ(mainWaitsBefore, mainCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(copyWaitsBefore, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    immCmdList->cmdQImmediate->setTaskCount(11u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0));
    EXPECT_EQ(mainWaitsBefore + 1, mainCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(copyWaitsBefore + 1, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(11u, mainCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->setTaskCount(21u);
    mainCsr->onWaitForCompletionWithTimeout = [&] {
        immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->setTaskCount(22u);
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0));
    mainCsr->onWaitForCompletionWithTimeout = nullptr;
    EXPECT_EQ(mainWaitsBefore + 2, mainCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(copyWaitsBefore + 2, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(21u, copyCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    copyCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::notReady;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0));
    EXPECT_EQ(copyWaitsBefore + 3, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(22u, copyCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenRepeatingBarriersThenNewCopyWorkRequiresAnotherBarrier, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());
    if (!device->getProductHelper().blitEnqueuePreferred(false)) {
        GTEST_SKIP();
    }

    immCmdList->cmdQImmediate->setTaskCount(1);

    CmdListWaitEventParameters waitEventsParameters{};
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters, signalEventParameters));
    const auto mainTaskCount = immCmdList->cmdQImmediate->getTaskCount();
    const auto copyTaskCount = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount();
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters, signalEventParameters));
    EXPECT_EQ(mainTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(copyTaskCount, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount());

    auto usmDevice = allocDeviceMem(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendMemoryCopy(usmDevice, &copyData2, 1, nullptr, 0, nullptr, copyParams));
    EXPECT_EQ(mainTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_GT(immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount(), copyTaskCount);
    const auto copyTaskCountAfterCopy = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount();

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters, signalEventParameters));
    EXPECT_GT(immCmdList->cmdQImmediate->getTaskCount(), mainTaskCount);
    EXPECT_GT(immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount(), copyTaskCountAfterCopy);
    context->freeMem(usmDevice);
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenAppendingBarrierWithSignalEventThenOnlySynchronizedWorkAllowsHostSignal, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());
    if (!device->getProductHelper().blitEnqueuePreferred(false)) {
        GTEST_SKIP();
    }

    auto eventPool = createEvents<FamilyType>(2, false);
    for (auto &event : events) {
        event->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
        ASSERT_FALSE(event->isCounterBased());
    }
    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));
    mainCsr->callBaseWaitForCompletionWithTimeout = false;
    copyCsr->callBaseWaitForCompletionWithTimeout = false;
    mainCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    copyCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;

    auto usmDevice = allocDeviceMem(1);
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendMemoryCopy(usmDevice, &copyData2, 1, nullptr, 0, nullptr, copyParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0));
    const auto mainTaskCount = immCmdList->cmdQImmediate->getTaskCount();
    const auto copyTaskCount = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount();
    ASSERT_GT(copyTaskCount, 0u);

    CmdListWaitEventParameters waitEventsParameters{};
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(events[0]->toHandle(), 0, nullptr, waitEventsParameters, signalEventParameters));
    EXPECT_EQ(mainTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(copyTaskCount, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount());
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->queryStatus(0));

    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendMemoryCopy(usmDevice, &copyData2, 1, nullptr, 0, nullptr, copyParams));
    EXPECT_EQ(mainTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    const auto copyTaskCountAfterCopy = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount();
    ASSERT_GT(copyTaskCountAfterCopy, copyTaskCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(events[1]->toHandle(), 0, nullptr, waitEventsParameters, signalEventParameters));
    EXPECT_GT(immCmdList->cmdQImmediate->getTaskCount(), mainTaskCount);
    EXPECT_GT(immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount(), copyTaskCountAfterCopy);
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[1]->queryStatus(0));
    context->freeMem(usmDevice);
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenAppendBarrierThenBothEnginesWaitForEachOther, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    if (!device->getProductHelper().blitEnqueuePreferred(false)) {
        GTEST_SKIP();
    }

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto usmDevice = allocDeviceMem(1);
    immCmdList->appendMemoryCopy(usmDevice, &copyData2, 1, nullptr, 0, nullptr, copyParams);

    const auto mainTaskCount = mainCsr->taskCount.load();
    const auto copyTaskCount = copyCsr->taskCount.load();
    const auto copyOffloadQueueTaskCount = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount();
    ASSERT_GT(copyTaskCount, 0u);
    ASSERT_GT(copyOffloadQueueTaskCount, 0u);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    const auto offset = cmdStream->getUsed();
    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters, signalEventParameters));

    // barrier is dispatched to both engines - compute first, copy offload second
    EXPECT_GT(mainCsr->taskCount.load(), mainTaskCount);
    EXPECT_GT(copyCsr->taskCount.load(), copyTaskCount);

    GenCmdList cmds;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmds, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(cmds.begin(), cmds.end());

    const auto mainTagAddress = mainCsr->getTagAllocation()->getGpuAddress();
    const auto copyTagAddress = copyCsr->getTagAllocation()->getGpuAddress();

    int32_t waitOnCopyEngineIndex = -1;
    int32_t waitOnComputeEngineIndex = -1;
    uint64_t waitOnCopyEngineData = 0;
    uint64_t waitOnComputeEngineData = 0;
    int32_t semaphoreIndex = 0;

    for (auto &semaphore : semaphores) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        const auto semaphoreAddress = NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd);

        if ((semaphoreAddress == copyTagAddress) && (waitOnCopyEngineIndex == -1)) {
            waitOnCopyEngineIndex = semaphoreIndex;
            waitOnCopyEngineData = NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semaphoreCmd);
        }
        if ((semaphoreAddress == mainTagAddress) && (waitOnComputeEngineIndex == -1)) {
            waitOnComputeEngineIndex = semaphoreIndex;
            waitOnComputeEngineData = NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semaphoreCmd);
        }
        semaphoreIndex++;
    }

    ASSERT_NE(-1, waitOnCopyEngineIndex);
    ASSERT_NE(-1, waitOnComputeEngineIndex);
    EXPECT_LT(waitOnCopyEngineIndex, waitOnComputeEngineIndex);

    // compute engine waits for the copy offload work submitted before the barrier
    EXPECT_EQ(static_cast<uint64_t>(copyOffloadQueueTaskCount), waitOnCopyEngineData);
    // copy offload engine waits for the barrier submitted to the compute engine
    EXPECT_EQ(static_cast<uint64_t>(immCmdList->cmdQImmediate->getTaskCount()), waitOnComputeEngineData);

    context->freeMem(usmDevice);
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWithoutPriorCopyWhenAppendBarrierThenOnlyCopyEngineWaitsForComputeEngine, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    ASSERT_EQ(0u, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount());
    immCmdList->cmdQImmediate->setTaskCount(1);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    const auto offset = cmdStream->getUsed();
    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters, signalEventParameters));

    GenCmdList cmds;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmds, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(cmds.begin(), cmds.end());

    const auto mainTagAddress = mainCsr->getTagAllocation()->getGpuAddress();
    const auto copyTagAddress = copyCsr->getTagAllocation()->getGpuAddress();

    bool waitsOnComputeEngine = false;
    for (auto &semaphore : semaphores) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        const auto semaphoreAddress = NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd);

        // nothing was submitted to the copy offload engine, so there is nothing to wait for
        EXPECT_NE(copyTagAddress, semaphoreAddress);

        if (semaphoreAddress == mainTagAddress) {
            waitsOnComputeEngine = true;
            EXPECT_EQ(static_cast<uint64_t>(immCmdList->cmdQImmediate->getTaskCount()), NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semaphoreCmd));
        }
    }

    EXPECT_TRUE(waitsOnComputeEngine);
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenAppendBarrierThenSubmitToComputeAndCopyOffloadQueues, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    immCmdList->cmdQImmediate->setTaskCount(1);
    const auto mainQueueTaskCount = immCmdList->cmdQImmediate->getTaskCount();
    const auto copyOffloadQueueTaskCount = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount();

    immCmdList->latestFlushIsDualCopyOffload = false;

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters, signalEventParameters));

    EXPECT_GT(immCmdList->cmdQImmediate->getTaskCount(), mainQueueTaskCount);
    EXPECT_GT(immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount(), copyOffloadQueueTaskCount);

    // copy offload queue is flushed as the last one
    EXPECT_TRUE(immCmdList->latestFlushIsDualCopyOffload);
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenAppendBarrierWithSignalEventThenEventIsSignaledFromCopyOffloadQueue, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto mainCsr = immCmdList->getCsr(false);
    auto copyCsr = immCmdList->getCsr(true);

    immCmdList->cmdQImmediate->setTaskCount(1);

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto eventHandle = event->toHandle();

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParameters, signalEventParameters));

    TaskCountType cleanupTaskCount = 0;
    EXPECT_FALSE(event->getCleanupTaskCount(mainCsr, cleanupTaskCount));
    ASSERT_TRUE(event->getCleanupTaskCount(copyCsr, cleanupTaskCount));
    EXPECT_EQ(immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount(), cleanupTaskCount);

    EXPECT_EQ(mainCsr, event->getCsrForCacheFlush());
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadAndNoWorkSinceLastBarrierWhenAppendBarrierWithTimestampEventThenSubmitOnlyToComputeQueue, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto mainCsr = immCmdList->getCsr(false);
    auto copyCsr = immCmdList->getCsr(true);

    immCmdList->cmdQImmediate->setTaskCount(1);

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters, signalEventParameters));

    const auto mainQueueTaskCount = immCmdList->cmdQImmediate->getTaskCount();
    const auto copyOffloadQueueTaskCount = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount();

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_TRUE(event->isEventTimestampFlagSet());

    immCmdList->latestFlushIsDualCopyOffload = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters, signalEventParameters));

    EXPECT_GT(immCmdList->cmdQImmediate->getTaskCount(), mainQueueTaskCount);
    EXPECT_EQ(copyOffloadQueueTaskCount, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount());
    EXPECT_FALSE(immCmdList->latestFlushIsDualCopyOffload);

    TaskCountType cleanupTaskCount = 0;
    EXPECT_FALSE(event->getCleanupTaskCount(copyCsr, cleanupTaskCount));
    ASSERT_TRUE(event->getCleanupTaskCount(mainCsr, cleanupTaskCount));
    EXPECT_EQ(immCmdList->cmdQImmediate->getTaskCount(), cleanupTaskCount);
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadAndCompletedMainQueueWorkWhenAppendRedundantBarrierWithSignalEventThenNothingIsSubmittedAndEventIsCompletedOnHost, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    auto mainCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    immCmdList->cmdQImmediate->setTaskCount(1);

    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParameters, signalEventParameters));

    const auto mainQueueTaskCount = immCmdList->cmdQImmediate->getTaskCount();
    const auto copyOffloadQueueTaskCount = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount();

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_FALSE(event->isEventTimestampFlagSet());

    mainCsr->testTaskCountReadyReturnValue = true;
    copyCsr->testTaskCountReadyReturnValue = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(event->toHandle(), 0, nullptr, waitEventsParameters, signalEventParameters));

    EXPECT_EQ(mainQueueTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(copyOffloadQueueTaskCount, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus(0));
}

HWTEST2_F(CopyOffloadInOrderTests, givenOutOfOrderDualStreamCopyOffloadWhenAppendBarrierThenWaitEventParametersFromCallerArePassedToBaseImplementation, IsAtLeastXe3pCore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createOutOfOrderImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isInOrderExecutionEnabled());

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto eventHandle = event->toHandle();

    CommandToPatchContainer outWaitCmds;
    CmdListWaitEventParameters waitEventsParameters = {
        .outWaitCmds = &outWaitCmds,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters{
        .relaxedOrderingDispatch = false,
    };
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendBarrier(nullptr, 1, &eventHandle, waitEventsParameters, signalEventParameters));

    // caller parameters are forwarded instead of being replaced by locally created ones
    EXPECT_EQ(1u, outWaitCmds.size());
}

HWTEST2_F(CopyOffloadInOrderTests, givenLatestFlushIsDualCopyOffloadButCopyOffloadNotInDualStreamModeWhenHostSynchronizeThenWaitOnMainQueueWithoutDereferencingCopyOffloadCsr, IsAtLeastXeCore) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->forceDisableInOrderWaits();

    // Without dual-stream copy offload the copy-offload queue and CSR are never created.
    ASSERT_EQ(nullptr, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    ASSERT_FALSE(immCmdList->isDualStreamCopyOffloadOperation(immCmdList->isCopyOffloadEnabled()));

    auto mainCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));

    mainCsr->callBaseWaitForCompletionWithTimeout = false;
    mainCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;

    immCmdList->cmdQImmediate->setTaskCount(10u);

    // latestFlushIsDualCopyOffload without dual-stream copy offload must not select the copy-offload wait path,
    // otherwise the null copy-offload CSR would be dereferenced.
    immCmdList->latestFlushIsDualCopyOffload = true;

    mainCsr->waitForCompletionWithTimeoutTaskCountCalled.store(0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, false));

    EXPECT_EQ(1u, mainCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(10u, mainCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonDualStreamModeWhenSubmittedThenDontProgramBcsMmioBase, IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);
    auto immCmdList0 = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList0->useAdditionalBlitProperties = true;
    auto immCmdList1 = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList1->useAdditionalBlitProperties = true;

    auto eventPool = createEvents<FamilyType>(2, true);
    auto eventHandle0 = events[0]->toHandle();
    auto eventHandle1 = events[1]->toHandle();

    immCmdList0->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle0, 0, nullptr, copyParams);
    immCmdList1->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle1, 0, nullptr, copyParams);
    immCmdList1->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 1, &eventHandle0, copyParams);
    auto cmdStream = immCmdList1->getCmdContainer().getCommandStream();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), 0));

    auto itor = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

    while (itor != cmdList.end()) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        EXPECT_TRUE(cmd->getRegisterOffset() < RegisterOffsets::bcs0Base);
        itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonDualStreamModeAndProfilingEventWithRelaxedOrderingWhenAppendingThenDontBcsCommands, IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = true;

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto mainQueueDirectSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*mainQueueCsr);
    auto offloadDirectSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*copyQueueCsr);

    mainQueueCsr->directSubmission.reset(mainQueueDirectSubmission);
    copyQueueCsr->blitterDirectSubmission.reset(offloadDirectSubmission);

    int client1, client2;

    mainQueueCsr->registerClient(&client1);
    mainQueueCsr->registerClient(&client2);
    copyQueueCsr->registerClient(&client1);
    copyQueueCsr->registerClient(&client2);

    auto eventPool = createEvents<FamilyType>(1, true);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle, 0, nullptr, copyParams);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto lrrCmds = findAll<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    auto lriCmds = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    auto lrmCmds = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());

    for (auto &lrr : lrrCmds) {
        auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrr);
        EXPECT_TRUE(lrrCmd->getSourceRegisterAddress() < RegisterOffsets::bcs0Base);
        EXPECT_TRUE(lrrCmd->getDestinationRegisterAddress() < RegisterOffsets::bcs0Base);
    }

    for (auto &lri : lriCmds) {
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lri);
        EXPECT_TRUE(lriCmd->getRegisterOffset() < RegisterOffsets::bcs0Base);
    }

    for (auto &lrm : lrmCmds) {
        auto lrmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*lrm);
        EXPECT_TRUE(lrmCmd->getRegisterAddress() < RegisterOffsets::bcs0Base);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenDebugFlagSetWhenCreatingRegularCmdListThenEnableCopyOffload, IsAtLeastXeCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(1);

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    EXPECT_EQ(CopyOffloadModes::disabled, regularCmdList->copyOffloadMode);

    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(2);

    regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    EXPECT_NE(CopyOffloadModes::disabled, regularCmdList->copyOffloadMode);
    EXPECT_EQ(nullptr, regularCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

    regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(true);
    EXPECT_EQ(CopyOffloadModes::disabled, regularCmdList->copyOffloadMode);
}

HWTEST2_F(CopyOffloadInOrderTests, givenDebugFlagSetWhenCreatingCmdListThenEnableCopyOffload, IsAtLeastXeCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(1);

    ze_command_list_handle_t cmdListHandle;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    auto crossEngineCacheFlushRequired = device->getGfxCoreHelper().crossEngineCacheFlushRequired();

    {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));

        if (crossEngineCacheFlushRequired) {
            EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else if (device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

            auto copyQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
            auto computeQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediate);

            EXPECT_EQ(computeQueue->getCsr()->getOsContext().isHighPriority(), copyQueue->getCsr()->getOsContext().isHighPriority());

            EXPECT_EQ(cmdQueueDesc.mode, copyQueue->desc.mode);
            EXPECT_TRUE(copyQueue->peekIsCopyOnlyCommandQueue());
            EXPECT_TRUE(NEO::EngineHelpers::isBcs(copyQueue->getCsr()->getOsContext().getEngineType()));
        }

        zeCommandListDestroy(cmdListHandle);
    }

    {
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));

        if (crossEngineCacheFlushRequired) {
            EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else if (device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

            auto copyQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
            auto computeQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediate);
            EXPECT_EQ(computeQueue->getCsr()->getOsContext().isHighPriority(), copyQueue->getCsr()->getOsContext().isHighPriority());

            EXPECT_EQ(cmdQueueDesc.mode, copyQueue->desc.mode);
            EXPECT_TRUE(copyQueue->peekIsCopyOnlyCommandQueue());
            EXPECT_TRUE(NEO::EngineHelpers::isBcs(copyQueue->getCsr()->getOsContext().getEngineType()));
        }

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    }

    {
        cmdQueueDesc.flags = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));

        if (crossEngineCacheFlushRequired || !device->getL0GfxCoreHelper().isCopyOffloadForOutOfOrderImmediateCmdListSupported()) {
            EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else if (device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        }

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    }

    {
        cmdQueueDesc.ordinal = static_cast<Device *>(device)->getCopyEngineOrdinal();

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.ordinal = 0;
    }

    {
        NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        if (device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        } else {
            EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        }
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

        zeCommandListDestroy(cmdListHandle);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenLowPriorityQueueWhenCopyEngineDoesNotSupportLowPriorityThenCopyOffloadIsDisabled, IsAtLeastXeCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(1);

    ze_command_list_handle_t cmdListHandle;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
    auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));

    auto computeCsr = cmdList->getCsr(false);
    bool computeIsLowPriority = computeCsr->getOsContext().isLowPriority();

    if (!computeIsLowPriority) {
        zeCommandListDestroy(cmdListHandle);
        GTEST_SKIP();
    }

    if (cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex] != nullptr) {
        auto copyQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        EXPECT_TRUE(copyQueue->getCsr()->getOsContext().isLowPriority());
    } else if (cmdList->copyOffloadMode == CopyOffloadModes::disabled) {
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
    }

    zeCommandListDestroy(cmdListHandle);
}

HWTEST2_F(CopyOffloadInOrderTests, givenMaxPowerHintWhenCreatingDualStreamCopyOffloadCmdListThenCopyQueueHasPowerHintOrOffloadIsDisabled, IsAtLeastXeCore) {
    auto defaultCopyOffloadMode = device->getL0GfxCoreHelper().getDefaultCopyOffloadMode(device->getProductHelper().useAdditionalBlitProperties());
    if (defaultCopyOffloadMode != CopyOffloadModes::dualStream) {
        GTEST_SKIP();
    }

    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(1);
    context->setPowerHint(NEO::OsContext::getUmdPowerHintMax());

    ze_command_list_handle_t cmdListHandle;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
    auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));

    if (cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex] != nullptr) {
        auto copyQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        EXPECT_TRUE(copyQueue->getCsr()->getOsContext().isPowerHint());
    } else {
        EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
    }

    zeCommandListDestroy(cmdListHandle);
}

HWTEST2_F(CopyOffloadInOrderTests, givenQueueDescriptorWhenCreatingCmdListThenEnableCopyOffload, IsAtLeastXeCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

    ze_command_list_handle_t cmdListHandle;

    zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadDesc = {ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES};
    copyOffloadDesc.copyOffloadEnabled = true;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    cmdQueueDesc.pNext = &copyOffloadDesc;

    auto crossEngineCacheFlushRequired = device->getGfxCoreHelper().crossEngineCacheFlushRequired();

    {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        if (crossEngineCacheFlushRequired) {
            EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else if (device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        }

        zeCommandListDestroy(cmdListHandle);
    }

    {
        copyOffloadDesc.copyOffloadEnabled = false;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        if (device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        } else {
            EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        }
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

        zeCommandListDestroy(cmdListHandle);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenQueueDescriptorWithCopyOffloadFlagWhenCreatingCmdListThenEnableCopyOffload, IsAtLeastXeCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

    ze_command_list_handle_t cmdListHandle;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER | ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    auto crossEngineCacheFlushRequired = device->getGfxCoreHelper().crossEngineCacheFlushRequired();

    {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));

        if (crossEngineCacheFlushRequired) {
            EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else if (device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        } else {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
            EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);
        }

        zeCommandListDestroy(cmdListHandle);
    }

    {
        cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        if (device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        } else {
            EXPECT_EQ(CopyOffloadModes::disabled, cmdList->copyOffloadMode);
        }
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]);

        zeCommandListDestroy(cmdListHandle);
    }
}

HWTEST_F(CopyOffloadInOrderTests, givenNonDualStreamOffloadWhenCreatingCmdListThenAcceptOffloadHintExtStruct) {
    zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadDesc = {ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES};
    copyOffloadDesc.copyOffloadEnabled = true;

    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.pNext = &copyOffloadDesc;
    ze_command_list_handle_t hCmdList{};

    {
        NEO::debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);

        cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList));

        if (device->getGfxCoreHelper().crossEngineCacheFlushRequired()) {
            EXPECT_EQ(CopyOffloadModes::disabled, CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));
        } else {
            EXPECT_EQ(nonDualStreamMode, CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));
        }

        zeCommandListDestroy(hCmdList);
    }

    {
        cmdListDesc.flags = 0;

        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList));
        bool supported = device->getL0GfxCoreHelper().isDefaultCmdListWithCopyOffloadSupported(device->getProductHelper().useAdditionalBlitProperties());

        EXPECT_EQ(!supported, CopyOffloadModes::disabled == CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));

        zeCommandListDestroy(hCmdList);
    }

    {
        NEO::debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

        cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList));

        const bool regularCmdListDualStreamSupported = device->getProductHelper().isDualStreamCopyOffloadForRegularCmdListSupported() &&
                                                       !device->getGfxCoreHelper().crossEngineCacheFlushRequired();
        const auto expectedMode = regularCmdListDualStreamSupported ? CopyOffloadModes::dualStream : CopyOffloadModes::disabled;
        EXPECT_EQ(expectedMode, CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));

        zeCommandListDestroy(hCmdList);
    }
}

HWTEST_F(CopyOffloadInOrderTests, givenNonDualStreamOffloadWhenCreatingCmdListThenAcceptOffloadHintFlag) {
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    ze_command_list_handle_t hCmdList{};

    {
        NEO::debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);

        cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER | ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT;

        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList));

        if (device->getGfxCoreHelper().crossEngineCacheFlushRequired()) {
            EXPECT_EQ(CopyOffloadModes::disabled, CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));
        } else {
            EXPECT_EQ(nonDualStreamMode, CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));
        }

        zeCommandListDestroy(hCmdList);
    }

    {
        cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT;

        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList));
        bool supported = device->getL0GfxCoreHelper().isDefaultCmdListWithCopyOffloadSupported(device->getProductHelper().useAdditionalBlitProperties());

        EXPECT_EQ(!supported, CopyOffloadModes::disabled == CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));

        zeCommandListDestroy(hCmdList);
    }

    {
        NEO::debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

        cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER | ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT;

        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList));

        const bool regularCmdListDualStreamSupported = device->getProductHelper().isDualStreamCopyOffloadForRegularCmdListSupported() &&
                                                       !device->getGfxCoreHelper().crossEngineCacheFlushRequired();
        const auto expectedMode = regularCmdListDualStreamSupported ? CopyOffloadModes::dualStream : CopyOffloadModes::disabled;
        EXPECT_EQ(expectedMode, CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));

        zeCommandListDestroy(hCmdList);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOffloadEnabledWhenProgrammingHwCmdsThenUseCopyCommands, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    EXPECT_FALSE(immCmdList->isCopyOnly(false));
    EXPECT_TRUE(immCmdList->isCopyOnly(true));

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    if (device->getProductHelper().useAdditionalBlitProperties()) {
        ASSERT_EQ(mainQueueCsr, copyQueueCsr);
    } else {
        ASSERT_NE(mainQueueCsr, copyQueueCsr);
    }

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto mainTaskCount = mainQueueCsr->taskCount.load();
    auto initialCopyTaskCount = copyQueueCsr->taskCount.load();

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), copyItor);

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(mainTaskCount, mainQueueCsr->taskCount);
        }
        EXPECT_EQ(initialCopyTaskCount + 1, copyQueueCsr->taskCount);
    }

    {
        auto offset = cmdStream->getUsed();

        ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
        immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), copyItor);

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(mainTaskCount, mainQueueCsr->taskCount);
        }
        EXPECT_EQ(initialCopyTaskCount + 2, copyQueueCsr->taskCount);
    }

    auto data = allocHostMem(immCmdList->maxFillPatternSizeForCopyEngine * 2);
    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryFill(data, data, 1, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto fillItor = findBltFillCmd<FamilyType>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), fillItor);

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(mainTaskCount, mainQueueCsr->taskCount);
        }
        EXPECT_EQ(initialCopyTaskCount + 3, copyQueueCsr->taskCount);
    }
    mainTaskCount++;
    {
        auto offset = cmdStream->getUsed();

        auto patternSize = immCmdList->maxFillPatternSizeForCopyEngine + 1;
        immCmdList->appendMemoryFill(data, data, patternSize, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto fillItor = findBltFillCmd<FamilyType>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), fillItor);

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(mainTaskCount, mainQueueCsr->taskCount);
            EXPECT_EQ(initialCopyTaskCount + 3, copyQueueCsr->taskCount);
        }
    }

    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.width = 10;
    zeDesc.height = 10;
    zeDesc.depth = 1;
    image->initialize(device, &zeDesc);
    static_cast<MemoryAllocation *>(image->getAllocation())->overrideMemoryPool(NEO::MemoryPool::system64KBPages);

    ze_image_region_t imgRegion = {0, 0, 0, 1, 1, 1};

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendImageCopyFromMemoryExt(image->toHandle(), data, &imgRegion, 0, 0, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), itor);

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(mainTaskCount, mainQueueCsr->taskCount);
            EXPECT_EQ(initialCopyTaskCount + 4, copyQueueCsr->taskCount);
        }
    }

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendImageCopyToMemoryExt(data, image->toHandle(), &imgRegion, 0, 0, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), itor);

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(mainTaskCount, mainQueueCsr->taskCount);
            EXPECT_EQ(initialCopyTaskCount + 5, copyQueueCsr->taskCount);
        }
    }

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendImageCopy(image->toHandle(), image->toHandle(), nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), itor);

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(initialCopyTaskCount + 6, copyQueueCsr->taskCount);
            EXPECT_EQ(mainTaskCount, mainQueueCsr->taskCount);
        }
    }
    context->freeMem(data);
}

HWTEST2_F(CopyOffloadInOrderTests, givenImageCopyOffloadDisallowedWhenAppendingImageCopyToAndFromMemoryThenComputeWalkerIsUsedInsteadOfCopyEngine, IsAtLeastXeCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    if (!device->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    for (const auto imageBuiltIn : {ImageBuiltIn::copyBufferToImage3dBytes, ImageBuiltIn::copyImage3dToBufferBytes}) {
        auto builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(imageBuiltIn, getDefaultBuiltInMode());
        static_cast<Mock<::L0::KernelImp> *>(builtinKernel)->setArgRedescribedImageCallBase = false;
    }

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->setImageCopyOffloadAllowed(false);

    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));
    const auto initialCopyTaskCount = copyQueueCsr->taskCount.load();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto data = allocHostMem(MemoryConstants::kiloByte);

    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.width = 10;
    zeDesc.height = 10;
    zeDesc.depth = 1;
    image->initialize(device, &zeDesc);
    static_cast<MemoryAllocation *>(image->getAllocation())->overrideMemoryPool(NEO::MemoryPool::system64KBPages);

    ze_image_region_t imgRegion = {0, 0, 0, 1, 1, 1};

    for (const bool imageToMemory : {false, true}) {
        const auto offset = cmdStream->getUsed();

        if (imageToMemory) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendImageCopyToMemoryExt(data, image->toHandle(), &imgRegion, 0, 0, nullptr, 0, nullptr, copyParams));
        } else {
            EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendImageCopyFromMemoryExt(image->toHandle(), data, &imgRegion, 0, 0, nullptr, 0, nullptr, copyParams));
        }

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        EXPECT_EQ(cmdList.end(), find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end()));
        EXPECT_NE(cmdList.end(), find<WalkerType *>(cmdList.begin(), cmdList.end()));
    }

    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        EXPECT_EQ(initialCopyTaskCount, copyQueueCsr->taskCount);
    }

    context->freeMem(data);
}

HWTEST2_F(CopyOffloadInOrderTests, givenImageCopyOffloadDisallowedAndBlitPreferredWhenAppendingImage1dBufferCopyToAndFromMemoryThenComputeWalkerIsUsedInsteadOfCopyEngine, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    if (!device->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->setImageCopyOffloadAllowed(false);

    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));
    const auto initialCopyTaskCount = copyQueueCsr->taskCount.load();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t width = 64u;
    auto data = allocHostMem(width * sizeof(uint32_t));
    void *imageStorage = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, width * sizeof(uint32_t), 0, &imageStorage));

    ze_image_pitched_exp_desc_t pitchedDesc = {};
    pitchedDesc.stype = ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC;
    pitchedDesc.ptr = imageStorage;

    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    zeDesc.pNext = &pitchedDesc;
    zeDesc.type = ZE_IMAGE_TYPE_BUFFER;
    zeDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_32;
    zeDesc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    zeDesc.width = width * sizeof(uint32_t);
    zeDesc.height = 1;
    zeDesc.depth = 1;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, image->initialize(device, &zeDesc));

    ze_image_region_t imgRegion = {0, 0, 0, static_cast<uint32_t>(width), 1, 1};

    for (const bool imageToMemory : {false, true}) {
        const auto offset = cmdStream->getUsed();

        if (imageToMemory) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendImageCopyToMemoryExt(data, image->toHandle(), &imgRegion, 0, 0, nullptr, 0, nullptr, copyParams));
        } else {
            EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendImageCopyFromMemoryExt(image->toHandle(), data, &imgRegion, 0, 0, nullptr, 0, nullptr, copyParams));
        }

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        EXPECT_EQ(cmdList.end(), find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end()));
        EXPECT_NE(cmdList.end(), find<WalkerType *>(cmdList.begin(), cmdList.end()));
    }

    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        EXPECT_EQ(initialCopyTaskCount, copyQueueCsr->taskCount);
    }

    image.reset();
    context->freeMem(imageStorage);
    context->freeMem(data);
}

HWTEST2_F(CopyOffloadInOrderTests, givenBlitEnqueuePreferenceWhenAppendFillCalledThenCopyOffloadUsed, IsAtLeastXeCore) {
    debugManager.flags.EnableBlitterForEnqueueOperations.set(-1);
    bool blitOffloadPreferred = device->getProductHelper().blitEnqueuePreferred(false);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    EXPECT_FALSE(immCmdList->isCopyOnly(false));
    EXPECT_TRUE(immCmdList->isCopyOnly(true));

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto data = allocHostMem(MemoryConstants::kiloByte * 10);

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryFill(data, data, 1, MemoryConstants::kiloByte * 8, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto fillItor = findBltFillCmd<FamilyType>(cmdList.begin(), cmdList.end());

        if (blitOffloadPreferred) {
            EXPECT_NE(cmdList.end(), fillItor);
        } else {
            EXPECT_EQ(cmdList.end(), fillItor);
        }
    }

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryFill(data, data, 1, (MemoryConstants::kiloByte * 8) + 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto fillItor = findBltFillCmd<FamilyType>(cmdList.begin(), cmdList.end());

        EXPECT_EQ(cmdList.end(), fillItor);
    }

    {
        auto offset = cmdStream->getUsed();

        debugManager.flags.OverrideFillCopyOffloadThresholdKb.set(9);

        immCmdList->appendMemoryFill(data, data, 1, MemoryConstants::kiloByte * 9, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto fillItor = findBltFillCmd<FamilyType>(cmdList.begin(), cmdList.end());

        if (blitOffloadPreferred) {
            EXPECT_NE(cmdList.end(), fillItor);
        } else {
            EXPECT_EQ(cmdList.end(), fillItor);
        }
    }

    context->freeMem(data);
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonDualStreamOffloadWhenImageCopyCalledThenSkipSycCommands, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = true;

    auto eventPool = createEvents<FamilyType>(1, true);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto data = allocHostMem(1);
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.width = 10;
    zeDesc.height = 10;
    zeDesc.depth = 1;
    image->initialize(device, &zeDesc);
    static_cast<MemoryAllocation *>(image->getAllocation())->overrideMemoryPool(NEO::MemoryPool::system64KBPages);
    ze_image_region_t imgRegion = {0, 0, 0, 1, 1, 1};

    immCmdList->appendImageCopyToMemoryExt(data, image->toHandle(), &imgRegion, 0, 0, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto lrrCmds = findAll<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    auto lriCmds = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    auto lrmCmds = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());

    for (auto &lrr : lrrCmds) {
        auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrr);
        EXPECT_TRUE(lrrCmd->getSourceRegisterAddress() < RegisterOffsets::bcs0Base);
        EXPECT_TRUE(lrrCmd->getDestinationRegisterAddress() < RegisterOffsets::bcs0Base);
    }

    for (auto &lri : lriCmds) {
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lri);
        EXPECT_TRUE(lriCmd->getRegisterOffset() < RegisterOffsets::bcs0Base);
    }

    for (auto &lrm : lrmCmds) {
        auto lrmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*lrm);
        EXPECT_TRUE(lrmCmd->getRegisterAddress() < RegisterOffsets::bcs0Base);
    }

    context->freeMem(data);
}

HWTEST2_F(CopyOffloadInOrderTests, givenDualStreamCopyOffloadWhenImageCopyWithSignalEventCalledThenEventIsMarkedAsDualCopyOffloadAndCounterIsSignaledAfterMiFlush, IsAtLeastXeCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = false;
    immCmdList->inOrderAtomicSignalingEnabled = false;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto data = allocHostMem(1);
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.width = 10;
    zeDesc.height = 10;
    zeDesc.depth = 1;
    image->initialize(device, &zeDesc);
    static_cast<MemoryAllocation *>(image->getAllocation())->overrideMemoryPool(NEO::MemoryPool::system64KBPages);
    ze_image_region_t imgRegion = {0, 0, 0, 1, 1, 1};

    EXPECT_FALSE(events[0]->isDualCopyOffloadEvent);

    auto offset = cmdStream->getUsed();

    immCmdList->appendImageCopyToMemoryExt(data, image->toHandle(), &imgRegion, 0, 0, eventHandle, 0, nullptr, copyParams);

    EXPECT_TRUE(events[0]->isDualCopyOffloadEvent);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto counterGpuVa = gmmHelper->decanonize(immCmdList->inOrderExecInfo->getBaseDeviceAddress());
    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    auto sdiItor = std::find_if(sdiCmds.begin(), sdiCmds.end(), [&](auto &itor) {
        return gmmHelper->decanonize(genCmdCast<MI_STORE_DATA_IMM *>(*itor)->getAddress()) == counterGpuVa;
    });
    ASSERT_NE(sdiCmds.end(), sdiItor);
    ASSERT_NE(cmdList.begin(), *sdiItor);

    auto prevItor = *sdiItor;
    --prevItor;
    EXPECT_NE(nullptr, genCmdCast<MI_FLUSH_DW *>(*prevItor));

    context->freeMem(data);
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonDualStreamOffloadWhenFillCalledThenSkipSycCommands, IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    debugManager.flags.OverrideCopyOffloadMode.set(nonDualStreamMode);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = true;

    auto eventPool = createEvents<FamilyType>(1, true);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto data = allocHostMem(1);

    immCmdList->appendMemoryFill(data, data, 1, 1, events[0]->toHandle(), 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto miFlushCmds = findAll<typename FamilyType::MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, miFlushCmds.size());

    auto lrrCmds = findAll<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    auto lriCmds = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    auto lrmCmds = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());

    for (auto &lrr : lrrCmds) {
        auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrr);
        EXPECT_TRUE(lrrCmd->getSourceRegisterAddress() < RegisterOffsets::bcs0Base);
        EXPECT_TRUE(lrrCmd->getDestinationRegisterAddress() < RegisterOffsets::bcs0Base);
    }

    for (auto &lri : lriCmds) {
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lri);
        EXPECT_TRUE(lriCmd->getRegisterOffset() < RegisterOffsets::bcs0Base);
    }

    for (auto &lrm : lrmCmds) {
        auto lrmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*lrm);
        EXPECT_TRUE(lrmCmd->getRegisterAddress() < RegisterOffsets::bcs0Base);
    }

    context->freeMem(data);
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOffloadAllowedWhenAppendBlitFillCalledThenProgramMiFlush, IsAtLeastXeCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->useAdditionalBlitProperties = false;
    ASSERT_TRUE(immCmdList->isCopyOffloadEnabled());

    auto alloc = allocDeviceMem(64);
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    copyParams.copyOffloadAllowed = true;
    immCmdList->appendBlitFill(alloc, &copyData1, 1, 16, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto it = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), it);

    context->freeMem(alloc);
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOffloadEnabledAndSmallD2DAllocWhenProgrammingHwCmdsThenUseCopyCommands, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    EXPECT_FALSE(immCmdList->isCopyOnly(false));
    EXPECT_TRUE(immCmdList->isCopyOnly(true));

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t smallSize = BlitterConstants::maxD2DBcsCopySize;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *deviceMem = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, smallSize, 1, &deviceMem));

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    if (device->getProductHelper().useAdditionalBlitProperties()) {
        ASSERT_EQ(mainQueueCsr, copyQueueCsr);
    } else {
        ASSERT_NE(mainQueueCsr, copyQueueCsr);
    }

    auto initialMainTaskCount = mainQueueCsr->taskCount.load();
    auto initialCopyTaskCount = copyQueueCsr->taskCount.load();

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(deviceMem, deviceMem, smallSize, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(cmdList.end(), copyItor);
            EXPECT_EQ(initialMainTaskCount, mainQueueCsr->taskCount);
            EXPECT_EQ(initialCopyTaskCount + 1, copyQueueCsr->taskCount);
        }
    }

    {
        auto offset = cmdStream->getUsed();

        const uint32_t regionWidth = static_cast<uint32_t>(smallSize);
        ze_copy_region_t region = {0, 0, 0, regionWidth, 1, 1};
        immCmdList->appendMemoryCopyRegion(deviceMem, &region, regionWidth, 0, deviceMem, &region, regionWidth, 0, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());

        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_NE(cmdList.end(), copyItor);
            EXPECT_EQ(initialMainTaskCount, mainQueueCsr->taskCount);
            EXPECT_EQ(initialCopyTaskCount + 2, copyQueueCsr->taskCount);
        }
    }

    context->freeMem(deviceMem);
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOffloadEnabledAndLargeD2DAllocWhenProgrammingHwCmdsThenDontUseCopyCommands, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t largeSize = BlitterConstants::maxD2DBcsCopySize + 1;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *deviceMem = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, largeSize, 1, &deviceMem));

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    if (device->getProductHelper().useAdditionalBlitProperties()) {
        ASSERT_EQ(mainQueueCsr, copyQueueCsr);
    } else {
        ASSERT_NE(mainQueueCsr, copyQueueCsr);
    }

    auto initialMainTaskCount = mainQueueCsr->taskCount.load();
    auto initialCopyTaskCount = copyQueueCsr->taskCount.load();

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(deviceMem, deviceMem, largeSize, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), copyItor);

        EXPECT_EQ(initialMainTaskCount + 1, mainQueueCsr->taskCount);
        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(initialCopyTaskCount, copyQueueCsr->taskCount);
        }
    }

    {
        auto offset = cmdStream->getUsed();

        const uint32_t regionWidth = static_cast<uint32_t>(largeSize);
        ze_copy_region_t region = {0, 0, 0, regionWidth, 1, 1};
        immCmdList->appendMemoryCopyRegion(deviceMem, &region, regionWidth, 0, deviceMem, &region, regionWidth, 0, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), copyItor);

        EXPECT_EQ(initialMainTaskCount + 2, mainQueueCsr->taskCount);
        if (!device->getProductHelper().useAdditionalBlitProperties()) {
            EXPECT_EQ(initialCopyTaskCount, copyQueueCsr->taskCount);
        }
    }

    context->freeMem(deviceMem);
}

HWTEST2_F(CopyOffloadInOrderTests, givenProfilingEventWhenAppendingThenUseBcsCommands, IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle, 0, nullptr, copyParams);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto pipeControls = findAll<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipeControls.size());

    auto miFlushCmds = findAll<typename FamilyType::MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, miFlushCmds.size());

    auto lrrCmds = findAll<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    auto lriCmds = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    auto lrmCmds = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());

    for (auto &lrr : lrrCmds) {
        auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrr);
        EXPECT_TRUE(lrrCmd->getSourceRegisterAddress() > RegisterOffsets::bcs0Base);
        EXPECT_TRUE(lrrCmd->getDestinationRegisterAddress() > RegisterOffsets::bcs0Base);
    }

    for (auto &lri : lriCmds) {
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lri);
        EXPECT_TRUE(lriCmd->getRegisterOffset() > RegisterOffsets::bcs0Base);
    }

    for (auto &lrm : lrmCmds) {
        auto lrmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*lrm);
        EXPECT_TRUE(lrmCmd->getRegisterAddress() > RegisterOffsets::bcs0Base);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenProfilingEventWithRelaxedOrderingWhenAppendingThenUseBcsCommands, IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    EXPECT_NE(mainQueueCsr, copyQueueCsr);

    auto mainQueueDirectSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*mainQueueCsr);
    auto offloadDirectSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*copyQueueCsr);

    mainQueueCsr->directSubmission.reset(mainQueueDirectSubmission);
    copyQueueCsr->blitterDirectSubmission.reset(offloadDirectSubmission);

    int client1, client2;

    mainQueueCsr->registerClient(&client1);
    mainQueueCsr->registerClient(&client2);
    copyQueueCsr->registerClient(&client1);
    copyQueueCsr->registerClient(&client2);

    auto eventPool = createEvents<FamilyType>(1, true);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle, 0, nullptr, copyParams);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto lrrCmds = findAll<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    auto lriCmds = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    auto lrmCmds = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());

    for (auto &lrr : lrrCmds) {
        auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrr);
        EXPECT_TRUE(lrrCmd->getSourceRegisterAddress() > RegisterOffsets::bcs0Base);
        EXPECT_TRUE(lrrCmd->getDestinationRegisterAddress() > RegisterOffsets::bcs0Base);
    }

    for (auto &lri : lriCmds) {
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lri);
        EXPECT_TRUE(lriCmd->getRegisterOffset() > RegisterOffsets::bcs0Base);
    }

    for (auto &lrm : lrmCmds) {
        auto lrmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*lrm);
        EXPECT_TRUE(lrmCmd->getRegisterAddress() > RegisterOffsets::bcs0Base);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CopyOffloadInOrderTests, givenCrossEngineDependencyWhenComputeWorkSubmittedThenUseSemaphore) {
    debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(immCmdList->latestFlushIsDualCopyOffload);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto hwCmds = HardwareParse::parseCommandBuffer<FamilyType>(*cmdStream, offset);

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(hwCmds.begin(), hwCmds.end());
    EXPECT_NE(hwCmds.end(), itor);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CopyOffloadInOrderTests, givenWalkerPostSyncSkipEnabledWhenAppendKernelsWithoutEventsThenFlushL1Cache) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    NEO::debugManager.flags.EnableWalkerPostSyncSkip.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    NEO::HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdStream, 0);

    hwParser.verifyL1FlushOnStallingBarrier<FamilyType>(false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CopyOffloadInOrderTests, givenWalkerPostSyncSkipEnabledWhenAppendKernelsWithEventsThenDontFlushL1Cache) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    NEO::debugManager.flags.EnableWalkerPostSyncSkip.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    NEO::HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdStream, 0);

    hwParser.verifyL1FlushOnStallingBarrier<FamilyType>(false, false);
}

HWTEST2_F(CopyOffloadInOrderTests, givenAtomicSignalingModeWhenUpdatingCounterThenUseCorrectHwCommands, IsAtLeastXe2HpgCore) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODE = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODE;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr uint32_t partitionCount = 4;

    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto gmmHelper = device->getNEODevice()->getGmmHelper();

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

        auto immCmdList = createMultiTileImmCmdListWithOffload<FamilyType::gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miAtomics = findAll<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(1u, miAtomics.size());

        auto atomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomics[0]);
        ASSERT_NE(nullptr, atomicCmd);

        auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

        EXPECT_EQ(gpuAddress, gmmHelper->canonize(atomicCmd->getMemoryAddress()));
        EXPECT_EQ(ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, atomicCmd->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, atomicCmd->getDataSize());
        EXPECT_EQ(getLowPart(partitionCount), atomicCmd->getOperand1DataDword0());
        EXPECT_EQ(getHighPart(partitionCount), atomicCmd->getOperand1DataDword1());
    }

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(0);

        auto immCmdList = createMultiTileImmCmdListWithOffload<FamilyType::gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miStoreDws = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(partitionCount, miStoreDws.size());

        for (uint32_t i = 0; i < partitionCount; i++) {

            auto storeDw = genCmdCast<MI_STORE_DATA_IMM *>(*miStoreDws[i]);
            ASSERT_NE(nullptr, storeDw);

            auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress() + (i * device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
            EXPECT_EQ(gpuAddress, gmmHelper->canonize(storeDw->getAddress()));
            EXPECT_EQ(1u, storeDw->getDataDword0());
        }
    }

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
        debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

        auto immCmdList = createMultiTileImmCmdListWithOffload<FamilyType::gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miStoreDws = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(partitionCount * 2, miStoreDws.size());

        for (uint32_t i = 0; i < partitionCount; i++) {

            auto storeDw = genCmdCast<MI_STORE_DATA_IMM *>(*miStoreDws[i + partitionCount]);
            ASSERT_NE(nullptr, storeDw);

            auto gpuAddress = immCmdList->inOrderExecInfo->getBaseHostGpuAddress() + (i * device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
            EXPECT_EQ(gpuAddress, storeDw->getAddress());
            EXPECT_EQ(1u, storeDw->getDataDword0());
        }
    }
}
HWTEST2_F(CopyOffloadInOrderTests, givenDeviceToHostCopyWhenProgrammingThenAddFence, IsAtLeastXeHpcCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device, result));

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    constexpr size_t allocSize = 1;
    void *hostBuffer = nullptr;
    void *deviceBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocHostMem(&hostDesc, allocSize, allocSize, &hostBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocSize, allocSize, &deviceBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_copy_region_t dstRegion = {0, 0, 0, 1, 1, 1};
    ze_copy_region_t srcRegion = {0, 0, 0, 1, 1, 1};

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, copyParams);

    bool expected = device->getProductHelper().isDeviceToHostCopySignalingFenceRequired();

    GenCmdList genCmdList;
    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    itor = find<MI_MEM_FENCE *>(itor, genCmdList.end());
    EXPECT_EQ(expected, genCmdList.end() != itor);

    context->freeMem(hostBuffer);
    context->freeMem(deviceBuffer);
}

HWTEST2_F(CopyOffloadInOrderTests, whenDispatchingSelectCorrectQueueAndCsr, IsAtLeastXeHpcCore) {
    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto regularEventsPool = createEvents<FamilyType>(1, false);

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    auto heaplessEnabled = immCmdList->isHeaplessModeEnabled();
    auto regularCsr = immCmdList->cmdQImmediate->getCsr();
    auto copyCsr = immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getCsr();

    auto expectedRegularCsrTaskCount = heaplessEnabled ? 1u : 0u;
    auto expectedCopyCsrTaskCount = heaplessEnabled ? 1u : 0u;
    auto expectedImmediateCmdTaskCount = 0u;
    auto expectedCopyOffloadTaskCount = 0u;

    EXPECT_EQ(expectedRegularCsrTaskCount, regularCsr->peekTaskCount());
    EXPECT_EQ(expectedImmediateCmdTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(expectedCopyCsrTaskCount, copyCsr->peekTaskCount());
    EXPECT_EQ(expectedCopyOffloadTaskCount, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0].get(), 0, nullptr, launchParams);
    expectedRegularCsrTaskCount++;
    expectedImmediateCmdTaskCount = expectedRegularCsrTaskCount;

    EXPECT_EQ(expectedRegularCsrTaskCount, regularCsr->peekTaskCount());
    EXPECT_EQ(expectedImmediateCmdTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(expectedCopyCsrTaskCount, copyCsr->peekTaskCount());
    EXPECT_EQ(expectedCopyOffloadTaskCount, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount());

    EXPECT_EQ(regularCsr, events[0]->csrs[0]);
    EXPECT_EQ(immCmdList->cmdQImmediate, events[0]->latestUsedCmdQueue);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, copyParams);
    expectedCopyCsrTaskCount++;
    expectedCopyOffloadTaskCount = expectedCopyCsrTaskCount;

    EXPECT_EQ(expectedRegularCsrTaskCount, regularCsr->peekTaskCount());
    EXPECT_EQ(expectedImmediateCmdTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(expectedCopyCsrTaskCount, copyCsr->peekTaskCount());
    EXPECT_EQ(expectedCopyOffloadTaskCount, immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex]->getTaskCount());

    EXPECT_EQ(copyCsr, events[0]->csrs[0]);
    EXPECT_EQ(immCmdList->cmdQImmediateCopyOffload[DualStreamCopyOffload::copyOffloadReadIndex], events[0]->latestUsedCmdQueue);
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOperationWithHostVisibleEventThenMarkAsNotHostVisibleSubmission, IsAtLeastXeHpcCore) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device, result));

    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, hostVisibleEvent.get(), 0, nullptr, launchParams);

    EXPECT_TRUE(immCmdList->latestFlushIsHostVisible);
    auto usmHost = allocHostMem(1);
    auto usmDevice = allocDeviceMem(1);
    immCmdList->appendMemoryCopy(usmDevice, usmHost, 1, hostVisibleEvent.get(), 0, nullptr, copyParams);

    EXPECT_EQ(!immCmdList->dcFlushSupport, immCmdList->latestFlushIsHostVisible);
    context->freeMem(usmHost);
    context->freeMem(usmDevice);
}

HWTEST2_F(CopyOffloadInOrderTests, givenRelaxedOrderingEnabledWhenDispatchingThenUseCorrectCsr, IsXeHpcCore) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> {
      public:
        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies,
                                   NEO::AppendOperations appendOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent, bool requireTaskCountUpdate,
                                   MutexLock *outerLock,
                                   std::unique_lock<std::mutex> *outerLockForIndirect) override {
            latestRelaxedOrderingMode = hasRelaxedOrderingDependencies;

            return ZE_RESULT_SUCCESS;
        }

        bool latestRelaxedOrderingMode = false;
    };

    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyMockCmdList>(true);

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    EXPECT_NE(mainQueueCsr, copyQueueCsr);

    auto mainQueueDirectSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*mainQueueCsr);
    auto offloadDirectSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*copyQueueCsr);

    mainQueueCsr->directSubmission.reset(mainQueueDirectSubmission);
    copyQueueCsr->blitterDirectSubmission.reset(offloadDirectSubmission);

    int client1, client2;

    // first dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    // compute CSR
    mainQueueCsr->registerClient(&client1);
    mainQueueCsr->registerClient(&client2);

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));
    EXPECT_FALSE(immCmdList->isRelaxedOrderingDispatchAllowed(0, true));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_TRUE(immCmdList->latestRelaxedOrderingMode);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_FALSE(immCmdList->latestRelaxedOrderingMode);

    // offload CSR
    mainQueueCsr->unregisterClient(&client1);
    mainQueueCsr->unregisterClient(&client2);
    copyQueueCsr->registerClient(&client1);
    copyQueueCsr->registerClient(&client2);

    EXPECT_FALSE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));
    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, true));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_FALSE(immCmdList->latestRelaxedOrderingMode);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(immCmdList->latestRelaxedOrderingMode);
}

HWTEST2_F(CopyOffloadInOrderTests, givenInOrderModeWhenCallingSyncThenHandleCompletionOnCorrectCsr, IsAtLeastXeCore) {
    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    if (!immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    GraphicsAllocation *expectedAlloc = nullptr;
    uint64_t *hostAddress = nullptr;

    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
        expectedAlloc = inOrderExecInfo->getHostCounterAllocation();
    } else {

        expectedAlloc = inOrderExecInfo->getDeviceCounterAllocation();
        hostAddress = static_cast<uint64_t *>(expectedAlloc->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    GraphicsAllocation *mainCsrDownloadedAlloc = nullptr;
    uint32_t mainCsrCallCounter = 0;

    GraphicsAllocation *offloadCsrDownloadedAlloc = nullptr;
    uint32_t offloadCsrCallCounter = 0;

    mainQueueCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation, uint64_t, size_t) {
        mainCsrCallCounter++;

        mainCsrDownloadedAlloc = &graphicsAllocation;
    };

    offloadCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation, uint64_t, size_t) {
        offloadCsrCallCounter++;

        offloadCsrDownloadedAlloc = &graphicsAllocation;
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    immCmdList->hostSynchronize(0, false);
    EXPECT_EQ(mainCsrDownloadedAlloc, expectedAlloc);
    EXPECT_EQ(offloadCsrDownloadedAlloc, nullptr);
    EXPECT_EQ(1u, mainCsrCallCounter);
    EXPECT_EQ(0u, offloadCsrCallCounter);
    EXPECT_EQ(1u, mainQueueCsr->checkGpuHangDetectedCalled);
    EXPECT_EQ(0u, offloadCsr->checkGpuHangDetectedCalled);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, copyParams);

    EXPECT_EQ(0u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    immCmdList->hostSynchronize(0, false);

    if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(1u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
        EXPECT_EQ(1u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    } else {
        EXPECT_EQ(mainCsrDownloadedAlloc, expectedAlloc);
        EXPECT_EQ(offloadCsrDownloadedAlloc, expectedAlloc);
        EXPECT_EQ(1u, mainCsrCallCounter);
        EXPECT_EQ(1u, offloadCsrCallCounter);
        EXPECT_EQ(1u, mainQueueCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(1u, offloadCsr->checkGpuHangDetectedCalled);

        EXPECT_EQ(0u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
        EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    }
    *hostAddress = std::numeric_limits<uint64_t>::max();
}

HWTEST2_F(CopyOffloadInOrderTests, givenTbxModeWhenSyncCalledAlwaysDownloadAllocationsFromBothCsrs, IsAtLeastXeCore) {
    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
    immCmdList->isTbxMode = true;

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        uint64_t *hostAddress = immCmdList->inOrderExecInfo->getBaseHostAddress();
        *hostAddress = 3;
    } else {
        auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
        auto hostAddress = static_cast<uint64_t *>(deviceAlloc->getUnderlyingBuffer());
        *hostAddress = 3;
    }

    *mainQueueCsr->getTagAddress() = 3;
    *offloadCsr->getTagAddress() = 3;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(0u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(0u, offloadCsr->downloadAllocationsCalledCount);

    immCmdList->hostSynchronize(0, false);

    EXPECT_EQ(1u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(1u, offloadCsr->downloadAllocationsCalledCount);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, copyParams);

    immCmdList->hostSynchronize(0, false);

    EXPECT_EQ(2u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(2u, offloadCsr->downloadAllocationsCalledCount);
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonInOrderModeWaitWhenCallingSyncThenHandleCompletionOnCorrectCsr, IsAtLeastXeCore) {
    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    *mainQueueCsr->getTagAddress() = 3;
    *offloadCsr->getTagAddress() = 3;

    auto mockAlloc = new MockGraphicsAllocation();

    auto internalAllocStorage = mainQueueCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(std::unique_ptr<MockGraphicsAllocation>(mockAlloc)), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    immCmdList->hostSynchronize(0, true);
    EXPECT_EQ(1u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, copyParams);

    immCmdList->hostSynchronize(0, true);
    EXPECT_EQ(2u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(1u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    *hostAddress = std::numeric_limits<uint64_t>::max();
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonInOrderModeWaitWhenCallingSyncThenHandleCompletionAndTempAllocations, IsAtLeastXeCore) {
    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }
    auto immCmdList = createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();

    auto memoryManager = static_cast<MockMemoryManager *>(device->getNEODevice()->getMemoryManager());
    memoryManager->callBaseAllocInUse = true;

    auto memoryManagerTempAllocsList = &memoryManager->getTemporaryAllocationsList();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    *mainQueueCsr->getTagAddress() = 10;
    *offloadCsr->getTagAddress() = 10;

    auto mainInternalStorage = mainQueueCsr->getInternalAllocationStorage();

    auto offloadInternalStorage = offloadCsr->getInternalAllocationStorage();

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);
    offloadInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);

    // only main is completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(memoryManagerTempAllocsList->peekIsEmpty());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_FALSE(memoryManagerTempAllocsList->peekIsEmpty());

    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);

    // both completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(memoryManagerTempAllocsList->peekIsEmpty());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_FALSE(memoryManagerTempAllocsList->peekIsEmpty());

    auto mockAlloc = new MockGraphicsAllocation();
    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::unique_ptr<MockGraphicsAllocation>(mockAlloc)), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    // only copy completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_FALSE(memoryManagerTempAllocsList->peekIsEmpty());

    mockAlloc->updateTaskCount(1, mainQueueCsr->getOsContext().getContextId());
    immCmdList->hostSynchronize(0, true);

    EXPECT_TRUE(memoryManagerTempAllocsList->peekIsEmpty());

    // stored only in copy storage
    offloadInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(memoryManagerTempAllocsList->peekIsEmpty());
    *hostAddress = std::numeric_limits<uint64_t>::max();
}

} // namespace ult
} // namespace L0
