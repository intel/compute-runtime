/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <array>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct EnqueueEventPlumbingFixture : public Test<LeoCaptureFixture> {
    void SetUp() override {
        Test<LeoCaptureFixture>::SetUp();
        buffer = createBuffer(bufferSize);
        ASSERT_NE(nullptr, buffer);
    }

    void TearDown() override {
        for (auto event : userEvents) {
            clReleaseEvent(event);
        }
        if (buffer != nullptr) {
            clReleaseMemObject(buffer);
        }
        Test<LeoCaptureFixture>::TearDown();
    }

    cl_event createUserEvent() {
        cl_int errcode = CL_SUCCESS;
        auto event = clCreateUserEvent(clContext, &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        userEvents.push_back(event);
        return event;
    }

    std::vector<cl_event> createUserEvents(size_t count) {
        std::vector<cl_event> events;
        events.reserve(count);
        for (size_t i = 0; i < count; i++) {
            events.push_back(createUserEvent());
        }
        return events;
    }

    static cl_command_type commandTypeOf(cl_event event) {
        return castToObject<Event>(event)->getCommandType();
    }

    static ze_event_handle_t l0HandleOf(cl_event event) {
        return castToObject<Event>(event)->getL0Handle();
    }

    static constexpr size_t bufferSize = 256u;

    cl_mem buffer = nullptr;
    std::vector<cl_event> userEvents;
    std::array<uint8_t, bufferSize> hostData{};
    std::array<uint8_t, bufferSize> usmLikeDst{};
    std::array<uint8_t, bufferSize> usmLikeSrc{};
    uint32_t pattern = 0xA5A5A5A5u;
    size_t origin[3] = {0u, 0u, 0u};
    size_t region[3] = {4u, 2u, 1u};
};

TEST_F(EnqueueEventPlumbingFixture, givenNoOutputEventWhenEnqueuingThenSignalEventIsNull) {
    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(nullptr, capturingCmdList.appendMemoryCopyArgs[0].signalEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenOutputEventWhenEnqueuingThenSignalEventMatchesReturnedEventHandle) {
    cl_event outEvent = nullptr;
    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, &outEvent));
    ASSERT_NE(nullptr, outEvent);

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(l0HandleOf(outEvent), capturingCmdList.appendMemoryCopyArgs[0].signalEvent);

    clReleaseEvent(outEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenWaitListWhenEnqueuingThenHandlesArePassedInGivenOrder) {
    auto events = createUserEvents(3u);

    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(),
                                               static_cast<cl_uint>(events.size()), events.data(), nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    const auto &waitEvents = capturingCmdList.appendMemoryCopyArgs[0].waitEvents;
    ASSERT_EQ(events.size(), waitEvents.size());
    for (size_t i = 0; i < events.size(); i++) {
        EXPECT_EQ(l0HandleOf(events[i]), waitEvents[i]) << "wait event index " << i;
    }
}

TEST_F(EnqueueEventPlumbingFixture, givenWaitListExceedingInlineCapacityWhenEnqueuingThenAllHandlesArePassed) {
    constexpr size_t aboveInlineCapacity = EventHandleSpan::maxInlineWaitEvents + 1u;
    auto events = createUserEvents(aboveInlineCapacity);

    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(),
                                               static_cast<cl_uint>(events.size()), events.data(), nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    const auto &waitEvents = capturingCmdList.appendMemoryCopyArgs[0].waitEvents;
    ASSERT_EQ(aboveInlineCapacity, waitEvents.size());
    for (size_t i = 0; i < events.size(); i++) {
        EXPECT_EQ(l0HandleOf(events[i]), waitEvents[i]) << "wait event index " << i;
    }
}

TEST_F(EnqueueEventPlumbingFixture, givenWaitListAtInlineCapacityWhenEnqueuingThenAllHandlesArePassed) {
    constexpr size_t atInlineCapacity = EventHandleSpan::maxInlineWaitEvents;
    auto events = createUserEvents(atInlineCapacity);

    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(),
                                               static_cast<cl_uint>(events.size()), events.data(), nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    ASSERT_EQ(atInlineCapacity, capturingCmdList.appendMemoryCopyArgs[0].waitEvents.size());
    EXPECT_EQ(l0HandleOf(events.back()), capturingCmdList.appendMemoryCopyArgs[0].waitEvents.back());
}

TEST_F(EnqueueEventPlumbingFixture, givenBufferCommandsWhenEnqueuedThenReturnedEventCarriesMatchingCommandType) {
    cl_event outEvent = nullptr;

    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_BUFFER), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueFillBuffer(getCommandQueue(), buffer, &pattern, sizeof(pattern), 0, bufferSize, 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_FILL_BUFFER), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueCopyBuffer(getCommandQueue(), buffer, buffer, 0, 0, 16u, 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenRectCommandsWhenEnqueuedThenReturnedEventCarriesMatchingCommandType) {
    cl_event outEvent = nullptr;

    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_FALSE, origin, origin, region,
                                                  0, 0, 0, 0, hostData.data(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_BUFFER_RECT), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBufferRect(getCommandQueue(), buffer, CL_FALSE, origin, origin, region,
                                                   0, 0, 0, 0, hostData.data(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER_RECT), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueCopyBufferRect(getCommandQueue(), buffer, buffer, origin, origin, region,
                                                  0, 0, 0, 0, 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER_RECT), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenSynchronizationCommandsWhenEnqueuedThenReturnedEventCarriesMatchingCommandType) {
    cl_event outEvent = nullptr;

    ASSERT_EQ(CL_SUCCESS, clEnqueueMarkerWithWaitList(getCommandQueue(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueBarrierWithWaitList(getCommandQueue(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_BARRIER), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueMigrateMemObjects(getCommandQueue(), 1, &buffer, 0, 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MIGRATE_MEM_OBJECTS), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    const void *svmPointer = usmLikeDst.data();
    size_t svmSize = usmLikeDst.size();
    ASSERT_EQ(CL_SUCCESS, clEnqueueSVMMigrateMem(getCommandQueue(), 1, &svmPointer, &svmSize, 0, 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MIGRATE_MEM), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenUsmCommandsWhenEnqueuedThenReturnedEventCarriesMatchingCommandType) {
    cl_event outEvent = nullptr;

    ASSERT_EQ(CL_SUCCESS, clEnqueueMemsetINTEL(getCommandQueue(), usmLikeDst.data(), 0, usmLikeDst.size(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MEMSET_INTEL), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueMemFillINTEL(getCommandQueue(), usmLikeDst.data(), &pattern, sizeof(pattern),
                                                usmLikeDst.size(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MEMFILL_INTEL), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);

    ASSERT_EQ(CL_SUCCESS, clEnqueueMemcpyINTEL(getCommandQueue(), CL_FALSE, usmLikeDst.data(), usmLikeSrc.data(),
                                               usmLikeDst.size(), 0, nullptr, &outEvent));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MEMCPY_INTEL), commandTypeOf(outEvent));
    clReleaseEvent(outEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenSvmMemcpyWhenEnqueuedThenCommandTypeIsOverriddenAfterDelegation) {
    cl_event outEvent = nullptr;

    ASSERT_EQ(CL_SUCCESS, clEnqueueSVMMemcpy(getCommandQueue(), CL_FALSE, usmLikeDst.data(), usmLikeSrc.data(),
                                             usmLikeDst.size(), 0, nullptr, &outEvent));

    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY), commandTypeOf(outEvent));
    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(l0HandleOf(outEvent), capturingCmdList.appendMemoryCopyArgs[0].signalEvent);

    clReleaseEvent(outEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenSvmMemFillWhenEnqueuedThenCommandTypeIsOverriddenAfterDelegation) {
    cl_event outEvent = nullptr;

    ASSERT_EQ(CL_SUCCESS, clEnqueueSVMMemFill(getCommandQueue(), usmLikeDst.data(), &pattern, sizeof(pattern),
                                              usmLikeDst.size(), 0, nullptr, &outEvent));

    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMFILL), commandTypeOf(outEvent));
    ASSERT_EQ(1u, capturingCmdList.appendMemoryFillArgs.count());
    EXPECT_EQ(l0HandleOf(outEvent), capturingCmdList.appendMemoryFillArgs[0].signalEvent);

    clReleaseEvent(outEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenBlockingMemcpyIntelWhenEnqueuedThenHostSynchronizeFollowsTheCopy) {
    ASSERT_EQ(CL_SUCCESS, clEnqueueMemcpyINTEL(getCommandQueue(), CL_TRUE, usmLikeDst.data(), usmLikeSrc.data(),
                                               usmLikeDst.size(), 0, nullptr, nullptr));

    ASSERT_EQ(2u, capturingCmdList.sequence.size());
    EXPECT_EQ(ApiId::appendMemoryCopy, capturingCmdList.sequence[0]);
    EXPECT_EQ(ApiId::hostSynchronize, capturingCmdList.sequence[1]);
}

TEST_F(EnqueueEventPlumbingFixture, givenNonBlockingMemcpyIntelWhenEnqueuedThenNoHostSynchronizeIsIssued) {
    ASSERT_EQ(CL_SUCCESS, clEnqueueMemcpyINTEL(getCommandQueue(), CL_FALSE, usmLikeDst.data(), usmLikeSrc.data(),
                                               usmLikeDst.size(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.sequence.size());
    EXPECT_EQ(ApiId::appendMemoryCopy, capturingCmdList.sequence[0]);
}

TEST_F(EnqueueEventPlumbingFixture, givenMemsetIntelWhenEnqueuedThenSingleBytePatternIsForwarded) {
    constexpr cl_int value = 0x7Bu;

    ASSERT_EQ(CL_SUCCESS, clEnqueueMemsetINTEL(getCommandQueue(), usmLikeDst.data(), value, usmLikeDst.size(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryFillArgs.count());
    const auto &params = capturingCmdList.appendMemoryFillArgs[0];
    EXPECT_EQ(usmLikeDst.data(), params.ptr);
    EXPECT_EQ(1u, params.patternSize);
    ASSERT_EQ(1u, params.pattern.size());
    EXPECT_EQ(static_cast<uint8_t>(value), params.pattern[0]);
    EXPECT_EQ(usmLikeDst.size(), params.size);
}

TEST_F(EnqueueEventPlumbingFixture, givenMemFillIntelWhenEnqueuedThenFullPatternIsForwarded) {
    const std::array<uint8_t, 4> patternBytes{0x11u, 0x22u, 0x33u, 0x44u};

    ASSERT_EQ(CL_SUCCESS, clEnqueueMemFillINTEL(getCommandQueue(), usmLikeDst.data(), patternBytes.data(),
                                                patternBytes.size(), usmLikeDst.size(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryFillArgs.count());
    const auto &params = capturingCmdList.appendMemoryFillArgs[0];
    EXPECT_EQ(patternBytes.size(), params.patternSize);
    ASSERT_EQ(patternBytes.size(), params.pattern.size());
    for (size_t i = 0; i < patternBytes.size(); i++) {
        EXPECT_EQ(patternBytes[i], params.pattern[i]) << "pattern byte " << i;
    }
}

TEST_F(EnqueueEventPlumbingFixture, givenWaitListWhenEnqueuingBarrierBasedCommandThenHandlesReachAppendBarrier) {
    auto events = createUserEvents(2u);
    cl_event outEvent = nullptr;

    ASSERT_EQ(CL_SUCCESS, clEnqueueMigrateMemObjects(getCommandQueue(), 1, &buffer, 0,
                                                     static_cast<cl_uint>(events.size()), events.data(), &outEvent));

    ASSERT_EQ(1u, capturingCmdList.appendBarrierArgs.count());
    const auto &params = capturingCmdList.appendBarrierArgs[0];
    ASSERT_EQ(events.size(), params.waitEvents.size());
    EXPECT_EQ(l0HandleOf(events[0]), params.waitEvents[0]);
    EXPECT_EQ(l0HandleOf(events[1]), params.waitEvents[1]);
    EXPECT_EQ(l0HandleOf(outEvent), params.signalEvent);

    clReleaseEvent(outEvent);
}

TEST_F(EnqueueEventPlumbingFixture, givenFailingEnqueueWhenOutputEventRequestedThenNoEventIsProduced) {
    cl_event outEvent = nullptr;

    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, nullptr, 0, nullptr, &outEvent));

    EXPECT_EQ(nullptr, outEvent);
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueEventPlumbingFixture, givenSeveralEnqueuesWithEventsWhenInspectingCaptureThenEachSignalEventIsDistinct) {
    cl_event firstEvent = nullptr;
    cl_event secondEvent = nullptr;

    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, &firstEvent));
    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, &secondEvent));

    ASSERT_EQ(2u, capturingCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(l0HandleOf(firstEvent), capturingCmdList.appendMemoryCopyArgs[0].signalEvent);
    EXPECT_EQ(l0HandleOf(secondEvent), capturingCmdList.appendMemoryCopyArgs[1].signalEvent);
    EXPECT_NE(capturingCmdList.appendMemoryCopyArgs[0].signalEvent, capturingCmdList.appendMemoryCopyArgs[1].signalEvent);

    clReleaseEvent(firstEvent);
    clReleaseEvent(secondEvent);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
