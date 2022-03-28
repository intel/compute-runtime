/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/helpers/timestamp_packet_tests.h"
#include "opencl/test/unit_test/mocks/mock_event.h"

using namespace NEO;

HWTEST_F(TimestampPacketTests, givenEmptyWaitlistAndNoOutputEventWhenEnqueueingMarkerThenDoNothing) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    cmdQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);

    EXPECT_EQ(0u, cmdQ->timestampPacketContainer->peekNodes().size());
    EXPECT_FALSE(csr.stallingCommandsOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, givenEmptyWaitlistAndEventWhenEnqueueingMarkerWithProfilingEnabledThenObtainNewNode) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    cmdQ->setProfilingEnabled();

    cl_event event;
    cmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);

    EXPECT_EQ(1u, cmdQ->timestampPacketContainer->peekNodes().size());

    clReleaseEvent(event);
}

template <typename FamilyType>
class MockCommandStreamReceiverHW : public UltCommandStreamReceiver<FamilyType> {
  public:
    MockCommandStreamReceiverHW(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {}
    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap *dsh,
        const IndirectHeap *ioh,
        const IndirectHeap *ssh,
        uint32_t taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override {
        stream = &commandStream;
        return UltCommandStreamReceiver<FamilyType>::flushTask(
            commandStream,
            commandStreamStart,
            dsh,
            ioh,
            ssh,
            taskLevel,
            dispatchFlags,
            device);
    }
    LinearStream *stream = nullptr;
};

HWTEST_F(TimestampPacketTests, givenEmptyWaitlistAndEventWhenMarkerProfilingEnabledThenPipeControlAddedBeforeWritingTimestamp) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto commandStreamReceiver = std::make_unique<MockCommandStreamReceiverHW<FamilyType>>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto commandStreamReceiverPtr = commandStreamReceiver.get();
    commandStreamReceiver->timestampPacketWriteEnabled = true;
    device->resetCommandStreamReceiver(commandStreamReceiver.release());

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    cmdQ->setProfilingEnabled();

    cl_event event;
    cmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*(commandStreamReceiverPtr->stream), 0);
    GenCmdList storeRegMemList = hwParser.getCommandsList<MI_STORE_REGISTER_MEM>();
    EXPECT_EQ(4u, storeRegMemList.size());
    auto storeRegMemIt = find<MI_STORE_REGISTER_MEM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(storeRegMemIt, hwParser.cmdList.end());
    auto pipeControlIt = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), storeRegMemIt);
    EXPECT_NE(storeRegMemIt, pipeControlIt);
    EXPECT_NE(hwParser.cmdList.end(), pipeControlIt);

    clReleaseEvent(event);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, TimestampPacketTests, givenEmptyWaitlistAndEventWhenMarkerProfilingEnabledOnMultiTileCommandQueueThenCrossTileBarrierAddedBeforeWritingTimestamp) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto commandStreamReceiver = std::make_unique<MockCommandStreamReceiverHW<FamilyType>>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto commandStreamReceiverPtr = commandStreamReceiver.get();
    commandStreamReceiver->timestampPacketWriteEnabled = true;
    commandStreamReceiver->activePartitions = 2;
    commandStreamReceiver->activePartitionsConfig = 2;
    commandStreamReceiver->staticWorkPartitioningEnabled = true;

    device->resetCommandStreamReceiver(commandStreamReceiver.release());
    *ptrOffset(commandStreamReceiverPtr->tagAddress, commandStreamReceiverPtr->postSyncWriteOffset) = *commandStreamReceiverPtr->tagAddress;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    cmdQ->setProfilingEnabled();

    cl_event event;
    cmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*(commandStreamReceiverPtr->stream), 0);
    GenCmdList storeRegMemList = hwParser.getCommandsList<MI_STORE_REGISTER_MEM>();
    EXPECT_EQ(4u, storeRegMemList.size());
    auto storeRegMemIt = find<MI_STORE_REGISTER_MEM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(storeRegMemIt, hwParser.cmdList.end());
    GenCmdList::reverse_iterator rItorStoreRegMemIt(storeRegMemIt);
    auto pipeControlIt = reverse_find<PIPE_CONTROL *>(rItorStoreRegMemIt, hwParser.cmdList.rbegin());
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIt);
    EXPECT_NE(nullptr, pipeControl);

    GenCmdList::iterator cmdIt = pipeControlIt.base();
    auto miAtomic = genCmdCast<MI_ATOMIC *>(*cmdIt);
    EXPECT_NE(nullptr, miAtomic);

    cmdIt++;
    auto miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*cmdIt);
    EXPECT_NE(nullptr, miSemaphore);

    cmdIt++;
    auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmdIt);
    EXPECT_NE(nullptr, bbStart);

    clReleaseEvent(event);
}

HWTEST_F(TimestampPacketTests, givenWithWaitlistAndEventWhenMarkerProfilingEnabledThenPipeControllNotAddedBeforeWritingTimestamp) {
    auto commandStreamReceiver = std::make_unique<MockCommandStreamReceiverHW<FamilyType>>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto commandStreamReceiverPtr = commandStreamReceiver.get();
    commandStreamReceiver->timestampPacketWriteEnabled = true;
    device->resetCommandStreamReceiver(commandStreamReceiver.release());

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    cmdQ->setProfilingEnabled();

    cl_event event;
    MockEvent<Event> events[] = {
        {cmdQ.get(), CL_COMMAND_READ_BUFFER, 0, 0},
        {cmdQ.get(), CL_COMMAND_READ_BUFFER, 0, 0},
        {cmdQ.get(), CL_COMMAND_READ_BUFFER, 0, 0},
    };
    const cl_event waitList[] = {events, events + 1, events + 2};
    const cl_uint waitListSize = static_cast<cl_uint>(arrayCount(waitList));

    cmdQ->enqueueMarkerWithWaitList(waitListSize, waitList, &event);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*(commandStreamReceiverPtr->stream), 0);
    auto storeRegMemIt = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(storeRegMemIt, hwParser.cmdList.end());
    auto pipeControlIt = find<typename FamilyType::PIPE_CONTROL *>(hwParser.cmdList.begin(), storeRegMemIt);
    EXPECT_EQ(storeRegMemIt, pipeControlIt);

    clReleaseEvent(event);
}

HWTEST_F(TimestampPacketTests, whenEnqueueingBarrierThenRequestPipeControlOnCsrFlush) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    EXPECT_FALSE(csr.stallingCommandsOnNextFlushRequired);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    MockKernelWithInternals mockKernel(*device, context);
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr); // obtain first TimestampPackets<uint32_t>

    TimestampPacketContainer cmdQNodes;
    cmdQNodes.assignAndIncrementNodesRefCounts(*cmdQ.timestampPacketContainer);

    cmdQ.enqueueBarrierWithWaitList(0, nullptr, nullptr);

    EXPECT_EQ(cmdQ.timestampPacketContainer->peekNodes().at(0), cmdQNodes.peekNodes().at(0)); // dont obtain new node
    EXPECT_EQ(1u, cmdQ.timestampPacketContainer->peekNodes().size());

    EXPECT_TRUE(csr.stallingCommandsOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteDisabledWhenEnqueueingBarrierThenDontRequestPipeControlOnCsrFlush) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = false;

    EXPECT_FALSE(csr.stallingCommandsOnNextFlushRequired);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    cmdQ.enqueueBarrierWithWaitList(0, nullptr, nullptr);

    EXPECT_FALSE(csr.stallingCommandsOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, givenBlockedQueueWhenEnqueueingBarrierThenRequestPipeControlOnCsrFlush) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    EXPECT_FALSE(csr.stallingCommandsOnNextFlushRequired);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    auto userEvent = make_releaseable<UserEvent>();
    cl_event waitlist[] = {userEvent.get()};
    cmdQ.enqueueBarrierWithWaitList(1, waitlist, nullptr);
    EXPECT_TRUE(csr.stallingCommandsOnNextFlushRequired);
    userEvent->setStatus(CL_COMPLETE);
}

HWTEST_F(TimestampPacketTests, givenPipeControlRequestWhenEstimatingCsrStreamSizeThenAddSizeForPipeControl) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    csr.stallingCommandsOnNextFlushRequired = false;
    auto sizeWithoutPcRequest = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, device->getDevice());

    csr.stallingCommandsOnNextFlushRequired = true;
    auto sizeWithPcRequest = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, device->getDevice());

    size_t extendedSize = sizeWithoutPcRequest + sizeof(typename FamilyType::PIPE_CONTROL);

    EXPECT_EQ(sizeWithPcRequest, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenPipeControlRequestWithBarrierWriteWhenEstimatingCsrStreamSizeThenAddSizeForPipeControlForWrite) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    TimestampPacketContainer barrierTimestampPacketNode;
    barrierTimestampPacketNode.add(csr.getTimestampPacketAllocator()->getTag());

    flags.barrierTimestampPacketNodes = &barrierTimestampPacketNode;

    csr.stallingCommandsOnNextFlushRequired = false;
    auto sizeWithoutPcRequest = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, device->getDevice());

    csr.stallingCommandsOnNextFlushRequired = true;
    auto sizeWithPcRequest = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, device->getDevice());

    size_t extendedSize = sizeWithoutPcRequest + MemorySynchronizationCommands<FamilyType>::getSizeForPipeControlWithPostSyncOperation(device->getHardwareInfo());

    EXPECT_EQ(sizeWithPcRequest, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenInstructionCacheRequesWhenSizeIsEstimatedThenPipeControlIsAdded) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    csr.requiresInstructionCacheFlush = false;
    auto sizeWithoutPcRequest = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, device->getDevice());

    csr.requiresInstructionCacheFlush = true;
    auto sizeWithPcRequest = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, device->getDevice());

    size_t extendedSize = sizeWithoutPcRequest + sizeof(typename FamilyType::PIPE_CONTROL);

    EXPECT_EQ(sizeWithPcRequest, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenPipeControlRequestWhenFlushingThenProgramPipeControlAndResetRequestFlag) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.stallingCommandsOnNextFlushRequired = true;
    csr.timestampPacketWriteEnabled = true;

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    MockKernelWithInternals mockKernel(*device, context);
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(csr.stallingCommandsOnNextFlushRequired);

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    hwParser.findHardwareCommands<FamilyType>();
    auto secondEnqueueOffset = csr.commandStream.getUsed();

    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*hwParser.pipeControlList.begin());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pipeControl->getPostSyncOperation());
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(secondEnqueueOffset, csr.commandStream.getUsed()); // nothing programmed when flag is not set
}

HWTEST_F(TimestampPacketTests, givenKernelWhichDoesntRequireFlushWhenEnqueueingKernelThenOneNodeIsCreated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(false);
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockTagAllocator = new MockTagAllocator<>(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    // obtain first node for cmdQ and event1
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto size = cmdQ->timestampPacketContainer->peekNodes().size();
    EXPECT_EQ(size, 1u);
}

HWTEST_F(TimestampPacketTests, givenKernelWhichRequiresFlushWhenEnqueueingKernelThenTwoNodesAreCreated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(true);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockTagAllocator = new MockTagAllocator<>(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    kernel->mockKernel->svmAllocationsRequireCacheFlush = true;
    // obtain first node for cmdQ and event1
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto node1 = cmdQ->timestampPacketContainer->peekNodes().at(0);
    auto node2 = cmdQ->timestampPacketContainer->peekNodes().at(1);
    auto size = cmdQ->timestampPacketContainer->peekNodes().size();
    EXPECT_EQ(size, 2u);
    EXPECT_NE(nullptr, node1);
    EXPECT_NE(nullptr, node2);
    EXPECT_NE(node1, node2);
}
