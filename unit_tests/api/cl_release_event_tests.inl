/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/event/event.h"
#include "test.h"

using namespace OCLRT;

namespace ClReleaseEventTests {
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

typedef EventFixture<::testing::Test> clEventTests;

TEST_F(clEventTests, releaseNullptr) {
    auto retVal = clReleaseEvent(nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST_F(clEventTests, releaseValidEvent) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    ASSERT_NE(nullptr, pEvent);

    cl_event event = (cl_event)pEvent;
    auto retVal = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    //no delete operation. clReleaseEvent should do this for us
}

TEST_F(clEventTests, releaseAfterRetain) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    ASSERT_NE(nullptr, pEvent);

    cl_event event = (cl_event)pEvent;
    auto retVal = clRetainEvent(event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pEvent->getReference(), 2);
    retVal = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pEvent->getReference(), 1);
    delete pEvent;
}

TEST_F(clEventTests, releaseTwiceAfterRetain) {
    Event *pEvent = new Event(nullptr, 0, 0, 0);
    ASSERT_NE(nullptr, pEvent);

    cl_event event = (cl_event)pEvent;
    auto retVal = clRetainEvent(event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pEvent->getReference(), 2);
    retVal = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pEvent->getReference(), 1);
    retVal = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEventTests, retainNullptr) {
    auto retVal = clRetainEvent(nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST_F(clEventTests, getEventsCommandQueue) {
    cl_command_queue cmdQ;
    Event *pEvent = new Event(nullptr, 0, 0, 0);

    cl_event event = (cl_event)pEvent;
    auto retVal = clGetEventInfo(event, CL_EVENT_COMMAND_QUEUE, sizeof(cmdQ), &cmdQ, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pEvent;
}

TEST_F(clEventTests, getInvalidEventInfo) {
    cl_command_queue cmdQ;

    auto retVal = clGetEventInfo(nullptr, CL_EVENT_COMMAND_QUEUE, sizeof(cmdQ), &cmdQ, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST_F(clEventTests, clWaitForEventsWithInvalidEvent) {
    char *ptr = new char[sizeof(Event)];
    cl_event event = (cl_event)ptr;

    auto retVal = clWaitForEvents(1, &event);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);

    delete[] ptr;
}

TEST_F(clEventTests, doubleSetStatusReturnsError) {
    cl_int error = 0;
    cl_event event = clCreateUserEvent(pContext, &error);
    EXPECT_EQ(CL_SUCCESS, error);

    error = clSetUserEventStatus(event, CL_COMPLETE);
    EXPECT_EQ(CL_SUCCESS, error);

    error = clSetUserEventStatus(event, CL_COMPLETE);
    EXPECT_EQ(CL_INVALID_OPERATION, error);

    clReleaseEvent(event);
}

typedef EventFixture<::testing::TestWithParam<std::tuple<int32_t, int32_t>>> clEventStatusTests;

TEST_P(clEventStatusTests, setUserEventStatus) {
    cl_int error = 0;
    cl_event event = clCreateUserEvent(pContext, &error);
    EXPECT_EQ(CL_SUCCESS, error);

    auto status = std::get<0>(GetParam());
    auto expect = std::get<1>(GetParam());
    error = clSetUserEventStatus(event, status);
    EXPECT_EQ(expect, error);

    clReleaseEvent(event);
}

cl_int validStatus[] = {CL_COMPLETE, -1};
cl_int expectValidStatus[] = {CL_SUCCESS};
cl_int invalidStatus[] = {CL_QUEUED, CL_SUBMITTED, 12};
cl_int expectInvalidStatus[] = {CL_INVALID_VALUE};

INSTANTIATE_TEST_CASE_P(SetValidStatus,
                        clEventStatusTests,
                        ::testing::Combine(
                            ::testing::ValuesIn(validStatus),
                            ::testing::ValuesIn(expectValidStatus)));

INSTANTIATE_TEST_CASE_P(SetInvalidStatus,
                        clEventStatusTests,
                        ::testing::Combine(
                            ::testing::ValuesIn(invalidStatus),
                            ::testing::ValuesIn(expectInvalidStatus)));
} // namespace ClReleaseEventTests
