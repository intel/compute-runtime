/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/event/event.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

struct EventOoqFixture : public OclFixture {
    void setUp() {
        OclFixture::setUp();
        clDeviceId = platform->getDevices()[0].get();
        cl_int errcode = CL_SUCCESS;
        clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clContext);
    }

    void tearDown() {
        clReleaseContext(clContext);
        OclFixture::tearDown();
    }

    cl_command_queue createQueue(cl_command_queue_properties properties) {
        cl_int errcode = CL_SUCCESS;
        cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, properties, 0};
        auto queue = clCreateCommandQueueWithProperties(clContext, clDeviceId, props, &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        return queue;
    }

    cl_device_id clDeviceId = nullptr;
    cl_context clContext = nullptr;
};

using EventOoqTests = Test<EventOoqFixture>;

TEST_F(EventOoqTests, givenOutOfOrderQueueWhenCreatingCommandEventThenEventIsRegularNotCounterBased) {
    auto queue = createQueue(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    ASSERT_NE(nullptr, queue);
    auto leoQueue = castToObject<CommandQueue>(queue);

    auto event = new Event(CL_COMMAND_MARKER, leoQueue);
    EXPECT_FALSE(L0::Event::fromHandle(event->getL0Handle())->isCounterBased());

    clReleaseEvent(event);
    clReleaseCommandQueue(queue);
}

TEST_F(EventOoqTests, givenInOrderQueueWhenCreatingCommandEventThenEventIsCounterBased) {
    auto queue = createQueue(0);
    ASSERT_NE(nullptr, queue);
    auto leoQueue = castToObject<CommandQueue>(queue);

    auto event = new Event(CL_COMMAND_MARKER, leoQueue);
    EXPECT_TRUE(L0::Event::fromHandle(event->getL0Handle())->isCounterBased());

    clReleaseEvent(event);
    clReleaseCommandQueue(queue);
}

TEST_F(EventOoqTests, givenOutOfOrderProfilingQueueWhenCreatingCommandEventThenEventIsRegularTimestamp) {
    auto queue = createQueue(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE);
    ASSERT_NE(nullptr, queue);
    auto leoQueue = castToObject<CommandQueue>(queue);

    auto event = new Event(CL_COMMAND_MARKER, leoQueue);
    auto l0Event = L0::Event::fromHandle(event->getL0Handle());
    EXPECT_FALSE(l0Event->isCounterBased());
    EXPECT_TRUE(l0Event->isEventTimestampFlagSet());

    clReleaseEvent(event);
    clReleaseCommandQueue(queue);
}

TEST_F(EventOoqTests, givenUserEventThenEventIsRegularNotCounterBased) {
    auto leoContext = castToObject<Context>(clContext);

    auto event = new Event(leoContext);
    EXPECT_FALSE(L0::Event::fromHandle(event->getL0Handle())->isCounterBased());

    clReleaseEvent(event);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
