/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/event/event.h"

#include "cl_api_tests.h"
using namespace NEO;

namespace ClSetEventCallbackTests {

static int cbInvoked = 0;
static void *cbData = nullptr;
void CL_CALLBACK eventCallBack(cl_event event, cl_int callbackType, void *userData) {
    cbInvoked++;
    cbData = userData;
}

class ClSetEventCallbackTests : public ApiFixture<>,
                                public ::testing::Test {

    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        DebugManager.flags.EnableAsyncEventsHandler.set(false);
        ApiFixture::SetUp();
        cbInvoked = 0;
        cbData = nullptr;
    }

    void TearDown() override {
        ApiFixture::TearDown();
    }

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

TEST_F(ClSetEventCallbackTests, GivenValidEventWhenSettingEventCallbackThenSuccessIsReturned) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();
}

TEST_F(ClSetEventCallbackTests, GivenInvalidEventWhenSettingEventCallbackThenInvalidEventErrorIsReturned) {
    std::unique_ptr<char> event(new char[sizeof(Event)]);
    memset(event.get(), 0, sizeof(Event));
    retVal = clSetEventCallback(reinterpret_cast<cl_event>(event.get()), CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST_F(ClSetEventCallbackTests, GivenValidCallbackTypeWhenSettingEventCallbackThenSuccessIsReturned) {
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

TEST_F(ClSetEventCallbackTests, GivenInvalidCallbackTypeWhenSettingEventCallbackThenInvalidValueErrorIsReturned) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE + CL_RUNNING + CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClSetEventCallbackTests, GivenNullCallbackWhenSettingEventCallbackThenInvalidValueErrorIsReturned) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClSetEventCallbackTests, GivenMultipleCallbacksWhenSettingEventCallbackThenSuccessIsReturned) {
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

TEST_F(ClSetEventCallbackTests, GivenValidCallbackWhenStatusIsSetToCompleteThenCallbackWasInvokedOnce) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);
}

TEST_F(ClSetEventCallbackTests, GivenThreeCallbacksWhenStatusIsSetToCompleteThenCallbackWasInvokedThreeTimes) {
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

TEST_F(ClSetEventCallbackTests, GivenValidCallbackWhenStatusIsSetToCompleteMultipleTimesThenCallbackWasInvokedOnce) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, nullptr);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);
}

TEST_F(ClSetEventCallbackTests, GivenThreeCallbacksWhenStatusIsSetToCompleteMultipleTimesThenCallbackWasInvokedThreeTimes) {
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

TEST_F(ClSetEventCallbackTests, GivenUserDataWhenStatusIsSetToCompleteThenCallbackWasInvokedOnce) {
    std::unique_ptr<Event> event(new Event(nullptr, 0, 0, 0));
    int data = 1;
    retVal = clSetEventCallback(event.get(), CL_COMPLETE, eventCallBack, &data);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);
    EXPECT_EQ(&data, cbData);
}
} // namespace ClSetEventCallbackTests
