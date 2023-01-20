/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueTaskTests;

struct EnqueueTaskWithRequiredWorkGroupSize : public HelloWorldTest<HelloWorldFixtureFactory> {
    typedef HelloWorldTest<HelloWorldFixtureFactory> Parent;

    void SetUp() override {
        Parent::kernelFilename = "required_work_group";
        Parent::kernelName = "CopyBuffer2";
        Parent::SetUp();
    }

    void TearDown() override {
        Parent::TearDown();
    }
};

namespace ULT {

TEST_F(clEnqueueTaskTests, GivenValidParametersWhenEnqueingTaskThenSuccessIsReturned) {
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    retVal = clEnqueueTask(
        pCommandQueue,
        pMultiDeviceKernel,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueTaskWithRequiredWorkGroupSize, GivenRequiredWorkGroupSizeWhenEnqueingTaskThenSuccessIsReturned) {
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    cl_command_queue commandQueue = static_cast<cl_command_queue>(pCmdQ);
    cl_kernel kernel = static_cast<cl_kernel>(pMultiDeviceKernel);

    retVal = clEnqueueTask(
        commandQueue,
        kernel,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
