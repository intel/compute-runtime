/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/event/event.h"

#include "cl_api_tests.h"
using namespace NEO;

namespace ClSetEventCallbackTests {

static int cbInvoked = 0;
static void *cbData = nullptr;
void CL_CALLBACK eventCallBack(cl_event event, cl_int callbackType, void *userData) {
    cbInvoked++;
    cbData = userData;
}

class clSetEventCallbackTests : public api_fixture,
                                public ::testing::Test {

    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        DebugManager.flags.EnableAsyncEventsHandler.set(false);
        api_fixture::SetUp();
        cbInvoked = 0;
        cbData = nullptr;
    }

    void TearDown() override {
        api_fixture::TearDown();
    }

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

TEST_F(clSetEventCallbackTests, GivenValidEventWhenSettingEventCallbackThenSuccessIsReturned) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();
}

TEST_F(clSetEventCallbackTests, GivenInvalidEventWhenSettingEventCallbackThenInvalidEventErrorIsReturned) {
    std::unique_ptr<char> event(new char[sizeof(Event)]);
    memset(event.get(), 0, sizeof(Event));
    retVal = clSetEventCallback(reinterpret_cast<cl_event>(event.get()), CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST_F(clSetEventCallbackTests, GivenValidCallbackTypeWhenSettingEventCallbackThenSuccessIsReturned) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();

    event.reset(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_RUNNING, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();

    event.reset(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clSetEventCallbackTests, GivenInvalidCallbackTypeWhenSettingEventCallbackThenInvalidValueErrorIsReturned) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE + CL_RUNNING + CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetEventCallbackTests, GivenNullCallbackWhenSettingEventCallbackThenInvalidValueErrorIsReturned) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetEventCallbackTests, GivenMultipleCallbacksWhenSettingEventCallbackThenSuccessIsReturned) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(event.get(), CL_RUNNING, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(event.get(), CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();
    event->decRefInternal();
}

TEST_F(clSetEventCallbackTests, GivenValidCallbackWhenStatusIsSetToCompleteThenCallbackWasInvokedOnce) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);
}

TEST_F(clSetEventCallbackTests, GivenThreeCallbacksWhenStatusIsSetToCompleteThenCallbackWasInvokedThreeTimes) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(event.get(), CL_RUNNING, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(event.get(), CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 3);
}

TEST_F(clSetEventCallbackTests, GivenValidCallbackWhenStatusIsSetToCompleteMultipleTimesThenCallbackWasInvokedOnce) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);
}

TEST_F(clSetEventCallbackTests, GivenThreeCallbacksWhenStatusIsSetToCompleteMultipleTimesThenCallbackWasInvokedThreeTimes) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(event.get(), CL_RUNNING, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(event.get(), CL_SUBMITTED, eventCallBack, nullptr);
    event->setStatus(CL_SUBMITTED);
    event->setStatus(CL_RUNNING);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 3);
}

TEST_F(clSetEventCallbackTests, GivenUserDataWhenStatusIsSetToCompleteThenCallbackWasInvokedOnce) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    int data = 1;
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, &data);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);
    EXPECT_EQ(&data, cbData);
}
} // namespace ClSetEventCallbackTests
