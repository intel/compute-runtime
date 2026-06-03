/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/event/event.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "CL/cl.h"

#include <limits>

namespace NEO {
namespace LEO {
namespace ult {

struct CommandQueueDependenciesTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();

        auto &clDevices = platform->getDevices();
        ASSERT_FALSE(clDevices.empty());
        clDevice = clDevices[0].get();

        cl_int errcode = CL_SUCCESS;
        cl_device_id clDeviceId = clDevice;
        clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clContext);

        context = castToObject<Context>(clContext);
        ASSERT_NE(nullptr, context);
    }

    void TearDown() override {
        clReleaseContext(clContext);
        Test<OclFixture>::TearDown();
    }

    CommandQueue *createCommandQueue() {
        return new CommandQueue(context, clDevice, nullptr, mockCmdList.toHandle());
    }

    cl_event createUserEvent() {
        cl_int errcode = CL_SUCCESS;
        auto event = clCreateUserEvent(clContext, &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        EXPECT_NE(nullptr, event);
        return event;
    }

    static constexpr uint64_t maxTimeout = std::numeric_limits<uint64_t>::max();

    ClDevice *clDevice = nullptr;
    cl_context clContext = nullptr;
    Context *context = nullptr;
    L0::ult::Mock<L0::ult::CommandList> mockCmdList{};
};

TEST_F(CommandQueueDependenciesTest, givenNewCommandQueueWhenHasDependenciesThenReturnsFalse) {
    auto *cmdQueue = createCommandQueue();

    EXPECT_FALSE(cmdQueue->hasDependencies());

    cmdQueue->decRefApi();
}

TEST_F(CommandQueueDependenciesTest, givenZeroEventsWhenStoreDependenciesThenNoDependenciesStored) {
    auto *cmdQueue = createCommandQueue();

    cmdQueue->storeDependencies(0, nullptr);
    EXPECT_FALSE(cmdQueue->hasDependencies());

    cmdQueue->decRefApi();
}

TEST_F(CommandQueueDependenciesTest, givenEventsWhenStoreDependenciesThenDependenciesStoredAndInternalRefCountIncremented) {
    auto *cmdQueue = createCommandQueue();

    cl_event events[] = {createUserEvent(), createUserEvent()};
    auto *event0 = castToObject<Event>(events[0]);
    auto *event1 = castToObject<Event>(events[1]);
    const auto baseRefCount0 = event0->getRefInternalCount();
    const auto baseRefCount1 = event1->getRefInternalCount();

    cmdQueue->storeDependencies(2, events);

    EXPECT_TRUE(cmdQueue->hasDependencies());
    EXPECT_EQ(baseRefCount0 + 1, event0->getRefInternalCount());
    EXPECT_EQ(baseRefCount1 + 1, event1->getRefInternalCount());

    cmdQueue->clearDependencies();
    clReleaseEvent(events[0]);
    clReleaseEvent(events[1]);
    cmdQueue->decRefApi();
}

TEST_F(CommandQueueDependenciesTest, givenStoredDependenciesWhenClearDependenciesThenDependenciesClearedAndInternalRefCountDecremented) {
    auto *cmdQueue = createCommandQueue();

    auto event = createUserEvent();
    auto *leoEvent = castToObject<Event>(event);
    const auto baseRefCount = leoEvent->getRefInternalCount();

    cmdQueue->storeDependencies(1, &event);
    ASSERT_TRUE(cmdQueue->hasDependencies());

    cmdQueue->clearDependencies();

    EXPECT_FALSE(cmdQueue->hasDependencies());
    EXPECT_EQ(baseRefCount, leoEvent->getRefInternalCount());

    clReleaseEvent(event);
    cmdQueue->decRefApi();
}

TEST_F(CommandQueueDependenciesTest, givenStoredDependenciesWhenHostSynchronizeSucceedsThenCommandListSynchronizedAndDependenciesCleared) {
    auto *cmdQueue = createCommandQueue();

    auto event = createUserEvent();
    auto *leoEvent = castToObject<Event>(event);
    const auto baseRefCount = leoEvent->getRefInternalCount();

    cmdQueue->storeDependencies(1, &event);
    ASSERT_TRUE(cmdQueue->hasDependencies());

    mockCmdList.hostSynchronizeResult = ZE_RESULT_SUCCESS;
    auto result = cmdQueue->hostSynchronize(maxTimeout);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);
    EXPECT_FALSE(cmdQueue->hasDependencies());
    EXPECT_EQ(baseRefCount, leoEvent->getRefInternalCount());

    clReleaseEvent(event);
    cmdQueue->decRefApi();
}

TEST_F(CommandQueueDependenciesTest, givenStoredDependenciesWhenHostSynchronizeFailsThenDependenciesNotCleared) {
    auto *cmdQueue = createCommandQueue();

    auto event = createUserEvent();
    auto *leoEvent = castToObject<Event>(event);
    const auto baseRefCount = leoEvent->getRefInternalCount();

    cmdQueue->storeDependencies(1, &event);
    ASSERT_TRUE(cmdQueue->hasDependencies());

    mockCmdList.hostSynchronizeResult = ZE_RESULT_ERROR_DEVICE_LOST;
    auto result = cmdQueue->hostSynchronize(maxTimeout);

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);
    EXPECT_TRUE(cmdQueue->hasDependencies());
    EXPECT_EQ(baseRefCount + 1, leoEvent->getRefInternalCount());

    cmdQueue->clearDependencies();
    clReleaseEvent(event);
    cmdQueue->decRefApi();
}

TEST_F(CommandQueueDependenciesTest, givenDependenciesWhenCommandQueueIsDestroyedThenHostSynchronizeIsCalledAndDependenciesReleased) {
    auto *cmdQueue = createCommandQueue();

    auto event = createUserEvent();
    auto *leoEvent = castToObject<Event>(event);
    const auto baseRefCount = leoEvent->getRefInternalCount();

    cmdQueue->storeDependencies(1, &event);
    ASSERT_TRUE(cmdQueue->hasDependencies());
    EXPECT_EQ(baseRefCount + 1, leoEvent->getRefInternalCount());

    mockCmdList.hostSynchronizeResult = ZE_RESULT_SUCCESS;
    cmdQueue->decRefApi();

    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);
    EXPECT_EQ(baseRefCount, leoEvent->getRefInternalCount());

    clReleaseEvent(event);
}

TEST_F(CommandQueueDependenciesTest, givenNoDependenciesWhenCommandQueueIsDestroyedThenHostSynchronizeIsNotCalled) {
    auto *cmdQueue = createCommandQueue();

    cmdQueue->decRefApi();

    EXPECT_EQ(0u, mockCmdList.hostSynchronizeCalled);
}

TEST_F(CommandQueueDependenciesTest, givenCommandQueueWithDependenciesWhenReleaseCommandQueueWithSingleReferenceThenHostSynchronizeIsCalled) {
    auto *cmdQueue = createCommandQueue();

    auto event = createUserEvent();
    cmdQueue->storeDependencies(1, &event);

    mockCmdList.hostSynchronizeResult = ZE_RESULT_SUCCESS;
    auto retVal = clReleaseCommandQueue(cmdQueue);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);

    clReleaseEvent(event);
}

TEST_F(CommandQueueDependenciesTest, givenCommandQueueWithDependenciesWhenReleaseCommandQueueWithMultipleReferencesThenHostSynchronizeIsNotCalled) {
    auto *cmdQueue = createCommandQueue();

    auto event = createUserEvent();
    cmdQueue->storeDependencies(1, &event);

    clRetainCommandQueue(cmdQueue);

    mockCmdList.hostSynchronizeResult = ZE_RESULT_SUCCESS;
    auto retVal = clReleaseCommandQueue(cmdQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockCmdList.hostSynchronizeCalled);

    // Last reference still holds the dependencies, so releasing it synchronizes.
    retVal = clReleaseCommandQueue(cmdQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);

    clReleaseEvent(event);
}

TEST_F(CommandQueueDependenciesTest, givenCommandQueueWithDependenciesWhenFinishThenHostSynchronizeIsCalledAndDependenciesCleared) {
    auto *cmdQueue = createCommandQueue();

    auto event = createUserEvent();
    cmdQueue->storeDependencies(1, &event);
    ASSERT_TRUE(cmdQueue->hasDependencies());

    mockCmdList.hostSynchronizeResult = ZE_RESULT_SUCCESS;
    auto retVal = clFinish(cmdQueue);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);
    EXPECT_FALSE(cmdQueue->hasDependencies());

    clReleaseEvent(event);
    cmdQueue->decRefApi();
}

TEST_F(CommandQueueDependenciesTest, givenEventsWhenEnqueueWaitForEventsThenDependenciesStored) {
    auto *cmdQueue = createCommandQueue();

    cl_event events[] = {createUserEvent(), createUserEvent()};
    auto *event0 = castToObject<Event>(events[0]);
    auto *event1 = castToObject<Event>(events[1]);
    const auto baseRefCount0 = event0->getRefInternalCount();
    const auto baseRefCount1 = event1->getRefInternalCount();

    auto retVal = clEnqueueWaitForEvents(cmdQueue, 2, events);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(cmdQueue->hasDependencies());
    EXPECT_EQ(baseRefCount0 + 1, event0->getRefInternalCount());
    EXPECT_EQ(baseRefCount1 + 1, event1->getRefInternalCount());

    cmdQueue->clearDependencies();
    clReleaseEvent(events[0]);
    clReleaseEvent(events[1]);
    cmdQueue->decRefApi();
}

} // namespace ult
} // namespace LEO
} // namespace NEO
