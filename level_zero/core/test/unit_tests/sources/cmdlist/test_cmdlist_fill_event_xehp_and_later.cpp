/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"

#include <limits>

namespace L0 {
namespace ult {

struct FillTestInput {
    DriverHandle *driver = nullptr;
    L0::Context *context = nullptr;
    L0::Device *device = nullptr;

    size_t allocSize = 0;
    size_t patternSize = 0;
    void *dstPtr = nullptr;
    void *patternPtr = nullptr;
    size_t storeDataImmOffset = 0;

    ze_event_pool_flags_t eventPoolFlags = 0;

    bool useFirstEventPacketAddress = false;
    bool signalAllPackets = false;
    bool allPackets = false;
};

template <int32_t usePipeControlMultiPacketEventSync, int32_t compactL3FlushEventPacket, uint32_t multiTile>
struct AppendFillMultiPacketEventFixture : public AppendFillFixture {
    void setUp() {
        DebugManager.flags.UsePipeControlMultiKernelEventSync.set(usePipeControlMultiPacketEventSync);
        DebugManager.flags.CompactL3FlushEventPacket.set(compactL3FlushEventPacket);
        if constexpr (multiTile == 1) {
            DebugManager.flags.CreateMultipleSubDevices.set(2);
            DebugManager.flags.EnableImplicitScaling.set(1);
        }
        AppendFillFixture::setUp();

        input.driver = driverHandle.get();
        input.context = context;
        input.device = device;
        input.signalAllPackets = L0GfxCoreHelper::useSignalAllEventPackets(device->getHwInfo());
        input.allPackets = !usePipeControlMultiPacketEventSync && !compactL3FlushEventPacket;

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        testEventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        testEvent = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(testEventPool.get(), &eventDesc, input.device));
    }

    void tearDown() {
        testEvent.reset(nullptr);
        testEventPool.reset(nullptr);

        AppendFillFixture::tearDown();
    }

    FillTestInput input = {};
    TestExpectedValues arg = {};

    std::unique_ptr<L0::EventPool> testEventPool;
    std::unique_ptr<L0::Event> testEvent;
};

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryFillManyImmediateKernels(FillTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + event->getSinglePacketSize();

    auto commandList = std::make_unique<CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryFill(input.dstPtr, input.patternPtr,
                                           input.patternSize, input.allocSize, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

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
void testSingleTileAppendMemoryFillManyKernels(FillTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + event->getSinglePacketSize();

    auto commandList = std::make_unique<CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryFill(input.dstPtr, input.patternPtr,
                                           input.patternSize, input.allocSize, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

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
void testSingleTileAppendMemoryFillManyKernelsAndL3Flush(FillTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = input.eventPoolFlags;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + event->getSinglePacketSize();

    auto commandList = std::make_unique<CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryFill(input.dstPtr, input.patternPtr,
                                           input.patternSize, input.allocSize, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

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
void testSingleTileAppendMemoryFillSingleKernel(FillTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    auto commandList = std::make_unique<CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);

    int pattern = 0;
    const size_t size = 1024;
    uint8_t array[size] = {};

    auto &commandContainer = commandList->commandContainer;
    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryFill(array, &pattern, 1, size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);
    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

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
void testSingleTileAppendMemoryFillSingleKernelAndL3Flush(FillTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = input.eventPoolFlags;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    auto commandList = std::make_unique<CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);

    int pattern = 0;
    const size_t size = 1024;
    uint8_t array[size] = {};

    auto &commandContainer = commandList->commandContainer;
    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryFill(array, &pattern, 1, size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);
    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    uint64_t l3FlushPostSyncAddress = gpuBaseAddress;
    if (!input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress += event->getSinglePacketSize();
    }

    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        if (!input.allPackets) {
            l3FlushPostSyncAddress = gpuBaseAddress + (event->getMaxPacketsCount() - 1) * event->getSinglePacketSize();
        }
        storeDataImmAddress += input.storeDataImmOffset;
        expectedPostSyncStoreDataImm = arg.expectStoreDataImm;
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
void testMultiTileAppendMemoryFillManyKernels(FillTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = input.eventPoolFlags;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress + 2 * event->getSinglePacketSize();

    auto commandList = std::make_unique<CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);
    EXPECT_EQ(2u, commandList->partitionCount);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryFill(input.dstPtr, input.patternPtr,
                                           input.patternSize, input.allocSize, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    uint32_t expectedDcFlush = 0;
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, input.device->getHwInfo())) {
        // 1st dc flush after cross-tile sync, 2nd dc flush for signal scope event
        expectedDcFlush = 2;
    }

    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    constexpr uint32_t kernels = 2;
    uint32_t sdiCount = NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired() ? 3 : 0;
    size_t extraCleanupStoreDataImm = kernels * sdiCount;

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
        storeDataImmAddress += event->getSinglePacketSize() * commandList->partitionCount;
    }

    auto itorPipeControls = findAll<PIPE_CONTROL *>(secondWalker, cmdList.end());

    uint32_t postSyncPipeControls = 0;
    uint32_t dcFlushFound = 0;

    for (auto it : itorPipeControls) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            postSyncPipeControls++;
        }
        if (cmd->getDcFlushEnable()) {
            dcFlushFound++;
        }
    }

    EXPECT_EQ(arg.expectedPostSyncPipeControls, postSyncPipeControls);
    EXPECT_EQ(expectedDcFlush, dcFlushFound);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testMultiTileAppendMemoryFillSingleKernelAndL3Flush(FillTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = input.eventPoolFlags;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    auto commandList = std::make_unique<CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);

    int pattern = 0;
    const size_t size = 1024;
    uint8_t array[size] = {};

    auto &commandContainer = commandList->commandContainer;
    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryFill(array, &pattern, 1, size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    uint64_t gpuBaseAddress = event->getGpuAddress(input.device);
    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : gpuBaseAddress;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    constexpr uint32_t kernels = 1;
    uint32_t sdiCount = NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired() ? 3 : 0;
    size_t extraCleanupStoreDataImm = kernels * sdiCount;

    uint64_t l3FlushPostSyncAddress = firstKernelEventAddress + 2 * event->getSinglePacketSize();
    if (event->isUsingContextEndOffset()) {
        l3FlushPostSyncAddress += event->getContextEndOffset();
    }

    if (event->isUsingContextEndOffset()) {
        gpuBaseAddress += event->getContextEndOffset();
    }

    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuBaseAddress;
    if (input.signalAllPackets) {
        if (!input.allPackets) {
            l3FlushPostSyncAddress = gpuBaseAddress + (event->getMaxPacketsCount() - commandList->partitionCount) * event->getSinglePacketSize();
        }
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
        storeDataImmAddress += event->getSinglePacketSize() * commandList->partitionCount;
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

    constexpr uint32_t expectedDcFlush = 2; // dc flush for last cross-tile sync and separately for signal scope event after last kernel split
    EXPECT_EQ(arg.expectedPostSyncPipeControls, postSyncPipeControls);
    EXPECT_EQ(expectedDcFlush, dcFlushFound);
}

using AppendFillMultiPacketEventTest = Test<AppendFillMultiPacketEventFixture<0, 0, 0>>;

HWTEST2_F(AppendFillMultiPacketEventTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesComputeWalkerPostSyncThenSeparateKernelsUsesPostSyncProfiling,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 2;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.dstPtr = immediateDstPtr;
    input.allocSize = immediateAllocSize;
    input.patternPtr = &immediatePattern;
    input.patternSize = sizeof(immediatePattern);

    if (input.signalAllPackets) {
        uint32_t reminderPostSyncOps = 1;
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
            reminderPostSyncOps = 2;
        }
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillManyImmediateKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillMultiPacketEventTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesComputeWalkerPostSyncThenSeparateKernelsUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 2;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        uint32_t reminderPostSyncOps = 1;
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
            reminderPostSyncOps = 2;
        }
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillMultiPacketEventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSync,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    if (input.signalAllPackets) {
        uint32_t reminderPostSyncOps = 2;
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
            reminderPostSyncOps = 3;
        }
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillSingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillMultiPacketEventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSyncAndL3PostSync,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using AppendFillSinglePacketEventTest = Test<AppendFillMultiPacketEventFixture<1, 0, 0>>;

HWTEST2_F(AppendFillSinglePacketEventTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.dstPtr = immediateDstPtr;
    input.allocSize = immediateAllocSize;
    input.patternPtr = &immediatePattern;
    input.patternSize = sizeof(immediatePattern);

    if (input.signalAllPackets) {
        arg.expectStoreDataImm = testEvent->getMaxPacketsCount() - 1;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillManyImmediateKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillSinglePacketEventTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        arg.expectStoreDataImm = testEvent->getMaxPacketsCount() - 1;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillSinglePacketEventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSync,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    if (input.signalAllPackets) {
        arg.expectStoreDataImm = testEvent->getMaxPacketsCount() - 1;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillSingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillSinglePacketEventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSyncAndL3PostSync,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using MultiTileAppendFillEventMultiPacketTest = Test<AppendFillMultiPacketEventFixture<0, 0, 1>>;

HWTEST2_F(MultiTileAppendFillEventMultiPacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeTimestampEventUsesComputeWalkerPostSyncThenSeparateKernelsUsesWalkerPostSyncProfilingAndSingleDcFlushWithImmediatePostSync, IsAtLeastXeHpCore) {
    // two kernels and each kernel uses two packets (for two tiles), in total 4
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 2;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 0;
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo())) {
        // last kernel uses 4 packets, in addition to kernel two packets, use 2 packets to two tile cache flush
        arg.expectedPacketsInUse = 6;
        // cache flush with event signal
        arg.expectedPostSyncPipeControls = 1;
    }
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 1;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testMultiTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendFillEventMultiPacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeImmediateEventUsesComputeWalkerPostSyncThenSeparateKernelsUsesWalkerPostSyncAndSingleDcFlushWithPostSync, IsAtLeastXeHpCore) {
    // two kernels and each kernel uses two packets (for two tiles), in total 4
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 2;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 0;
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo())) {
        // last kernel uses 4 packets, in addition to kernel two packets, use 2 packets to two tile cache flush
        arg.expectedPacketsInUse = 6;
        // cache flush with event signal
        arg.expectedPostSyncPipeControls = 1;
    }
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = 0;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 1;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testMultiTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendFillEventMultiPacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeTimestampEventUsesComputeWalkerPostSyncThenSingleKernelsUsesWalkerPostSyncProfilingAndSingleDcFlushWithImmediatePostSync, IsXeHpOrXeHpgCore) {
    // kernel uses 4 packets, in addition to kernel two packets, use 2 packets to two tile cache flush
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    // cache flush with event signal
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testMultiTileAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendFillEventMultiPacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeImmediateEventUsesComputeWalkerPostSyncThenSingleKernelUsesWalkerPostSyncAndSingleDcFlushWithPostSync, IsXeHpOrXeHpgCore) {
    // kernel uses 4 packets, in addition to kernel two packets, use 2 packets to two tile cache flush
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    // cache flush with event signal
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = 0;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testMultiTileAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using MultiTileAppendFillEventSinglePacketTest = Test<AppendFillMultiPacketEventFixture<1, 0, 1>>;

HWTEST2_F(MultiTileAppendFillEventSinglePacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfilingAndDcFlushWithNoPostSync, IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo())) {
            constexpr uint32_t reminderPostSyncOps = 1;
            arg.expectStoreDataImm = reminderPostSyncOps;
            input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
        }
    }

    testMultiTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendFillEventSinglePacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeImmediateEventUsesPipeControlPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfilingAndDcFlushWithImmediatePostSync, IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo())) {
            constexpr uint32_t reminderPostSyncOps = 1;
            arg.expectStoreDataImm = reminderPostSyncOps;
        }
    }

    testMultiTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

using AppendFillCompactL3EventTest = Test<AppendFillMultiPacketEventFixture<0, 1, 0>>;

HWTEST2_F(AppendFillCompactL3EventTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesWalkerPostSyncThenSeparateKernelsUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 2;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.dstPtr = immediateDstPtr;
    input.allocSize = immediateAllocSize;
    input.patternPtr = &immediatePattern;
    input.patternSize = sizeof(immediatePattern);

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 1;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillManyImmediateKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillCompactL3EventTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesWalkerPostSyncThenSeparateKernelsUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 2;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 1;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillCompactL3EventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSync,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillSingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillCompactL3EventTest,
          givenAppendMemoryFillUsingL3CompactEventWhenPatternDispatchOneKernelThenUseRegisterPostSync,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testSingleTileAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillCompactL3EventTest,
          givenCallToAppendMemoryFillWhenL3CompactImmediateEventUsesPipeControlPostSyncThenSinglePipeControlPostSyncUsed,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    if (input.signalAllPackets) {
        constexpr uint32_t reminderPostSyncOps = 2;
        arg.expectStoreDataImm = reminderPostSyncOps;
    }

    testSingleTileAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using MultiTileAppendFillCompactL3EventTest = Test<AppendFillMultiPacketEventFixture<0, 1, 1>>;

HWTEST2_F(MultiTileAppendFillCompactL3EventTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenPlatformNeedsDcFlushAndL3CompactTimestampEventThenRegisterPostSyncUsedOtherwiseUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
        arg.expectedPacketsInUse = 2;
        arg.expectedKernelCount = 1;
        arg.expectedWalkerPostSyncOp = 0;
        arg.expectedPostSyncPipeControls = 0;
        arg.postSyncAddressZero = true;
    } else {
        arg.expectedPacketsInUse = 4;
        arg.expectedKernelCount = 2;
        arg.expectedWalkerPostSyncOp = 3;
        arg.expectedPostSyncPipeControls = 0;
        arg.postSyncAddressZero = false;
    }

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        constexpr uint32_t partitionCount = 2;
        arg.expectStoreDataImm = (testEvent->getMaxPacketsCount() - arg.expectedPacketsInUse) / partitionCount;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    testMultiTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendFillCompactL3EventTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenPlatformNeedsDcFlushAndL3CompactImmediateEventThenPipeControlPostSyncUsedOtherwiseUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
        arg.expectedPacketsInUse = 2;
        arg.expectedKernelCount = 1;
        arg.expectedWalkerPostSyncOp = 0;
        arg.expectedPostSyncPipeControls = 1;
        arg.postSyncAddressZero = true;
        input.storeDataImmOffset = 0;
    } else {
        arg.expectedPacketsInUse = 4;
        arg.expectedKernelCount = 2;
        arg.expectedWalkerPostSyncOp = 3;
        arg.expectedPostSyncPipeControls = 0;
        arg.postSyncAddressZero = false;
        input.storeDataImmOffset = arg.expectedPacketsInUse * testEvent->getSinglePacketSize();
    }

    input.eventPoolFlags = 0;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    if (input.signalAllPackets) {
        constexpr uint32_t partitionCount = 2;
        arg.expectStoreDataImm = (testEvent->getMaxPacketsCount() - arg.expectedPacketsInUse) / partitionCount;
    }

    testMultiTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

using AppendFillKernelSplitAndCompactL3EventTest = Test<AppendFillMultiPacketEventFixture<1, 1, 0>>;

HWTEST2_F(AppendFillKernelSplitAndCompactL3EventTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.dstPtr = immediateDstPtr;
    input.allocSize = immediateAllocSize;
    input.patternPtr = &immediatePattern;
    input.patternSize = sizeof(immediatePattern);

    testSingleTileAppendMemoryFillManyImmediateKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillKernelSplitAndCompactL3EventTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    testSingleTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillKernelSplitAndCompactL3EventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSync,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    testSingleTileAppendMemoryFillSingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillKernelSplitAndCompactL3EventTest,
          givenAppendMemoryFillUsingL3CompactTimestampEventWhenPatternDispatchOneKernelThenUseRegisterPostSync,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendFillKernelSplitAndCompactL3EventTest,
          givenAppendMemoryFillUsingL3CompactImmediateEventWhenPatternDispatchOneKernelThenUsePipeControlPostSync,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testSingleTileAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using MultiTileAppendFillKernelSplitAndCompactL3EventTest = Test<AppendFillMultiPacketEventFixture<1, 1, 1>>;

HWTEST2_F(MultiTileAppendFillKernelSplitAndCompactL3EventTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenL3CompactTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfilingAndDcFlushWithNoPostSync, IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    testMultiTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendFillKernelSplitAndCompactL3EventTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenL3CompactImmediateEventUsesPipeControlPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfilingAndDcFlushWithImmediatePostSync, IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;

    input.dstPtr = dstPtr;
    input.allocSize = allocSize;
    input.patternPtr = pattern;
    input.patternSize = patternSize;

    testMultiTileAppendMemoryFillManyKernels<gfxCoreFamily>(input, arg);
}

} // namespace ult
} // namespace L0
