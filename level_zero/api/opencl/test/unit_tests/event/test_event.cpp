/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/mock_method_macros.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/event/event.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(EventHandleSpanTests, givenZeroCountAndNullListWhenConstructThenSizeIsZeroAndDataIsNull) {
    EventHandleSpan span{0, nullptr};
    EXPECT_EQ(0u, span.size());
    EXPECT_EQ(nullptr, span.data());
}

TEST(EventHandleSpanTests, givenNonZeroCountAndNullListWhenConstructThenSizeIsZeroAndDataIsNull) {
    EventHandleSpan span{5, nullptr};
    EXPECT_EQ(0u, span.size());
    EXPECT_EQ(nullptr, span.data());
}

TEST(EventHandleSpanTests, givenZeroCountAndNonNullListWhenConstructThenSizeIsZeroAndDataIsNull) {
    cl_event fakeEvent = reinterpret_cast<cl_event>(0x1);
    EventHandleSpan span{0, &fakeEvent};
    EXPECT_EQ(0u, span.size());
    EXPECT_EQ(nullptr, span.data());
}

TEST(EventHandleSpanTests, givenDefaultConstructedWhenQueryThenSizeIsZeroAndDataIsNull) {
    EventHandleSpan span{};
    EXPECT_EQ(0u, span.size());
    EXPECT_EQ(nullptr, span.data());
}

struct MockCmdListWithPrintf : public L0::ult::Mock<L0::ult::CommandList> {
    void handlePostSyncPrintfAndAssert(bool hangDetected) override {
        handlePostSyncPrintfAndAssertCalled++;
        lastHangDetected = hangDetected;
    }

    uint32_t handlePostSyncPrintfAndAssertCalled = 0u;
    bool lastHangDetected = false;
};

struct MockEventWithWait : public NEO::LEO::Event {
    using NEO::LEO::Event::Event;

    ADDMETHOD_NOBASE(wait, ze_result_t, ZE_RESULT_SUCCESS, ());
};

struct QueryAndUpdateEventStatusFixture : public OclFixture {
    void setUp() {
        OclFixture::setUp();

        auto &clDevices = platform->getDevices();
        ASSERT_FALSE(clDevices.empty());
        clDevice = clDevices[0].get();

        cl_int errcode = CL_SUCCESS;
        cl_device_id clDeviceId = clDevice;
        clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clContext);

        context = castToObject<Context>(clContext);
        commandQueue = new CommandQueue(context, clDevice, nullptr, mockCmdList.toHandle());
    }

    void tearDown() {
        commandQueue->decRefApi();
        clReleaseContext(clContext);
        OclFixture::tearDown();
    }

    ClDevice *clDevice = nullptr;
    cl_context clContext = nullptr;
    Context *context = nullptr;
    MockCmdListWithPrintf mockCmdList{};
    CommandQueue *commandQueue = nullptr;
};

using QueryAndUpdateEventStatusTests = Test<QueryAndUpdateEventStatusFixture>;

TEST_F(QueryAndUpdateEventStatusTests, givenEventNotReadyWhenQueryAndUpdateThenStatusRemainsSubmitted) {
    MockEventWithWait event{CL_COMMAND_NDRANGE_KERNEL, commandQueue};

    auto status = event.queryAndUpdateEventStatus();

    EXPECT_EQ(CL_SUBMITTED, status);
    EXPECT_EQ(0u, mockCmdList.handlePostSyncPrintfAndAssertCalled);
}

TEST_F(QueryAndUpdateEventStatusTests, givenUserEventReadyWhenQueryAndUpdateThenStatusIsCompleteAndPrintfNotCalled) {
    MockEventWithWait event{context};
    L0::Event::fromHandle(event.getL0Handle())->hostSignal(false);

    auto status = event.queryAndUpdateEventStatus();

    EXPECT_EQ(CL_COMPLETE, status);
    EXPECT_EQ(0u, mockCmdList.handlePostSyncPrintfAndAssertCalled);
}

TEST_F(QueryAndUpdateEventStatusTests, givenReadyEventWithEmptyPrintfContainerWhenQueryAndUpdateThenStatusIsCompleteAndPrintfNotCalled) {
    MockEventWithWait event{CL_COMMAND_NDRANGE_KERNEL, commandQueue};
    L0::Event::fromHandle(event.getL0Handle())->hostSignal(false);

    ASSERT_TRUE(mockCmdList.getPrintfKernelContainer().empty());

    auto status = event.queryAndUpdateEventStatus();

    EXPECT_EQ(CL_COMPLETE, status);
    EXPECT_EQ(0u, mockCmdList.handlePostSyncPrintfAndAssertCalled);
}

TEST_F(QueryAndUpdateEventStatusTests, givenReadyEventWithNonEmptyPrintfContainerWhenWaitSucceedsThenPrintfCalledWithNoHang) {
    MockEventWithWait event{CL_COMMAND_NDRANGE_KERNEL, commandQueue};
    event.waitResult = ZE_RESULT_SUCCESS;
    L0::Event::fromHandle(event.getL0Handle())->hostSignal(false);
    mockCmdList.getPrintfKernelContainer().push_back({});

    auto status = event.queryAndUpdateEventStatus();

    EXPECT_EQ(CL_COMPLETE, status);
    EXPECT_EQ(1u, mockCmdList.handlePostSyncPrintfAndAssertCalled);
    EXPECT_FALSE(mockCmdList.lastHangDetected);
}

TEST_F(QueryAndUpdateEventStatusTests, givenReadyEventWithNonEmptyPrintfContainerWhenWaitFailsThenPrintfCalledWithHang) {
    MockEventWithWait event{CL_COMMAND_NDRANGE_KERNEL, commandQueue};
    event.waitResult = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::Event::fromHandle(event.getL0Handle())->hostSignal(false);
    mockCmdList.getPrintfKernelContainer().push_back({});

    auto status = event.queryAndUpdateEventStatus();

    EXPECT_EQ(CL_COMPLETE, status);
    EXPECT_EQ(1u, mockCmdList.handlePostSyncPrintfAndAssertCalled);
    EXPECT_TRUE(mockCmdList.lastHangDetected);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
