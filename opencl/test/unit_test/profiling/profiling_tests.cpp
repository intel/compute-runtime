/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/hw_timestamps.h"
#include "shared/source/utilities/perf_counter.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/source/command_queue/enqueue_marker.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/event/event_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

using namespace NEO;

struct ProfilingTests : public CommandEnqueueFixture,
                        public ::testing::Test {
    void SetUp() override {
        CommandEnqueueFixture::setUp(CL_QUEUE_PROFILING_ENABLE);

        program = ReleaseableObjectPtr<MockProgram>(new MockProgram(toClDeviceVector(*pClDevice)));
        program->setContext(&ctx);

        kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
        kernelInfo.setCrossThreadDataSize(sizeof(crossThreadData));

        kernelInfo.setLocalIds({1, 1, 1});

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.kernelHeapSize = sizeof(kernelIsa);
    }

    void TearDown() override {
        program.reset();
        CommandEnqueueFixture::tearDown();
    }

    ReleaseableObjectPtr<MockProgram> program;

    SKernelBinaryHeaderCommon kernelHeader = {};
    MockKernelInfo kernelInfo;
    MockContext ctx;

    uint32_t kernelIsa[32];
    uint32_t crossThreadData[32];
};

template <typename TagType>
struct MockTagNode : public TagNode<TagType> {
  public:
    using TagNode<TagType>::tagForCpuAccess;
    using TagNode<TagType>::gfxAllocation;
    MockTagNode() {
        gfxAllocation = nullptr;
        tagForCpuAccess = nullptr;
    }
};

class MyOSDeviceTime : public DeviceTime {
    double getDynamicDeviceTimerResolution() const override {
        EXPECT_FALSE(true);
        return 1.0;
    }
    uint64_t getDynamicDeviceTimerClock() const override {
        EXPECT_FALSE(true);
        return 0;
    }
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *) override {
        EXPECT_FALSE(true);
        return TimeQueryStatus::deviceLost;
    }
};

class MockOsTime2 : public OSTime {
  public:
    static int instanceNum;
    MockOsTime2() {
        instanceNum++;
        this->deviceTime = std::make_unique<MyOSDeviceTime>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        EXPECT_FALSE(true);
        return false;
    };
    double getHostTimerResolution() const override {
        EXPECT_FALSE(true);
        return 0;
    }
    uint64_t getCpuRawTimestamp() override {
        EXPECT_FALSE(true);
        return 0;
    }
};

int MockOsTime2::instanceNum = 0;

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingTests, GivenCommandQueueWithProfilingAndForWorkloadWithKernelWhenGetCSFromCmdQueueThenEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(MI_STORE_REGISTER_MEM) + sizeof(GPGPU_WALKER) + HardwareCommandsHelper<FamilyType>::getSizeRequiredCS();

    MultiDispatchInfo multiDispatchInfo(&kernel);
    auto &commandStreamNDRangeKernel = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, CsrDependencies(), true, false, false,
                                                                                               multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, true, false, *pCmdQ, &kernel, {});
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamNDRangeKernel.getAvailableSpace(), requiredSize);

    auto &commandStreamTask = getCommandStream<FamilyType, CL_COMMAND_TASK>(*pCmdQ, CsrDependencies(), true, false, false,
                                                                            multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_TASK, true, false, *pCmdQ, &kernel, {});
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamTask.getAvailableSpace(), requiredSize);
}

HWTEST_F(ProfilingTests, GivenCommandQueueWithProfilingAndForWorkloadWithNoKernelWhenGetCSFromCmdQueueThenEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM);

    MultiDispatchInfo multiDispatchInfo(nullptr);
    auto &commandStreamMigrateMemObjects = getCommandStream<FamilyType, CL_COMMAND_MIGRATE_MEM_OBJECTS>(*pCmdQ, CsrDependencies(),
                                                                                                        true, false, false,
                                                                                                        multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MIGRATE_MEM_OBJECTS, true, false, *pCmdQ, nullptr, {});
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMigrateMemObjects.getAvailableSpace(), requiredSize);

    auto &commandStreamMarker = getCommandStream<FamilyType, CL_COMMAND_MARKER>(*pCmdQ, CsrDependencies(), true,
                                                                                false, false, multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MARKER, true, false, *pCmdQ, nullptr, {});
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMarker.getAvailableSpace(), requiredSize);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingTests, GivenCommandQueueWithProfilingAndForWorkloadWithTwoKernelsInMdiWhenGetCSFromCmdQueueThenEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM) + HardwareCommandsHelper<FamilyType>::getSizeRequiredCS();
    requiredSize += 2 * sizeof(GPGPU_WALKER);

    DispatchInfo dispatchInfo;
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    multiDispatchInfo.push(dispatchInfo);
    auto &commandStreamTask = getCommandStream<FamilyType, CL_COMMAND_TASK>(*pCmdQ, CsrDependencies(), true, false, false,
                                                                            multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_TASK, CsrDependencies(), true, false,
                                                                               false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamTask.getAvailableSpace(), requiredSize);
}

/*
#   Two additional PIPE_CONTROLs are expected before first MI_STORE_REGISTER_MEM (which is before GPGPU_WALKER)
#   and after second MI_STORE_REGISTER_MEM (which is after GPGPU_WALKER).
*/
HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingTests, GivenCommandQueueWithProfolingWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        &kernel,
        dimensions,
        globalOffsets,
        workItems,
        nullptr,
        0,
        nullptr,
        &event);

    parseCommands<FamilyType>(*pCmdQ);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check PIPE_CONTROLs
    auto itorBeforePC = reverseFind<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);
    EXPECT_EQ(1u, pBeforePC->getCommandStreamerStallEnable());

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);
    EXPECT_EQ(1u, pAfterPC->getCommandStreamerStallEnable());

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pBeforePC->getPostSyncOperation());

    EXPECT_TRUE(static_cast<MockEvent<Event> *>(event)->calcProfilingData());

    clReleaseEvent(event);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingTests, GivenCommandQueueWithProfilingWhenNonBlockedEnqueueIsExecutedThenSubmittedTimestampHasGPUTime) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        &kernel,
        dimensions,
        globalOffsets,
        workItems,
        nullptr,
        0,
        nullptr,
        &event);

    auto mockEvent = static_cast<MockEvent<Event> *>(event);
    EXPECT_NE(0u, mockEvent->queueTimeStamp.gpuTimeStamp);
    EXPECT_NE(0u, mockEvent->queueTimeStamp.gpuTimeInNs);
    EXPECT_NE(0u, mockEvent->queueTimeStamp.cpuTimeInNs);
    EXPECT_LT(mockEvent->queueTimeStamp.cpuTimeInNs, mockEvent->submitTimeStamp.cpuTimeInNs);
    EXPECT_LT(mockEvent->queueTimeStamp.gpuTimeInNs, mockEvent->submitTimeStamp.gpuTimeInNs);
    EXPECT_LT(mockEvent->queueTimeStamp.gpuTimeStamp, mockEvent->submitTimeStamp.gpuTimeStamp);

    clReleaseEvent(event);
}

/*
#   One additional MI_STORE_REGISTER_MEM is expected before and after GPGPU_WALKER.
*/

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingTests, GivenCommandQueueWithProflingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        &kernel,
        dimensions,
        globalOffsets,
        workItems,
        nullptr,
        0,
        nullptr,
        &event);

    parseCommands<FamilyType>(*pCmdQ);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check MI_STORE_REGISTER_MEMs
    auto itorBeforeMI = reverseFind<MI_STORE_REGISTER_MEM *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforeMI);
    auto pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    ASSERT_NE(nullptr, pBeforeMI);
    EXPECT_EQ(RegisterOffsets::gpThreadTimeRegAddressOffsetLow, pBeforeMI->getRegisterAddress());

    auto itorAfterMI = find<MI_STORE_REGISTER_MEM *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterMI);
    auto pAfterMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorAfterMI);
    ASSERT_NE(nullptr, pAfterMI);
    EXPECT_EQ(RegisterOffsets::gpThreadTimeRegAddressOffsetLow, pAfterMI->getRegisterAddress());
    ++itorAfterMI;
    pAfterMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorAfterMI);
    EXPECT_EQ(nullptr, pAfterMI);

    clReleaseEvent(event);
}

/*
#   Two additional PIPE_CONTROLs are expected before first MI_STORE_REGISTER_MEM (which is before GPGPU_WALKER)
#   and after second MI_STORE_REGISTER_MEM (which is after GPGPU_WALKER).
#   If queue is blocked commands should be added to event
*/
HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingTests, GivenCommandQueueBlockedWithProfilingWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());
    kernel.incRefInternal();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_event ue = new UserEvent();
    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        &kernel,
        dimensions,
        globalOffsets,
        workItems,
        nullptr,
        1,   // one user event to block queue
        &ue, // user event not signaled
        &event);

    // rseCommands<FamilyType>(*pCmdQ);
    ASSERT_NE(nullptr, pCmdQ->virtualEvent);
    ASSERT_NE(nullptr, pCmdQ->virtualEvent->peekCommand());
    NEO::LinearStream *eventCommandStream = pCmdQ->virtualEvent->peekCommand()->getCommandStream();
    ASSERT_NE(nullptr, eventCommandStream);
    parseCommands<FamilyType>(*eventCommandStream);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check PIPE_CONTROLs
    auto itorBeforePC = reverseFind<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pBeforePC->getPostSyncOperation());

    clReleaseEvent(event);
    ((UserEvent *)ue)->release();
    pCmdQ->isQueueBlocked();
}

/*
#   One additional MI_STORE_REGISTER_MEM is expected before and after GPGPU_WALKER.
#   If queue is blocked commands should be added to event
*/

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingTests, GivenCommandQueueBlockedWithProfilingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());
    kernel.incRefInternal();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_event ue = new UserEvent();

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        &kernel,
        dimensions,
        globalOffsets,
        workItems,
        nullptr,
        1,   // one user event to block queue
        &ue, // user event not signaled
        &event);

    // parseCommands<FamilyType>(*pCmdQ);
    ASSERT_NE(nullptr, pCmdQ->virtualEvent);
    ASSERT_NE(nullptr, pCmdQ->virtualEvent->peekCommand());
    NEO::LinearStream *eventCommandStream = pCmdQ->virtualEvent->peekCommand()->getCommandStream();
    ASSERT_NE(nullptr, eventCommandStream);
    parseCommands<FamilyType>(*eventCommandStream);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check MI_STORE_REGISTER_MEMs
    auto itorBeforeMI = reverseFind<MI_STORE_REGISTER_MEM *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforeMI);
    auto pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    ASSERT_NE(nullptr, pBeforeMI);
    EXPECT_EQ(RegisterOffsets::gpThreadTimeRegAddressOffsetLow, pBeforeMI->getRegisterAddress());

    auto itorAfterMI = find<MI_STORE_REGISTER_MEM *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterMI);
    auto pAfterMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorAfterMI);
    ASSERT_NE(nullptr, pAfterMI);
    EXPECT_EQ(RegisterOffsets::gpThreadTimeRegAddressOffsetLow, pAfterMI->getRegisterAddress());
    ++itorAfterMI;
    EXPECT_EQ(itorAfterMI, cmdList.end());
    clReleaseEvent(event);
    ((UserEvent *)ue)->release();
    pCmdQ->isQueueBlocked();
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingTests, GivenCommandQueueWithProflingWhenMarkerIsDispatchedThenPipeControlIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueMarkerWithWaitList(
        0,
        nullptr,
        &event);

    parseCommands<FamilyType>(*pCmdQ);

    // Check PIPE_CONTROLs
    auto itorFirstPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorFirstPC);
    auto pFirstPC = genCmdCast<PIPE_CONTROL *>(*itorFirstPC);
    ASSERT_NE(nullptr, pFirstPC);

    auto itorSecondPC = find<PIPE_CONTROL *>(itorFirstPC, cmdList.end());
    ASSERT_NE(cmdList.end(), itorSecondPC);
    auto pSecondPC = genCmdCast<PIPE_CONTROL *>(*itorSecondPC);
    ASSERT_NE(nullptr, pSecondPC);

    EXPECT_TRUE(static_cast<MockEvent<Event> *>(event)->calcProfilingData());

    clReleaseEvent(event);
}

HWTEST_F(ProfilingTests, givenBarrierEnqueueWhenNonBlockedEnqueueThenSetGpuPath) {
    cl_event event;
    pCmdQ->enqueueBarrierWithWaitList(0, nullptr, &event);
    auto eventObj = static_cast<Event *>(event);
    EXPECT_FALSE(eventObj->isCPUProfilingPath());
    pCmdQ->finish();

    uint64_t queued, submit;
    cl_int retVal;

    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_QUEUED, sizeof(uint64_t), &queued, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submit, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_LT(0u, queued);
    EXPECT_LT(queued, submit);

    eventObj->release();
}

HWTEST_F(ProfilingTests, givenMarkerEnqueueWhenNonBlockedEnqueueThenSetGpuPath) {
    cl_event event;
    pCmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);
    auto eventObj = static_cast<Event *>(event);
    EXPECT_FALSE(eventObj->isCPUProfilingPath());
    pCmdQ->finish();

    uint64_t queued, submit;
    cl_int retVal;

    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_QUEUED, sizeof(uint64_t), &queued, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submit, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_LT(0u, queued);
    EXPECT_LT(queued, submit);

    eventObj->release();
}

HWTEST_F(ProfilingTests, givenMarkerEnqueueWhenBlockedEnqueueThenSetGpuPath) {
    cl_event event = nullptr;
    cl_event userEvent = new UserEvent();
    pCmdQ->enqueueMarkerWithWaitList(1, &userEvent, &event);

    auto eventObj = static_cast<Event *>(event);
    EXPECT_FALSE(eventObj->isCPUProfilingPath());

    auto userEventObj = static_cast<UserEvent *>(userEvent);

    pCmdQ->flush();
    userEventObj->setStatus(CL_COMPLETE);
    Event::waitForEvents(1, &event);

    uint64_t queued = 0u, submit = 0u;
    cl_int retVal;
    HwTimeStamps timestamp;
    timestamp.globalStartTS = 10;
    timestamp.contextStartTS = 10;
    timestamp.globalEndTS = 80;
    timestamp.contextEndTS = 80;
    MockTagNode<HwTimeStamps> timestampNode;
    timestampNode.tagForCpuAccess = &timestamp;
    static_cast<MockEvent<Event> *>(eventObj)->timeStampNode = &timestampNode;
    if (eventObj->getTimestampPacketNodes()) {
        eventObj->getTimestampPacketNodes()->releaseNodes();
    }
    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_QUEUED, sizeof(uint64_t), &queued, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submit, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_LT(0u, queued);
    EXPECT_LT(queued, submit);

    static_cast<MockEvent<Event> *>(eventObj)->timeStampNode = nullptr;
    eventObj->release();
    userEventObj->release();
}

HWTEST_F(ProfilingTests, givenNonKernelEnqueueWhenNonBlockedEnqueueThenSetCpuPath) {
    cl_event event;
    MockBuffer buffer;
    auto bufferMemObj = static_cast<cl_mem>(&buffer);
    auto pBufferMemObj = &bufferMemObj;

    auto retVal = pCmdQ->enqueueMigrateMemObjects(
        1,
        pBufferMemObj,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &event);
    auto eventObj = static_cast<Event *>(event);
    EXPECT_TRUE(eventObj->isCPUProfilingPath() == CL_TRUE);
    pCmdQ->finish();

    uint64_t queued, submit, start, end;

    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_QUEUED, sizeof(uint64_t), &queued, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submit, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(uint64_t), &start, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = eventObj->getEventProfilingInfo(CL_PROFILING_COMMAND_END, sizeof(uint64_t), &end, 0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_LT(0u, queued);
    EXPECT_LT(queued, submit);
    EXPECT_LT(submit, start);
    EXPECT_LT(start, end);
    eventObj->release();
}

HWTEST_F(ProfilingTests, givenDebugFlagSetWhenWaitingForTimestampThenPrint) {
    DebugManagerStateRestore restorer;
    debugManager.flags.LogWaitingForCompletion.set(true);
    debugManager.flags.EnableTimestampWaitForQueues.set(4);
    debugManager.flags.EnableTimestampWaitForEvents.set(4);

    using TimestampPacketType = typename FamilyType::TimestampPacketType;
    using TimestampPacketsT = TimestampPackets<TimestampPacketType, FamilyType::timestampPacketCount>;

    MockGraphicsAllocation alloc;
    MultiGraphicsAllocation multiAlloc(1);
    multiAlloc.addAllocation(&alloc);

    struct MyMockTagNode : public TagNode<TimestampPacketsT> {
      public:
        using TagNode<TimestampPacketsT>::gfxAllocation;
        MyMockTagNode(MultiGraphicsAllocation *alloc) {
            this->gfxAllocation = alloc;
        }
        uint64_t getContextEndValue(uint32_t packetIndex) const override {
            EXPECT_NE(0u, failCountdown);
            failCountdown--;

            if (failCountdown == 0) {
                storage = 123;
            }

            return storage;
        }

        void const *getContextEndAddress(uint32_t packetIndex) const override {
            return &storage;
        }

        void returnTag() override {}

        mutable uint32_t failCountdown = 2;
        mutable uint32_t storage = 1;
    };

    auto node = std::make_unique<MyMockTagNode>(&multiAlloc);

    auto queue = std::make_unique<MockCommandQueueHw<FamilyType>>(pCmdQ->getContextPtr(), pClDevice, nullptr);
    queue->timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
    auto container = queue->timestampPacketContainer.get();

    StreamCapture capture;
    capture.captureStdout();

    Range<CopyEngineState> copyEngineStates;
    WaitStatus status;

    container->add(node.get());

    queue->waitForTimestamps(copyEngineStates, status, container, nullptr);

    std::string output = capture.getCapturedStdout();

    EXPECT_NE(output.npos, output.find("Waiting for TS 0x"));
    EXPECT_NE(output.npos, output.find("Waiting for TS completed"));
    EXPECT_EQ(output.npos, output.find("Waiting for TS failed"));

    capture.captureStdout();

    auto &csr = static_cast<UltCommandStreamReceiver<FamilyType> &>(queue->getGpgpuCommandStreamReceiver());
    csr.forceReturnGpuHang = true;
    node->failCountdown = 2;
    node->storage = 1;
    queue->waitForTimestamps(copyEngineStates, status, container, nullptr);

    output = capture.getCapturedStdout();

    EXPECT_NE(output.npos, output.find("Waiting for TS 0x"));
    EXPECT_EQ(output.npos, output.find("Waiting for TS completed"));
    EXPECT_NE(output.npos, output.find("Waiting for TS failed"));

    MockEvent<Event> event(queue.get(), CL_COMMAND_READ_BUFFER, 0, 0);
    event.timestampPacketContainer = std::make_unique<TimestampPacketContainer>();

    event.timestampPacketContainer->add(node.get());

    capture.captureStdout();
    node->failCountdown = 2;
    node->storage = 1;
    event.areTimestampsCompleted();

    output = capture.getCapturedStdout();

    EXPECT_NE(output.npos, output.find("Checking TS 0x"));
    EXPECT_EQ(output.npos, output.find("TS ready"));
    EXPECT_NE(output.npos, output.find("TS not ready"));

    capture.captureStdout();
    event.areTimestampsCompleted();
    output = capture.getCapturedStdout();

    EXPECT_NE(output.npos, output.find("Checking TS 0x"));
    EXPECT_NE(output.npos, output.find("TS ready"));
    EXPECT_EQ(output.npos, output.find("TS not ready"));
}

using EventProfilingTest = ProfilingTests;

HWCMDTEST_F(IGFX_GEN12LP_CORE, EventProfilingTest, givenEventWhenCompleteIsZeroThenCalcProfilingDataSetsEndTimestampInCompleteTimestampAndDoesntCallOsTimeMethods) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockOsTime2::instanceNum = 0;
    device->setOSTime(new MockOsTime2());
    EXPECT_EQ(1, MockOsTime2::instanceNum);
    MockContext context;
    cl_command_queue_properties props[5] = {0, 0, 0, 0, 0};
    MockCommandQueue cmdQ(&context, device.get(), props, false);
    cmdQ.setProfilingEnabled();
    cmdQ.device = device.get();

    HwTimeStamps timestamp;
    timestamp.globalStartTS = 10;
    timestamp.contextStartTS = 20;
    timestamp.globalEndTS = 80;
    timestamp.contextEndTS = 56;
    timestamp.globalCompleteTS = 0;
    timestamp.contextCompleteTS = 0;

    MockTagNode<HwTimeStamps> timestampNode;
    timestampNode.tagForCpuAccess = &timestamp;

    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);

    event.setCPUProfilingPath(false);
    event.timeStampNode = &timestampNode;
    event.calcProfilingData();

    EXPECT_EQ(timestamp.contextEndTS, timestamp.contextCompleteTS);
    cmdQ.device = nullptr;
    event.timeStampNode = nullptr;
}

using EventProfilingTests = ProfilingTests;

HWCMDTEST_F(IGFX_GEN12LP_CORE, EventProfilingTests, givenRawTimestampsDebugModeWhenDataIsQueriedThenRawDataIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.ReturnRawGpuTimestamps.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockOsTime2::instanceNum = 0;
    device->setOSTime(new MockOsTime2());
    EXPECT_EQ(1, MockOsTime2::instanceNum);
    MockContext context;
    cl_command_queue_properties props[5] = {0, 0, 0, 0, 0};
    MockCommandQueue cmdQ(&context, device.get(), props, false);
    cmdQ.setProfilingEnabled();
    cmdQ.device = device.get();

    HwTimeStamps timestamp;
    timestamp.globalStartTS = 10;
    timestamp.contextStartTS = 20;
    timestamp.globalEndTS = 80;
    timestamp.contextEndTS = 56;
    timestamp.globalCompleteTS = 0;
    timestamp.contextCompleteTS = 70;

    MockTagNode<HwTimeStamps> timestampNode;
    timestampNode.tagForCpuAccess = &timestamp;

    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);
    cl_event clEvent = &event;

    event.queueTimeStamp.gpuTimeStamp = 2;

    event.submitTimeStamp.gpuTimeStamp = 4;

    event.setCPUProfilingPath(false);
    event.timeStampNode = &timestampNode;
    event.calcProfilingData();

    cl_ulong queued, submited, start, end, complete;

    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &submited, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_COMPLETE, sizeof(cl_ulong), &complete, nullptr);

    EXPECT_EQ(timestamp.contextCompleteTS, complete);
    EXPECT_EQ(timestamp.contextEndTS, end);
    EXPECT_EQ(timestamp.contextStartTS, start);
    EXPECT_EQ(event.submitTimeStamp.gpuTimeStamp, submited);
    EXPECT_EQ(event.queueTimeStamp.gpuTimeStamp, queued);
    event.timeStampNode = nullptr;
}

TEST_F(EventProfilingTests, givenSubmitTimeMuchGreaterThanQueueTimeWhenCalculatingStartTimeThenItIsGreaterThanSubmitTime) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    MockCommandQueue cmdQ(&context, device.get(), nullptr, false);
    cmdQ.setProfilingEnabled();

    HwTimeStamps timestamp{};
    timestamp.globalStartTS = 10;
    timestamp.contextStartTS = 20;
    timestamp.globalEndTS = 80;
    timestamp.contextEndTS = 56;

    MockTagNode<HwTimeStamps> timestampNode{};
    timestampNode.tagForCpuAccess = &timestamp;

    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);
    cl_event clEvent = &event;

    event.queueTimeStamp.cpuTimeInNs = 1;
    event.queueTimeStamp.gpuTimeInNs = 1;
    event.queueTimeStamp.gpuTimeStamp = 2;

    event.submitTimeStamp.cpuTimeInNs = (1ull << 33) + 3;
    event.submitTimeStamp.gpuTimeInNs = (1ull << 33) + 3;
    event.submitTimeStamp.gpuTimeStamp = (1ull << 33) + 4;

    event.timeStampNode = &timestampNode;

    cl_ulong queued = 0ul;
    cl_ulong submited = 0ul;
    cl_ulong start = 0ul;
    cl_ulong end = 0ul;

    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &submited, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, nullptr);

    EXPECT_LT(0ull, queued);
    EXPECT_LT(queued, submited);
    EXPECT_LT(submited, start);
    EXPECT_LT(start, end);

    event.timeStampNode = nullptr;
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EventProfilingTest, givenRawTimestampsDebugModeWhenStartTimeStampLTQueueTimeStampThenIncreaseStartTimeStamp) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.ReturnRawGpuTimestamps.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockOsTime2::instanceNum = 0;
    device->setOSTime(new MockOsTime2());
    EXPECT_EQ(1, MockOsTime2::instanceNum);
    MockContext context(device.get());
    MockCommandQueue cmdQ(&context, device.get(), nullptr, false);
    cmdQ.setProfilingEnabled();
    cmdQ.device = device.get();

    HwTimeStamps timestamp;
    timestamp.globalStartTS = 0;
    timestamp.contextStartTS = 20;
    timestamp.globalEndTS = 80;
    timestamp.contextEndTS = 56;
    timestamp.globalCompleteTS = 0;
    timestamp.contextCompleteTS = 70;

    MockTagNode<HwTimeStamps> timestampNode;
    timestampNode.tagForCpuAccess = &timestamp;

    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);
    cl_event clEvent = &event;

    event.queueTimeStamp.cpuTimeInNs = 83;
    event.queueTimeStamp.gpuTimeStamp = 1;

    event.submitTimeStamp.cpuTimeInNs = 83;
    event.submitTimeStamp.gpuTimeStamp = 1;

    event.setCPUProfilingPath(false);
    event.timeStampNode = &timestampNode;
    event.calcProfilingData();

    cl_ulong queued, start;

    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, nullptr);

    EXPECT_LT(queued, start);
    event.timeStampNode = nullptr;
}

struct ProfilingWithPerfCountersTests : public PerformanceCountersFixture, ::testing::Test {
    void SetUp() override {
        setUp(defaultHwInfo.get());
    }

    void setUp(const NEO::HardwareInfo *hardwareInfo) {
        PerformanceCountersFixture::setUp();

        HardwareInfo hwInfo = *hardwareInfo;
        if (hwInfo.capabilityTable.defaultEngineType == aub_stream::EngineType::ENGINE_CCS) {
            hwInfo.featureTable.flags.ftrCCSNode = true;
        }

        pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
        pClDevice = std::make_unique<ClDevice>(*pDevice, nullptr);

        pDevice->setPerfCounters(MockPerformanceCounters::create());

        context = std::make_unique<MockContext>(pClDevice.get());

        cl_int retVal = CL_SUCCESS;
        cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
        pCmdQ.reset(CommandQueue::create(context.get(), pClDevice.get(), properties, false, retVal));

        kernel = std::make_unique<MockKernelWithInternals>(*pClDevice);
    }

    void TearDown() override {
        PerformanceCountersFixture::tearDown();
    }

    template <typename GfxFamily>
    GenCmdList::iterator expectStoreRegister(const GenCmdList &cmdList, GenCmdList::iterator itor, uint64_t memoryAddress, uint32_t registerAddress) {
        using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

        itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
        EXPECT_NE(cmdList.end(), itor);
        auto pStore = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(memoryAddress, pStore->getMemoryAddress());
        EXPECT_EQ(registerAddress, pStore->getRegisterAddress());
        itor++;
        return itor;
    }

    MockDevice *pDevice = nullptr;
    std::unique_ptr<ClDevice> pClDevice;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<CommandQueue> pCmdQ;
    std::unique_ptr<MockKernelWithInternals> kernel;
};

struct ProfilingWithPerfCountersOnCCSTests : ProfilingWithPerfCountersTests {
    void SetUp() override {
        auto hwInfo = *defaultHwInfo;
        hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
        ProfilingWithPerfCountersTests::setUp(&hwInfo);
    }

    void TearDown() override {
        ProfilingWithPerfCountersTests::TearDown();
    }
};

HWTEST_F(ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCounterAndForWorkloadWithNoKernelWhenGetCSFromCmdQueueThenEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    pCmdQ->setPerfCountersEnabled();

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM);

    MultiDispatchInfo multiDispatchInfo(nullptr);
    auto &commandStreamMigrateMemObjects = getCommandStream<FamilyType, CL_COMMAND_MIGRATE_MEM_OBJECTS>(*pCmdQ, CsrDependencies(),
                                                                                                        true, true, false, multiDispatchInfo,
                                                                                                        nullptr, 0, false, false, false, nullptr);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MIGRATE_MEM_OBJECTS, true, true, *pCmdQ, nullptr, {});
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMigrateMemObjects.getAvailableSpace(), requiredSize);

    auto &commandStreamMarker = getCommandStream<FamilyType, CL_COMMAND_MARKER>(*pCmdQ, CsrDependencies(), true, true, false,
                                                                                multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MARKER, true, true, *pCmdQ, nullptr, {});
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMarker.getAvailableSpace(), requiredSize);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(kernel->mockKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    ClHardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*pCmdQ);

    // expect MI_REPORT_PERF_COUNT before WALKER
    auto itorBeforeReportPerf = find<MI_REPORT_PERF_COUNT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBeforeReportPerf);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(itorBeforeReportPerf, cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check PIPE_CONTROLs
    auto itorBeforePC = reverseFind<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);
    EXPECT_EQ(1u, pBeforePC->getCommandStreamerStallEnable());

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);
    EXPECT_EQ(1u, pAfterPC->getCommandStreamerStallEnable());

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pBeforePC->getPostSyncOperation());

    // expect MI_REPORT_PERF_COUNT after WALKER
    auto itorAfterReportPerf = find<MI_REPORT_PERF_COUNT *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterReportPerf);

    EXPECT_TRUE(static_cast<MockEvent<Event> *>(event)->calcProfilingData());

    clReleaseEvent(event);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersNoUserRegistersWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(kernel->mockKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    ClHardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*pCmdQ);

    // expect MI_REPORT_PERF_COUNT before WALKER
    auto itorBeforeReportPerf = find<MI_REPORT_PERF_COUNT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBeforeReportPerf);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(itorBeforeReportPerf, cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check PIPE_CONTROLs
    auto itorBeforePC = reverseFind<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);
    EXPECT_EQ(1u, pBeforePC->getCommandStreamerStallEnable());

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);
    EXPECT_EQ(1u, pAfterPC->getCommandStreamerStallEnable());

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pBeforePC->getPostSyncOperation());

    // expect MI_REPORT_PERF_COUNT after WALKER
    auto itorAfterReportPerf = find<MI_REPORT_PERF_COUNT *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterReportPerf);

    EXPECT_TRUE(static_cast<MockEvent<Event> *>(event)->calcProfilingData());

    clReleaseEvent(event);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingWithPerfCountersTests, GivenCommandQueueBlockedWithProflingPerfCounterWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_event ue = new UserEvent();
    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(kernel->mockKernel, dimensions, globalOffsets, workItems, nullptr,
                                                                          1,   // one user event to block queue
                                                                          &ue, // user event not signaled
                                                                          &event);

    // rseCommands<FamilyType>(*pCmdQ);
    ASSERT_NE(nullptr, pCmdQ->virtualEvent);
    ASSERT_NE(nullptr, pCmdQ->virtualEvent->peekCommand());
    NEO::LinearStream *eventCommandStream = pCmdQ->virtualEvent->peekCommand()->getCommandStream();
    ASSERT_NE(nullptr, eventCommandStream);

    HardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*eventCommandStream);

    // expect MI_REPORT_PERF_COUNT before WALKER
    auto itorBeforeReportPerf = find<MI_REPORT_PERF_COUNT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBeforeReportPerf);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(itorBeforeReportPerf, cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check PIPE_CONTROLs
    auto itorBeforePC = reverseFind<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pBeforePC->getPostSyncOperation());

    // expect MI_REPORT_PERF_COUNT after WALKER
    auto itorAfterReportPerf = find<MI_REPORT_PERF_COUNT *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterReportPerf);

    clReleaseEvent(event);
    ((UserEvent *)ue)->release();
    pCmdQ->isQueueBlocked();
}

HWTEST_F(ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersNoEventWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsNotPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(kernel->mockKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, nullptr);

    ClHardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*pCmdQ);

    // expect no MI_REPORT_PERF_COUNT before WALKER
    auto itorBeforeReportPerf = find<MI_REPORT_PERF_COUNT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorBeforeReportPerf);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check PIPE_CONTROLs
    auto itorBeforePC = reverseFind<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);
    EXPECT_EQ(1u, pBeforePC->getCommandStreamerStallEnable());

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);
    EXPECT_EQ(1u, pAfterPC->getCommandStreamerStallEnable());

    if (pCmdQ->getHeaplessStateInitEnabled()) {
        EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pBeforePC->getPostSyncOperation());

    } else {
        EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE, pBeforePC->getPostSyncOperation());
    }

    // expect MI_REPORT_PERF_COUNT after WALKER
    auto itorAfterReportPerf = find<MI_REPORT_PERF_COUNT *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_EQ(cmdList.end(), itorAfterReportPerf);
}

template <typename TagType>
struct FixedGpuAddressTagAllocator : MockTagAllocator<TagType> {
    using TagAllocator<TagType>::usedTags;
    using TagAllocator<TagType>::deferredTags;

    struct MockTagNode : TagNode<TagType> {
        void setGpuAddress(uint64_t value) { this->gpuAddress = value; }
    };

    FixedGpuAddressTagAllocator(CommandStreamReceiver &csr, uint64_t gpuAddress)
        : MockTagAllocator<TagType>(csr.getRootDeviceIndex(), csr.getMemoryManager(), csr.getPreferredTagPoolSize(), MemoryConstants::cacheLineSize,
                                    sizeof(TagType), false, csr.getOsContext().getDeviceBitfield()) {
        auto tag = reinterpret_cast<MockTagNode *>(this->freeTags.peekHead());
        tag->setGpuAddress(gpuAddress);
    }
};

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenRegisterStoresArePresentInCS) {
    uint64_t timeStampGpuAddress = 0x123456000;
    uint64_t perfCountersGpuAddress = 0xabcdef000;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.profilingTimeStampAllocator.reset(new FixedGpuAddressTagAllocator<HwTimeStamps>(csr, timeStampGpuAddress));
    csr.perfCounterAllocator.reset(new FixedGpuAddressTagAllocator<HwPerfCounter>(csr, perfCountersGpuAddress));

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(kernel->mockKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    auto pEvent = static_cast<MockEvent<Event> *>(event);
    EXPECT_EQ(pEvent->getHwTimeStampNode()->getGpuAddress(), timeStampGpuAddress);
    EXPECT_EQ(pEvent->getHwPerfCounterNode()->getGpuAddress(), perfCountersGpuAddress);
    ClHardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*pCmdQ);

    auto itor = expectStoreRegister<FamilyType>(cmdList, cmdList.begin(), timeStampGpuAddress + offsetof(HwTimeStamps, contextStartTS), RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    // after WALKER:

    itor = expectStoreRegister<FamilyType>(cmdList, itor, timeStampGpuAddress + offsetof(HwTimeStamps, contextEndTS), RegisterOffsets::gpThreadTimeRegAddressOffsetLow);

    EXPECT_TRUE(pEvent->calcProfilingData());

    clReleaseEvent(event);
}

HWTEST_F(ProfilingWithPerfCountersTests, givenTimestampPacketsEnabledWhenEnqueueIsCalledThenDontAllocateHwTimeStamps) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockAllocator = new FixedGpuAddressTagAllocator<HwTimeStamps>(csr, 0x123);
    csr.profilingTimeStampAllocator.reset(mockAllocator);

    auto myCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(pCmdQ->getContextPtr(), pClDevice.get(), nullptr);
    myCmdQ->setProfilingEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    cl_event event;

    myCmdQ->enqueueKernel(kernel->mockKernel, 1, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    EXPECT_EQ(!!myCmdQ->getTimestampPacketContainer(), mockAllocator->usedTags.peekIsEmpty());
    EXPECT_TRUE(mockAllocator->deferredTags.peekIsEmpty());

    clReleaseEvent(event);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingWithPerfCountersOnCCSTests, givenCommandQueueBlockedWithProfilingPerfCountersWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_REPORT_PERF_COUNT = typename FamilyType::MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_event userEvent = clCreateUserEvent(context.get(), nullptr);
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get());

    cmdQHw->enqueueKernel(kernel->mockKernel, dimensions, globalOffsets, workItems, nullptr, 1, &userEvent, &event);
    ASSERT_NE(nullptr, pCmdQ->virtualEvent);
    ASSERT_NE(nullptr, pCmdQ->virtualEvent->peekCommand());
    NEO::LinearStream *eventCommandStream = pCmdQ->virtualEvent->peekCommand()->getCommandStream();
    ASSERT_NE(nullptr, eventCommandStream);

    HardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*eventCommandStream);

    // expect MI_REPORT_PERF_COUNT before WALKER
    auto itorBeforeReportPerf = find<MI_REPORT_PERF_COUNT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBeforeReportPerf);

    // find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(itorBeforeReportPerf, cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // check PIPE_CONTROLs
    auto itorBeforePC = reverseFind<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pBeforePC->getPostSyncOperation());

    // expect MI_REPORT_PERF_COUNT after WALKER
    auto itorAfterReportPerf = find<MI_REPORT_PERF_COUNT *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterReportPerf);

    clReleaseEvent(event);
    clReleaseEvent(userEvent);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProfilingWithPerfCountersOnCCSTests, givenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_REPORT_PERF_COUNT = typename FamilyType::MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get());

    cmdQHw->enqueueKernel(kernel->mockKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    ClHardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*pCmdQ);

    // expect MI_REPORT_PERF_COUNT before WALKER
    auto itorBeforeReportPerf = find<MI_REPORT_PERF_COUNT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBeforeReportPerf);

    // find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(itorBeforeReportPerf, cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // check PIPE_CONTROLs
    auto itorBeforePC = reverseFind<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);
    EXPECT_EQ(1u, pBeforePC->getCommandStreamerStallEnable());

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);
    EXPECT_EQ(1u, pAfterPC->getCommandStreamerStallEnable());

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pBeforePC->getPostSyncOperation());

    // expect MI_REPORT_PERF_COUNT after WALKER
    auto itorAfterReportPerf = find<MI_REPORT_PERF_COUNT *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterReportPerf);

    EXPECT_TRUE(static_cast<MockEvent<Event> *>(event)->calcProfilingData());

    clReleaseEvent(event);
}

struct MockTimestampContainer : public TimestampPacketContainer {
    ~MockTimestampContainer() override {
        for (const auto &node : timestampPacketNodes) {
            auto mockNode = static_cast<MockTagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> *>(node);
            delete mockNode->tagForCpuAccess;
            delete node;
        }
        timestampPacketNodes.clear();
    }
};

struct ProfilingTimestampPacketsTest : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.ReturnRawGpuTimestamps.set(true);
        cmdQ->setProfilingEnabled();
        ev->timestampPacketContainer = std::make_unique<MockTimestampContainer>();
    }

    void addTimestampNode(uint32_t contextStart, uint32_t contextEnd, uint32_t globalStart, uint32_t globalEnd) {
        auto node = new MockTagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>>();
        auto timestampPacketStorage = new TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>();
        node->tagForCpuAccess = timestampPacketStorage;

        uint32_t values[4] = {contextStart, globalStart, contextEnd, globalEnd};
        timestampPacketStorage->assignDataToAllTimestamps(0, values);

        ev->timestampPacketContainer->add(node);
    }

    void addTimestampNodeMultiOsContext(uint32_t globalStart[16], uint32_t globalEnd[16], uint32_t contextStart[16], uint32_t contextEnd[16], uint32_t size) {
        auto node = new MockTagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>>();
        auto timestampPacketStorage = new TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>();
        node->setPacketsUsed(size);

        for (uint32_t i = 0u; i < node->getPacketsUsed(); ++i) {
            uint32_t values[4] = {contextStart[i], globalStart[i], contextEnd[i], globalEnd[i]};

            timestampPacketStorage->assignDataToAllTimestamps(i, values);
        }

        node->tagForCpuAccess = timestampPacketStorage;
        ev->timestampPacketContainer->add(node);
    }

    void initTimestampNodeMultiOsContextData(uint32_t globalStart[16], uint32_t globalEnd[16], uint32_t size) {

        for (uint32_t i = 0u; i < size; ++i) {
            globalStart[i] = 100;
        }
        globalStart[5] = {50};

        for (uint32_t i = 0u; i < size; ++i) {
            globalEnd[i] = 200;
        }
        globalEnd[7] = {350};
    }

    DebugManagerStateRestore restorer;
    MockContext context;
    cl_command_queue_properties props[5] = {0, 0, 0, 0, 0};
    ReleaseableObjectPtr<MockCommandQueue> cmdQ = clUniquePtr(new MockCommandQueue(&context, context.getDevice(0), props, false));
    ReleaseableObjectPtr<MockEvent<MyEvent>> ev = clUniquePtr(new MockEvent<MyEvent>(cmdQ.get(), CL_COMMAND_USER, CompletionStamp::notReady, CompletionStamp::notReady));
};

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithOneElementAndTimestampNodeWhenCalculatingProfilingThenTimesAreTakenFromPacket) {
    addTimestampNode(10, 11, 12, 13);

    HwTimeStamps hwTimestamps;
    hwTimestamps.contextStartTS = 100;
    hwTimestamps.contextEndTS = 110;
    hwTimestamps.globalStartTS = 120;

    MockTagNode<HwTimeStamps> hwTimestampsNode;
    hwTimestampsNode.tagForCpuAccess = &hwTimestamps;
    ev->timeStampNode = &hwTimestampsNode;

    ev->calcProfilingData();

    EXPECT_EQ(12u, ev->startTimeStamp.gpuTimeStamp);
    EXPECT_EQ(13u, ev->endTimeStamp.gpuTimeStamp);
    EXPECT_EQ(12u, ev->getGlobalStartTimestamp());

    ev->timeStampNode = nullptr;
}

TEST_F(ProfilingTimestampPacketsTest, givenMultiOsContextCapableSetToTrueWhenCalcProfilingDataIsCalledThenCorrectedValuesAreReturned) {
    uint32_t globalStart[16] = {0};
    uint32_t globalEnd[16] = {0};
    uint32_t contextStart[16] = {0};
    uint32_t contextEnd[16] = {0};
    initTimestampNodeMultiOsContextData(globalStart, globalEnd, 16u);
    addTimestampNodeMultiOsContext(globalStart, globalEnd, contextStart, contextEnd, 16u);
    auto &device = reinterpret_cast<MockDevice &>(cmdQ->getDevice());
    auto &csr = device.getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();
    csr.multiOsContextCapable = true;

    ev->calcProfilingData();
    EXPECT_EQ(50u, ev->startTimeStamp.gpuTimeStamp);
    EXPECT_EQ(350u, ev->endTimeStamp.gpuTimeStamp);
}

TEST_F(ProfilingTimestampPacketsTest, givenTimestampPacketWithoutProfilingDataWhenCalculatingThenDontUseThatPacket) {
    uint32_t globalStart0 = 20;
    uint32_t globalEnd0 = 51;
    uint32_t contextStart0 = 21;
    uint32_t contextEnd0 = 50;

    uint32_t globalStart1 = globalStart0 - 1;
    uint32_t globalEnd1 = globalEnd0 + 1;
    uint32_t contextStart1 = contextStart0 - 1;
    uint32_t contextEnd1 = contextEnd0 + 1;

    addTimestampNodeMultiOsContext(&globalStart0, &globalEnd0, &contextStart0, &contextEnd0, 1);
    addTimestampNodeMultiOsContext(&globalStart1, &globalEnd1, &contextStart1, &contextEnd1, 1);
    auto &device = reinterpret_cast<MockDevice &>(cmdQ->getDevice());
    auto &csr = device.getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();
    csr.multiOsContextCapable = true;

    ev->timestampPacketContainer->peekNodes()[1]->setProfilingCapable(false);

    ev->calcProfilingData();
    EXPECT_EQ(static_cast<uint64_t>(globalStart0), ev->startTimeStamp.gpuTimeStamp);
    EXPECT_EQ(static_cast<uint64_t>(globalEnd0), ev->endTimeStamp.gpuTimeStamp);
}

TEST_F(ProfilingTimestampPacketsTest, givenPrintTimestampPacketContentsSetWhenCalcProfilingDataThenTimeStampsArePrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintTimestampPacketContents.set(true);
    StreamCapture capture;
    capture.captureStdout();

    auto &device = reinterpret_cast<MockDevice &>(cmdQ->getDevice());
    auto &csr = device.getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();
    csr.multiOsContextCapable = true;

    uint32_t globalStart[16] = {0};
    uint32_t globalEnd[16] = {0};
    uint32_t contextStart[16] = {0};
    uint32_t contextEnd[16] = {0};
    for (int i = 0; i < 16; i++) {
        globalStart[i] = 2 * i;
        globalEnd[i] = 500 * i;
        contextStart[i] = 7 * i;
        contextEnd[i] = 94 * i;
    }
    addTimestampNodeMultiOsContext(globalStart, globalEnd, contextStart, contextEnd, 16u);

    ev->calcProfilingData();

    std::string output = capture.getCapturedStdout();
    std::stringstream expected;

    expected << "Timestamp 0, cmd type: " << ev->getCommandType() << ", ";
    for (int i = 0; i < 16; i++) {
        expected << "packet " << i << ": "
                 << "global start: " << globalStart[i] << ", "
                 << "global end: " << globalEnd[i] << ", "
                 << "context start: " << contextStart[i] << ", "
                 << "context end: " << contextEnd[i] << ", "
                 << "global delta: " << globalEnd[i] - globalStart[i] << ", "
                 << "context delta: " << contextEnd[i] - contextStart[i] << std::endl;
    }
    EXPECT_EQ(0, output.compare(expected.str().c_str()));
}

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithThreeElementsWhenCalculatingProfilingThenTimesAreTakenFromProperPacket) {
    addTimestampNode(10, 11, 12, 13);
    addTimestampNode(1, 21, 22, 13);
    addTimestampNode(5, 31, 2, 13);

    ev->calcProfilingData();

    EXPECT_EQ(2u, ev->startTimeStamp.gpuTimeStamp);
    EXPECT_EQ(13u, ev->endTimeStamp.gpuTimeStamp);
    EXPECT_EQ(2u, ev->getGlobalStartTimestamp());
}

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithZeroElementsWhenCalculatingProfilingThenDataIsNotCalculated) {
    EXPECT_EQ(0u, ev->timestampPacketContainer->peekNodes().size());
    ev->calcProfilingData();

    EXPECT_FALSE(ev->getDataCalcStatus());
}
