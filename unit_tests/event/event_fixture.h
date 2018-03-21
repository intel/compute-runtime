/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/event/user_event.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct EventTest
    : public DeviceFixture,
      public CommandQueueFixture,
      public CommandStreamFixture,
      public IndirectHeapFixture,
      public MemoryManagementFixture,
      public ::testing::Test {

    using CommandQueueFixture::SetUp;
    using IndirectHeapFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(&mockContext, pDevice, 0);
        CommandStreamFixture::SetUp(pCmdQ);
        IndirectHeapFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        IndirectHeapFixture::TearDown();
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }
    MockContext mockContext;
};

struct InternalsEventTest
    : public DeviceFixture,
      public MemoryManagementFixture,
      public ::testing::Test {

    InternalsEventTest() {
    }

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        DeviceFixture::SetUp();
        mockContext = new MockContext(pDevice);
    }

    void TearDown() override {
        delete mockContext;
        DeviceFixture::TearDown();
        MemoryManagementFixture::TearDown();
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
};
