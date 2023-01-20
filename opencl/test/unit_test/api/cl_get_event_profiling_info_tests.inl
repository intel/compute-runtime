/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/fixtures/device_instrumentation_fixture.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

using namespace NEO;

template <typename T>
class EventFixture : public ApiFixture<>, public T {
  public:
    void SetUp() override {
        ApiFixture::setUp();
    }

    void TearDown() override {
        ApiFixture::tearDown();
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
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t paramValueSize = sizeof(cl_ulong);
    cl_ulong paramValue;
    size_t paramValueSizeRet;
    cl_int retVal = CL_PROFILING_INFO_NOT_AVAILABLE;

    cl_event event = (cl_event)pEvent;
    pEvent->setProfilingEnabled(true);
    retVal = clGetEventProfilingInfo(event,
                                     0,
                                     paramValueSize,
                                     &paramValue,
                                     &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenInvalidParametersWhenGettingEventProfilingInfoThenValueSizeRetIsNotUpdated) {
    Event event{pCommandQueue, 0, 0, 0};
    event.setStatus(CL_COMPLETE);
    size_t paramValueSize = sizeof(cl_ulong);
    cl_ulong paramValue;
    size_t paramValueSizeRet = 0x1234;
    cl_int retVal = CL_PROFILING_INFO_NOT_AVAILABLE;

    event.setProfilingEnabled(true);
    retVal = clGetEventProfilingInfo(&event,
                                     0,
                                     paramValueSize,
                                     &paramValue,
                                     &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, paramValueSizeRet);
}

TEST_F(clEventProfilingTests, GivenInvalidParamValueSizeWhenGettingEventProfilingInfoThenInvalidValueErrorIsReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t paramValueSize = sizeof(cl_ulong);
    cl_ulong paramValue;
    size_t paramValueSizeRet;
    cl_int retVal = CL_PROFILING_INFO_NOT_AVAILABLE;

    cl_event event = (cl_event)pEvent;
    pEvent->setProfilingEnabled(true);
    retVal = clGetEventProfilingInfo(event,
                                     ProfilingInfo[0],
                                     paramValueSize - 1,
                                     &paramValue,
                                     &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenValidParametersWhenGettingEventProfilingInfoThenSuccessIsReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t paramValueSize = sizeof(cl_ulong);
    cl_ulong paramValue;
    size_t paramValueSizeRet;

    cl_event event = (cl_event)pEvent;
    pEvent->setProfilingEnabled(true);
    for (auto infoId : ::ProfilingInfo) {
        cl_int retVal = clGetEventProfilingInfo(event,
                                                infoId,
                                                paramValueSize,
                                                &paramValue,
                                                &paramValueSizeRet);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenNullParamValueSizeRetWhenGettingEventProfilingInfoThenSuccessIsReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    pEvent->setStatus(CL_COMPLETE);
    size_t paramValueSize = sizeof(cl_ulong);
    cl_ulong paramValue;

    cl_event event = (cl_event)pEvent;
    pEvent->setProfilingEnabled(true);

    cl_int retVal = clGetEventProfilingInfo(event,
                                            ProfilingInfo[0],
                                            paramValueSize,
                                            &paramValue,
                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenNullEventWhenGettingEventProfilingInfoThenInvalidEventErrorIsReturned) {

    auto retVal = clGetEventProfilingInfo(nullptr, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), 0u, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST_F(clEventProfilingTests, GivenNullParamValueAndZeroParamValueSizeWhenGettingEventProfilingInfoThenSuccessIsReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    size_t paramValueSize = 0;

    pEvent->setStatus(CL_COMPLETE);
    pEvent->setProfilingEnabled(true);

    cl_event event = (cl_event)pEvent;
    cl_int retVal = clGetEventProfilingInfo(event,
                                            CL_PROFILING_COMMAND_QUEUED,
                                            paramValueSize,
                                            nullptr,
                                            nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenNullParamValueAndCorrectParamValueSizeWhenGettingEventProfilingInfoThenSuccessIsReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    size_t paramValueSize = sizeof(cl_ulong);

    pEvent->setStatus(CL_COMPLETE);
    pEvent->setProfilingEnabled(true);

    cl_event event = (cl_event)pEvent;
    cl_int retVal = clGetEventProfilingInfo(event,
                                            CL_PROFILING_COMMAND_QUEUED,
                                            paramValueSize,
                                            nullptr,
                                            nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenUserEventWhenGettingEventProfilingInfoThenProfilingInfoNotAvailableErrorIsReturned) {
    UserEvent *ue = new UserEvent();
    size_t paramValueSize = sizeof(cl_ulong);
    cl_ulong paramValue;
    size_t paramValueSizeRet;

    cl_event event = (cl_event)ue;
    for (auto infoId : ::ProfilingInfo) {
        cl_int retVal = clGetEventProfilingInfo(event,
                                                infoId,
                                                paramValueSize,
                                                &paramValue,
                                                &paramValueSizeRet);
        EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, retVal);
    }
    delete ue;
}

TEST_F(clEventProfilingTests, GivenStartAndEndTimeWhenGettingDeltaThenCorrectDeltaIsReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    cl_ulong startTime = 1;
    cl_ulong endTime = 2;
    cl_ulong delta = 0;

    delta = pEvent->getDelta(startTime, endTime);
    EXPECT_EQ(endTime - startTime, delta);

    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenStartTimeGreaterThenEndTimeWhenGettingDeltaThenCorrectDeltaIsReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    cl_ulong startTime = 2;
    cl_ulong endTime = 1;
    cl_ulong delta = 0;
    cl_ulong timeMax = maxNBitValue(pDevice->getHardwareInfo().capabilityTable.kernelTimestampValidBits);

    delta = pEvent->getDelta(startTime, endTime);
    EXPECT_EQ((timeMax + (endTime - startTime)), delta);
    delete pEvent;
}

TEST_F(clEventProfilingTests, givenTimestampThatOverlapWhenGetDeltaIsCalledThenProperDeltaIsComputed) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    cl_ulong timeMax = maxNBitValue(pDevice->getHardwareInfo().capabilityTable.kernelTimestampValidBits);
    cl_ulong realDelta = 10;

    cl_ulong startTime = timeMax - realDelta;
    cl_ulong endTime = 2;
    cl_ulong delta = 0;
    delta = pEvent->getDelta(startTime, endTime);
    EXPECT_EQ(realDelta + endTime, delta);
    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenProfilingDisabledWhenCalculatingProfilingDataThenFalseIsReturned) {
    auto *pEvent = new MockEvent<Event>(nullptr, 0, 0, 0);
    EXPECT_FALSE(pEvent->calcProfilingData());
    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenProfilingEnabledWhenCalculatingProfilingDataThenFalseIsNotReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    cl_bool result = pEvent->isProfilingEnabled();
    EXPECT_EQ(((cl_bool)CL_FALSE), result);
    pEvent->setProfilingEnabled(true);
    result = pEvent->isProfilingEnabled();
    EXPECT_NE(((cl_bool)CL_FALSE), result);
    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenProfilingEnabledAndUserEventsWhenCalculatingProfilingDataThenFalseIsReturned) {
    Event *pEvent = new UserEvent();
    cl_bool result = pEvent->isProfilingEnabled();
    EXPECT_EQ(((cl_bool)CL_FALSE), result);
    delete pEvent;
}

TEST_F(clEventProfilingTests, GivenPerfCountersEnabledWhenCheckingPerfCountersThenTrueIsReturned) {
    Event *pEvent = new Event(pCommandQueue, 0, 0, 0);
    bool result = pEvent->isPerfCountersEnabled();
    EXPECT_FALSE(result);
    pEvent->setPerfCountersEnabled(true);
    result = pEvent->isPerfCountersEnabled();
    EXPECT_TRUE(result);
    delete pEvent;
}

class ClEventProfilingWithPerfCountersTests : public DeviceInstrumentationFixture,
                                              public PerformanceCountersDeviceFixture,
                                              public ::testing::Test {
  public:
    void SetUp() override {
        PerformanceCountersDeviceFixture::setUp();
        DeviceInstrumentationFixture::setUp(true);

        cl_device_id deviceId = device.get();
        cl_int retVal = CL_SUCCESS;
        context = std::unique_ptr<Context>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1),
                                                                        nullptr, nullptr, retVal));
        commandQueue = std::make_unique<MockCommandQueue>(context.get(), device.get(), nullptr, false);
        event = std::make_unique<Event>(commandQueue.get(), 0, 0, 0);
        event->setStatus(CL_COMPLETE);
        event->setProfilingEnabled(true);
        commandQueue->getPerfCounters()->getApiReport(event->getHwPerfCounterNode(), 0, nullptr, &param_value_size, true);

        eventCl = static_cast<cl_event>(event.get());
    }

    void TearDown() override {
        PerformanceCountersDeviceFixture::tearDown();
    }

    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
    std::unique_ptr<Event> event;
    size_t param_value_size = 0;
    cl_event eventCl = nullptr;
    cl_ulong param_value = 0;
    size_t param_value_size_ret = 0;
};

TEST_F(ClEventProfilingWithPerfCountersTests, GivenDisabledPerfCountersWhenGettingEventProfilingInfoThenInvalidValueErrorIsReturned) {
    event->setPerfCountersEnabled(false);
    cl_int retVal = clGetEventProfilingInfo(eventCl,
                                            CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                            param_value_size,
                                            &param_value,
                                            &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClEventProfilingWithPerfCountersTests, GivenEnabledPerfCountersWhenGettingEventProfilingInfoThenSuccessIsReturned) {
    event->setPerfCountersEnabled(true);
    cl_int retVal = clGetEventProfilingInfo(eventCl,
                                            CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                            param_value_size,
                                            &param_value,
                                            &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEventProfilingWithPerfCountersTests, GivenEnabledPerfCountersAndIncorrectParamValueSizeWhenGettingEventProfilingInfoThenProfilingInfoNotAvailableErrorIsReturned) {
    event->setPerfCountersEnabled(true);
    cl_int retVal = clGetEventProfilingInfo(eventCl,
                                            CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
                                            param_value_size - 1,
                                            &param_value,
                                            &param_value_size_ret);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, retVal);
}
