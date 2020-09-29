/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/source/command_queue/enqueue_kernel.h"
#include "opencl/source/command_queue/enqueue_marker.h"
#include "opencl/source/command_queue/enqueue_migrate_mem_objects.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/event/event_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"
#include "test.h"

namespace NEO {

struct ProfilingTests : public CommandEnqueueFixture,
                        public ::testing::Test {
    void SetUp() override {
        CommandEnqueueFixture::SetUp(CL_QUEUE_PROFILING_ENABLE);

        program = ReleaseableObjectPtr<MockProgram>(new MockProgram(*pDevice->getExecutionEnvironment()));
        program->setContext(&ctx);

        memset(&dataParameterStream, 0, sizeof(dataParameterStream));
        dataParameterStream.DataParameterStreamSize = sizeof(crossThreadData);

        executionEnvironment = {};
        memset(&executionEnvironment, 0, sizeof(executionEnvironment));
        executionEnvironment.CompiledSIMD32 = 1;
        executionEnvironment.LargestCompiledSIMDSize = 32;

        memset(&threadPayload, 0, sizeof(threadPayload));
        threadPayload.LocalIDXPresent = 1;
        threadPayload.LocalIDYPresent = 1;
        threadPayload.LocalIDZPresent = 1;

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.KernelHeapSize = sizeof(kernelIsa);
        kernelInfo.patchInfo.dataParameterStream = &dataParameterStream;
        kernelInfo.patchInfo.executionEnvironment = &executionEnvironment;
        kernelInfo.patchInfo.threadPayload = &threadPayload;
    }

    void TearDown() override {
        CommandEnqueueFixture::TearDown();
    }

    ReleaseableObjectPtr<MockProgram> program;

    SKernelBinaryHeaderCommon kernelHeader = {};
    SPatchDataParameterStream dataParameterStream = {};
    SPatchExecutionEnvironment executionEnvironment = {};
    SPatchThreadPayload threadPayload = {};
    KernelInfo kernelInfo;
    MockContext ctx;

    uint32_t kernelIsa[32];
    uint32_t crossThreadData[32];
};

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GivenCommandQueueWithProfilingAndForWorkloadWithKernelWhenGetCSFromCmdQueueThenEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(MI_STORE_REGISTER_MEM) + sizeof(GPGPU_WALKER) + HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);

    MultiDispatchInfo multiDispatchInfo(&kernel);
    auto &commandStreamNDRangeKernel = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, CsrDependencies(), true, false, false,
                                                                                               multiDispatchInfo, nullptr, 0);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, true, false, *pCmdQ, &kernel);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamNDRangeKernel.getAvailableSpace(), requiredSize);

    auto &commandStreamTask = getCommandStream<FamilyType, CL_COMMAND_TASK>(*pCmdQ, CsrDependencies(), true, false, false,
                                                                            multiDispatchInfo, nullptr, 0);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_TASK, true, false, *pCmdQ, &kernel);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamTask.getAvailableSpace(), requiredSize);
}

HWTEST_F(ProfilingTests, GivenCommandQueueWithProfilingAndForWorkloadWithNoKernelWhenGetCSFromCmdQueueThenEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM);

    MultiDispatchInfo multiDispatchInfo(nullptr);
    auto &commandStreamMigrateMemObjects = getCommandStream<FamilyType, CL_COMMAND_MIGRATE_MEM_OBJECTS>(*pCmdQ, CsrDependencies(),
                                                                                                        true, false, false,
                                                                                                        multiDispatchInfo, nullptr, 0);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MIGRATE_MEM_OBJECTS, true, false, *pCmdQ, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMigrateMemObjects.getAvailableSpace(), requiredSize);

    auto &commandStreamMarker = getCommandStream<FamilyType, CL_COMMAND_MARKER>(*pCmdQ, CsrDependencies(), true,
                                                                                false, false, multiDispatchInfo, nullptr, 0);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MARKER, true, false, *pCmdQ, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMarker.getAvailableSpace(), requiredSize);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GivenCommandQueueWithProfilingAndForWorkloadWithTwoKernelsInMdiWhenGetCSFromCmdQueueThenEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM) + HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);
    requiredSize += 2 * sizeof(GPGPU_WALKER);

    DispatchInfo dispatchInfo;
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    multiDispatchInfo.push(dispatchInfo);
    auto &commandStreamTask = getCommandStream<FamilyType, CL_COMMAND_TASK>(*pCmdQ, CsrDependencies(), true, false, false,
                                                                            multiDispatchInfo, nullptr, 0);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_TASK, CsrDependencies(), true, false,
                                                                               false, *pCmdQ, multiDispatchInfo);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamTask.getAvailableSpace(), requiredSize);
}

/*
#   Two additional PIPE_CONTROLs are expected before first MI_STORE_REGISTER_MEM (which is before GPGPU_WALKER)
#   and after second MI_STORE_REGISTER_MEM (which is after GPGPU_WALKER).
*/
HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GivenCommandQueueWithProfolingWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_kernel clKernel = &kernel;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        clKernel,
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
    auto itorBeforePC = reverse_find<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
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

/*
#   One additional MI_STORE_REGISTER_MEM is expected before and after GPGPU_WALKER.
*/

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GivenCommandQueueWithProflingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS) {
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
    auto itorBeforeMI = reverse_find<MI_STORE_REGISTER_MEM *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforeMI);
    auto pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    ASSERT_NE(nullptr, pBeforeMI);
    EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, pBeforeMI->getRegisterAddress());

    auto itorAfterMI = find<MI_STORE_REGISTER_MEM *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterMI);
    auto pAfterMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorAfterMI);
    ASSERT_NE(nullptr, pAfterMI);
    EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, pAfterMI->getRegisterAddress());
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
HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GivenCommandQueueBlockedWithProfilingWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

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

    //rseCommands<FamilyType>(*pCmdQ);
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
    auto itorBeforePC = reverse_find<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
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

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GivenCommandQueueBlockedWithProfilingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

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
    auto itorBeforeMI = reverse_find<MI_STORE_REGISTER_MEM *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforeMI);
    auto pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    ASSERT_NE(nullptr, pBeforeMI);
    EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, pBeforeMI->getRegisterAddress());

    auto itorAfterMI = find<MI_STORE_REGISTER_MEM *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterMI);
    auto pAfterMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorAfterMI);
    ASSERT_NE(nullptr, pAfterMI);
    EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, pAfterMI->getRegisterAddress());
    ++itorAfterMI;
    EXPECT_EQ(itorAfterMI, cmdList.end());
    clReleaseEvent(event);
    ((UserEvent *)ue)->release();
    pCmdQ->isQueueBlocked();
}

HWTEST_F(ProfilingTests, givenNonKernelEnqueueWhenNonBlockedEnqueueThenSetCpuPath) {
    cl_event event;
    pCmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);
    auto eventObj = static_cast<Event *>(event);
    EXPECT_TRUE(eventObj->isCPUProfilingPath() == CL_TRUE);
    pCmdQ->finish();

    uint64_t queued, submit, start, end;
    cl_int retVal;

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

class MyOSTime : public OSTime {
  public:
    static int instanceNum;
    MyOSTime() {
        instanceNum++;
    }
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        EXPECT_FALSE(true);
        return 1.0;
    }
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime) override {
        EXPECT_FALSE(true);
        return false;
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

int MyOSTime::instanceNum = 0;

using EventProfilingTest = ProfilingTests;

HWCMDTEST_F(IGFX_GEN8_CORE, EventProfilingTest, givenEventWhenCompleteIsZeroThenCalcProfilingDataSetsEndTimestampInCompleteTimestampAndDoesntCallOsTimeMethods) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MyOSTime::instanceNum = 0;
    device->setOSTime(new MyOSTime());
    EXPECT_EQ(1, MyOSTime::instanceNum);
    MockContext context;
    cl_command_queue_properties props[5] = {0, 0, 0, 0, 0};
    MockCommandQueue cmdQ(&context, device.get(), props);
    cmdQ.setProfilingEnabled();
    cmdQ.device = device.get();

    HwTimeStamps timestamp;
    timestamp.GlobalStartTS = 10;
    timestamp.ContextStartTS = 20;
    timestamp.GlobalEndTS = 80;
    timestamp.ContextEndTS = 56;
    timestamp.GlobalCompleteTS = 0;
    timestamp.ContextCompleteTS = 0;

    MockTagNode<HwTimeStamps> timestampNode;
    timestampNode.tagForCpuAccess = &timestamp;

    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);

    event.setCPUProfilingPath(false);
    event.timeStampNode = &timestampNode;
    event.calcProfilingData();

    EXPECT_EQ(timestamp.ContextEndTS, timestamp.ContextCompleteTS);
    cmdQ.device = nullptr;
    event.timeStampNode = nullptr;
}

using EventProfilingTests = ProfilingTests;

HWCMDTEST_F(IGFX_GEN8_CORE, EventProfilingTests, givenRawTimestampsDebugModeWhenDataIsQueriedThenRawDataIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.ReturnRawGpuTimestamps.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MyOSTime::instanceNum = 0;
    device->setOSTime(new MyOSTime());
    EXPECT_EQ(1, MyOSTime::instanceNum);
    MockContext context;
    cl_command_queue_properties props[5] = {0, 0, 0, 0, 0};
    MockCommandQueue cmdQ(&context, device.get(), props);
    cmdQ.setProfilingEnabled();
    cmdQ.device = device.get();

    HwTimeStamps timestamp;
    timestamp.GlobalStartTS = 10;
    timestamp.ContextStartTS = 20;
    timestamp.GlobalEndTS = 80;
    timestamp.ContextEndTS = 56;
    timestamp.GlobalCompleteTS = 0;
    timestamp.ContextCompleteTS = 70;

    MockTagNode<HwTimeStamps> timestampNode;
    timestampNode.tagForCpuAccess = &timestamp;

    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);
    cl_event clEvent = &event;

    event.queueTimeStamp.CPUTimeinNS = 1;
    event.queueTimeStamp.GPUTimeStamp = 2;

    event.submitTimeStamp.CPUTimeinNS = 3;
    event.submitTimeStamp.GPUTimeStamp = 4;

    event.setCPUProfilingPath(false);
    event.timeStampNode = &timestampNode;
    event.calcProfilingData();

    cl_ulong queued, submited, start, end, complete;

    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &submited, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_COMPLETE, sizeof(cl_ulong), &complete, nullptr);

    EXPECT_EQ(timestamp.ContextCompleteTS, complete);
    EXPECT_EQ(timestamp.ContextEndTS, end);
    EXPECT_EQ(timestamp.ContextStartTS, start);
    EXPECT_EQ(event.submitTimeStamp.GPUTimeStamp, submited);
    EXPECT_EQ(event.queueTimeStamp.GPUTimeStamp, queued);
    event.timeStampNode = nullptr;
}

HWCMDTEST_F(IGFX_GEN8_CORE, EventProfilingTest, givenRawTimestampsDebugModeWhenStartTimeStampLTQueueTimeStampThenIncreaseStartTimeStamp) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.ReturnRawGpuTimestamps.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MyOSTime::instanceNum = 0;
    device->setOSTime(new MyOSTime());
    EXPECT_EQ(1, MyOSTime::instanceNum);
    MockContext context(device.get());
    MockCommandQueue cmdQ(&context, device.get(), nullptr);
    cmdQ.setProfilingEnabled();
    cmdQ.device = device.get();

    HwTimeStamps timestamp;
    timestamp.GlobalStartTS = 0;
    timestamp.ContextStartTS = 20;
    timestamp.GlobalEndTS = 80;
    timestamp.ContextEndTS = 56;
    timestamp.GlobalCompleteTS = 0;
    timestamp.ContextCompleteTS = 70;

    MockTagNode<HwTimeStamps> timestampNode;
    timestampNode.tagForCpuAccess = &timestamp;

    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);
    cl_event clEvent = &event;

    event.queueTimeStamp.CPUTimeinNS = 83;
    event.queueTimeStamp.GPUTimeStamp = 1;

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
        SetUp(defaultHwInfo.get());
    }

    void SetUp(const NEO::HardwareInfo *hardwareInfo) {
        PerformanceCountersFixture::SetUp();
        createPerfCounters();

        HardwareInfo hwInfo = *hardwareInfo;
        if (hwInfo.capabilityTable.defaultEngineType == aub_stream::EngineType::ENGINE_CCS) {
            hwInfo.featureTable.ftrCCSNode = true;
        }

        pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
        pClDevice = std::make_unique<ClDevice>(*pDevice, nullptr);

        pDevice->setPerfCounters(performanceCountersBase.release());

        context = std::make_unique<MockContext>(pClDevice.get());

        cl_int retVal = CL_SUCCESS;
        cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
        pCmdQ.reset(CommandQueue::create(context.get(), pClDevice.get(), properties, false, retVal));

        kernel = std::make_unique<MockKernelWithInternals>(*pClDevice);
    }

    void TearDown() override {
        PerformanceCountersFixture::TearDown();
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
        ProfilingWithPerfCountersTests::SetUp(&hwInfo);
    }

    void TearDown() override {
        ProfilingWithPerfCountersTests::TearDown();
    }
};

HWTEST_F(ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCounterAndForWorkloadWithNoKernelWhenGetCSFromCmdQueueThenEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;

    pCmdQ->setPerfCountersEnabled();

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM);

    MultiDispatchInfo multiDispatchInfo(nullptr);
    auto &commandStreamMigrateMemObjects = getCommandStream<FamilyType, CL_COMMAND_MIGRATE_MEM_OBJECTS>(*pCmdQ, CsrDependencies(),
                                                                                                        true, true, false, multiDispatchInfo,
                                                                                                        nullptr, 0);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MIGRATE_MEM_OBJECTS, true, true, *pCmdQ, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMigrateMemObjects.getAvailableSpace(), requiredSize);

    auto &commandStreamMarker = getCommandStream<FamilyType, CL_COMMAND_MARKER>(*pCmdQ, CsrDependencies(), true, true, false,
                                                                                multiDispatchInfo, nullptr, 0);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MARKER, true, true, *pCmdQ, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMarker.getAvailableSpace(), requiredSize);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_kernel clKernel = kernel->mockKernel;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(clKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    HardwareParse parse;
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
    auto itorBeforePC = reverse_find<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
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

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersNoUserRegistersWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_kernel clKernel = kernel->mockKernel;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(clKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    HardwareParse parse;
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
    auto itorBeforePC = reverse_find<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
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

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GivenCommandQueueBlockedWithProflingPerfCounterWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_event ue = new UserEvent();
    cl_kernel clKernel = kernel->mockKernel;
    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(clKernel, dimensions, globalOffsets, workItems, nullptr,
                                                                          1,   // one user event to block queue
                                                                          &ue, // user event not signaled
                                                                          &event);

    //rseCommands<FamilyType>(*pCmdQ);
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
    auto itorBeforePC = reverse_find<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
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
    typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_kernel clKernel = kernel->mockKernel;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(clKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, nullptr);

    HardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*pCmdQ);

    // expect no MI_REPORT_PERF_COUNT before WALKER
    auto itorBeforeReportPerf = find<MI_REPORT_PERF_COUNT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorBeforeReportPerf);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check PIPE_CONTROLs
    auto itorBeforePC = reverse_find<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforePC);
    auto pBeforePC = genCmdCast<PIPE_CONTROL *>(*itorBeforePC);
    ASSERT_NE(nullptr, pBeforePC);
    EXPECT_EQ(1u, pBeforePC->getCommandStreamerStallEnable());

    auto itorAfterPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterPC);
    auto pAfterPC = genCmdCast<PIPE_CONTROL *>(*itorAfterPC);
    ASSERT_NE(nullptr, pAfterPC);
    EXPECT_EQ(1u, pAfterPC->getCommandStreamerStallEnable());

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE, pBeforePC->getPostSyncOperation());

    // expect MI_REPORT_PERF_COUNT after WALKER
    auto itorAfterReportPerf = find<MI_REPORT_PERF_COUNT *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_EQ(cmdList.end(), itorAfterReportPerf);
}

template <typename TagType>
struct FixedGpuAddressTagAllocator : TagAllocator<TagType> {

    struct MockTagNode : TagNode<TagType> {
        void setGpuAddress(uint64_t value) { this->gpuAddress = value; }
    };

    FixedGpuAddressTagAllocator(CommandStreamReceiver &csr, uint64_t gpuAddress)
        : TagAllocator<TagType>(csr.getRootDeviceIndex(), csr.getMemoryManager(), csr.getPreferredTagPoolSize(), MemoryConstants::cacheLineSize,
                                sizeof(TagType), false, csr.getOsContext().getDeviceBitfield()) {
        auto tag = reinterpret_cast<MockTagNode *>(this->freeTags.peekHead());
        tag->setGpuAddress(gpuAddress);
    }
};

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenRegisterStoresArePresentInCS) {
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
    cl_kernel clKernel = kernel->mockKernel;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get())->enqueueKernel(clKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    auto pEvent = static_cast<MockEvent<Event> *>(event);
    EXPECT_EQ(pEvent->getHwTimeStampNode()->getGpuAddress(), timeStampGpuAddress);
    EXPECT_EQ(pEvent->getHwPerfCounterNode()->getGpuAddress(), perfCountersGpuAddress);
    HardwareParse parse;
    auto &cmdList = parse.cmdList;
    parse.parseCommands<FamilyType>(*pCmdQ);

    auto itor = expectStoreRegister<FamilyType>(cmdList, cmdList.begin(), timeStampGpuAddress + offsetof(HwTimeStamps, ContextStartTS), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    // after WALKER:

    itor = expectStoreRegister<FamilyType>(cmdList, itor, timeStampGpuAddress + offsetof(HwTimeStamps, ContextEndTS), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);

    EXPECT_TRUE(pEvent->calcProfilingData());

    clReleaseEvent(event);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersOnCCSTests, givenCommandQueueBlockedWithProfilingPerfCountersWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_REPORT_PERF_COUNT = typename FamilyType::MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_event userEvent = clCreateUserEvent(context.get(), nullptr);
    cl_kernel clKernel = kernel->mockKernel;
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get());

    cmdQHw->enqueueKernel(clKernel, dimensions, globalOffsets, workItems, nullptr, 1, &userEvent, &event);
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
    auto itorBeforePC = reverse_find<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
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

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersOnCCSTests, givenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenPipeControlWithTimeStampIsPresentInCS) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_REPORT_PERF_COUNT = typename FamilyType::MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled();

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;
    cl_kernel clKernel = kernel->mockKernel;
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(pCmdQ.get());

    cmdQHw->enqueueKernel(clKernel, dimensions, globalOffsets, workItems, nullptr, 0, nullptr, &event);

    HardwareParse parse;
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
    auto itorBeforePC = reverse_find<PIPE_CONTROL *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
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
            delete node->tagForCpuAccess;
            delete node;
        }
        timestampPacketNodes.clear();
    }
};

struct ProfilingTimestampPacketsTest : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.ReturnRawGpuTimestamps.set(true);
        cmdQ->setProfilingEnabled();
        ev->timestampPacketContainer = std::make_unique<MockTimestampContainer>();
    }

    void addTimestampNode(int contextStart, int contextEnd, int globalStart, int globalEnd) {
        auto node = new MockTagNode<TimestampPacketStorage>();
        auto timestampPacketStorage = new TimestampPacketStorage();
        node->tagForCpuAccess = timestampPacketStorage;

        timestampPacketStorage->packets[0].contextStart = contextStart;
        timestampPacketStorage->packets[0].contextEnd = contextEnd;
        timestampPacketStorage->packets[0].globalStart = globalStart;
        timestampPacketStorage->packets[0].globalEnd = globalEnd;

        ev->timestampPacketContainer->add(node);
    }

    void addTimestampNodeMultiOsContext(int globalStart[16], int globalEnd[16], int contextStart[16], int contextEnd[16], uint32_t size) {
        auto node = new MockTagNode<TimestampPacketStorage>();
        auto timestampPacketStorage = new TimestampPacketStorage();
        timestampPacketStorage->packetsUsed = size;

        for (uint32_t i = 0u; i < timestampPacketStorage->packetsUsed; ++i) {
            timestampPacketStorage->packets[i].globalStart = globalStart[i];
            timestampPacketStorage->packets[i].globalEnd = globalEnd[i];
            timestampPacketStorage->packets[i].contextStart = contextStart[i];
            timestampPacketStorage->packets[i].contextEnd = contextEnd[i];
        }

        node->tagForCpuAccess = timestampPacketStorage;
        ev->timestampPacketContainer->add(node);
    }

    void initTimestampNodeMultiOsContextData(int globalStart[16], int globalEnd[16], uint32_t size) {

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
    ReleaseableObjectPtr<MockCommandQueue> cmdQ = clUniquePtr(new MockCommandQueue(&context, context.getDevice(0), props));
    ReleaseableObjectPtr<MockEvent<MyEvent>> ev = clUniquePtr(new MockEvent<MyEvent>(cmdQ.get(), CL_COMMAND_USER, CompletionStamp::notReady, CompletionStamp::notReady));
};

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithOneElementAndTimestampNodeWhenCalculatingProfilingThenTimesAreTakenFromPacket) {
    addTimestampNode(10, 11, 12, 13);

    HwTimeStamps hwTimestamps;
    hwTimestamps.ContextStartTS = 100;
    hwTimestamps.ContextEndTS = 110;
    hwTimestamps.GlobalStartTS = 120;

    MockTagNode<HwTimeStamps> hwTimestampsNode;
    hwTimestampsNode.tagForCpuAccess = &hwTimestamps;
    ev->timeStampNode = &hwTimestampsNode;

    ev->calcProfilingData();

    EXPECT_EQ(12u, ev->getStartTimeStamp());
    EXPECT_EQ(13u, ev->getEndTimeStamp());
    EXPECT_EQ(12u, ev->getGlobalStartTimestamp());

    ev->timeStampNode = nullptr;
}

TEST_F(ProfilingTimestampPacketsTest, givenMultiOsContextCapableSetToTrueWhenCalcProfilingDataIsCalledThenCorrectedValuesAreReturned) {
    int globalStart[16] = {0};
    int globalEnd[16] = {0};
    int contextStart[16] = {0};
    int contextEnd[16] = {0};
    initTimestampNodeMultiOsContextData(globalStart, globalEnd, 16u);
    addTimestampNodeMultiOsContext(globalStart, globalEnd, contextStart, contextEnd, 16u);
    auto &device = reinterpret_cast<MockDevice &>(cmdQ->getDevice());
    auto &csr = device.getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();
    csr.multiOsContextCapable = true;

    ev->calcProfilingData();
    EXPECT_EQ(50u, ev->getStartTimeStamp());
    EXPECT_EQ(350u, ev->getEndTimeStamp());
}

TEST_F(ProfilingTimestampPacketsTest, givenTimestampPacketWithoutProfilingDataWhenCalculatingThenDontUseThatPacket) {
    int globalStart0 = 20;
    int globalEnd0 = 51;
    int contextStart0 = 21;
    int contextEnd0 = 50;

    int globalStart1 = globalStart0 - 1;
    int globalEnd1 = globalEnd0 + 1;
    int contextStart1 = contextStart0 - 1;
    int contextEnd1 = contextEnd0 + 1;

    addTimestampNodeMultiOsContext(&globalStart0, &globalEnd0, &contextStart0, &contextEnd0, 1);
    addTimestampNodeMultiOsContext(&globalStart1, &globalEnd1, &contextStart1, &contextEnd1, 1);
    auto &device = reinterpret_cast<MockDevice &>(cmdQ->getDevice());
    auto &csr = device.getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();
    csr.multiOsContextCapable = true;

    ev->timestampPacketContainer->peekNodes()[1]->setProfilingCapable(false);

    ev->calcProfilingData();
    EXPECT_EQ(static_cast<uint64_t>(globalStart0), ev->getStartTimeStamp());
    EXPECT_EQ(static_cast<uint64_t>(globalEnd0), ev->getEndTimeStamp());
}

TEST_F(ProfilingTimestampPacketsTest, givenPrintTimestampPacketContentsSetWhenCalcProfilingDataThenTimeStampsArePrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintTimestampPacketContents.set(true);
    testing::internal::CaptureStdout();

    auto &device = reinterpret_cast<MockDevice &>(cmdQ->getDevice());
    auto &csr = device.getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();
    csr.multiOsContextCapable = true;

    int globalStart[16] = {0};
    int globalEnd[16] = {0};
    int contextStart[16] = {0};
    int contextEnd[16] = {0};
    for (int i = 0; i < 16; i++) {
        globalStart[i] = 2 * i;
        globalEnd[i] = 500 * i;
        contextStart[i] = 7 * i;
        contextEnd[i] = 94 * i;
    }
    addTimestampNodeMultiOsContext(globalStart, globalEnd, contextStart, contextEnd, 16u);

    ev->calcProfilingData();

    std::string output = testing::internal::GetCapturedStdout();
    std::stringstream expected;

    expected << "Timestamp 0, profiling capable: " << ev->timestampPacketContainer->peekNodes()[0]->isProfilingCapable() << ", ";
    for (int i = 0; i < 16; i++) {
        expected << "packet " << i << ": "
                 << "global start: " << globalStart[i] << ", "
                 << "global end: " << globalEnd[i] << ", "
                 << "context start: " << contextStart[i] << ", "
                 << "context end: " << contextEnd[i] << std::endl;
    }
    EXPECT_EQ(0, output.compare(expected.str().c_str()));
}

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithThreeElementsWhenCalculatingProfilingThenTimesAreTakenFromProperPacket) {
    addTimestampNode(10, 11, 12, 13);
    addTimestampNode(1, 21, 22, 13);
    addTimestampNode(5, 31, 2, 13);

    ev->calcProfilingData();

    EXPECT_EQ(2u, ev->getStartTimeStamp());
    EXPECT_EQ(13u, ev->getEndTimeStamp());
    EXPECT_EQ(2u, ev->getGlobalStartTimestamp());
}

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithZeroElementsWhenCalculatingProfilingThenDataIsNotCalculated) {
    EXPECT_EQ(0u, ev->timestampPacketContainer->peekNodes().size());
    ev->calcProfilingData();

    EXPECT_FALSE(ev->getDataCalcStatus());
}
} // namespace NEO
