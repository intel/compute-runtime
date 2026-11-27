/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture_extended.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/driver_experimental/zex_api.h"

#include <type_traits>
#include <variant>

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
} // namespace CpuIntrinsicsTests

namespace L0 {
namespace ult {

using InOrderCmdListTests = InOrderCmdListFixture;

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCbEventWithProfilingWhenAppendBarrierWithoutSignalEventThenSkipBarrier) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    immCmdList->latestOperationHasCbEventWithProfiling = true;

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };

    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };

    immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

    EXPECT_EQ(offset, cmdStream->getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAfterKernelWithoutEventThenDontInheritSignalSyncAllocation) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };

    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };

    immCmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);
    immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

    if (immCmdList->isWalkerPostSyncSkipEnabled || immCmdList->dcFlushSupport) {
        EXPECT_NE(offset, cmdStream->getUsed());
        EXPECT_EQ(2u, events[0]->getInOrderExecBaseSignalValue());
    } else {
        EXPECT_EQ(offset, cmdStream->getUsed());
        EXPECT_EQ(1u, events[0]->getInOrderExecBaseSignalValue());
    }
}

HWTEST_F(InOrderCmdListTests, givenRegularCmdListWhenProgrammingAppendBarrierWithoutWaitlistThenInheritSignalSyncAllocation) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);

    auto cmdStream = cmdList->getCmdContainer().getCommandStream();
    auto eventPool = createEvents<FamilyType>(2, false);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    EXPECT_EQ(1u, cmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventHandle = events[1]->toHandle();
    events[1]->signalScope = 0;

    events[1]->setLatestUsedCmdQueue(reinterpret_cast<L0::CommandQueue *>(0x1234));
    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false};
    cmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);
    cmdList->latestOperationHasCbEventWithProfiling = true;
    EXPECT_FALSE(events[1]->cbEventWithProfiling);
    cmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);
    EXPECT_EQ(reinterpret_cast<void *>(0x1234), events[1]->getLatestUsedCmdQueue());
    EXPECT_FALSE(events[1]->cbEventWithProfiling);

    EXPECT_LT(offset, cmdStream->getUsed());

    EXPECT_EQ(2u, events[1]->getInOrderExecBaseSignalValue());

    offset = cmdStream->getUsed();

    events[1]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    cmdList->appendBarrier(nullptr, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);
    cmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

    if (cmdList->dcFlushSupport) {
        EXPECT_NE(offset, cmdStream->getUsed());
        EXPECT_EQ(3u, events[1]->getInOrderExecBaseSignalValue());
    } else {
        EXPECT_EQ(offset, cmdStream->getUsed());
        EXPECT_EQ(2u, events[1]->getInOrderExecBaseSignalValue());
    }
}

HWTEST_F(InOrderCmdListTests, givenEventCounterReusedFromPreviousAppendWhenHostSynchronizeThenFlushCaches) {
    auto isHeaplessModeDisabled = !device->getCompilerProductHelper().isHeaplessModeEnabled(device->getHwInfo());
    auto cacheFlushRequired = device->getProductHelper().isDcFlushAllowed() && isHeaplessModeDisabled;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(2, false);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(1u, cmdList->inOrderExecInfo->getCounterValue());

    auto eventHandle = events[1]->toHandle();
    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    cmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    auto flushTagCount = cacheFlushRequired ? 1 : 0;
    EXPECT_EQ(flushTagCount, ultCsr->flushTagUpdateCalled);
    EXPECT_EQ(1u, events[0]->getInOrderExecBaseSignalValue());
}

HWTEST_F(InOrderCmdListTests, givenTsCbEventWhenAppendNonKernelOperationOnNonHeaplessNonDcFlushPlatformThenWaitOnCounter) {
    if (device->getProductHelper().isDcFlushAllowed()) {
        GTEST_SKIP();
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto cmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    if (cmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    CpuIntrinsicsTests::pauseCounter = 0u;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    cmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(1u, CpuIntrinsicsTests::pauseCounter);
    EXPECT_EQ(1u, events[0]->getInOrderExecBaseSignalValue());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithDifferentEventsThenDontInherit) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList1 = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(3, false);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto offset = cmdStream->getUsed();

    ze_event_handle_t waitlist[] = {events[0]->toHandle(), events[1]->toHandle()};

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList2->appendBarrier(events[2]->toHandle(), 2, waitlist, waitEventsParametersForBarrier, signalEventParameters);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor); // implicit dependency

    itor = find<MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor); // event0

    itor = find<MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList2->getCsr(false));
    const bool walkerPostSyncSkipped = immCmdList2->isWalkerPostSyncSkipEnabled && ultCsr->getNumClients() < 2u;
    EXPECT_EQ(walkerPostSyncSkipped ? 2u : 3u, events[2]->getInOrderExecBaseSignalValue());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAndTimestampEventThenSignalSyncAllocation) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto pcCmd = findInOrderCounterSignalPipeControl<FamilyType>(cmdList, inOrderExecInfo->getBaseDeviceAddress());
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_EQ(2u, pcCmd->getImmediateData());
    EXPECT_EQ(immCmdList->getDcFlushRequired(events[0]->isSignalScope()), pcCmd->getDcFlushEnable());
}

HWTEST_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAndRegularEventThenSignalSyncAllocation) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto eventHandle = events[0]->toHandle();

    CmdListWaitEventParameters waitEventsParametersForBarrier = {
        .outWaitCmds = nullptr,
        .relaxedOrderingAllowed = false,
        .trackDependencies = true,
        .waitForImplicitInOrderDependency = true,
        .skipAddingWaitEventsToResidency = false,
        .dualStreamCopyOffloadOperation = false,
    };
    CmdListSignalEventParameters signalEventParameters = {
        .relaxedOrderingDispatch = false,
    };
    immCmdList->appendBarrier(eventHandle, 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto pcCmd = findInOrderCounterSignalPipeControl<FamilyType>(cmdList, inOrderExecInfo->getBaseDeviceAddress());
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_EQ(2u, pcCmd->getImmediateData());
    EXPECT_EQ(immCmdList->getDcFlushRequired(events[0]->isSignalScope()), pcCmd->getDcFlushEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenCounterBasedEventWithoutPoolAllocationWhenAppendingSplitMemoryFillThenPostOperationBarrierFlushesDataCacheOnlyForHostScopeInHeaplessMode) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    constexpr size_t splitFillSize = 1000;
    auto hostAlloc = allocHostMem(MemoryConstants::pageSize);
    uint8_t pattern = 0;

    for (ze_event_scope_flags_t signalScope : {ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_DEVICE}) {
        ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
        counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE;
        counterBasedDesc.signal = signalScope;
        ze_event_handle_t eventHandle = nullptr;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &eventHandle));
        auto event = Event::fromHandle(eventHandle);
        ASSERT_TRUE(event->isCounterBased());
        ASSERT_EQ(nullptr, event->getAllocation(device));

        for (bool heaplessModeEnabled : {false, true}) {
            auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
            immCmdList->dcFlushSupport = true;
            immCmdList->heaplessModeEnabled = heaplessModeEnabled;

            auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
            auto offset = cmdStream->getUsed();

            CmdListMemoryCopyParams copyParams = {};
            EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendMemoryFill(hostAlloc, &pattern, sizeof(pattern), splitFillSize, eventHandle, 0, nullptr, copyParams));

            GenCmdList cmdList;
            ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                              ptrOffset(cmdStream->getCpuBase(), offset),
                                                              (cmdStream->getUsed() - offset)));

            auto walkers = findAll<WalkerType *>(cmdList.begin(), cmdList.end());
            ASSERT_LT(1u, walkers.size());

            auto pipeControls = findAll<PIPE_CONTROL *>(walkers.back(), cmdList.end());
            auto dataCacheFlushFound = std::any_of(pipeControls.begin(), pipeControls.end(), [](auto &itor) {
                return genCmdCast<PIPE_CONTROL *>(*itor)->getDcFlushEnable();
            });
            EXPECT_EQ(heaplessModeEnabled && (signalScope == ZE_EVENT_SCOPE_FLAG_HOST), dataCacheFlushFound);
        }

        zeEventDestroy(eventHandle);
    }

    context->freeMem(hostAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithHostVisibleEventThenSignalCounterWithStallingPipeControl) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(2, false);

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    events[1]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    events[1]->signalScope = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    const uint64_t deviceSyncVa = inOrderExecInfo->getBaseDeviceAddress();

    {
        auto offset = cmdStream->getUsed();
        CmdListWaitEventParameters waitEventsParametersForBarrier = {
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };
        CmdListSignalEventParameters signalEventParameters = {
            .relaxedOrderingDispatch = false,
        };
        immCmdList->appendBarrier(events[0]->toHandle(), 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto pcCmd = findInOrderCounterSignalPipeControl<FamilyType>(cmdList, deviceSyncVa);
        ASSERT_NE(nullptr, pcCmd);

        EXPECT_TRUE(pcCmd->getCommandStreamerStallEnable());
        EXPECT_EQ(2u, pcCmd->getImmediateData());
        EXPECT_EQ(immCmdList->getDcFlushRequired(events[0]->isSignalScope()), pcCmd->getDcFlushEnable());

        auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        for (auto &it : sdiCmds) {
            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*it);
            EXPECT_NE(deviceSyncVa, sdiCmd->getAddress());
        }
    }

    {
        auto offset = cmdStream->getUsed();
        CmdListWaitEventParameters waitEventsParametersForBarrier = {
            .outWaitCmds = nullptr,
            .relaxedOrderingAllowed = false,
            .trackDependencies = true,
            .waitForImplicitInOrderDependency = true,
            .skipAddingWaitEventsToResidency = false,
            .dualStreamCopyOffloadOperation = false,
        };
        CmdListSignalEventParameters signalEventParameters = {
            .relaxedOrderingDispatch = false,
        };
        immCmdList->appendBarrier(events[1]->toHandle(), 0, nullptr, waitEventsParametersForBarrier, signalEventParameters);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        EXPECT_EQ(nullptr, findInOrderCounterSignalPipeControl<FamilyType>(cmdList, deviceSyncVa));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenCallingSyncThenHandleCompletion) {
    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    if (!immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    uint64_t *hostAddress = nullptr;
    GraphicsAllocation *expectedAlloc = nullptr;
    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        expectedAlloc = immCmdList->inOrderExecInfo->getHostCounterAllocation(0);
        hostAddress = ptrOffset(immCmdList->inOrderExecInfo->getBaseHostAddress(0), counterOffset);
    } else {
        expectedAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
        hostAddress = static_cast<uint64_t *>(ptrOffset(expectedAlloc->getUnderlyingBuffer(), counterOffset));
    }

    *hostAddress = 0;

    GraphicsAllocation *downloadedAlloc = nullptr;
    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;
    bool forceFail = false;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation, uint64_t, size_t) {
        callCounter++;
        if (callCounter >= failCounter && !forceFail) {
            (*hostAddress)++;
        }
        downloadedAlloc = &graphicsAllocation;
    };

    // single check - not ready
    {
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
        EXPECT_EQ(downloadedAlloc, expectedAlloc);
        EXPECT_EQ(1u, callCounter);
        EXPECT_EQ(1u, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(0u, *hostAddress);
    }

    // timeout - not ready
    {
        forceFail = true;
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, expectedAlloc);
        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // gpu hang
    {
        ultCsr->forceReturnGpuHang = true;

        EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, expectedAlloc);

        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // success
    {
        ultCsr->checkGpuHangDetectedCalled = 0;
        ultCsr->forceReturnGpuHang = false;
        forceFail = false;
        callCounter = 0;
        immCmdList->getInOrderExecInfo()->addCounterValue(1);
        immCmdList->getInOrderExecInfo()->setProgrammedCounterValue(immCmdList->getInOrderExecInfo()->getCounterValue());
        EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(std::numeric_limits<uint64_t>::max(), false));
        EXPECT_EQ(downloadedAlloc, expectedAlloc);

        EXPECT_EQ(failCounter + 1, callCounter);
        EXPECT_EQ(failCounter, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(2u, *hostAddress);
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    *ultCsr->getTagAddress() = ultCsr->taskCount - 1;

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, true));

    *ultCsr->getTagAddress() = ultCsr->taskCount + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, true));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenDebugFlagSetWhenCallingSyncThenHandleCompletionOnHostAlloc) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    if (!immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    immCmdList->inOrderExecInfo->setAllocationOffset(counterOffset);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto mockAlloc = std::make_unique<MockGraphicsAllocation>();

    auto internalAllocStorage = ultCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(mockAlloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto hostAlloc = immCmdList->inOrderExecInfo->getHostCounterAllocation(0);

    auto hostAddress = static_cast<uint64_t *>(ptrOffset(hostAlloc->getUnderlyingBuffer(), counterOffset));
    *hostAddress = 0;

    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;
    bool forceFail = false;

    GraphicsAllocation *downloadedAlloc = nullptr;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation, uint64_t, size_t) {
        callCounter++;
        if (callCounter >= failCounter && !forceFail) {
            (*hostAddress)++;
        }
        downloadedAlloc = &graphicsAllocation;
    };

    // single check - not ready
    {
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
        EXPECT_EQ(downloadedAlloc, hostAlloc);
        EXPECT_EQ(1u, callCounter);
        EXPECT_EQ(1u, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(0u, *hostAddress);
    }

    // timeout - not ready
    {
        forceFail = true;
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, hostAlloc);
        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // gpu hang
    {
        ultCsr->forceReturnGpuHang = true;

        EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, immCmdList->hostSynchronize(10, false));
        EXPECT_EQ(downloadedAlloc, hostAlloc);
        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // success
    {
        immCmdList->getInOrderExecInfo()->addCounterValue(1);
        immCmdList->getInOrderExecInfo()->setProgrammedCounterValue(immCmdList->getInOrderExecInfo()->getCounterValue());

        ultCsr->checkGpuHangDetectedCalled = 0;
        ultCsr->forceReturnGpuHang = false;
        forceFail = false;
        callCounter = 0;
        EXPECT_EQ(downloadedAlloc, hostAlloc);
        EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(std::numeric_limits<uint64_t>::max(), false));

        EXPECT_EQ(failCounter + 1, callCounter);
        EXPECT_EQ(failCounter, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(2u, *hostAddress);
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    *ultCsr->getTagAddress() = ultCsr->taskCount - 1;

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, true));

    *ultCsr->getTagAddress() = ultCsr->taskCount + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, true));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenDoingCpuCopyThenSynchronize) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;
    if (!immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress(0);
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation, uint64_t, size_t) {
        callCounter++;
        if (callCounter >= failCounter) {
            (*hostAddress)++;
        }
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    events[0]->setIsCompleted();

    ultCsr->waitForCompletionWithTimeoutTaskCountCalled = 0;
    ultCsr->flushTagUpdateCalled = false;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 1, &eventHandle, copyParams);

    EXPECT_EQ(3u, callCounter);
    EXPECT_EQ(1u, *hostAddress);
    EXPECT_EQ(2u, ultCsr->checkGpuHangDetectedCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    context->freeMem(deviceAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenImmediateCmdListWhenDoingCpuCopyThenPassInfoToEvent) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());

    uint32_t hostCopyData = 0;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress(0);
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 3;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, copyParams);

    EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_EQ(0u, events[0]->getInOrderExecBaseSignalValue());
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_EQ(1u, events[0]->getInOrderExecBaseSignalValue());
    EXPECT_FALSE(events[0]->isAlreadyCompleted());

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, copyParams);

    EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_EQ(1u, events[0]->getInOrderExecBaseSignalValue());
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle, 0, nullptr, copyParams);

    EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_EQ(2u, events[0]->getInOrderExecBaseSignalValue());
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    context->freeMem(deviceAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenAubModeWhenSyncCalledAlwaysPollForCompletion) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::aub;
    auto eventPool = createEvents<FamilyType>(1, false);

    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        uint64_t *hostAddress = immCmdList->inOrderExecInfo->getBaseHostAddress(0);
        *hostAddress = 3;
    } else {
        auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
        auto hostAddress = static_cast<uint64_t *>(deviceAlloc->getUnderlyingBuffer());
        *hostAddress = 3;
    }

    *ultCsr->getTagAddress() = 3;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    immCmdList->hostSynchronize(0, false);

    auto expectPollForCompletion = immCmdList->latestFlushIsHostVisible && !immCmdList->latestOperationHasCbEventWithProfiling ? 1u : 0u;
    EXPECT_EQ(expectPollForCompletion++, ultCsr->pollForAubCompletionCalled);

    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(expectPollForCompletion++, ultCsr->pollForAubCompletionCalled);

    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::hardwareWithAub;

    events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(expectPollForCompletion, ultCsr->pollForAubCompletionCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenProfilingEventWhenDoingCpuCopyThenSetProfilingData) {
    debugManager.flags.ExperimentalForceCopyThroughLock.set(1);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto nonProfilingeventPool = createEvents<FamilyType>(1, false);
    auto profilingeventPool = createEvents<FamilyType>(1, true);

    auto eventHandle0 = events[0]->toHandle();
    auto eventHandle1 = events[1]->toHandle();

    EXPECT_FALSE(events[0]->getInOrderExecEventHelper().isDataAssigned());

    uint32_t hostCopyData = 0;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress(0);
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 3;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle0, 0, nullptr, copyParams);

    EXPECT_TRUE(events[0]->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_TRUE(events[0]->isAlreadyCompleted());

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, eventHandle1, 0, nullptr, copyParams);

    EXPECT_TRUE(events[1]->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_TRUE(events[1]->isAlreadyCompleted());
    EXPECT_NE(L0::Event::STATE_CLEARED, *static_cast<uint32_t *>(events[1]->getHostAddress()));

    context->freeMem(deviceAlloc);
}

HWTEST_F(InOrderCmdListTests, givenEventCreatedFromPoolWhenItIsQueriedForAddressItReturnsProperAddressFromPool) {
    auto eventPool = createEvents<FamilyType>(1, false);
    uint64_t counterValue = 0;
    uint64_t address = 0;

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetDeviceAddress(eventHandle, &counterValue, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetDeviceAddress(eventHandle, nullptr, &address));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetDeviceAddress(nullptr, &counterValue, &address));

    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(Event::State::STATE_SIGNALED, counterValue);
    EXPECT_EQ(address, events[0]->getCompletionFieldGpuAddress(events[0]->peekEventPool()->getDevice()));
}
HWTEST_F(InOrderCmdListTests, givenEventCreatedFromPoolWithTimestampsWhenQueriedForAddressErrorIsReturned) {
    auto eventPool = createEvents<FamilyType>(1, true);
    uint64_t counterValue = 0;
    uint64_t address = 0;

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetDeviceAddress(eventHandle, &counterValue, &address));
}

HWTEST_F(InOrderCmdListTests, givenCorrectInputParamsWhenCreatingCbEventThenReturnSuccess) {
    uint64_t counterValue = 2;

    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue, &eventDesc, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue, nullptr, &handle));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate(context, nullptr, gpuAddress, hostAddress, counterValue, &eventDesc, &handle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate(context, device, gpuAddress, &counterValue, counterValue, &eventDesc, &handle));
    zeEventDestroy(handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate(context, device, gpuAddress, nullptr, counterValue, &eventDesc, &handle));
    auto eventObj = Event::fromHandle(handle);
    EXPECT_FALSE(eventObj->getInOrderExecEventHelper().isDataAssigned());
    zeEventDestroy(handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate(context, device, nullptr, hostAddress, counterValue, &eventDesc, &handle));
    eventObj = Event::fromHandle(handle);
    EXPECT_FALSE(eventObj->getInOrderExecEventHelper().isDataAssigned());
    zeEventDestroy(handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate(context, device, nullptr, nullptr, counterValue, &eventDesc, &handle));
    eventObj = Event::fromHandle(handle);
    EXPECT_FALSE(eventObj->getInOrderExecEventHelper().isDataAssigned());
    zeEventDestroy(handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue, &eventDesc, &handle));

    eventObj = Event::fromHandle(handle);

    ASSERT_NE(nullptr, eventObj);
    EXPECT_TRUE(eventObj->getInOrderExecEventHelper().isDataAssigned());

    EXPECT_EQ(counterValue, eventObj->getInOrderExecEventHelper().getEventData()->counterValue);
    EXPECT_EQ(hostAddress, eventObj->getInOrderExecEventHelper().getBaseHostCpuAddress());
    EXPECT_EQ(0u, eventObj->getInOrderExecEventHelper().getBaseHostGpuAddress());
    EXPECT_EQ(castToUint64(gpuAddress), eventObj->getInOrderExecEventHelper().getBaseDeviceAddress());

    uint64_t address = 0;
    uint64_t value = 0;
    zeEventCounterBasedGetDeviceAddress(handle, &value, &address);

    EXPECT_EQ(address, eventObj->getInOrderExecEventHelper().getBaseDeviceAddress());
    EXPECT_EQ(value, counterValue);

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenCorrectInputParamsWhenCreatingCbEvent2ThenReturnSuccess) {
    uint64_t counterValue = 2;

    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    zex_counter_based_event_external_sync_alloc_properties_t externalSyncAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES};
    externalSyncAllocProperties.completionValue = counterValue;
    externalSyncAllocProperties.deviceAddress = gpuAddress;
    externalSyncAllocProperties.hostAddress = hostAddress;

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSyncAllocProperties;
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate2(context, nullptr, &counterBasedDesc, &handle));
    externalSyncAllocProperties.hostAddress = &counterValue;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));
    zeEventDestroy(handle);

    externalSyncAllocProperties.hostAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));
    zeEventDestroy(handle);

    externalSyncAllocProperties.hostAddress = hostAddress;
    externalSyncAllocProperties.deviceAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    externalSyncAllocProperties.hostAddress = nullptr;
    externalSyncAllocProperties.deviceAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    counterBasedDesc.pNext = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));
    auto eventObj = Event::fromHandle(handle);

    zeEventDestroy(handle);

    counterBasedDesc.pNext = &externalSyncAllocProperties;
    externalSyncAllocProperties.hostAddress = hostAddress;
    externalSyncAllocProperties.deviceAddress = gpuAddress;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &handle));

    eventObj = Event::fromHandle(handle);

    ASSERT_NE(nullptr, eventObj);
    EXPECT_TRUE(eventObj->getInOrderExecEventHelper().isDataAssigned());

    EXPECT_EQ(counterValue, eventObj->getInOrderExecEventHelper().getEventData()->counterValue);
    EXPECT_EQ(hostAddress, eventObj->getInOrderExecEventHelper().getBaseHostCpuAddress());
    EXPECT_EQ(0u, eventObj->getInOrderExecEventHelper().getBaseHostGpuAddress());
    EXPECT_EQ(castToUint64(gpuAddress), eventObj->getInOrderExecEventHelper().getBaseDeviceAddress());
    EXPECT_EQ(nullptr, eventObj->getInOrderExecEventHelper().getDeviceCounterAllocation());

    eventObj->resetCompletionStatus();
    eventObj->setIsCompleted();
    EXPECT_FALSE(eventObj->isAlreadyCompleted());

    uint64_t address = 0;
    uint64_t value = 0;
    zeEventCounterBasedGetDeviceAddress(handle, &value, &address);

    EXPECT_EQ(address, eventObj->getInOrderExecEventHelper().getBaseDeviceAddress());
    EXPECT_EQ(value, counterValue);

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenExpFlagsWhenComparingToCoreFlagsThenValuesAreTheSame) {
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE), static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE));
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE), static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE));
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_COUNTER_BASED_FLAG_HOST_VISIBLE), static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_HOST_VISIBLE));
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_COUNTER_BASED_FLAG_IPC), static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_IPC));
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP), static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP));
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP), static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_MAPPED_TIMESTAMP));
    EXPECT_EQ(sizeof(zex_ipc_counter_based_event_handle_t::data), sizeof(ze_ipc_event_counter_based_handle_t::data));
}

HWTEST_F(InOrderCmdListTests, givenCorrectInputParamsWhenCreatingCoreCbEventThenReturnSuccess) {
    uint64_t counterValue = 2;

    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_counter_based_external_sync_allocation_desc_t externalSyncAllocProperties = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
    externalSyncAllocProperties.completionValue = counterValue;
    externalSyncAllocProperties.deviceAddress = gpuAddress;
    externalSyncAllocProperties.hostAddress = hostAddress;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSyncAllocProperties;
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, device, &counterBasedDesc, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, nullptr, &counterBasedDesc, &handle));
    externalSyncAllocProperties.hostAddress = &counterValue;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    zeEventDestroy(handle);

    externalSyncAllocProperties.hostAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    zeEventDestroy(handle);

    externalSyncAllocProperties.hostAddress = hostAddress;
    externalSyncAllocProperties.deviceAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    externalSyncAllocProperties.hostAddress = nullptr;
    externalSyncAllocProperties.deviceAddress = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    counterBasedDesc.pNext = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    auto eventObj = Event::fromHandle(handle);

    zeEventDestroy(handle);

    counterBasedDesc.pNext = &externalSyncAllocProperties;
    externalSyncAllocProperties.hostAddress = hostAddress;
    externalSyncAllocProperties.deviceAddress = gpuAddress;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    eventObj = Event::fromHandle(handle);

    ASSERT_NE(nullptr, eventObj);
    EXPECT_TRUE(eventObj->getInOrderExecEventHelper().isDataAssigned());

    EXPECT_EQ(counterValue, eventObj->getInOrderExecEventHelper().getEventData()->counterValue);
    EXPECT_EQ(hostAddress, eventObj->getInOrderExecEventHelper().getBaseHostCpuAddress());
    EXPECT_EQ(0u, eventObj->getInOrderExecEventHelper().getBaseHostGpuAddress());
    EXPECT_EQ(castToUint64(gpuAddress), eventObj->getInOrderExecEventHelper().getBaseDeviceAddress());
    EXPECT_EQ(nullptr, eventObj->getInOrderExecEventHelper().getDeviceCounterAllocation());

    eventObj->resetCompletionStatus();
    eventObj->setIsCompleted();
    EXPECT_FALSE(eventObj->isAlreadyCompleted());

    uint64_t address = 0;
    uint64_t value = 0;
    zeEventCounterBasedGetDeviceAddress(handle, &value, &address);

    EXPECT_EQ(address, eventObj->getInOrderExecEventHelper().getBaseDeviceAddress());
    EXPECT_EQ(value, counterValue);

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenUsmDeviceAllocationWhenCreatingCbEventFromExternalMemoryThenAssignGraphicsAllocation) {
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));
    auto deviceAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));

    ze_event_counter_based_external_sync_allocation_desc_t externalSyncAllocProperties = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
    externalSyncAllocProperties.completionValue = 2;
    externalSyncAllocProperties.deviceAddress = deviceAddress;
    externalSyncAllocProperties.hostAddress = hostAddress;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSyncAllocProperties;

    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    auto eventObj = Event::fromHandle(handle);

    ASSERT_NE(nullptr, eventObj);
    EXPECT_TRUE(eventObj->getInOrderExecEventHelper().isDataAssigned());

    EXPECT_EQ(castToUint64(deviceAddress), eventObj->getInOrderExecEventHelper().getBaseDeviceAddress());
    EXPECT_NE(nullptr, eventObj->getInOrderExecEventHelper().getDeviceCounterAllocation());

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
    context->freeMem(deviceAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenCreatingCounterBasedEventThenSetAllParams) {
    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t) * 2));

    ze_event_counter_based_external_aggregate_storage_desc_t externalStorageAllocProperties = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_AGGREGATE_STORAGE_DESC};
    externalStorageAllocProperties.completionValue = counterValue;
    externalStorageAllocProperties.deviceAddress = nullptr;
    externalStorageAllocProperties.incrementValue = incValue;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalStorageAllocProperties;
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    externalStorageAllocProperties.deviceAddress = ptrOffset(devAddress, sizeof(uint64_t));
    externalStorageAllocProperties.incrementValue = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    externalStorageAllocProperties.incrementValue = incValue;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    auto eventObj = Event::fromHandle(handle);
    EXPECT_TRUE(eventObj->getInOrderExecEventHelper().isDataAssigned());

    auto &inOrderExecHelper = eventObj->getInOrderExecEventHelper();

    EXPECT_EQ(incValue, eventObj->getInOrderIncrementValue(1));
    EXPECT_EQ(counterValue, inOrderExecHelper.getEventData()->counterValue);
    EXPECT_EQ(castToUint64(externalStorageAllocProperties.deviceAddress), inOrderExecHelper.getBaseDeviceAddress());
    EXPECT_NE(nullptr, inOrderExecHelper.getDeviceCounterAllocation());
    SvmAllocationData *deviceAlloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(devAddress));
    auto offset = ptrDiff(devAddress, deviceAlloc->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress());
    auto lockedPtr = reinterpret_cast<uint64_t *>(ptrOffset(inOrderExecHelper.getDeviceCounterAllocation()->getLockedPtr(), sizeof(uint64_t) + offset));

    EXPECT_EQ(inOrderExecHelper.getBaseHostCpuAddress(), lockedPtr);
    EXPECT_EQ(inOrderExecHelper.getBaseHostGpuAddress(), inOrderExecHelper.getBaseDeviceAddress());
    EXPECT_EQ(inOrderExecHelper.getHostCounterAllocation(), inOrderExecHelper.getDeviceCounterAllocation());

    zeEventDestroy(handle);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenCreatingCounterBasedEventAndAdditionalTimestampNodeThenSetAdditionalParamsVectorCorrectly) {
    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto event = createAggregatedEvent(counterValue, incValue, devAddress);
    EXPECT_EQ(Event::State::HOST_CACHING_DISABLED_PERMANENT, event->isCompleted.load());
    event->isTimestampEvent = true;
    ASSERT_TRUE(event->getInOrderExecEventHelper().isDataAssigned());

    auto node0 = device->getDeviceInOrderCounterAllocator()->getTag();

    event->resetAdditionalTimestampNode(node0, 1, false);

    auto node1 = device->getDeviceInOrderCounterAllocator()->getTag();
    event->resetAdditionalTimestampNode(node1, 1, false);
    EXPECT_EQ(2u, event->inOrderExecHelper.getAdditionalTimestampNodesCount());

    event->resetAdditionalTimestampNode(nullptr, 1, true);
    EXPECT_EQ(0u, event->inOrderExecHelper.getAdditionalTimestampNodesCount());

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenCallingAppendThenDontResetInOrderExecInfo) {
    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t) * 2));

    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    auto handle = eventObj->toHandle();

    ASSERT_TRUE(eventObj->getInOrderExecEventHelper().isDataAssigned());

    auto &inOrderExecHelper = eventObj->getInOrderExecEventHelper();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    EXPECT_EQ(counterValue, inOrderExecHelper.getEventData()->counterValue);
    EXPECT_EQ(incValue, eventObj->getInOrderIncrementValue(1));

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenCallingAppendThenAggregateTimestampNodes) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());
    eventObj->setPacketsInUse(2);

    auto handle = eventObj->toHandle();

    auto immCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    ASSERT_EQ(1u, eventObj->inOrderExecHelper.getTimestampNodesCount());
    eventObj->setPacketsInUse(2);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    eventObj->setPacketsInUse(2);

    ASSERT_EQ(2u, eventObj->inOrderExecHelper.getTimestampNodesCount());

    auto node0 = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(0)->getCpuBase());
    auto node1 = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(1)->getCpuBase());

    TagSizeT packet00[4] = {1, 2, 3, 4};
    TagSizeT packet01[4] = {13, 14, 15, 16};

    TagSizeT packet10[4] = {5, 6, 7, 8};
    TagSizeT packet11[4] = {17, 18, 19, 20};

    node0->assignDataToAllTimestamps(0, packet00);
    node0->assignDataToAllTimestamps(1, packet01);
    node1->assignDataToAllTimestamps(0, packet10);
    node1->assignDataToAllTimestamps(1, packet11);

    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->queryKernelTimestamp(&result));

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();

    if (gfxCoreHelper.useOnlyGlobalTimestamps()) {
        EXPECT_EQ(2u, result.context.kernelStart);
    } else {
        EXPECT_EQ(1u, result.context.kernelStart);
    }
    EXPECT_EQ(2u, result.global.kernelStart);

    if (gfxCoreHelper.useOnlyGlobalTimestamps()) {
        EXPECT_EQ(20u, result.context.kernelEnd);
    } else {
        EXPECT_EQ(19u, result.context.kernelEnd);
    }
    EXPECT_EQ(20u, result.global.kernelEnd);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWithMoreThanPreferredPacketCountNodesWhenQueryKernelTimestampThenAllNodesAreFoldedWithoutOverflow) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    constexpr uint32_t nodeCount = NEO::TimestampPacketConstants::preferredPacketCount + 4;
    constexpr uint64_t counterValue = 100;
    constexpr uint64_t incValue = 1;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    for (uint32_t i = 0; i < nodeCount; i++) {
        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    }
    ASSERT_EQ(nodeCount, eventObj->inOrderExecHelper.getTimestampNodesCount());

    eventObj->setPacketsInUse(1);

    TagSizeT expectedContextStartMin = std::numeric_limits<TagSizeT>::max();
    TagSizeT expectedGlobalStartMin = std::numeric_limits<TagSizeT>::max();
    TagSizeT expectedContextEndMax = 0;
    TagSizeT expectedGlobalEndMax = 0;

    for (uint32_t i = 0; i < nodeCount; i++) {
        const TagSizeT contextStart = static_cast<TagSizeT>(500 + (i * 7) % 50);
        const TagSizeT globalStart = static_cast<TagSizeT>(1500 + (i * 11) % 70);
        const TagSizeT contextEnd = static_cast<TagSizeT>(2500 + (i * 13) % 90);
        const TagSizeT globalEnd = static_cast<TagSizeT>(3500 + (i * 17) % 110);

        const TagSizeT packet[4] = {contextStart, globalStart, contextEnd, globalEnd};
        auto node = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(i)->getCpuBase());
        node->assignDataToAllTimestamps(0, packet);

        expectedContextStartMin = std::min(expectedContextStartMin, contextStart);
        expectedGlobalStartMin = std::min(expectedGlobalStartMin, globalStart);
        expectedContextEndMax = std::max(expectedContextEndMax, contextEnd);
        expectedGlobalEndMax = std::max(expectedGlobalEndMax, globalEnd);
    }

    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->queryKernelTimestamp(&result));

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();

    EXPECT_EQ(static_cast<uint64_t>(expectedGlobalStartMin), result.global.kernelStart);
    EXPECT_EQ(static_cast<uint64_t>(expectedGlobalEndMax), result.global.kernelEnd);

    if (gfxCoreHelper.useOnlyGlobalTimestamps()) {
        EXPECT_EQ(static_cast<uint64_t>(expectedGlobalStartMin), result.context.kernelStart);
        EXPECT_EQ(static_cast<uint64_t>(expectedGlobalEndMax), result.context.kernelEnd);
    } else {
        EXPECT_EQ(static_cast<uint64_t>(expectedContextStartMin), result.context.kernelStart);
        EXPECT_EQ(static_cast<uint64_t>(expectedContextEndMax), result.context.kernelEnd);
    }

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWithOverflowingEndTimestampWhenQueryKernelTimestampThenWrappedEndIsSelected) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    constexpr uint64_t counterValue = 100;
    constexpr uint64_t incValue = 1;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    ASSERT_EQ(2u, eventObj->inOrderExecHelper.getTimestampNodesCount());

    eventObj->setPacketsInUse(1);

    auto node0 = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(0)->getCpuBase());
    auto node1 = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(1)->getCpuBase());

    const TagSizeT packet0[4] = {10, 10, 20, 20};
    const TagSizeT packet1[4] = {30, 100, 40, 50};
    node0->assignDataToAllTimestamps(0, packet0);
    node1->assignDataToAllTimestamps(0, packet1);

    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->queryKernelTimestamp(&result));

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();

    EXPECT_EQ(10u, result.global.kernelStart);
    EXPECT_EQ(50u, result.global.kernelEnd);

    if (!gfxCoreHelper.useOnlyGlobalTimestamps()) {
        EXPECT_EQ(10u, result.context.kernelStart);
        EXPECT_EQ(40u, result.context.kernelEnd);
    }

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWithAdditionalTimestampNodesWhenQueryKernelTimestampThenAdditionalNodesAreFolded) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    constexpr uint64_t counterValue = 100;
    constexpr uint64_t incValue = 1;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    ASSERT_EQ(1u, eventObj->inOrderExecHelper.getTimestampNodesCount());

    eventObj->resetAdditionalTimestampNode(device->getInOrderTimestampAllocator()->getTag(), 1, false);
    eventObj->resetAdditionalTimestampNode(device->getInOrderTimestampAllocator()->getTag(), 1, false);
    ASSERT_EQ(2u, eventObj->inOrderExecHelper.getAdditionalTimestampNodesCount());

    eventObj->setPacketsInUse(1);

    auto node = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(0)->getCpuBase());
    auto additionalNode0 = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getAdditionalTimestampNode(0)->getCpuBase());
    auto additionalNode1 = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getAdditionalTimestampNode(1)->getCpuBase());

    // MIN start comes from the first additional node and MAX end from the second one.
    const TagSizeT packet[4] = {20, 120, 30, 130};
    const TagSizeT additionalPacket0[4] = {10, 110, 25, 125};
    const TagSizeT additionalPacket1[4] = {22, 122, 40, 140};
    node->assignDataToAllTimestamps(0, packet);
    additionalNode0->assignDataToAllTimestamps(0, additionalPacket0);
    additionalNode1->assignDataToAllTimestamps(0, additionalPacket1);

    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->queryKernelTimestamp(&result));

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();

    EXPECT_EQ(110u, result.global.kernelStart);
    EXPECT_EQ(140u, result.global.kernelEnd);

    if (!gfxCoreHelper.useOnlyGlobalTimestamps()) {
        EXPECT_EQ(10u, result.context.kernelStart);
        EXPECT_EQ(40u, result.context.kernelEnd);
    }

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedStorageEventWhenCallingQueryTimestampsExpThenSingleFoldedResultIsReturned) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    constexpr uint64_t counterValue = 100;
    constexpr uint64_t incValue = 1;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    constexpr uint32_t nodeCount = 3;
    for (uint32_t i = 0; i < nodeCount; i++) {
        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    }
    ASSERT_EQ(nodeCount, eventObj->inOrderExecHelper.getTimestampNodesCount());

    eventObj->setPacketsInUse(1);

    const TagSizeT packet0[4] = {30, 130, 41, 141};
    const TagSizeT packet1[4] = {10, 110, 40, 140};
    const TagSizeT packet2[4] = {20, 120, 99, 199};
    static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(0)->getCpuBase())->assignDataToAllTimestamps(0, packet0);
    static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(1)->getCpuBase())->assignDataToAllTimestamps(0, packet1);
    static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(2)->getCpuBase())->assignDataToAllTimestamps(0, packet2);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->queryTimestampsExp(device, &count, nullptr));
    EXPECT_EQ(1u, count);

    ze_kernel_timestamp_result_t results[1] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->queryTimestampsExp(device, &count, results));
    EXPECT_EQ(1u, count);

    EXPECT_EQ(110u, results[0].global.kernelStart);
    EXPECT_EQ(199u, results[0].global.kernelEnd);
    EXPECT_EQ(10u, results[0].context.kernelStart);
    EXPECT_EQ(99u, results[0].context.kernelEnd);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenHostEventSetValueTimestampsCalledThenTimestampNodeIsNotOverwritten) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    constexpr uint64_t counterValue = 100;
    constexpr uint64_t incValue = 1;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventObj->toHandle(), 0, nullptr, launchParams);
    ASSERT_EQ(1u, eventObj->inOrderExecHelper.getTimestampNodesCount());
    ASSERT_TRUE(Event::isAggregatedEvent(eventObj.get()));

    ASSERT_NE(nullptr, eventObj->getAllocation(device));

    eventObj->setPacketsInUse(1);

    const TagSizeT seededPacket[4] = {11, 22, 33, 44};
    auto node = static_cast<NEO::TimestampPackets<TagSizeT, 1> *>(eventObj->inOrderExecHelper.getTimestampNode(0)->getCpuBase());
    node->assignDataToAllTimestamps(0, seededPacket);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, eventObj->hostSignal(true));
    EXPECT_FALSE(eventObj->isAlreadyCompleted());

    EXPECT_EQ(11u, node->getContextStartValue(0));
    EXPECT_EQ(22u, node->getGlobalStartValue(0));
    EXPECT_EQ(33u, node->getContextEndValue(0));
    EXPECT_EQ(44u, node->getGlobalEndValue(0));

    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->queryKernelTimestamp(&result));
    EXPECT_EQ(22u, result.global.kernelStart);
    EXPECT_EQ(44u, result.global.kernelEnd);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenCallingAppendThenHandleResidency) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->storeMakeResidentAllocations = true;
    ultCsr->residencyContainerDuplicateRemovalRequired = true;

    constexpr uint64_t counterValue = 4;
    constexpr uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    auto node0 = eventObj->inOrderExecHelper.getTimestampNode(0);
    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[node0->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()]);

    while (device->getInOrderTimestampAllocator()->getGfxAllocations().size() == 1) {
        device->getInOrderTimestampAllocator()->getTag();
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    ASSERT_EQ(2u, eventObj->inOrderExecHelper.getTimestampNodesCount());

    auto node1 = eventObj->inOrderExecHelper.getTimestampNode(1);

    EXPECT_NE(node0->getBaseGraphicsAllocation(), node1->getBaseGraphicsAllocation());

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[node1->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()]);

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenCallingAppendMoreThanCounterValueThenDontResetNodes) {
    using TagSizeT = typename FamilyType::TimestampPacketType;

    constexpr uint64_t counterValue = 4;
    constexpr uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    ASSERT_EQ(3u, eventObj->inOrderExecHelper.getTimestampNodesCount());

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenCallingSplitAppendThenSetCorrectGpuVa) {
    using TagSizeT = typename FamilyType::TimestampPacketType;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    constexpr uint64_t counterValue = 4;
    constexpr uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();

    const size_t ptrBaseSize = 128;
    const size_t copyOffset = 1;
    auto alignedPtr = reinterpret_cast<void *>(0x100000);
    auto unalignedPtr = ptrOffset(alignedPtr, copyOffset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - copyOffset, handle, 0, nullptr, copyParams);

    ASSERT_EQ(1u, eventObj->inOrderExecHelper.getTimestampNodesCount());
    auto expectedAddress0 = eventObj->inOrderExecHelper.getTimestampNode(0)->getGpuAddress() + eventObj->getGlobalStartOffset();

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
        auto srmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

        EXPECT_EQ(expectedAddress0, srmCmd->getMemoryAddress());
    }

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - copyOffset, handle, 0, nullptr, copyParams);
    ASSERT_EQ(2u, eventObj->inOrderExecHelper.getTimestampNodesCount());
    auto expectedAddress1 = eventObj->inOrderExecHelper.getTimestampNode(1)->getGpuAddress() + eventObj->getGlobalStartOffset();

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
        auto srmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

        EXPECT_EQ(expectedAddress1, srmCmd->getMemoryAddress());
    }

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventWhenCallingAppendSignalInOrderDependencyCounterThenProgramAtomicOperation) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODE = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODE;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    constexpr uint64_t incValue = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1234;
    const uint64_t counterValue = device->getL0GfxCoreHelper().getCounterBasedEventMaxValue();

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    immCmdList->inOrderAtomicSignalingEnabled = false;
    immCmdList->appendSignalInOrderDependencyCounter(eventObj.get(), false, false, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto it = find<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), it);

    auto miAtomic = genCmdCast<MI_ATOMIC *>(*it);
    EXPECT_EQ(ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, miAtomic->getAtomicOpcode());
    EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());
    EXPECT_EQ(getLowPart(incValue), miAtomic->getOperand1DataDword0());
    EXPECT_EQ(getHighPart(incValue), miAtomic->getOperand1DataDword1());

    EXPECT_EQ(castToUint64(devAddress), miAtomic->getMemoryAddress());

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenCopyOnlyCmdListAndDebugFlagWhenCounterSignaledThenProgramMiFlush) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    immCmdList->appendSignalInOrderDependencyCounter(nullptr, false, false, false, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        if (immCmdList->useAdditionalBlitProperties) {
            auto it = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

            EXPECT_EQ(cmdList.end(), it);
        } else {
            auto it = cmdList.begin();
            if (BlitCommandsHelper<FamilyType>::isDummyBlitWaNeeded(immCmdList->dummyBlitWa)) {
                it++;
            }

            auto miFlush = genCmdCast<MI_FLUSH_DW *>(*it);
            EXPECT_NE(nullptr, miFlush);
        }
    }

    debugManager.flags.InOrderCopyMiFlushSync.set(0);

    offset = cmdStream->getUsed();
    immCmdList->appendSignalInOrderDependencyCounter(nullptr, false, false, false, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto it = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), it);
    }
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventAndCopyOnlyCmdListWhenCallingAppendMemoryCopyWithDisabledInOrderSignalingThenSignalAtomicStorage) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODE = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODE;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    constexpr uint64_t incValue = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1234;
    const uint64_t counterValue = device->getL0GfxCoreHelper().getCounterBasedEventMaxValue();

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    auto eventObj = createAggregatedEvent(counterValue, incValue, devAddress);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();
    uint32_t copyData = 0;
    copyParams.forceDisableCopyOnlyInOrderSignaling = true;

    {
        immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventObj->toHandle(), 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto it = find<MI_ATOMIC *>(cmdList.begin(), cmdList.end());

        if (immCmdList->useAdditionalBlitProperties) {
            EXPECT_EQ(cmdList.end(), it);
        } else {
            ASSERT_NE(cmdList.end(), it);

            auto miAtomic = genCmdCast<MI_ATOMIC *>(*it);
            EXPECT_EQ(ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, miAtomic->getAtomicOpcode());
            EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());
            EXPECT_EQ(getLowPart(incValue), miAtomic->getOperand1DataDword0());
            EXPECT_EQ(getHighPart(incValue), miAtomic->getOperand1DataDword1());

            EXPECT_EQ(castToUint64(devAddress), miAtomic->getMemoryAddress());
        }
    }

    offset = cmdStream->getUsed();

    {
        ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

        immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, eventObj->toHandle(), 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto it = find<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
        if (immCmdList->useAdditionalBlitProperties) {
            EXPECT_EQ(cmdList.end(), it);
        } else {
            ASSERT_NE(cmdList.end(), it);

            auto miAtomic = genCmdCast<MI_ATOMIC *>(*it);
            EXPECT_EQ(ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, miAtomic->getAtomicOpcode());
            EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());
            EXPECT_EQ(getLowPart(incValue), miAtomic->getOperand1DataDword0());
            EXPECT_EQ(getHighPart(incValue), miAtomic->getOperand1DataDword1());

            EXPECT_EQ(castToUint64(devAddress), miAtomic->getMemoryAddress());
        }
    }

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenTimestmapEnabledWhenCreatingStandaloneCbEventThenSetCorrectPacketSize) {
    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP;

    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    auto eventObj = Event::fromHandle(handle);

    using TagSizeT = typename FamilyType::TimestampPacketType;
    constexpr auto singlePacetSize = NEO::TimestampPackets<TagSizeT, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();

    EXPECT_EQ(singlePacetSize, eventObj->getSinglePacketSize());

    zeEventDestroy(handle);
}

HWTEST_F(InOrderCmdListTests, givenStandaloneEventWhenCallingSynchronizeThenReturnCorrectValue) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_handle_t handle = nullptr;

    ze_event_counter_based_external_sync_allocation_desc_t externalSync = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
    externalSync.deviceAddress = gpuAddress;
    externalSync.hostAddress = hostAddress;
    externalSync.completionValue = counterValue + 1;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSync;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    auto eventObj = Event::fromHandle(handle);

    EXPECT_EQ(ZE_RESULT_NOT_READY, eventObj->hostSynchronize(1));

    (*hostAddress)++;

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->hostSynchronize(1));

    zeEventDestroy(handle);

    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenMitigateHostVisibleSignalWhenCallingSynchronizeOnCbEventThenFlushDcIfSupported) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.MitigateHostVisibleSignal.set(true);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));
    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);
    ze_event_handle_t handle = nullptr;

    ze_event_counter_based_external_sync_allocation_desc_t externalSync = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
    externalSync.deviceAddress = gpuAddress;
    externalSync.hostAddress = hostAddress;
    externalSync.completionValue = counterValue;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    counterBasedDesc.pNext = &externalSync;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    auto eventObj = Event::fromHandle(handle);

    EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    ultCsr->taskCount++;
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->hostSynchronize(-1));

    if (device->getProductHelper().isDcFlushAllowed() && !ultCsr->heaplessModeEnabled) {
        EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
        EXPECT_TRUE(ultCsr->flushTagUpdateCalled);
    } else {
        EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
        EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    }

    // Verify that repeated host synchronize calls do not flush again
    ultCsr->waitForTaskCountCalled = false;
    ultCsr->flushTagUpdateCalled = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->hostSynchronize(-1));
    if (device->getProductHelper().isDcFlushAllowed() && !ultCsr->heaplessModeEnabled) {
        EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
    } else {
        EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
    }
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    zeEventDestroy(handle);
    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenCbEventsSignaledByImmCmdListWhenOtherWorkIsSubmittedBeforeHostSynchronizeThenCacheFlushWaitsOnlyForEventSubmission) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    if (!device->getProductHelper().isDcFlushAllowed() || immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    ultCsr->waitForTaskCountReturnValue = WaitStatus::ready;
    auto eventPool = createEvents<FamilyType>(2, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams));
    const auto firstEventSubmissionTaskCount = ultCsr->peekTaskCount();
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams));
    const auto secondEventSubmissionTaskCount = ultCsr->peekTaskCount();
    EXPECT_LT(firstEventSubmissionTaskCount, secondEventSubmissionTaskCount);

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getBaseHostAddress(0));
    *hostAddress = immCmdList->inOrderExecInfo->getCounterValue();

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[1]->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
    EXPECT_EQ(secondEventSubmissionTaskCount, ultCsr->latestWaitForTaskCountValue);

    ultCsr->taskCount += 4;
    ultCsr->flushTagUpdateCalled = false;
    ultCsr->waitForTaskCountCalled = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
    EXPECT_EQ(firstEventSubmissionTaskCount, ultCsr->latestWaitForTaskCountValue);
}

HWTEST_F(InOrderCmdListTests, givenCbEventSignaledByImmCmdListAndThenByRegularCmdListWhenHostSynchronizingThenCacheFlushWaitsForLatestCsrTaskCount) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    if (!device->getProductHelper().isDcFlushAllowed() || immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    ultCsr->waitForTaskCountReturnValue = WaitStatus::ready;
    auto eventPool = createEvents<FamilyType>(1, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams));

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams));
    regularCmdList->close();

    auto hostAddress = static_cast<uint64_t *>(regularCmdList->inOrderExecInfo->getBaseHostAddress(0));
    *hostAddress = regularCmdList->inOrderExecInfo->getCounterValue();

    ultCsr->taskCount += 4;
    ultCsr->flushTagUpdateCalled = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_TRUE(ultCsr->flushTagUpdateCalled);
    EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
    EXPECT_EQ(ultCsr->peekLatestFlushedTaskCount(), ultCsr->latestWaitForTaskCountValue);
}

HWTEST_F(InOrderCmdListTests, givenCbEventSignaledByImmCmdListWhenInOrderStateIsReattachedThenHostSynchronizeFlushesCacheForLatestCsrTaskCount) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    if (!device->getProductHelper().isDcFlushAllowed() || immCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    ultCsr->waitForTaskCountReturnValue = WaitStatus::ready;
    auto eventPool = createEvents<FamilyType>(2, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams));

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams));
    regularCmdList->close();

    events[0]->updateInOrdeState(events[1]->getInOrderExecEventHelper());

    auto hostAddress = static_cast<uint64_t *>(regularCmdList->inOrderExecInfo->getBaseHostAddress(0));
    *hostAddress = regularCmdList->inOrderExecInfo->getCounterValue();

    ultCsr->taskCount += 4;
    ultCsr->flushTagUpdateCalled = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_TRUE(ultCsr->flushTagUpdateCalled);
    EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
    EXPECT_EQ(ultCsr->peekLatestFlushedTaskCount(), ultCsr->latestWaitForTaskCountValue);
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedTimestampHostVisibleSignalWhenCallingSynchronizeOnCbEventThenFlushDcIfSupported) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP | ZE_EVENT_COUNTER_BASED_FLAG_HOST_VISIBLE;
    counterBasedDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    auto eventObj = Event::fromHandle(handle);
    *static_cast<Event::State *>(ptrOffset(eventObj->getHostAddress(), eventObj->getContextEndOffset())) = Event::State::STATE_SIGNALED;
    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getBaseHostAddress(0));
    *hostAddress = 1u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObj->hostSynchronize(-1));

    if (device->getProductHelper().isDcFlushAllowed() && !immCmdList->isHeaplessModeEnabled()) {
        EXPECT_TRUE(ultCsr->waitForTaskCountCalled);
        EXPECT_TRUE(ultCsr->flushTagUpdateCalled);
    } else {
        EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
        EXPECT_FALSE(ultCsr->flushTagUpdateCalled);
    }

    zeEventDestroy(handle);
}

HWTEST_F(InOrderCmdListTests, givenCachedTimestampWhenCallingSynchronizeOnRegularEventThenFlushDcIfSupported) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event.get(), 0, nullptr, launchParams);
    EXPECT_FALSE(ultCsr->waitForTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    *static_cast<Event::State *>(ptrOffset(event->getHostAddress(), event->getContextStartOffset())) = Event::State::STATE_SIGNALED;
    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getBaseHostAddress(0));
    *hostAddress = 1u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(-1));
    auto expectCacheFlush = device->getProductHelper().isNonCoherentTimestampsModeEnabled() &&
                            device->getProductHelper().isDcFlushAllowed() &&
                            !immCmdList->isHeaplessModeEnabled();
    EXPECT_EQ(expectCacheFlush, ultCsr->waitForTaskCountCalled);
    EXPECT_EQ(expectCacheFlush, ultCsr->flushTagUpdateCalled);
}

HWTEST_F(InOrderCmdListTests, givenStandaloneCbEventWhenPassingExternalInterruptIdThenAssign) {
    ze_event_sync_mode_desc_t syncModeDesc = {ZE_STRUCTURE_TYPE_EVENT_SYNC_MODE_DESC};
    syncModeDesc.externalInterruptId = 123;

    auto mockProductHelper = new MockProductHelper;
    mockProductHelper->isInterruptSupportedResult = true;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(mockProductHelper);

    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_SIGNAL_INTERRUPT;
    auto event1 = createStandaloneCbEvent(reinterpret_cast<const ze_base_desc_t *>(&syncModeDesc));
    EXPECT_EQ(NEO::InterruptId::notUsed, event1->externalInterruptId);
    EXPECT_FALSE(event1->isKmdWaitModeEnabled());

    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_EXTERNAL_INTERRUPT_WAIT;
    auto event2 = createStandaloneCbEvent(reinterpret_cast<const ze_base_desc_t *>(&syncModeDesc));
    EXPECT_EQ(syncModeDesc.externalInterruptId, event2->externalInterruptId);
    EXPECT_TRUE(event2->isKmdWaitModeEnabled());

    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_EXTERNAL_INTERRUPT_WAIT | ZE_EVENT_SYNC_MODE_FLAG_LOW_POWER_WAIT;
    auto event3 = createStandaloneCbEvent(reinterpret_cast<const ze_base_desc_t *>(&syncModeDesc));
    EXPECT_EQ(syncModeDesc.externalInterruptId, event3->externalInterruptId);
    EXPECT_TRUE(event3->isKmdWaitModeEnabled());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenStandaloneEventWhenCallingAppendThenSuccess) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;
    ze_event_handle_t eHandle3 = nullptr;

    auto createCbEvent = [&](ze_event_handle_t &outHandle) {
        ze_event_counter_based_external_sync_allocation_desc_t externalSync = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
        externalSync.deviceAddress = gpuAddress;
        externalSync.hostAddress = hostAddress;
        externalSync.completionValue = counterValue + 1;

        ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
        desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
        desc.pNext = &externalSync;

        return zeEventCounterBasedCreate(context, device, &desc, &outHandle);
    };

    EXPECT_EQ(ZE_RESULT_SUCCESS, createCbEvent(eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, createCbEvent(eHandle2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, createCbEvent(eHandle3));

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, copyParams);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, copyParams);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eHandle3, 0, nullptr, launchParams);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    zeEventDestroy(eHandle3);
    context->freeMem(hostAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenStandaloneEventAndKernelSplitWhenCallingAppendThenSuccess) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;

    auto createCbEvent = [&](ze_event_handle_t &outHandle) {
        ze_event_counter_based_external_sync_allocation_desc_t externalSync = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
        externalSync.deviceAddress = gpuAddress;
        externalSync.hostAddress = hostAddress;
        externalSync.completionValue = counterValue + 1;

        ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
        desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
        desc.pNext = &externalSync;

        return zeEventCounterBasedCreate(context, device, &desc, &outHandle);
    };

    EXPECT_EQ(ZE_RESULT_SUCCESS, createCbEvent(eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, createCbEvent(eHandle2));

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eHandle1, 0, nullptr, copyParams);
    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 1, &eHandle2, copyParams);

    alignedFree(alignedPtr);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenStandaloneEventAndCopyOnlyCmdListWhenCallingAppendThenSuccess) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;

    auto createCbEvent = [&](ze_event_handle_t &outHandle) {
        ze_event_counter_based_external_sync_allocation_desc_t externalSync = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
        externalSync.deviceAddress = gpuAddress;
        externalSync.hostAddress = hostAddress;
        externalSync.completionValue = counterValue + 1;

        ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
        desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
        desc.pNext = &externalSync;

        return zeEventCounterBasedCreate(context, device, &desc, &outHandle);
    };

    EXPECT_EQ(ZE_RESULT_SUCCESS, createCbEvent(eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, createCbEvent(eHandle2));

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    auto immCmdList = createCopyOnlyImmCmdList<FamilyType::gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, copyParams);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, copyParams);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedEventAndAdditionalTimestampNodeAndResetCalledThenVectorIsCleared) {
    auto eventPool = createEvents<FamilyType>(1, true);

    auto node0 = device->getDeviceInOrderCounterAllocator()->getTag();

    events[0]->resetAdditionalTimestampNode(node0, 1, false);
    EXPECT_EQ(1u, events[0]->inOrderExecHelper.getAdditionalTimestampNodesCount());

    events[0]->resetAdditionalTimestampNode(nullptr, 1, true);
    EXPECT_EQ(0u, events[0]->inOrderExecHelper.getAdditionalTimestampNodesCount());
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedEventWhenAskingForEventAddressAndValueThenReturnCorrectValues) {
    auto eventPool = createEvents<FamilyType>(1, false);
    uint64_t counterValue = -1;
    uint64_t address = -1;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto deviceAlloc = cmdList->inOrderExecInfo->getDeviceCounterAllocation();

    auto eventHandle = events[0]->toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedGetDeviceAddress(eventHandle, &counterValue, &address));

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(2u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress(), address);

    cmdList->close();

    ze_command_queue_desc_t desc = {};
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);

    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(2u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress(), address);

    static_cast<WhiteboxInOrderExecEventHelper &>(events[0]->inOrderExecHelper).getInOrderExecEventDataPtr()->counterOffset = 0x12300;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetDeviceAddress(eventHandle, &counterValue, &address));
    EXPECT_EQ(2u, counterValue);
    EXPECT_EQ(deviceAlloc->getGpuAddress() + events[0]->getInOrderAllocationOffset(), address);
}

HWTEST_F(InOrderCmdListTests, givenIncorrectArgumentswhenCallingZexDeviceGetAggregatedCopyOffloadIncrementValueThenReturnError) {
    uint32_t incValue = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexDeviceGetAggregatedCopyOffloadIncrementValue(device->toHandle(), nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zexDeviceGetAggregatedCopyOffloadIncrementValue(nullptr, &incValue));
}

HWTEST_F(InOrderCmdListTests, givenIncorrectArgumentswhenCallingZeDeviceGetAggregatedCopyOffloadIncrementValueThenReturnError) {
    uint32_t incValue = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeDeviceGetAggregatedCopyOffloadIncrementValue(device->toHandle(), nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeDeviceGetAggregatedCopyOffloadIncrementValue(nullptr, &incValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenGpuHangDetectedInCpuCopyPathThenReportError) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto eventPool = createEvents<FamilyType>(1, false);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress(0);
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;

    ultCsr->forceReturnGpuHang = true;
    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;
    ultCsr->callBaseWaitForCompletionWithTimeout = false;

    auto status = immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, status);

    ultCsr->forceReturnGpuHang = false;

    *hostAddress = std::numeric_limits<uint64_t>::max();

    context->freeMem(deviceAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithoutEventThenAddBarrierAndSignalCounter) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = reinterpret_cast<void *>(0x100000);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto cmdItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pcCmd->getPostSyncOperation());

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));

    while (sdiCmd == nullptr && cmdItor != cmdList.end()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));
    }

    ASSERT_NE(nullptr, sdiCmd);
    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenDebugFlagSetWhenKernelSplitIsExpectedThenDontSplit) {
    debugManager.flags.ForceNonWalkerSplitMemoryCopy.set(1);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = reinterpret_cast<void *>(0x100000);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, walkers.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithEventThenSignalCounter) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = reinterpret_cast<void *>(0x100000);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_FALSE(sdiCmds.empty());
    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiCmds[0]);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();
    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithProfilingEventThenSignalCounter) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = reinterpret_cast<void *>(0x100000);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto cmdItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pcCmd);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));

    while (sdiCmd == nullptr && cmdItor != cmdList.end()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));
    }

    ASSERT_NE(nullptr, sdiCmd);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t syncVa = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress(0)) : inOrderExecInfo->getBaseDeviceAddress();
    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenImplicitScalingEnabledWhenAskingForExtensionsThenReturnSyncDispatchExtension) {
    uint32_t count = 0;
    ze_result_t res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_driver_extension_properties_t> extensionProperties(count);

    res = driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &param) {
        return (strcmp(param.name, ZE_SYNCHRONIZED_DISPATCH_EXP_NAME) == 0);
    });

    EXPECT_EQ(extensionProperties.end(), it);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, InOrderCmdListTests, givenInOrderModeAndNoopWaitEventsAllowedWhenEventBoundToCmdListThenNoopSpaceForWaitCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    char noopedLriBuffer[sizeof(MI_LOAD_REGISTER_IMM)] = {};
    memset(noopedLriBuffer, 0, sizeof(MI_LOAD_REGISTER_IMM));
    char noopedSemWaitBuffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    memset(noopedSemWaitBuffer, 0, sizeof(MI_SEMAPHORE_WAIT));

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->allowCbWaitEventsNoopDispatch = true;

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    CommandToPatchContainer outCbWaitEventCmds;
    launchParams.outListCommands = &outCbWaitEventCmds;
    result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    const bool useSemaphore64bCmd = regularCmdList->getDevice()->getDeviceInfo().semaphore64bCmdSupport;
    size_t expectedLoadRegImmCount = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, useSemaphore64bCmd) ? 2 : 0;

    size_t expectedWaitCmds = 1 + expectedLoadRegImmCount;
    ASSERT_EQ(expectedWaitCmds, outCbWaitEventCmds.size());

    size_t outCbWaitEventCmdsIndex = 0;
    for (; outCbWaitEventCmdsIndex < expectedLoadRegImmCount; outCbWaitEventCmdsIndex++) {
        auto *cmd = std::get_if<PatchCbWaitEventLoadRegisterImm>(&outCbWaitEventCmds[outCbWaitEventCmdsIndex]);

        ASSERT_NE(nullptr, cmd);
        auto registerNumber = 0x2600 + (4 * outCbWaitEventCmdsIndex);
        EXPECT_EQ(registerNumber, cmd->offset);

        ASSERT_NE(nullptr, cmd->pDestination);
        auto memCmpRet = memcmp(cmd->pDestination, noopedLriBuffer, sizeof(MI_LOAD_REGISTER_IMM));
        EXPECT_EQ(0, memCmpRet);
    }

    auto *cmd = std::get_if<PatchCbWaitEventSemaphoreWait>(&outCbWaitEventCmds[outCbWaitEventCmdsIndex]);
    ASSERT_NE(nullptr, cmd);

    ASSERT_NE(nullptr, cmd->pDestination);
    auto memCmpRet = memcmp(cmd->pDestination, noopedSemWaitBuffer, sizeof(MI_SEMAPHORE_WAIT));
    EXPECT_EQ(0, memCmpRet);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            InOrderCmdListTests,
            givenInOrderModeAndNoopWaitEventsAllowedWhenEmptyTimestampEventUsedThenNoopSpaceForWaitCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    char noopedSemWaitBuffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    memset(noopedSemWaitBuffer, 0, sizeof(MI_SEMAPHORE_WAIT));

    char noopedLriBuffer[sizeof(MI_LOAD_REGISTER_IMM)] = {};
    memset(noopedLriBuffer, 0, sizeof(MI_LOAD_REGISTER_IMM));

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->allowCbWaitEventsNoopDispatch = true;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    CommandToPatchContainer outCbWaitEventCmds;
    launchParams.outListCommands = &outCbWaitEventCmds;
    auto result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    const bool useSemaphore64bCmd = this->device->getDeviceInfo().semaphore64bCmdSupport;
    const bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, useSemaphore64bCmd);

    size_t expectedWaitCmds = 1;
    if (regularCmdList->isHeaplessModeEnabled() && qwordIndirect) {
        expectedWaitCmds += 2;
    }
    ASSERT_EQ(expectedWaitCmds, outCbWaitEventCmds.size());

    size_t cmdIndex = 0;

    if (regularCmdList->isHeaplessModeEnabled()) {
        if (qwordIndirect) {
            for (; cmdIndex < 2; cmdIndex++) {
                auto *cmd = std::get_if<PatchCbWaitEventLoadRegisterImm>(&outCbWaitEventCmds[cmdIndex]);
                ASSERT_NE(nullptr, cmd);
                ASSERT_NE(nullptr, cmd->pDestination);
                auto memCmpRet = memcmp(cmd->pDestination, noopedLriBuffer, sizeof(MI_LOAD_REGISTER_IMM));
                EXPECT_EQ(0, memCmpRet);
            }
        }
        auto *cmd = std::get_if<PatchCbWaitEventSemaphoreWait>(&outCbWaitEventCmds[cmdIndex]);
        ASSERT_NE(nullptr, cmd);
        ASSERT_NE(nullptr, cmd->pDestination);
        auto memCmpRet = memcmp(cmd->pDestination, noopedSemWaitBuffer, sizeof(MI_SEMAPHORE_WAIT));
        EXPECT_EQ(0, memCmpRet);
    } else {
        auto *cmd = std::get_if<PatchWaitEventSemaphoreWait>(&outCbWaitEventCmds[cmdIndex]);
        ASSERT_NE(nullptr, cmd);
        ASSERT_NE(nullptr, cmd->pDestination);
        auto memCmpRet = memcmp(cmd->pDestination, noopedSemWaitBuffer, sizeof(MI_SEMAPHORE_WAIT));
        EXPECT_EQ(0, memCmpRet);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            InOrderCmdListTests,
            givenInOrderModeAndNoopWaitEventsDisallowedWhenEmptyTimestampEventUsedThenNoDispatchNoopSpaceForWaitCommands) {
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->allowCbWaitEventsNoopDispatch = false;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    CommandToPatchContainer outCbWaitEventCmds;
    launchParams.outListCommands = &outCbWaitEventCmds;
    auto result = regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    size_t expectedWaitCmds = 0;
    ASSERT_EQ(expectedWaitCmds, outCbWaitEventCmds.size());
}

HWTEST_F(InOrderCmdListTests, givenAggregatedEventBoundToCmdListWhenCheckingCanSkipWaitThenReturnFalse) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto aggregatedEvent = createAggregatedEvent(counterValue, incValue, devAddress);

    EXPECT_TRUE(Event::isAggregatedEvent(aggregatedEvent.get()));
    EXPECT_TRUE(aggregatedEvent->isCounterBased());

    auto aggregatedEventHandle = aggregatedEvent->toHandle();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, aggregatedEventHandle, 0, nullptr, launchParams);
    EXPECT_FALSE(immCmdList->canSkipInOrderEventWait(*aggregatedEvent, false));

    auto eventPool = createEvents<FamilyType>(1, false);
    auto regularEventHandle = events[0]->toHandle();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, regularEventHandle, 0, nullptr, launchParams);
    EXPECT_TRUE(immCmdList->canSkipInOrderEventWait(*events[0], false));

    context->freeMem(devAddress);
}

HWTEST_F(InOrderCmdListTests, givenCounterBasedEventWhenAppendingLaunchKernelMultipleTimesThenAllocateUserFenceOnceCorrectly) {
    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP | ZE_EVENT_COUNTER_BASED_FLAG_HOST_VISIBLE;
    counterBasedDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(ultCsr->userFenceAllocationAttemptCount, 0u);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    EXPECT_EQ(ultCsr->userFenceAllocationAttemptCount, 0u);

    auto mockProductHelper = new MockProductHelper;
    mockProductHelper->isInterruptSupportedResult = true;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(mockProductHelper);

    ze_event_sync_mode_desc_t syncModeDesc = {ZE_STRUCTURE_TYPE_EVENT_SYNC_MODE_DESC};
    syncModeDesc.syncModeFlags = ZE_EVENT_SYNC_MODE_FLAG_SIGNAL_INTERRUPT;
    counterBasedDesc.pNext = &syncModeDesc;

    ze_event_handle_t handleWithInterruptHint = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handleWithInterruptHint));

    if (ImplicitScalingHelper::isImplicitScalingEnabled(device->getNEODevice()->getDeviceBitfield(), true) || !immCmdList->duplicatedInOrderCounterStorageEnabled) {
        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handleWithInterruptHint, 0, nullptr, launchParams);
        EXPECT_EQ(ultCsr->userFenceAllocationAttemptCount, 0u);
    } else {
        debugManager.flags.EnableWalkerPartition.set(1);
        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handleWithInterruptHint, 0, nullptr, launchParams);
        EXPECT_EQ(ultCsr->userFenceAllocationAttemptCount, 0u);

        const uint32_t expectedTimestampEventUserFenceAllocations = (immCmdList->isHeaplessModeEnabled() && immCmdList->dcFlushSupport) ? 0u : 1u;

        debugManager.flags.EnableWalkerPartition.set(0);
        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handleWithInterruptHint, 0, nullptr, launchParams);
        EXPECT_EQ(ultCsr->userFenceAllocationAttemptCount, expectedTimestampEventUserFenceAllocations);

        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handleWithInterruptHint, 0, nullptr, launchParams);
        EXPECT_EQ(ultCsr->userFenceAllocationAttemptCount, expectedTimestampEventUserFenceAllocations);

        ze_event_counter_based_desc_t nonTimestampDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
        nonTimestampDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_HOST_VISIBLE;
        nonTimestampDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
        nonTimestampDesc.pNext = &syncModeDesc;

        ze_event_handle_t nonTimestampHandleWithInterruptHint = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &nonTimestampDesc, &nonTimestampHandleWithInterruptHint));

        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nonTimestampHandleWithInterruptHint, 0, nullptr, launchParams);
        EXPECT_EQ(ultCsr->userFenceAllocationAttemptCount, 1u);

        immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nonTimestampHandleWithInterruptHint, 0, nullptr, launchParams);
        EXPECT_EQ(ultCsr->userFenceAllocationAttemptCount, 1u);

        zeEventDestroy(nonTimestampHandleWithInterruptHint);
    }

    zeEventDestroy(handle);
    zeEventDestroy(handleWithInterruptHint);
}

HWTEST_F(InOrderCmdListTests, givenImmediateCmdListWhenAppendLaunchKernelWithSignalEventThenInOrderNonWalkerSignalingRequiredIsCachedInLaunchParams) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);

    CmdListKernelLaunchParams localLaunchParams = {};
    EXPECT_FALSE(localLaunchParams.inOrderNonWalkerSignalingRequired);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, localLaunchParams);

    const bool expectedCachedValue = immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get());
    EXPECT_EQ(expectedCachedValue, localLaunchParams.inOrderNonWalkerSignalingRequired);
}

HWTEST_F(InOrderCmdListTests, givenImmediateCmdListWhenAppendLaunchKernelWithoutSignalEventThenInOrderNonWalkerSignalingRequiredCacheStaysFalse) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    CmdListKernelLaunchParams localLaunchParams = {};
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, localLaunchParams);

    EXPECT_FALSE(localLaunchParams.inOrderNonWalkerSignalingRequired);
}

HWTEST_F(InOrderCmdListTests, givenInOrderNonWalkerSignalingRequiredAndNullSignalEventWhenAppendLaunchKernelThenNoNullDereferenceOccurs) {
    class ForceNonWalkerSignalingCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t appendLaunchKernelWithParams(L0::Kernel *kernel,
                                                 const ze_group_count_t &threadGroupDimensions,
                                                 L0::Event *event,
                                                 CmdListKernelLaunchParams &launchParams) override {
            auto result = BaseClass::appendLaunchKernelWithParams(kernel, threadGroupDimensions, event, launchParams);
            launchParams.inOrderNonWalkerSignalingRequired = true;
            return result;
        }
    };

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, ForceNonWalkerSignalingCmdList>(false);

    CmdListKernelLaunchParams localLaunchParams = {};
    auto result = immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, localLaunchParams);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(localLaunchParams.inOrderNonWalkerSignalingRequired);
}

} // namespace ult
} // namespace L0
