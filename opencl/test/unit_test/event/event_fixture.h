/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct EventTest
    : public ClDeviceFixture,
      public CommandQueueFixture,
      public CommandStreamFixture,
      public ::testing::Test {

    using CommandQueueFixture::setUp;

    void SetUp() override {
        ClDeviceFixture::setUp();
        CommandQueueFixture::setUp(&mockContext, pClDevice, 0);
        CommandStreamFixture::setUp(pCmdQ);
    }

    void TearDown() override {
        CommandStreamFixture::tearDown();
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
    MockContext mockContext;
};

template <typename GfxFamily>
struct TestEventCsr;

struct EventTestWithTestEventCsr
    : public EventTest {
    void SetUp() override {}
    void TearDown() override {}
    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<TestEventCsr<FamilyType>>();
        EventTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        EventTest::TearDown();
    }
};

struct InternalsEventTest
    : public ClDeviceFixture,
      public ::testing::Test {

    InternalsEventTest() {
    }

    void SetUp() override {
        ClDeviceFixture::setUp();
        mockContext = new MockContext(pClDevice);
    }

    void TearDown() override {
        delete mockContext;
        ClDeviceFixture::tearDown();
    }

    MockContext *mockContext = nullptr;
};

struct InternalsEventTestWithMockCsr
    : public InternalsEventTest {

    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockCsr<FamilyType>>();
        InternalsEventTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        InternalsEventTest::TearDown();
    }
};

struct MyUserEvent : public VirtualEvent {
    WaitStatus wait(bool blocking, bool quickKmdSleep) override {
        return VirtualEvent::wait(blocking, quickKmdSleep);
    };
    TaskCountType getTaskLevel() override {
        return VirtualEvent::getTaskLevel();
    };
};

struct MyEvent : public Event {

    using Event::completeTimeStamp;
    using Event::endTimeStamp;
    using Event::queueTimeStamp;
    using Event::startTimeStamp;
    using Event::submitTimeStamp;

    MyEvent(CommandQueue *cmdQueue, cl_command_type cmdType, TaskCountType taskLevel, TaskCountType taskCount)
        : Event(cmdQueue, cmdType, taskLevel, taskCount) {
    }

    uint64_t getGlobalStartTimestamp() const {
        return this->globalStartTimestamp;
    }

    bool getDataCalcStatus() const {
        return this->dataCalculated;
    }

    void calculateProfilingDataInternal(uint64_t contextStartTS, uint64_t contextEndTS, uint64_t *contextCompleteTS, uint64_t globalStartTS) override {
        if (debugManager.flags.ReturnRawGpuTimestamps.get()) {
            globalStartTimestamp = globalStartTS;
        }
        Event::calculateProfilingDataInternal(contextStartTS, contextEndTS, contextCompleteTS, globalStartTS);
    }

    uint64_t globalStartTimestamp;
};

class MockEventTests : public HelloWorldTest<HelloWorldFixtureFactory> {
  public:
    void TearDown() override {
        if (uEvent) {
            uEvent->setStatus(-1);
            uEvent.reset();
        }
        HelloWorldFixture::tearDown();
    }

  protected:
    ReleaseableObjectPtr<UserEvent> uEvent = nullptr;
};
