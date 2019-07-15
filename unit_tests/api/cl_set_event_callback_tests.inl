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

class clSetEventCallback_ : public api_fixture,
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

TEST_F(clSetEventCallback_, ValidEvent) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();
    delete event;
}

TEST_F(clSetEventCallback_, InvalidEvent) {
    char *event = new char[sizeof(Event)];
    memset(event, 0, sizeof(Event));
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
    delete[] event;
}

TEST_F(clSetEventCallback_, ValidCallbackTypes) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();
    delete event;
    event = new Event(nullptr, 0, 0, 0);
    clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_RUNNING, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();
    delete event;
    event = new Event(nullptr, 0, 0, 0);
    clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    delete event;
}

TEST_F(clSetEventCallback_, InvalidCallbackType) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE + CL_RUNNING + CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    delete event;
}

TEST_F(clSetEventCallback_, NullCallback) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    delete event;
}

TEST_F(clSetEventCallback_, MultipleCallbacks) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(clevent, CL_RUNNING, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(clevent, CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->decRefInternal();
    event->decRefInternal();
    delete event;
}

TEST_F(clSetEventCallback_, CallbackCalled) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);

    delete event;
}

TEST_F(clSetEventCallback_, MultipleCallbacksCalled) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(clevent, CL_RUNNING, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(clevent, CL_SUBMITTED, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 3);

    delete event;
}

TEST_F(clSetEventCallback_, MultipleSetStatusCallbackOnce) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, nullptr);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);

    delete event;
}

TEST_F(clSetEventCallback_, MultipleCallbacksCount) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(clevent, CL_RUNNING, eventCallBack, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(clevent, CL_SUBMITTED, eventCallBack, nullptr);
    event->setStatus(CL_SUBMITTED);
    event->setStatus(CL_RUNNING);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 3);

    delete event;
}

TEST_F(clSetEventCallback_, UserDataPassed) {
    Event *event = new Event(nullptr, 0, 0, 0);
    cl_event clevent = (cl_event)event;
    int data = 1;
    retVal = clSetEventCallback(clevent, CL_COMPLETE, eventCallBack, &data);
    EXPECT_EQ(CL_SUCCESS, retVal);
    event->setStatus(CL_COMPLETE);
    EXPECT_EQ(cbInvoked, 1);
    EXPECT_EQ(&data, cbData);

    delete event;
}
} // namespace ClSetEventCallbackTests
