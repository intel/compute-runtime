/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/extensions/public/cl_ext_private.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/fixtures/device_instrumentation_fixture.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"
#include "test.h"

using namespace NEO;

template <typename T>
class EventFixture : public ApiFixture<>, public T {
  public:
    void SetUp() override {
        ApiFixture::SetUp();
    }

    void TearDown() override {
        ApiFixture::TearDown();
    }
};

typedef EventFixture<::testing::Test> clEventProfilingTests;

cl_int ProfilingInfo[] = {
    CL_PROFILING_COMMAND_QUEUED,
    CL_PROFILING_COMMAND_SUBMIT,
    CL_PROFILING_COMMAND_START,
    CL_PROFILING_COMMAND_END,
    CL_PROFILING_COMMAND_COMPLETE};

TEST_F(clEventProfilingTests, GivenInvalidParamNameWhenGettingEventProfilingInfoThenInvalidValueErrorIsReturned) {
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

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenInvalidParamValueSizeWhenGettingEventProfilingInfoThenInvalidValueErrorIsReturned) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t param_value_size = sizeof(cl_ulong);
    cl_ulong param_value;
    size_t param_value_size_ret;
    cl_int retVal = CL_PROFILING_INFO_NOT_AVAILABLE;

    cl_event event = (cl_event)pEvent;
    pEvent->setProfilingEnabled(true);
    retVal = clGetEventProfilingInfo(event,
                                     ProfilingInfo[0],
                                     param_value_size - 1,
                                     &param_value,
                                     &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenValidParametersWhenGettingEventProfilingInfoThenSuccessIsReturned) {
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

TEST_F(clEventProfilingTests, GivenNullParamValueSizeRetWhenGettingEventProfilingInfoThenSuccessIsReturned) {
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

TEST_F(clEventProfilingTests, GivenNullEventWhenGettingEventProfilingInfoThenInvalidEventErrorIsReturned) {

    auto retVal = clGetEventProfilingInfo(nullptr, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), 0u, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST(clGetEventProfilingInfo, GivenNullParamValueAndZeroParamValueSizeWhenGettingEventProfilingInfoThenSuccessIsReturned) {
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

TEST(clGetEventProfilingInfo, GivenNullParamValueAndCorrectParamValueSizeWhenGettingEventProfilingInfoThenSuccessIsReturned) {
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

TEST(clGetEventProfilingInfo, GivenUserEventWhenGettingEventProfilingInfoThenProfilingInfoNotAvailableErrorIsReturned) {
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

TEST(clGetEventProfilingInfo, GivenStartAndEndTimeWhenGettingDeltaThenCorrectDeltaIsReturned) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    cl_ulong startTime = 1;
    cl_ulong endTime = 2;
    cl_ulong delta = 0;

    delta = pEvent->getDelta(startTime, endTime);
    EXPECT_EQ(endTime - startTime, delta);

    delete pEvent;
}

TEST(clGetEventProfilingInfo, GivenStartTimeGreaterThenEndTimeWhenGettingDeltaThenCorrectDeltaIsReturned) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    cl_ulong startTime = 2;
    cl_ulong endTime = 1;
    cl_ulong delta = 0;
    cl_ulong timeMax = 0xffffffffULL;

    delta = pEvent->getDelta(startTime, endTime);
    EXPECT_EQ((timeMax + (endTime - startTime)), delta);
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

TEST(clGetEventProfilingInfo, GivenProfilingDisabledWhenCalculatingProfilingDataThenFalseIsReturned) {
    auto *pEvent = new MockEvent<Event>(nullptr, 0, 0, 0);
    EXPECT_FALSE(pEvent->calcProfilingData());
    delete pEvent;
}

TEST(clGetEventProfilingInfo, GivenProfilingEnabledWhenCalculatingProfilingDataThenFalseIsNotReturned) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    cl_bool Result = pEvent->isProfilingEnabled();
    EXPECT_EQ(((cl_bool)CL_FALSE), Result);
    pEvent->setProfilingEnabled(true);
    Result = pEvent->isProfilingEnabled();
    EXPECT_NE(((cl_bool)CL_FALSE), Result);
    delete pEvent;
}

TEST(clGetEventProfilingInfo, GivenProfilingEnabledAndUserEventsWhenCalculatingProfilingDataThenFalseIsReturned) {
    Event *pEvent = new UserEvent();
    cl_bool Result = pEvent->isProfilingEnabled();
    EXPECT_EQ(((cl_bool)CL_FALSE), Result);
    delete pEvent;
}

TEST(clGetEventProfilingInfo, GivenPerfCountersEnabledWhenCheckingPerfCountersThenTrueIsReturned) {
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

        cl_device_id deviceId = device.get();
        cl_int retVal = CL_SUCCESS;
        context = std::unique_ptr<Context>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1),
                                                                        nullptr, nullptr, retVal));
        commandQueue = std::make_unique<CommandQueue>(context.get(), device.get(), nullptr);
        event = std::make_unique<Event>(commandQueue.get(), 0, 0, 0);
        event->setStatus(CL_COMPLETE);
        commandQueue->getPerfCounters()->getApiReport(0, nullptr, &param_value_size, true);
        event->setProfilingEnabled(true);

        eventCl = static_cast<cl_event>(event.get());
    }

    void TearDown() override {
        PerformanceCountersDeviceFixture::TearDown();
    }

    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
    std::unique_ptr<Event> event;
    size_t param_value_size = 0;
    cl_event eventCl = nullptr;
    cl_ulong param_value = 0;
    size_t param_value_size_ret = 0;
};

TEST_F(clEventProfilingWithPerfCountersTests, GivenDisabledPerfCountersWhenGettingEventProfilingInfoThenInvalidValueErrorIsReturned) {
    event->setPerfCountersEnabled(false);
    cl_int retVal = clGetEventProfilingInfo(eventCl,
                                            CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                            param_value_size,
                                            &param_value,
                                            &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEventProfilingWithPerfCountersTests, GivenEnabledPerfCountersWhenGettingEventProfilingInfoThenSuccessIsReturned) {
    event->setPerfCountersEnabled(true);
    cl_int retVal = clGetEventProfilingInfo(eventCl,
                                            CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                            param_value_size,
                                            &param_value,
                                            &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEventProfilingWithPerfCountersTests, GivenEnabledPerfCountersAndIncorrectParamValueSizeWhenGettingEventProfilingInfoThenProfilingInfoNotAvailableErrorIsReturned) {
    event->setPerfCountersEnabled(true);
    cl_int retVal = clGetEventProfilingInfo(eventCl,
                                            CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                            param_value_size - 1,
                                            &param_value,
                                            &param_value_size_ret);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, retVal);
}
