/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/enqueue_marker.h"
#include "runtime/command_queue/enqueue_migrate_mem_objects.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/utilities/tag_allocator.h"
#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/event/event_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/os_interface/mock_performance_counters.h"
#include "unit_tests/utilities/base_object_utils.h"

namespace NEO {

struct ProfilingTests : public CommandEnqueueFixture,
                        public ::testing::Test {
    void SetUp() override {
        CommandEnqueueFixture::SetUp(CL_QUEUE_PROFILING_ENABLE);

        program = ReleaseableObjectPtr<MockProgram>(new MockProgram(*pDevice->getExecutionEnvironment()));

        memset(&kernelHeader, 0, sizeof(kernelHeader));
        kernelHeader.KernelHeapSize = sizeof(kernelIsa);

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
        kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
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

    uint32_t kernelIsa[32];
    uint32_t crossThreadData[32];
};

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GIVENCommandQueueWithProfilingAndForWorkloadWithKernelWHENGetCSFromCmdQueueTHENEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(MI_STORE_REGISTER_MEM) + sizeof(GPGPU_WALKER) + HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);

    auto &commandStreamNDRangeKernel = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, true, false, &kernel);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, true, false, *pCmdQ, &kernel);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamNDRangeKernel.getAvailableSpace(), requiredSize);

    auto &commandStreamTask = getCommandStream<FamilyType, CL_COMMAND_TASK>(*pCmdQ, true, false, &kernel);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_TASK, true, false, *pCmdQ, &kernel);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamTask.getAvailableSpace(), requiredSize);
}

HWTEST_F(ProfilingTests, GIVENCommandQueueWithProfilingAndForWorkloadWithNoKernelWHENGetCSFromCmdQueueTHENEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM);

    auto &commandStreamMigrateMemObjects = getCommandStream<FamilyType, CL_COMMAND_MIGRATE_MEM_OBJECTS>(*pCmdQ, true, false, nullptr);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MIGRATE_MEM_OBJECTS, true, false, *pCmdQ, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMigrateMemObjects.getAvailableSpace(), requiredSize);

    auto &commandStreamMarker = getCommandStream<FamilyType, CL_COMMAND_MARKER>(*pCmdQ, true, false, nullptr);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MARKER, true, false, *pCmdQ, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMarker.getAvailableSpace(), requiredSize);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GIVENCommandQueueWithProfilingAndForWorkloadWithTwoKernelsInMdiWHENGetCSFromCmdQueueTHENEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM) + HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);
    requiredSize += 2 * sizeof(GPGPU_WALKER);

    DispatchInfo dispatchInfo;
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    multiDispatchInfo.push(dispatchInfo);
    auto &commandStreamTask = getCommandStream<FamilyType, CL_COMMAND_TASK>(*pCmdQ, true, false, &kernel);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_TASK, CsrDependencies(), true, false, *pCmdQ, multiDispatchInfo);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamTask.getAvailableSpace(), requiredSize);
}

/*
#   Two additional PIPE_CONTROLs are expected before first MI_STORE_REGISTER_MEM (which is before GPGPU_WALKER)
#   and after second MI_STORE_REGISTER_MEM (which is after GPGPU_WALKER).
*/
HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GIVENCommandQueueWithProfolingWHENWalkerIsDispatchedTHENPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
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

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GIVENCommandQueueWithProflingWHENWalkerIsDispatchedTHENMiStoreRegisterMemIsPresentInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
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
HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GIVENCommandQueueBlockedWithProfilingWHENWalkerIsDispatchedTHENPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
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

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingTests, GIVENCommandQueueBlockedWithProfilingWHENWalkerIsDispatchedTHENMiStoreRegisterMemIsPresentInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
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
    pCmdQ->finish(false);

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

TEST(EventProfilingTest, givenEventWhenCompleteIsZeroThenCalcProfilingDataSetsEndTimestampInCompleteTimestampAndDoesntCallOsTimeMethods) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
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

TEST(EventProfilingTest, givenRawTimestampsDebugModeWhenDataIsQueriedThenRawDataIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.ReturnRawGpuTimestamps.set(1);
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
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

struct ProfilingWithPerfCountersTests : public ProfilingTests,
                                        public PerformanceCountersFixture {
    void SetUp() override {
        PerformanceCountersFixture::SetUp();
        ProfilingTests::SetUp();
        createPerfCounters();
        performanceCountersBase->initialize(platformDevices[0]);
        pDevice->setPerfCounters(performanceCountersBase.release());
    }

    void TearDown() override {
        ProfilingTests::TearDown();
        PerformanceCountersFixture::TearDown();
    }

    template <typename GfxFamily>
    GenCmdList::iterator expectStoreRegister(GenCmdList::iterator itor, uint64_t memoryAddress, uint32_t registerAddress) {
        using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

        itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
        EXPECT_NE(cmdList.end(), itor);
        auto pStore = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(memoryAddress, pStore->getMemoryAddress());
        EXPECT_EQ(registerAddress, pStore->getRegisterAddress());
        itor++;
        return itor;
    }
};

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GIVENCommandQueueWithProfilingPerfCounterAndForWorkloadWithKernelWHENGetCSFromCmdQueueTHENEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled(true, 1);

    MockKernel kernel(program.get(), kernelInfo, *pDevice);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM) + sizeof(GPGPU_WALKER) + HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);
    //begin perf cmds
    requiredSize += 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(MI_STORE_REGISTER_MEM) + NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(MI_STORE_REGISTER_MEM) + sizeof(MI_REPORT_PERF_COUNT) + pCmdQ->getPerfCountersUserRegistersNumber() * sizeof(MI_STORE_REGISTER_MEM);
    //end perf cmds
    requiredSize += 2 * sizeof(PIPE_CONTROL) + 3 * sizeof(MI_STORE_REGISTER_MEM) + NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(MI_STORE_REGISTER_MEM) + sizeof(MI_REPORT_PERF_COUNT) + pCmdQ->getPerfCountersUserRegistersNumber() * sizeof(MI_STORE_REGISTER_MEM);

    auto &commandStreamNDRangeKernel = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, true, true, &kernel);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, true, true, *pCmdQ, &kernel);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamNDRangeKernel.getAvailableSpace(), requiredSize);

    auto &commandStreamTask = getCommandStream<FamilyType, CL_COMMAND_TASK>(*pCmdQ, true, true, &kernel);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_TASK, true, true, *pCmdQ, &kernel);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamTask.getAvailableSpace(), requiredSize);
    bool retVal = false;
    retVal = pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
    EXPECT_TRUE(retVal);
    retVal = pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
    EXPECT_TRUE(retVal);
}

HWTEST_F(ProfilingWithPerfCountersTests,
         GIVENCommandQueueWithProfilingPerfCounterAndForWorkloadWithNoKernelWHENGetCSFromCmdQueueTHENEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;

    pCmdQ->setPerfCountersEnabled(true, 1);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM);

    auto &commandStreamMigrateMemObjects = getCommandStream<FamilyType, CL_COMMAND_MIGRATE_MEM_OBJECTS>(*pCmdQ, true, true, nullptr);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MIGRATE_MEM_OBJECTS, true, true, *pCmdQ, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMigrateMemObjects.getAvailableSpace(), requiredSize);

    auto &commandStreamMarker = getCommandStream<FamilyType, CL_COMMAND_MARKER>(*pCmdQ, true, true, nullptr);
    expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_MARKER, true, true, *pCmdQ, nullptr);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamMarker.getAvailableSpace(), requiredSize);

    pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GIVENCommandQueueWithProfilingPerfCountersAndForWorkloadWithTwoKernelsInMdiWHENGetCSFromCmdQueueTHENEnoughSpaceInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);

    pCmdQ->setPerfCountersEnabled(true, 1);

    uint64_t requiredSize = 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(MI_STORE_REGISTER_MEM) + HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);
    requiredSize += 2 * sizeof(GPGPU_WALKER);

    //begin perf cmds
    requiredSize += 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(MI_STORE_REGISTER_MEM) + NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(MI_STORE_REGISTER_MEM) + sizeof(MI_REPORT_PERF_COUNT) + pCmdQ->getPerfCountersUserRegistersNumber() * sizeof(MI_STORE_REGISTER_MEM);
    //end perf cmds
    requiredSize += 2 * sizeof(PIPE_CONTROL) + 3 * sizeof(MI_STORE_REGISTER_MEM) + NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(MI_STORE_REGISTER_MEM) + sizeof(MI_REPORT_PERF_COUNT) + pCmdQ->getPerfCountersUserRegistersNumber() * sizeof(MI_STORE_REGISTER_MEM);

    DispatchInfo dispatchInfo;
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    multiDispatchInfo.push(dispatchInfo);
    auto &commandStreamTask = getCommandStream<FamilyType, CL_COMMAND_TASK>(*pCmdQ, true, true, &kernel);
    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_TASK, CsrDependencies(), true, true, *pCmdQ, multiDispatchInfo);
    EXPECT_GE(expectedSizeCS, requiredSize);
    EXPECT_GE(commandStreamTask.getAvailableSpace(), requiredSize);

    pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GIVENCommandQueueWithProfilingPerfCountersWHENWalkerIsDispatchedTHENPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled(true, 1);

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
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

    pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GIVENCommandQueueWithProfilingPerfCountersNoUserRegistersWHENWalkerIsDispatchedTHENPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled(true, 2);

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
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

    pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingWithPerfCountersTests, GIVENCommandQueueBlockedWithProflingPerfCounterWHENWalkerIsDispatchedTHENPipeControlWithTimeStampIsPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled(true, 1);

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
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
    pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
}

HWTEST_F(ProfilingWithPerfCountersTests,
         GIVENCommandQueueWithProfilingPerfCountersNoEventWHENWalkerIsDispatchedTHENPipeControlWithTimeStampIsNotPresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;
    typedef typename FamilyType::MI_REPORT_PERF_COUNT MI_REPORT_PERF_COUNT;

    pCmdQ->setPerfCountersEnabled(true, 1);

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_kernel clKernel = &kernel;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        clKernel,
        dimensions,
        globalOffsets,
        workItems,
        nullptr,
        0,
        nullptr,
        nullptr);

    parseCommands<FamilyType>(*pCmdQ);

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

    pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
}

template <typename TagType>
struct FixedGpuAddressTagAllocator : TagAllocator<TagType> {

    struct MockTagNode : TagNode<TagType> {
        void setGpuAddress(uint64_t value) { this->gpuAddress = value; }
    };

    FixedGpuAddressTagAllocator(CommandStreamReceiver &csr, uint64_t gpuAddress)
        : TagAllocator<TagType>(csr.getMemoryManager(), csr.getPreferredTagPoolSize(), MemoryConstants::cacheLineSize) {
        auto tag = reinterpret_cast<MockTagNode *>(this->freeTags.peekHead());
        tag->setGpuAddress(gpuAddress);
    }
};

HWTEST_F(ProfilingWithPerfCountersTests, GIVENCommandQueueWithProfilingPerfCountersWHENWalkerIsDispatchedTHENRegisterStoresArePresentInCS) {
    uint64_t timeStampGpuAddress = 0x123456000;
    uint64_t perfCountersGpuAddress = 0xabcdef000;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.profilingTimeStampAllocator.reset(new FixedGpuAddressTagAllocator<HwTimeStamps>(csr, timeStampGpuAddress));
    csr.perfCounterAllocator.reset(new FixedGpuAddressTagAllocator<HwPerfCounter>(csr, perfCountersGpuAddress));

    pCmdQ->setPerfCountersEnabled(true, 1);

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
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

    auto pEvent = static_cast<MockEvent<Event> *>(event);
    EXPECT_EQ(pEvent->getHwTimeStampNode()->getGpuAddress(), timeStampGpuAddress);
    EXPECT_EQ(pEvent->getHwPerfCounterNode()->getGpuAddress(), perfCountersGpuAddress);
    parseCommands<FamilyType>(*pCmdQ);

    auto itor = expectStoreRegister<FamilyType>(cmdList.begin(), timeStampGpuAddress + offsetof(HwTimeStamps, ContextStartTS), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.DMAFenceIdBegin), INSTR_MMIO_NOOPID);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.CoreFreqBegin), INSTR_MMIO_RPSTAT1);
    for (auto i = 0u; i < NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT; i++) {
        itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportBegin.Gp) + i * sizeof(cl_uint),
                                               INSTR_GFX_OFFSETS::INSTR_PERF_CNT_1_DW0 + i * sizeof(cl_uint));
    }
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportBegin.User), 0);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportBegin.User) + 8, 0);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportBegin.User) + 12, 4);

    // after WALKER:

    itor = expectStoreRegister<FamilyType>(itor, timeStampGpuAddress + offsetof(HwTimeStamps, ContextEndTS), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.OaStatus), INSTR_GFX_OFFSETS::INSTR_OA_STATUS);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.OaHead), INSTR_GFX_OFFSETS::INSTR_OA_HEAD_PTR);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.OaTail), INSTR_GFX_OFFSETS::INSTR_OA_TAIL_PTR);
    for (auto i = 0u; i < NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT; i++) {
        itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportEnd.Gp) + i * sizeof(cl_uint),
                                               INSTR_GFX_OFFSETS::INSTR_PERF_CNT_1_DW0 + i * sizeof(cl_uint));
    }
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.DMAFenceIdEnd), INSTR_MMIO_NOOPID);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.CoreFreqEnd), INSTR_MMIO_RPSTAT1);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportEnd.User), 0);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportEnd.User) + 8, 0);
    itor = expectStoreRegister<FamilyType>(itor, perfCountersGpuAddress + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportEnd.User) + 12, 4);

    EXPECT_TRUE(pEvent->calcProfilingData());

    clReleaseEvent(event);

    pCmdQ->setPerfCountersEnabled(false, UINT32_MAX);
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

    void addTimestampNode(int contextStart, int contextEnd, int globalStart) {
        auto node = new MockTagNode<TimestampPacketStorage>();
        auto timestampPacketStorage = new TimestampPacketStorage();
        node->tagForCpuAccess = timestampPacketStorage;

        timestampPacketStorage->packets[0].contextStart = contextStart;
        timestampPacketStorage->packets[0].contextEnd = contextEnd;
        timestampPacketStorage->packets[0].globalStart = globalStart;

        ev->timestampPacketContainer->add(node);
    }

    DebugManagerStateRestore restorer;
    MockContext context;
    cl_command_queue_properties props[5] = {0, 0, 0, 0, 0};
    ReleaseableObjectPtr<MockCommandQueue> cmdQ = clUniquePtr(new MockCommandQueue(&context, context.getDevice(0), props));
    ReleaseableObjectPtr<MockEvent<MyEvent>> ev = clUniquePtr(new MockEvent<MyEvent>(cmdQ.get(), CL_COMMAND_USER, Event::eventNotReady, Event::eventNotReady));
};

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithOneElementAndTimestampNodeWhenCalculatingProfilingThenTimesAreTakenFromPacket) {
    addTimestampNode(10, 11, 12);

    HwTimeStamps hwTimestamps;
    hwTimestamps.ContextStartTS = 100;
    hwTimestamps.ContextEndTS = 110;
    hwTimestamps.GlobalStartTS = 120;
    MockTagNode<HwTimeStamps> hwTimestampsNode;
    hwTimestampsNode.tagForCpuAccess = &hwTimestamps;
    ev->timeStampNode = &hwTimestampsNode;

    ev->calcProfilingData();

    EXPECT_EQ(10u, ev->getStartTimeStamp());
    EXPECT_EQ(11u, ev->getEndTimeStamp());
    EXPECT_EQ(12u, ev->getGlobalStartTimestamp());

    ev->timeStampNode = nullptr;
}

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithThreeElementsWhenCalculatingProfilingThenTimesAreTakenFromProperPacket) {
    addTimestampNode(10, 11, 12);
    addTimestampNode(1, 21, 22);
    addTimestampNode(5, 31, 2);

    ev->calcProfilingData();

    EXPECT_EQ(1u, ev->getStartTimeStamp());
    EXPECT_EQ(31u, ev->getEndTimeStamp());
    EXPECT_EQ(2u, ev->getGlobalStartTimestamp());
}

TEST_F(ProfilingTimestampPacketsTest, givenTimestampsPacketContainerWithZeroElementsWhenCalculatingProfilingThenDataIsNotCalculated) {
    EXPECT_EQ(0u, ev->timestampPacketContainer->peekNodes().size());
    ev->calcProfilingData();

    EXPECT_FALSE(ev->getDataCalcStatus());
}
} // namespace NEO
