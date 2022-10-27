/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

struct CopyTestInput {
    DriverHandle *driver = nullptr;
    L0::Context *context = nullptr;
    L0::Device *device = nullptr;

    size_t size = 0;
    void *dstPtr = nullptr;
    void *srcPtr = nullptr;

    ze_event_pool_flags_t eventPoolFlags = 0;

    int32_t usePipeControlMultiPacketEventSync;

    bool useFirstEventPacketAddress = false;
};

template <int32_t usePipeControlMultiPacketEventSync, int32_t compactL3FlushEventPacket, uint32_t multiTile>
struct AppendMemoryCopyMultiPacketEventFixture : public DeviceFixture {
    void setUp() {
        DebugManager.flags.UsePipeControlMultiKernelEventSync.set(usePipeControlMultiPacketEventSync);
        DebugManager.flags.CompactL3FlushEventPacket.set(compactL3FlushEventPacket);
        if (multiTile == 1) {
            DebugManager.flags.CreateMultipleSubDevices.set(2);
            DebugManager.flags.EnableImplicitScaling.set(1);
        }
        DeviceFixture::setUp();

        input.driver = driverHandle.get();
        input.context = context;
        input.device = device;
        input.usePipeControlMultiPacketEventSync = usePipeControlMultiPacketEventSync;
    }

    DebugManagerStateRestore restorer;

    CopyTestInput input = {};
    TestExpectedValues arg = {};
};

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryCopyThreeKernels(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device) + event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device) + 2 * event->getSinglePacketSize();

    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];
    auto thirdWalker = itorWalkers[2];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*thirdWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(thirdKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);

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

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device) + event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device) + 2 * event->getSinglePacketSize();

    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];
    auto thirdWalker = itorWalkers[2];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*thirdWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(thirdKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    uint64_t l3FlushPostSyncAddress = event->getGpuAddress(input.device) + 2 * event->getSinglePacketSize() + event->getSinglePacketSize();
    if (input.usePipeControlMultiPacketEventSync == 1 || input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress = event->getGpuAddress(input.device);
    }
    if (event->isUsingContextEndOffset()) {
        l3FlushPostSyncAddress += event->getContextEndOffset();
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
void testSingleTileAppendMemoryCopySingleKernel(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);

    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(1u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testSingleTileAppendMemoryCopySingleKernelAndL3Flush(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);

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

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);

    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(1u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    uint64_t l3FlushPostSyncAddress = event->getGpuAddress(input.device) + event->getSinglePacketSize();
    if (input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress = event->getGpuAddress(input.device);
    }
    if (event->isUsingContextEndOffset()) {
        l3FlushPostSyncAddress += event->getContextEndOffset();
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
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, input.device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_SUBDEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event.get(), 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalker = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
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
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);
    EXPECT_EQ(2u, commandList.partitionCount);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device) + 2 * event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device) + 4 * event->getSinglePacketSize();

    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];
    auto thirdWalker = itorWalkers[2];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*thirdWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(thirdKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);
    uint64_t secondKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device) + 2 * event->getSinglePacketSize();
    uint64_t thirdKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device) + 4 * event->getSinglePacketSize();

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(3u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];
    auto secondWalker = itorWalkers[1];
    auto thirdWalker = itorWalkers[2];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    walkerCmd = genCmdCast<COMPUTE_WALKER *>(*thirdWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(thirdKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

    uint64_t l3FlushPostSyncAddress = thirdKernelEventAddress + 2 * event->getSinglePacketSize();
    if (input.usePipeControlMultiPacketEventSync == 1 || input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress = event->getGpuAddress(input.device);
    }
    if (event->isUsingContextEndOffset()) {
        l3FlushPostSyncAddress += event->getContextEndOffset();
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
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);
    EXPECT_EQ(2u, commandList.partitionCount);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);

    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(1u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, itorWalkers.size());
    auto firstWalker = itorWalkers[0];

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
    EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
    EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void testMultiTileAppendMemoryCopySingleKernelAndL3Flush(CopyTestInput &input, TestExpectedValues &arg) {
    using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using OPERATION = typename POSTSYNC_DATA::OPERATION;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(input.device, NEO::EngineGroupType::RenderCompute, 0u);
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

    uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList.appendMemoryCopy(input.dstPtr, input.srcPtr, input.size, event->toHandle(), 0, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    EXPECT_EQ(1u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
    EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

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

    uint64_t l3FlushPostSyncAddress = 0;
    if (input.useFirstEventPacketAddress) {
        l3FlushPostSyncAddress = event->getGpuAddress(input.device);
    } else {
        l3FlushPostSyncAddress = event->getGpuAddress(input.device) + 2 * event->getSinglePacketSize();
    }
    if (event->isUsingContextEndOffset()) {
        l3FlushPostSyncAddress += event->getContextEndOffset();
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
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 3;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopyThreeKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernel,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testSingleTileAppendMemoryCopySingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernelsAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernelsAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = L0HwHelper::get(gfxCoreFamily).multiTileCapablePlatform() ? 3 : 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateKernelAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateKernelAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = L0HwHelper::get(gfxCoreFamily).multiTileCapablePlatform() ? 3 : 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterMultiPacket,
          givenCommandListWhenMemoryCopyWithSignalEventScopeSetToSubDeviceThenB2BPipeControlIsAddedWithDcFlushWithPostSyncForLastPC, IsXeHpOrXeHpgCore) {
    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopySignalScopeEventToSubDevice<gfxCoreFamily>(input, arg);
}

using AppendMemoryCopyXeHpAndLaterSinglePacket = Test<AppendMemoryCopyMultiPacketEventFixture<1, 0, 0>>;

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForRegisterOnly,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopyThreeKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernel,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testSingleTileAppendMemoryCopySingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForRegisterAndL3FlushWithNoPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenEventProfilingCalledForPipeControlAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateKernelAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateKernelAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = L0HwHelper::get(gfxCoreFamily).multiTileCapablePlatform() ? 3 : 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyXeHpAndLaterSinglePacket,
          givenCommandListWhenMemoryCopyWithSignalEventScopeSetToSubDeviceThenB2BPipeControlIsAddedWithDcFlushWithPostSyncForLastPC, IsXeHpOrXeHpgCore) {
    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopySignalScopeEventToSubDevice<gfxCoreFamily>(input, arg);
}

using MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket = Test<AppendMemoryCopyMultiPacketEventFixture<0, 0, 1>>;

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernels,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 6;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testMultiTileAppendMemoryCopyThreeKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernel,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testMultiTileAppendMemoryCopySingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernelsAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 8;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListAndEventWithSignalScopeWhenImmdiateProvidedByComputeWalkerAndPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernelsAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 8;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernelAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterMultiPacket,
          givenMultiTileCommandListAndEventWithSignalScopeWhenImmdiateProvidedByComputeWalkerAndPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernelAndL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket = Test<AppendMemoryCopyMultiPacketEventFixture<1, 0, 1>>;

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForMultiTileRegisterPipeControlPacket,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testMultiTileAppendMemoryCopyThreeKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernel,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testMultiTileAppendMemoryCopySingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForMultiTileRegisterPostSyncAndL3FlushForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForPipeControlPostSyncAndL3FlushAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernelPostSync,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyXeHpAndLaterSinglePacket,
          givenMultiTileCommandListAndEventWithSignalScopeWhenImmediateProvidedByComputeWalkerAndPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernelAndL3FlushPipeControlPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 4;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = 0;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using AppendMemoryCopyL3CompactEventTest = Test<AppendMemoryCopyMultiPacketEventFixture<0, 1, 0>>;

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernels,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 3;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopyThreeKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernel,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testSingleTileAppendMemoryCopySingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateKernelsAndL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForForL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
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

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactEventTest,
          givenCommandListAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
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

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using MultiTileAppendMemoryCopyL3CompactEventTest = Test<AppendMemoryCopyMultiPacketEventFixture<0, 1, 1>>;

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernels,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 6;
    arg.expectedKernelCount = 3;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testMultiTileAppendMemoryCopyThreeKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleSeparateMultiTileKernel,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testMultiTileAppendMemoryCopySingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelsAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelsAndEventWithSignalScopeWhenImmdiateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
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

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListCopyUsingSingleKernelAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactEventTest,
          givenMultiTileCommandListCopyUsingSingleKernelAndEventWithSignalScopeWhenImmdiateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
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

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest = Test<AppendMemoryCopyMultiPacketEventFixture<1, 1, 0>>;

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSinglePacket,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testSingleTileAppendMemoryCopyThreeKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleKernel,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testSingleTileAppendMemoryCopySingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListCopyUsingThreeKernelsAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListCopyUsingThreeKernelsAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testSingleTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListCopyUsingSingleKernelAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 1;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(AppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenCommandListCopyUsingSingleKernelAndEventWithSignalScopeWhenImmediateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedOnce,
          IsXeHpOrXeHpgCore) {
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

    testSingleTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest = Test<AppendMemoryCopyMultiPacketEventFixture<1, 1, 1>>;

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForThreeSeparateMultiTileKernels,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    testMultiTileAppendMemoryCopyThreeKernels<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListWhenTimestampProvidedByComputeWalkerPostSyncPassedToMemoryCopyThenAppendProfilingCalledForSingleMultiTileKernel,
          IsAtLeastXeHpCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    testMultiTileAppendMemoryCopySingleKernel<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelsAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelsAndEventWithSignalScopeWhenImmdiateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 1;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1231);
    input.dstPtr = reinterpret_cast<void *>(0x200002345);
    input.size = 0x100002345;

    input.eventPoolFlags = 0;

    testMultiTileAppendMemoryCopyThreeKernelsAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListCopyUsingThreeKernelAndTimestampEventWithSignalScopeWhenTimestampProvidedByRegisterPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
    arg.expectedPacketsInUse = 2;
    arg.expectedKernelCount = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.expectedPostSyncPipeControls = 0;
    arg.postSyncAddressZero = true;

    input.srcPtr = reinterpret_cast<void *>(0x1000);
    input.dstPtr = reinterpret_cast<void *>(0x20000000);
    input.size = 0x100000000;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(MultiTileAppendMemoryCopyL3CompactAndSingleKernelPacketEventTest,
          givenMultiTileCommandListCopyUsingSingleKernelAndEventWithSignalScopeWhenImmdiateProvidedByPipeControlPostSyncPassedToMemoryCopyThenAppendProfilingCalledForL3FlushWithPostSyncAddedForScopedEvent,
          IsXeHpOrXeHpgCore) {
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

    testMultiTileAppendMemoryCopySingleKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

} // namespace ult
} // namespace L0
