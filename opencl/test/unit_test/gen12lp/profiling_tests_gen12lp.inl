/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/hw_timestamps.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct ProfilingTestsGen12LP : public CommandEnqueueFixture,
                               public ::testing::Test {
    void SetUp() override {
        CommandEnqueueFixture::setUp(CL_QUEUE_PROFILING_ENABLE);
        mockKernelWithInternals = std::make_unique<MockKernelWithInternals>(*pClDevice, nullptr);
    }

    void TearDown() override {
        mockKernelWithInternals.reset();
        CommandEnqueueFixture::tearDown();
    }

    std::unique_ptr<MockKernelWithInternals> mockKernelWithInternals;
};

GEN12LPTEST_F(ProfilingTestsGen12LP, GivenCommandQueueWithProflingWhenWalkerIsDispatchedThenTwoPIPECONTROLSWithOPERATION_WRITE_TIMESTAMPArePresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        *mockKernelWithInternals,
        dimensions,
        globalOffsets,
        workItems,
        nullptr,
        0,
        nullptr,
        &event);

    parseCommands<FamilyType>(*pCmdQ);

    uint32_t writeCounter = 0u;
    // Find GPGPU_WALKER
    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    // auto itorPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());

    while (itorPC != cmdList.end()) {
        auto pPipeControl = genCmdCast<PIPE_CONTROL *>(*itorPC);
        ASSERT_NE(nullptr, pPipeControl);
        if (PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP == pPipeControl->getPostSyncOperation()) {
            ++writeCounter;
        }
        ++itorPC;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
    }

    EXPECT_EQ(writeCounter, 2u);

    clReleaseEvent(event);
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

class MyDeviceTime : public DeviceTime {
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

class MyOSTime : public OSTime {
  public:
    static int instanceNum;
    MyOSTime() {
        instanceNum++;
        this->deviceTime.reset(new MyDeviceTime());
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

GEN12LPTEST_F(ProfilingTestsGen12LP, givenRawTimestampsDebugModeWhenDataIsQueriedThenRawDataIsReturnedGen12Lp) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.ReturnRawGpuTimestamps.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MyOSTime::instanceNum = 0;
    device->setOSTime(new MyOSTime());
    EXPECT_EQ(1, MyOSTime::instanceNum);
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

    event.queueTimeStamp.cpuTimeInNs = 1;
    event.queueTimeStamp.gpuTimeStamp = 2;

    event.submitTimeStamp.cpuTimeInNs = 3;
    event.submitTimeStamp.gpuTimeStamp = 4;

    event.setCPUProfilingPath(false);
    event.timeStampNode = &timestampNode;
    event.calcProfilingData();

    cl_ulong queued, submitted, start, end, complete;

    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &submitted, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, nullptr);
    clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_COMPLETE, sizeof(cl_ulong), &complete, nullptr);

    EXPECT_EQ(timestamp.globalEndTS, complete);
    EXPECT_EQ(timestamp.globalEndTS, end);
    EXPECT_EQ(timestamp.globalStartTS, start);
    EXPECT_EQ(event.submitTimeStamp.gpuTimeStamp, submitted);
    EXPECT_EQ(event.queueTimeStamp.gpuTimeStamp, queued);
    event.timeStampNode = nullptr;
}
