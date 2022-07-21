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
    auto pipeControlIt = reverseFind<PIPE_CONTROL *>(rItorStoreRegMemIt, hwParser.cmdList.rbegin());
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

    auto userEvent = makeReleaseable<UserEvent>();
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

    size_t extendedSize = sizeWithoutPcRequest + MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(device->getHardwareInfo());

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

HWTEST_F(TimestampPacketTests, givenEventsWaitlistFromDifferentCSRsWhenEnqueueingThenMakeAllTimestampsResident) {
    MockTagAllocator<TimestampPackets<uint32_t>> tagAllocator(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1, 1,
                                                              sizeof(TimestampPackets<uint32_t>), false, device->getDeviceBitfield());

    auto &ultCsr = device->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.timestampPacketWriteEnabled = true;
    ultCsr.storeMakeResidentAllocations = true;

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);
    cmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

    MockTimestampPacketContainer node1(*ultCsr.getTimestampPacketAllocator(), 0);
    MockTimestampPacketContainer node2(*ultCsr.getTimestampPacketAllocator(), 0);

    auto tagNode1 = tagAllocator.getTag();
    node1.add(tagNode1);
    auto tagNode2 = tagAllocator.getTag();
    node2.add(tagNode2);

    Event event0(cmdQ1.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(node2);

    cl_event waitlist[] = {&event0, &event1};

    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 2, waitlist, nullptr);

    EXPECT_NE(tagNode1->getBaseGraphicsAllocation(), tagNode2->getBaseGraphicsAllocation());
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode1->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), ultCsr.taskCount));
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode2->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), ultCsr.taskCount));
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWhenEnqueueingNonBlockedThenMakeItResident) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.storeMakeResidentAllocations = true;

    MockKernelWithInternals mockKernel(*device, context);
    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto timestampPacketNode = cmdQ.timestampPacketContainer->peekNodes().at(0);

    EXPECT_TRUE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWhenEnqueueingBlockedThenMakeItResidentOnSubmit) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    MockKernelWithInternals mockKernel(*device, context);

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    csr.storeMakeResidentAllocations = true;

    UserEvent userEvent;
    cl_event clEvent = &userEvent;

    cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 1, &clEvent, nullptr);
    auto timestampPacketNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_FALSE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_TRUE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
    cmdQ->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingBlockedThenVirtualEventIncrementsRefInternalAndDecrementsAfterCompleteEvent) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    MockKernelWithInternals mockKernelWithInternals(*device, context);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    UserEvent userEvent;
    cl_event waitlist = &userEvent;

    auto internalCount = userEvent.getRefInternalCount();
    cmdQ->enqueueKernel(mockKernel, 1, nullptr, gws, nullptr, 1, &waitlist, nullptr);
    EXPECT_EQ(internalCount + 1, userEvent.getRefInternalCount());
    userEvent.setStatus(CL_COMPLETE);
    cmdQ->isQueueBlocked();
    EXPECT_EQ(internalCount, mockKernel->getRefInternalCount());
}

TEST_F(TimestampPacketTests, givenDispatchSizeWhenAskingForNewTimestampsThenObtainEnoughTags) {
    size_t dispatchSize = 3;

    mockCmdQ->timestampPacketContainer = std::make_unique<MockTimestampPacketContainer>(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 0);
    EXPECT_EQ(0u, mockCmdQ->timestampPacketContainer->peekNodes().size());

    TimestampPacketContainer previousNodes;
    mockCmdQ->obtainNewTimestampPacketNodes(dispatchSize, previousNodes, false, mockCmdQ->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(dispatchSize, mockCmdQ->timestampPacketContainer->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenWaitlistAndOutputEventWhenEnqueueingWithoutKernelThenInheritTimestampPacketsWithoutSubmitting) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    MockKernelWithInternals mockKernel(*device, context);
    cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr); // obtain first TimestampPackets<uint32_t>

    TimestampPacketContainer cmdQNodes;
    cmdQNodes.assignAndIncrementNodesRefCounts(*cmdQ->timestampPacketContainer);

    MockTimestampPacketContainer node1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(cmdQ.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(node2);
    UserEvent userEvent;
    Event eventWithoutContainer(nullptr, 0, 0, 0);

    uint32_t numEventsWithContainer = 2;
    uint32_t numEventsOnWaitlist = numEventsWithContainer + 2; // UserEvent + eventWithoutContainer

    cl_event waitlist[] = {&event0, &event1, &userEvent, &eventWithoutContainer};

    cl_event clOutEvent;
    cmdQ->enqueueMarkerWithWaitList(numEventsOnWaitlist, waitlist, &clOutEvent);

    auto outEvent = castToObject<Event>(clOutEvent);

    EXPECT_EQ(cmdQ->timestampPacketContainer->peekNodes().at(0), cmdQNodes.peekNodes().at(0)); // no new nodes obtained
    EXPECT_EQ(1u, cmdQ->timestampPacketContainer->peekNodes().size());

    auto &eventsNodes = outEvent->getTimestampPacketNodes()->peekNodes();
    EXPECT_EQ(numEventsWithContainer + 1, eventsNodes.size()); // numEventsWithContainer + command queue
    EXPECT_EQ(cmdQNodes.peekNodes().at(0), eventsNodes.at(0));
    EXPECT_EQ(event0.getTimestampPacketNodes()->peekNodes().at(0), eventsNodes.at(1));
    EXPECT_EQ(event1.getTimestampPacketNodes()->peekNodes().at(0), eventsNodes.at(2));

    clReleaseEvent(clOutEvent);
    userEvent.setStatus(CL_COMPLETE);
    cmdQ->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenBlockedEnqueueWithoutKernelWhenSubmittingThenDispatchBlockedCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto mockCsr = new MockCsrHw2<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->storeFlushedTaskStream = true;

    auto cmdQ0 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    auto &secondEngine = device->getEngine(getChosenEngineType(device->getHardwareInfo()), EngineUsage::LowPriority);
    static_cast<UltCommandStreamReceiver<FamilyType> *>(secondEngine.commandStreamReceiver)->timestampPacketWriteEnabled = true;

    auto cmdQ1 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    cmdQ1->gpgpuEngine = &secondEngine;
    cmdQ1->timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
    EXPECT_NE(&cmdQ0->getGpgpuCommandStreamReceiver(), &cmdQ1->getGpgpuCommandStreamReceiver());

    MockTimestampPacketContainer node0(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ0.get(), 0, 0, 0); // on the same CSR
    event0.addTimestampPacketNodes(node0);
    Event event1(cmdQ1.get(), 0, 0, 0); // on different CSR
    event1.addTimestampPacketNodes(node1);

    uint32_t numEventsOnWaitlist = 3;

    uint32_t commands[] = {CL_COMMAND_MARKER, CL_COMMAND_BARRIER};
    for (int i = 0; i < 2; i++) {
        UserEvent userEvent;
        cl_event waitlist[] = {&event0, &event1, &userEvent};
        if (commands[i] == CL_COMMAND_MARKER) {
            cmdQ0->enqueueMarkerWithWaitList(numEventsOnWaitlist, waitlist, nullptr);
        } else if (commands[i] == CL_COMMAND_BARRIER) {
            cmdQ0->enqueueBarrierWithWaitList(numEventsOnWaitlist, waitlist, nullptr);
        } else {
            EXPECT_TRUE(false);
        }

        auto initialCsrStreamOffset = mockCsr->commandStream.getUsed();
        userEvent.setStatus(CL_COMPLETE);

        HardwareParse hwParserCsr;
        HardwareParse hwParserCmdQ;
        LinearStream taskStream(mockCsr->storedTaskStream.get(), mockCsr->storedTaskStreamSize);
        taskStream.getSpace(mockCsr->storedTaskStreamSize);
        hwParserCsr.parseCommands<FamilyType>(mockCsr->commandStream, initialCsrStreamOffset);
        hwParserCmdQ.parseCommands<FamilyType>(taskStream, 0);

        auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
        auto expectedQueueSemaphoresCount = 1u;
        if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo())) {
            expectedQueueSemaphoresCount += 1;
        }
        EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
        verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), node0.getNode(0), 0);

        auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
        EXPECT_EQ(1u, csrSemaphores.size());
        verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(csrSemaphores[0])), node1.getNode(0), 0);

        EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
        EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
        EXPECT_EQ(device->getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);

        cmdQ0->isQueueBlocked();
    }
}

HWTEST_F(TimestampPacketTests, givenWaitlistAndOutputEventWhenEnqueueingMarkerWithoutKernelThenInheritTimestampPacketsAndProgramSemaphores) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer node1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(node2);

    uint32_t numEventsOnWaitlist = 2;

    cl_event waitlist[] = {&event0, &event1};

    cmdQ->enqueueMarkerWithWaitList(numEventsOnWaitlist, waitlist, nullptr);

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    hwParserCsr.parseCommands<FamilyType>(device->getUltCommandStreamReceiver<FamilyType>().commandStream, 0);
    hwParserCmdQ.parseCommands<FamilyType>(*cmdQ->commandStream, 0);

    auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    EXPECT_EQ(1u, csrSemaphores.size());
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(csrSemaphores[0])), node2.getNode(0), 0);

    auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
    auto expectedQueueSemaphoresCount = 1u;
    if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo())) {
        expectedQueueSemaphoresCount += 1;
    }
    EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), node1.getNode(0), 0);
}

HWTEST_F(TimestampPacketTests, givenWaitlistAndOutputEventWhenEnqueueingBarrierWithoutKernelThenInheritTimestampPacketsAndProgramSemaphores) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer node1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(node2);

    uint32_t numEventsOnWaitlist = 2;

    cl_event waitlist[] = {&event0, &event1};

    cmdQ->enqueueBarrierWithWaitList(numEventsOnWaitlist, waitlist, nullptr);

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    hwParserCsr.parseCommands<FamilyType>(device->getUltCommandStreamReceiver<FamilyType>().commandStream, 0);
    hwParserCmdQ.parseCommands<FamilyType>(*cmdQ->commandStream, 0);

    auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    EXPECT_EQ(1u, csrSemaphores.size());
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(csrSemaphores[0])), node2.getNode(0), 0);

    auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
    auto expectedQueueSemaphoresCount = 1u;
    if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo())) {
        expectedQueueSemaphoresCount += 1;
    }
    EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), node1.getNode(0), 0);
}
