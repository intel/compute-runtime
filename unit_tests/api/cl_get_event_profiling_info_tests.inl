/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_ext_private.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "runtime/event/user_event.h"
#include "unit_tests/api/cl_api_tests.h"
#include "unit_tests/fixtures/device_instrumentation_fixture.h"
#include "unit_tests/os_interface/mock_performance_counters.h"
#include "test.h"

using namespace OCLRT;

template <typename T>
class EventFixture : public api_fixture, public T {
  public:
    void SetUp() override {
        api_fixture::SetUp();
    }

    void TearDown() override {
        api_fixture::TearDown();
    }
};

typedef EventFixture<::testing::Test> clEventProfilingTests;

cl_int ProfilingInfo[] = {
    CL_PROFILING_COMMAND_QUEUED,
    CL_PROFILING_COMMAND_SUBMIT,
    CL_PROFILING_COMMAND_START,
    CL_PROFILING_COMMAND_END,
    CL_PROFILING_COMMAND_COMPLETE};

TEST_F(clEventProfilingTests, clGetEventProfilingInfo_InvlidParams_CL_INVALID_VALUE) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t param_value_size = sizeof(cl_ulong);
    cl_ulong param_value;
    size_t param_value_size_ret;
    cl_int retVal = CL_PROFILING_INFO_NOT_AVAILABLE;

    cl_event event = (cl_event)pEvent;
    pEvent->setProfilingEnabled(true);
    retVal = clGetEventProfilingInfo(event,
                                     0,
                                     param_value_size,
                                     &param_value,
                                     &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clGetEventProfilingInfo(event,
                                     ProfilingInfo[0],
                                     param_value_size - 1,
                                     &param_value,
                                     &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete pEvent;
}

TEST_F(clEventProfilingTests, clGetEventProfilingInfo_ValidParams_CL_SUCCESS) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t param_value_size = sizeof(cl_ulong);
    cl_ulong param_value;
    size_t param_value_size_ret;

    cl_event event = (cl_event)pEvent;
    pEvent->setProfilingEnabled(true);
    for (auto infoId : ::ProfilingInfo) {
        cl_int retVal = clGetEventProfilingInfo(event,
                                                infoId,
                                                param_value_size,
                                                &param_value,
                                                &param_value_size_ret);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    delete pEvent;
}

TEST_F(clEventProfilingTests, clGetEventProfilingInfo_ValidParams_CL_SUCCESS_no_size_ret) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t param_value_size = sizeof(cl_ulong);
    cl_ulong param_value;

    cl_event event = (cl_event)pEvent;
    pEvent->setProfilingEnabled(true);

    cl_int retVal = clGetEventProfilingInfo(event,
                                            ProfilingInfo[0],
                                            param_value_size,
                                            &param_value,
                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pEvent;
}

TEST_F(clEventProfilingTests, clGetEventProfilingInfo_InvalidEvent) {

    auto retVal = clGetEventProfilingInfo(nullptr, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), 0u, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST(clGetEventProfilingInfo, whenSizeIsTooSmallAndParamValueIsNotNullThenInvalidValueIsReturned) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    size_t param_value_size = 0;
    cl_ulong param_value;

    pEvent->setStatus(CL_COMPLETE);
    pEvent->setProfilingEnabled(true);

    cl_event event = (cl_event)pEvent;
    cl_int retVal = clGetEventProfilingInfo(event,
                                            CL_PROFILING_COMMAND_QUEUED,
                                            param_value_size,
                                            &param_value,
                                            nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete pEvent;
}

TEST(clGetEventProfilingInfo, whenSizeIsTooSmallAndParamValueIsNullThenSuccessIsReturned) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    size_t param_value_size = 0;

    pEvent->setStatus(CL_COMPLETE);
    pEvent->setProfilingEnabled(true);

    cl_event event = (cl_event)pEvent;
    cl_int retVal = clGetEventProfilingInfo(event,
                                            CL_PROFILING_COMMAND_QUEUED,
                                            param_value_size,
                                            nullptr,
                                            nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pEvent;
}

TEST(clGetEventProfilingInfo, whenSizeIsCorrectAndParamValueIsNullThenSuccessIsReturned) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    size_t param_value_size = sizeof(cl_ulong);

    pEvent->setStatus(CL_COMPLETE);
    pEvent->setProfilingEnabled(true);

    cl_event event = (cl_event)pEvent;
    cl_int retVal = clGetEventProfilingInfo(event,
                                            CL_PROFILING_COMMAND_QUEUED,
                                            param_value_size,
                                            nullptr,
                                            nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pEvent;
}

TEST(clGetEventProfilingInfo, UserEventOnQueryReturnsNotAvailable) {
    UserEvent *ue = new UserEvent();
    size_t param_value_size = sizeof(cl_ulong);
    cl_ulong param_value;
    size_t param_value_size_ret;

    cl_event event = (cl_event)ue;
    for (auto infoId : ::ProfilingInfo) {
        cl_int retVal = clGetEventProfilingInfo(event,
                                                infoId,
                                                param_value_size,
                                                &param_value,
                                                &param_value_size_ret);
        EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, retVal);
    }
    delete ue;
}

TEST(clGetEventProfilingInfo, GetDeltaBeetwenTimes) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    cl_ulong startTime = 1;
    cl_ulong endTime = 2;
    cl_ulong Delta = 0;
    Delta = pEvent->getDelta(startTime, endTime);
    EXPECT_EQ(endTime - startTime, Delta);

    startTime = 2;
    endTime = 1;
    cl_ulong TimeMax = 0xffffffffULL;
    Delta = pEvent->getDelta(startTime, endTime);
    EXPECT_EQ((TimeMax + (endTime - startTime)), Delta);
    delete pEvent;
}

TEST(clGetEventProfilingInfo, givenTimestampThatOverlapWhenGetDeltaIsCalledThenProperDeltaIsComputed) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    cl_ulong TimeMax = 0xffffffffULL;
    cl_ulong realDelta = 10;

    cl_ulong startTime = TimeMax - realDelta;
    cl_ulong endTime = 2;
    cl_ulong Delta = 0;
    Delta = pEvent->getDelta(startTime, endTime);
    EXPECT_EQ(realDelta + endTime, Delta);
    delete pEvent;
}

TEST(clGetEventProfilingInfo, WHENCalcProfilingDataTHENFalse) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    EXPECT_FALSE(pEvent->calcProfilingData());
    delete pEvent;
}

TEST(clGetEventProfilingInfo, IsProfilingEnabledRegularEventTrue) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    cl_bool Result = pEvent->isProfilingEnabled();
    EXPECT_EQ(((cl_bool)CL_FALSE), Result);
    pEvent->setProfilingEnabled(true);
    Result = pEvent->isProfilingEnabled();
    EXPECT_NE(((cl_bool)CL_FALSE), Result);
    delete pEvent;
}

TEST(clGetEventProfilingInfo, IsProfilingEnabledUserEventFalse) {
    Event *pEvent = new UserEvent();
    cl_bool Result = pEvent->isProfilingEnabled();
    EXPECT_EQ(((cl_bool)CL_FALSE), Result);
    delete pEvent;
}

TEST(clGetEventProfilingInfo, IsPerfCountersEnabledRegularEvent) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    bool Result = pEvent->isPerfCountersEnabled();
    EXPECT_FALSE(Result);
    pEvent->setPerfCountersEnabled(true);
    Result = pEvent->isPerfCountersEnabled();
    EXPECT_TRUE(Result);
    delete pEvent;
}

class clEventProfilingWithPerfCountersTests : public DeviceInstrumentationFixture,
                                              public PerformanceCountersDeviceFixture,
                                              public ::testing::Test {
  public:
    void SetUp() override {
        PerformanceCountersDeviceFixture::SetUp();
        DeviceInstrumentationFixture::SetUp(true);

        cl_device_id clDevice = device.get();
        cl_int retVal = CL_SUCCESS;
        context = std::unique_ptr<Context>(Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1),
                                                                        nullptr, nullptr, retVal));
    }

    void TearDown() override {
        PerformanceCountersDeviceFixture::TearDown();
    }

    std::unique_ptr<Context> context;
};

TEST_F(clEventProfilingWithPerfCountersTests, clGetEventProfilingInfoGetPerfCounters) {
    CommandQueue *pCommandQueue;
    pCommandQueue = new CommandQueue(context.get(), device.get(), 0);
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t param_value_size;

    //query size
    pCommandQueue->getPerfCounters()->processEventReport(0, nullptr, &param_value_size, nullptr, nullptr, true);

    pEvent->setProfilingEnabled(true);
    bool Result = pEvent->isPerfCountersEnabled();
    EXPECT_FALSE(Result);

    cl_event event = (cl_event)pEvent;
    cl_ulong param_value = 0;
    size_t param_value_size_ret;
    cl_int retVal = CL_PROFILING_INFO_NOT_AVAILABLE;

    retVal = clGetEventProfilingInfo(event,
                                     CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                     param_value_size,
                                     &param_value,
                                     &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    pEvent->setPerfCountersEnabled(true);
    retVal = clGetEventProfilingInfo(event,
                                     CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                     param_value_size,
                                     &param_value,
                                     &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clGetEventProfilingInfo(event,
                                     CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                     param_value_size - 1,
                                     &param_value,
                                     &param_value_size_ret);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, retVal);

    delete pEvent;
    delete pCommandQueue;
}
