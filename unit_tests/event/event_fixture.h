/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/aligned_memory.h"
#include "core/helpers/ptr_math.h"
#include "core/unit_tests/utilities/base_object_utils.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/event/user_event.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

struct EventTest
    : public DeviceFixture,
      public CommandQueueFixture,
      public CommandStreamFixture,
      public ::testing::Test {

    using CommandQueueFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(&mockContext, pClDevice, 0);
        CommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }
    MockContext mockContext;
};

struct InternalsEventTest
    : public DeviceFixture,
      public ::testing::Test {

    InternalsEventTest() {
    }

    void SetUp() {
        DeviceFixture::SetUp();
        mockContext = new MockContext(pClDevice);
    }

    void TearDown() {
        delete mockContext;
        DeviceFixture::TearDown();
    }

    MockContext *mockContext;
};

struct MyUserEvent : public VirtualEvent {
    bool wait(bool blocking, bool quickKmdSleep) override {
        return VirtualEvent::wait(blocking, quickKmdSleep);
    };
    uint32_t getTaskLevel() override {
        return VirtualEvent::getTaskLevel();
    };
};

struct MyEvent : public Event {
    MyEvent(CommandQueue *cmdQueue, cl_command_type cmdType, uint32_t taskLevel, uint32_t taskCount)
        : Event(cmdQueue, cmdType, taskLevel, taskCount) {
    }
    TimeStampData getQueueTimeStamp() {
        return this->queueTimeStamp;
    };

    TimeStampData getSubmitTimeStamp() {
        return this->submitTimeStamp;
    };

    uint64_t getStartTimeStamp() {
        return this->startTimeStamp;
    };

    uint64_t getEndTimeStamp() {
        return this->endTimeStamp;
    };

    uint64_t getCompleteTimeStamp() {
        return this->completeTimeStamp;
    }

    uint64_t getGlobalStartTimestamp() const {
        return this->globalStartTimestamp;
    }

    bool getDataCalcStatus() const {
        return this->dataCalculated;
    }

    void calculateProfilingDataInternal(uint64_t contextStartTS, uint64_t contextEndTS, uint64_t *contextCompleteTS, uint64_t globalStartTS) override {
        if (DebugManager.flags.ReturnRawGpuTimestamps.get()) {
            globalStartTimestamp = globalStartTS;
        }
        Event::calculateProfilingDataInternal(contextStartTS, contextEndTS, contextCompleteTS, globalStartTS);
    }

    uint64_t globalStartTimestamp;
};

class MockEventTests : public HelloWorldTest<HelloWorldFixtureFactory> {
  public:
    void TearDown() {
        uEvent->setStatus(-1);
        uEvent.reset();
        HelloWorldFixture::TearDown();
    }

  protected:
    ReleaseableObjectPtr<UserEvent> uEvent;
};
