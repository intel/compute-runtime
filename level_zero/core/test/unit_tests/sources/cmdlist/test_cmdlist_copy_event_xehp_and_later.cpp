/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
struct Context;
struct DriverHandle;

namespace ult {

struct CopyTestInput {
    DriverHandle *driver = nullptr;
    L0::Context *context = nullptr;
    L0::Device *device = nullptr;

    size_t size = 0;
    void *dstPtr = nullptr;
    void *srcPtr = nullptr;
    size_t storeDataImmOffset = 0;

    ze_event_pool_flags_t eventPoolFlags = 0;

    int32_t usePipeControlMultiPacketEventSync;

    bool useFirstEventPacketAddress = false;
    bool signalAllPackets = false;
    bool allPackets = false;
};

template <int32_t usePipeControlMultiPacketEventSync, int32_t compactL3FlushEventPacket, uint32_t multiTile>
struct AppendMemoryCopyMultiPacketEventFixture : public DeviceFixture {
    void setUp() {
        debugManager.flags.UsePipeControlMultiKernelEventSync.set(usePipeControlMultiPacketEventSync);
        debugManager.flags.CompactL3FlushEventPacket.set(compactL3FlushEventPacket);
        debugManager.flags.EnableL3FlushAfterPostSync.set(0);

        if constexpr (multiTile == 1) {
            debugManager.flags.CreateMultipleSubDevices.set(2);
            debugManager.flags.EnableImplicitScaling.set(1);
        }
        DeviceFixture::setUp();

        input.driver = driverHandle.get();
        input.context = context;
        input.device = device;
        input.usePipeControlMultiPacketEventSync = usePipeControlMultiPacketEventSync;
        input.signalAllPackets = L0GfxCoreHelper::useSignalAllEventPackets(device->getHwInfo());
        input.allPackets = !usePipeControlMultiPacketEventSync && !compactL3FlushEventPacket;

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        testEventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        testEvent = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(testEventPool.get(), &eventDesc, input.device));
    }

    void tearDown() {
        testEvent.reset(nullptr);
        testEventPool.reset(nullptr);

        DeviceFixture::tearDown();
    }

    DebugManagerStateRestore restorer;

    CopyTestInput input = {};
    TestExpectedValues arg = {};

    std::unique_ptr<L0::EventPool> testEventPool;
    std::unique_ptr<L0::Event> testEvent;
};

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryCopyThreeKernels(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::renderCompute, 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + 2 * event->getSinglePacketSize();
    CmdListMemoryCopyParams copyParams = {};
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());

    constexpr size_t nWalkers = 3u;
    ASSERT_EQ(nWalkers, itWalkers.size());

    uint64_t kernelEventAddresses[nWalkers] = {firstKernelEventAddress,
                                               secondKernelEventAddress,
                                               thirdKernelEventAddress};

    for (auto i = 0u; i < itWalkers.size(); i++) {
        auto walkerItor = itWalkers[i];
        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();

        EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
        EXPECT_EQ(kernelEventAddresses[i], postSync.getDestinationAddress());
    }

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
        storeDataImmAddress += input.storeDataImmOffset;
    }

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(itWalkers[0], cmdList.end());
    ASSERT_EQ(expectedPostSyncStoreDataImm, itorStoreDataImm.size());

    for (size_t i = 0; i < expectedPostSyncStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        storeDataImmAddress += event->getSinglePacketSize();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::renderCompute, 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = input.eventPoolFlags;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + 2 * event->getSinglePacketSize();

    CmdListMemoryCopyParams copyParams = {};
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());

    constexpr size_t nWalkers = 3u;
    ASSERT_EQ(nWalkers, itWalkers.size());

    uint64_t kernelEventAddresses[nWalkers] = {firstKernelEventAddress,
                                               secondKernelEventAddress,
                                               thirdKernelEventAddress};

    for (auto i = 0u; i < itWalkers.size(); i++) {
        auto walkerItor = itWalkers[i];
        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();

        EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
        EXPECT_EQ(kernelEventAddresses[i], postSync.getDestinationAddress());
    }

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    uint64_t l3FlushPostSyncAddress = gpuBaseAddress + 2 * event->getSinglePacketSize() + event->getSinglePacketSize();
    if (input.usePipeControlMultiPacketEventSync == 1 || input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress = gpuBaseAddress;
    }

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(itWalkers[2], cmdList.end());
    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        l3FlushPostSyncAddress = gpuBaseAddress + (event->getMaxPacketsCount() - 1) * event->getSinglePacketSize();

        storeDataImmAddress += input.storeDataImmOffset;
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
    }
    ASSERT_EQ(expectedPostSyncStoreDataImm, itorStoreDataImm.size());

    for (size_t i = 0; i < expectedPostSyncStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        storeDataImmAddress += event->getSinglePacketSize();
    }

    auto itorPipeControls = findAll<PIPE_CONTROL *>(itWalkers[0], cmdList.end());

    uint32_t postSyncPipeControls = 0;
    uint32_t dcFlushFound = 0;
    for (auto it : itorPipeControls) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            postSyncPipeControls++;
            EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }
    EXPECT_EQ(arg.expectedPostSyncPipeControls, postSyncPipeControls);
    EXPECT_EQ(1u, dcFlushFound);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryCopySingleKernel(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;

    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::renderCompute, 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);
    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    CmdListMemoryCopyParams copyParams = {};
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walker = genCmdCast<WalkerType *>(*firstWalker);
    auto &postSync = walker->getPostSync();

    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
    EXPECT_EQ(firstKernelEventAddress, postSync.getDestinationAddress());

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
        storeDataImmAddress += input.storeDataImmOffset;
    }

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(firstWalker, cmdList.end());
    ASSERT_EQ(expectedPostSyncStoreDataImm, itorStoreDataImm.size());

    for (size_t i = 0; i < expectedPostSyncStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        storeDataImmAddress += event->getSinglePacketSize();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryCopySingleKernelAndL3Flush(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::renderCompute, 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = input.eventPoolFlags;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    CmdListMemoryCopyParams copyParams = {};
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walker = genCmdCast<WalkerType *>(*firstWalker);
    auto &postSync = walker->getPostSync();

    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
    EXPECT_EQ(firstKernelEventAddress, postSync.getDestinationAddress());

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    uint64_t l3FlushPostSyncAddress = gpuBaseAddress + event->getSinglePacketSize();
    if (input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress = gpuBaseAddress;
    }

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(firstWalker, cmdList.end());
    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        if (!input.allPackets) {
            l3FlushPostSyncAddress = gpuBaseAddress + (event->getMaxPacketsCount() - 1) * event->getSinglePacketSize();
        }

        storeDataImmAddress += input.storeDataImmOffset;
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
    }
    ASSERT_EQ(expectedPostSyncStoreDataImm, itorStoreDataImm.size());

    for (size_t i = 0; i < expectedPostSyncStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        storeDataImmAddress += event->getSinglePacketSize();
    }

    auto itorPipeControls = findAll<PIPE_CONTROL *>(firstWalker, cmdList.end());

    uint32_t postSyncPipeControls = 0;
    uint32_t dcFlushFound = 0;
    for (auto it : itorPipeControls) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            postSyncPipeControls++;
            EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }
    EXPECT_EQ(arg.expectedPostSyncPipeControls, postSyncPipeControls);
    EXPECT_EQ(1u, dcFlushFound);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryCopySignalScopeEventToSubDevice(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, input.device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    auto &commandContainer = commandList->getCmdContainer();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_SUBDEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event.get(), 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalker = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);

    auto pipeControls = findAll<PIPE_CONTROL *>(itorWalker, cmdList.end());
    uint32_t postSyncFound = 0;
    uint32_t dcFlushFound = 0;
    ASSERT_NE(0u, pipeControls.size());
    for (auto it : pipeControls) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA &&
            cmd->getImmediateData() == Event::STATE_SIGNALED) {
            postSyncFound++;
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }

    constexpr uint32_t expectedDcFlushFound = 1u;

    EXPECT_EQ(1u, postSyncFound);
    EXPECT_EQ(expectedDcFlushFound, dcFlushFound);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testMultiTileAppendMemoryCopyThreeKernels(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;

    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(2u, commandList.partitionCount);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + 2 * event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + 4 * event->getSinglePacketSize();
    CmdListMemoryCopyParams copyParams = {};
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    uint64_t expectedKernelEventAddress[]{firstKernelEventAddress, secondKernelEventAddress, thirdKernelEventAddress};

    for (auto i = 0u; i < itorWalkers.size(); i++) {
        auto walkerItor = itorWalkers[i];
        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();

        EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
        EXPECT_EQ(expectedKernelEventAddress[i], postSync.getDestinationAddress());
    }

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    // three kernels, each kernel cleanup of 3 SDI
    constexpr uint32_t kernels = 3;
    uint32_t sdiCount = NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired() ? 3 : 0;
    size_t extraCleanupStoreDataImm = kernels * sdiCount;

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(firstWalker, cmdList.end());
    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        storeDataImmAddress += input.storeDataImmOffset;
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
    }
    ASSERT_EQ(expectedPostSyncStoreDataImm + extraCleanupStoreDataImm, itorStoreDataImm.size());

    for (size_t i = extraCleanupStoreDataImm; i < expectedPostSyncStoreDataImm + extraCleanupStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
        storeDataImmAddress += event->getSinglePacketSize() * commandList.partitionCount;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(2u, commandList.partitionCount);
    auto &commandContainer = commandList.commandContainer;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = input.eventPoolFlags;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + 2 * event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + 4 * event->getSinglePacketSize();
    CmdListMemoryCopyParams copyParams = {};
    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr, copyParams);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto thirdWalker = itorWalkers[2];

    uint64_t expectedKernelEventAddress[]{firstKernelEventAddress, secondKernelEventAddress, thirdKernelEventAddress};

    for (auto i = 0u; i < itorWalkers.size(); i++) {
        auto walkerItor = itorWalkers[i];
        auto walker = genCmdCast<WalkerType *>(*walkerItor);

        auto &postSync = walker->getPostSync();

        EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
        EXPECT_EQ(expectedKernelEventAddress[i], postSync.getDestinationAddress());
    }

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    uint64_t l3FlushPostSyncAddress = gpuBaseAddress + 6 * event->getSinglePacketSize();
    if (input.usePipeControlMultiPacketEventSync == 1 || input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress = gpuBaseAddress;
    }

    // three kernels, each kernel cleanup of 3 SDI
    constexpr uint32_t kernels = 3;
    uint32_t sdiCount = NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired() ? 3 : 0;
    size_t extraCleanupStoreDataImm = kernels * sdiCount;

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(firstWalker, cmdList.end());
    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        if (!input.allPackets) {
            l3FlushPostSyncAddress = gpuBaseAddress + (event->getMaxPacketsCount() - commandList.partitionCount) * event->getSinglePacketSize();
        }

        storeDataImmAddress += input.storeDataImmOffset;
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
    }
    ASSERT_EQ(expectedPostSyncStoreDataImm + extraCleanupStoreDataImm, itorStoreDataImm.size());

    for (size_t i = extraCleanupStoreDataImm; i < expectedPostSyncStoreDataImm + extraCleanupStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
        storeDataImmAddress += event->getSinglePacketSize() * commandList.partitionCount;
    }

    auto itorPipeControls = findAll<PIPE_CONTROL *>(thirdWalker, cmdList.end());

    uint32_t postSyncPipeControls = 0;
    uint32_t dcFlushFound = 0;

    for (auto it : itorPipeControls) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncPipeControls++;
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }

    constexpr uint32_t expectedDcFlush = 2; // dc flush for last cross-tile sync and separately for signal scope event after last kernel split
    EXPECT_EQ(arg.expectedPostSyncPipeControls, postSyncPipeControls);
    EXPECT_EQ(expectedDcFlush, dcFlushFound);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testMultiTileAppendMemoryCopySingleKernel(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;

    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(2u, commandList.partitionCount);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);
    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    CmdListMemoryCopyParams copyParams = {};
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walker = genCmdCast<WalkerType *>(*firstWalker);
    auto &postSync = walker->getPostSync();

    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
    EXPECT_EQ(firstKernelEventAddress, postSync.getDestinationAddress());

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }
    // single kernel, cleanup of 3 SDI
    size_t extraCleanupStoreDataImm = NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired() ? 3 : 0;

    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
        storeDataImmAddress += input.storeDataImmOffset;
    }

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(firstWalker, cmdList.end());
    ASSERT_EQ(expectedPostSyncStoreDataImm + extraCleanupStoreDataImm, itorStoreDataImm.size());

    for (size_t i = extraCleanupStoreDataImm; i < expectedPostSyncStoreDataImm + extraCleanupStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
        storeDataImmAddress += event->getSinglePacketSize() * commandList.partitionCount;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testMultiTileAppendMemoryCopySingleKernelAndL3Flush(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(2u, commandList.partitionCount);
    auto &commandContainer = commandList.commandContainer;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = input.eventPoolFlags;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);
    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    CmdListMemoryCopyParams copyParams = {};
    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr, copyParams);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(1u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walker = genCmdCast<WalkerType *>(*firstWalker);
    auto &postSync = walker->getPostSync();

    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
    EXPECT_EQ(firstKernelEventAddress, postSync.getDestinationAddress());

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    uint64_t l3FlushPostSyncAddress = gpuBaseAddress;
    if (!input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress += 2 * event->getSinglePacketSize();
    }

    size_t extraCleanupStoreDataImm = NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired() ? 3 : 0;

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(firstWalker, cmdList.end());
    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        if (!input.allPackets) {
            l3FlushPostSyncAddress = gpuBaseAddress + (event->getMaxPacketsCount() - commandList.partitionCount) * event->getSinglePacketSize();
        }

        storeDataImmAddress += input.storeDataImmOffset;
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
    }
    ASSERT_EQ(expectedPostSyncStoreDataImm + extraCleanupStoreDataImm, itorStoreDataImm.size());

    for (size_t i = extraCleanupStoreDataImm; i < expectedPostSyncStoreDataImm + extraCleanupStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
        storeDataImmAddress += event->getSinglePacketSize() * commandList.partitionCount;
    }

    auto itorPipeControls = findAll<PIPE_CONTROL *>(firstWalker, cmdList.end());

    uint32_t postSyncPipeControls = 0;
    uint32_t dcFlushFound = 0;

    for (auto it : itorPipeControls) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncPipeControls++;
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }

    constexpr uint32_t expectedDcFlush = 2; // dc flush for last cross-tile sync and separately for signal scope event after last kernel split
    EXPECT_EQ(arg.expectedPostSyncPipeControls, postSyncPipeControls);
    EXPECT_EQ(expectedDcFlush, dcFlushFound);
}

using AppendMemoryCopyXeHpAndLaterMultiPacket = Test<AppendMemoryCopyMultiPacketEventFixture<0, 0, 0>>;

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernels,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 3;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    if (input.signalAllPackets) {
        arg.expectStoreDataImm = testEvent->getMaxPacketsCount() - arg.expectedPacketsInUse;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopyThreeKernels<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernel,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    if (input.signalAllPackets) {
        uint32_t reminderPostSyncOps = 2;
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, input.device->getNEODevice()->getRootDeviceEnvironment())) {
            reminderPostSyncOps = 3;
        }
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopySingleKernel<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernelsAndL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernelsAndL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = input.device->isImplicitScalingCapable() ? 3 : 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateKernelAndL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateKernelAndL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = input.device->isImplicitScalingCapable() ? 3 : 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListWhenMemoryCopyWithSignalEventScopeSetToSubDeviceThenB2BPipeControlIsAddedWithDcFlushWithPostSyncForLastPC, IsXeHpgCore) {
    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopySignalScopeEventToSubDevice<FamilyType::gfxCoreFamily>(input, arg);
}

using AppendMemoryCopyXeHpAndLaterSinglePacket = Test<AppendMemoryCopyMultiPacketEventFixture<1, 0, 0>>;

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForRegisterOnly,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    if (input.signalAllPackets) {
        uint32_t reminderPostSyncOps = 0;
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, input.device->getNEODevice()->getRootDeviceEnvironment())) {
            reminderPostSyncOps = 1;
        }
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopyThreeKernels<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernel,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    if (input.signalAllPackets) {
        uint32_t reminderPostSyncOps = 0;
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, input.device->getNEODevice()->getRootDeviceEnvironment())) {
            reminderPostSyncOps = 1;
        }
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopySingleKernel<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForRegisterAndL3FlushWithNoPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 1;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenEventProfilingCalledForPipeControlAndL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 1;
        arg.expectStoreDataImm = reminderPostSyncOps;
    }

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateKernelAndL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateKernelAndL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = input.device->isImplicitScalingCapable() ? 3 : 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListWhenMemoryCopyWithSignalEventScopeSetToSubDeviceThenB2BPipeControlIsAddedWithDcFlushWithPostSyncForLastPC, IsXeHpgCore) {
    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopySignalScopeEventToSubDevice<FamilyType::gfxCoreFamily>(input, arg);
}

using MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket = Test<AppendMemoryCopyMultiPacketEventFixture<0, 0, 1>>;

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernels,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 6;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    if (input.signalAllPackets) {
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, input.device->getNEODevice()->getRootDeviceEnvironment())) {
            constexpr uint32_t reminderPostSyncOps = 1;
            arg.expectStoreDataImm = reminderPostSyncOps;
            input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
        }
    }

    testMultiTileAppendMemoryCopyThreeKernels<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernel,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    if (input.signalAllPackets) {
        uint32_t reminderPostSyncOps = 2;
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, input.device->getNEODevice()->getRootDeviceEnvironment())) {
            reminderPostSyncOps = 3;
        }
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testMultiTileAppendMemoryCopySingleKernel<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernelsAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 8;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListAndEventWithSignalScopeWhenImmdiateProvidedByComputeWalkerAndPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernelsAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 8;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernelAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListAndEventWithSignalScopeWhenImmdiateProvidedByComputeWalkerAndPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernelAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

using MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket = Test<AppendMemoryCopyMultiPacketEventFixture<1, 0, 1>>;

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForMultiTileRegisterPipeControlPacket,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    if (input.signalAllPackets) {
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, input.device->getNEODevice()->getRootDeviceEnvironment())) {
            constexpr uint32_t reminderPostSyncOps = 1;
            arg.expectStoreDataImm = reminderPostSyncOps;
            input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
        }
    }

    testMultiTileAppendMemoryCopyThreeKernels<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernel,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    if (input.signalAllPackets) {
        constexpr uint32_t partitionCount = 2;
        arg.expectStoreDataImm = (testEvent->getMaxPacketsCount() - partitionCount) / partitionCount;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testMultiTileAppendMemoryCopySingleKernel<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForMultiTileRegisterPostSyncAndL3FlushForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 1;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForPipeControlPostSyncAndL3FlushAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 1;
        arg.expectStoreDataImm = reminderPostSyncOps;
    }

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernelPostSync,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListAndEventWithSignalScopeWhenImmediateProvidedByComputeWalkerAndPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernelAndL3FlushPipeControlPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

using AppendMemoryCopyL3CompactEventTest = Test<AppendMemoryCopyMultiPacketEventFixture<0, 1, 0>>;

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernels,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 3;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopyThreeKernels<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernel,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopySingleKernel<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernelsAndL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForForL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
    }

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
    }

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

using MultiTileAppendMemoryCopyL3CompactEventTest = Test<AppendMemoryCopyMultiPacketEventFixture<0, 1, 1>>;

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernels,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 6;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testMultiTileAppendMemoryCopyThreeKernels<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernel,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testMultiTileAppendMemoryCopySingleKernel<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelsAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelsAndEventWithSignalScopeWhenImmdiateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
    }

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListCopyUsingSingleKernelAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    }

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListCopyUsingSingleKernelAndEventWithSignalScopeWhenImmdiateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
    }

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

using AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest = Test<AppendMemoryCopyMultiPacketEventFixture<1, 1, 0>>;

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSinglePacket,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopyThreeKernels<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernel,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testSingleTileAppendMemoryCopySingleKernel<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListCopyUsingThreeKernelsAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListCopyUsingThreeKernelsAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListCopyUsingSingleKernelAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListCopyUsingSingleKernelAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

using MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest = Test<AppendMemoryCopyMultiPacketEventFixture<1, 1, 1>>;

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernels,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testMultiTileAppendMemoryCopyThreeKernels<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleMultiTileKernel,
          IsAtLeastXeCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testMultiTileAppendMemoryCopySingleKernel<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelsAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelsAndEventWithSignalScopeWhenImmdiateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListCopyUsingSingleKernelAndEventWithSignalScopeWhenImmdiateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

} // namespace ult
} // namespace L0
