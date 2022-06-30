/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct ProfilingTestsGen12LP : public CommandEnqueueFixture,
                               public ::testing::Test {
    void SetUp() override {
        CommandEnqueueFixture::SetUp(CL_QUEUE_PROFILING_ENABLE);
        mockKernelWithInternals = std::make_unique<MockKernelWithInternals>(*pClDevice, nullptr);
    }

    void TearDown() override {
        mockKernelWithInternals.reset();
        CommandEnqueueFixture::TearDown();
    }

    std::unique_ptr<MockKernelWithInternals> mockKernelWithInternals;
};

GEN12LPTEST_F(ProfilingTestsGen12LP, GivenCommandQueueWithProflingWhenWalkerIsDispatchedThenTwoPIPECONTROLSWithOPERATION_WRITE_TIMESTAMPArePresentInCS) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

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

    //auto itorPC = find<PIPE_CONTROL *>(itorGPGPUWalkerCmd, cmdList.end());

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
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        EXPECT_FALSE(true);
        return 1.0;
    }
    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const override {
        EXPECT_FALSE(true);
        return 0;
    }
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *) override {
        EXPECT_FALSE(true);
        return false;
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
    DebugManager.flags.ReturnRawGpuTimestamps.set(1);
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

    EXPECT_EQ(timestamp.GlobalEndTS, complete);
    EXPECT_EQ(timestamp.GlobalEndTS, end);
    EXPECT_EQ(timestamp.GlobalStartTS, start);
    EXPECT_EQ(event.submitTimeStamp.GPUTimeStamp, submited);
    EXPECT_EQ(event.queueTimeStamp.GPUTimeStamp, queued);
    event.timeStampNode = nullptr;
}
