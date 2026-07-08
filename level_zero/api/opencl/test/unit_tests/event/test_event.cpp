/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/test/common/test_macros/mock_method_macros.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
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

struct MockEventForProfilingWiring : public NEO::LEO::Event {
    using NEO::LEO::Event::completeTimeStamp;
    using NEO::LEO::Event::dataCalculated;
    using NEO::LEO::Event::endTimeStamp;
    using NEO::LEO::Event::Event;
    using NEO::LEO::Event::queueTimeStamp;
    using NEO::LEO::Event::setQueueTimeStamp;
    using NEO::LEO::Event::setSubmitTimeStamp;
    using NEO::LEO::Event::startTimeStamp;
    using NEO::LEO::Event::submitTimeStamp;

    ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t &result) override {
        queryKernelTimestampCalled++;
        result.global.kernelStart = injectedKernelStart;
        result.global.kernelEnd = injectedKernelEnd;
        return ZE_RESULT_SUCCESS;
    }

    uint64_t injectedKernelStart = 0;
    uint64_t injectedKernelEnd = 0;
    uint32_t queryKernelTimestampCalled = 0;
};

using ProfilingInfoWiringTests = Test<QueryAndUpdateEventStatusFixture>;

TEST_F(ProfilingInfoWiringTests, givenProfilingEventWhenGettingProfilingInfoThenPacketIsRebasedAndTimestampsAreMonotonic) {
    MockEventForProfilingWiring event{CL_COMMAND_NDRANGE_KERNEL, commandQueue};
    event.setQueueTimeStamp();
    event.setSubmitTimeStamp();

    // Start at/after submit so the rebased ordering is deterministic; end trails start.
    event.injectedKernelStart = event.submitTimeStamp.gpuTimeStamp + 0x1000;
    event.injectedKernelEnd = event.injectedKernelStart + 0x500;

    auto query = [&](cl_profiling_info paramName) {
        cl_ulong value = 0;
        EXPECT_EQ(CL_SUCCESS, event.getProfilingInfo(paramName, sizeof(value), &value, nullptr));
        return value;
    };

    const auto queued = query(CL_PROFILING_COMMAND_QUEUED);
    const auto submit = query(CL_PROFILING_COMMAND_SUBMIT);
    const auto start = query(CL_PROFILING_COMMAND_START);
    const auto end = query(CL_PROFILING_COMMAND_END);
    const auto complete = query(CL_PROFILING_COMMAND_COMPLETE);

    EXPECT_LE(queued, submit);
    EXPECT_LE(submit, start);
    EXPECT_LE(start, end);
    EXPECT_LE(end, complete);
    EXPECT_TRUE(event.dataCalculated);
    EXPECT_NE(0u, event.queryKernelTimestampCalled);

    // Exact wiring (resolution-independent): start == kernelStart (idempotent restore here), end == kernelEnd, complete == end.
    EXPECT_EQ(event.injectedKernelStart, event.startTimeStamp.gpuTimeStamp);
    EXPECT_EQ(event.injectedKernelEnd, event.endTimeStamp.gpuTimeStamp);
    EXPECT_EQ(event.endTimeStamp.gpuTimeStamp, event.completeTimeStamp.gpuTimeStamp);
}

TEST_F(ProfilingInfoWiringTests, givenProfilingInfoAlreadyCalculatedWhenQueriedAgainThenStartIsStable) {
    MockEventForProfilingWiring event{CL_COMMAND_NDRANGE_KERNEL, commandQueue};
    event.setQueueTimeStamp();
    event.setSubmitTimeStamp();
    event.injectedKernelStart = event.submitTimeStamp.gpuTimeStamp + 0x1000;
    event.injectedKernelEnd = event.injectedKernelStart + 0x500;

    cl_ulong first = 0;
    EXPECT_EQ(CL_SUCCESS, event.getProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(first), &first, nullptr));
    ASSERT_TRUE(event.dataCalculated);

    // Change the injected packet after the memoized computation; START must not move.
    event.injectedKernelStart = event.submitTimeStamp.gpuTimeStamp + 0x9000;
    cl_ulong second = 0;
    EXPECT_EQ(CL_SUCCESS, event.getProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(second), &second, nullptr));

    EXPECT_EQ(first, second);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
