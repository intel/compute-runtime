/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
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

struct EnqueueValidationFixture : public Test<LeoCaptureFixture> {
    void SetUp() override {
        Test<LeoCaptureFixture>::SetUp();
        buffer = createBuffer(bufferSize);
        ASSERT_NE(nullptr, buffer);
    }

    void TearDown() override {
        for (auto event : ownedEvents) {
            clReleaseEvent(event);
        }
        if (foreignContext != nullptr) {
            clReleaseContext(foreignContext);
        }
        if (buffer != nullptr) {
            clReleaseMemObject(buffer);
        }
        Test<LeoCaptureFixture>::TearDown();
    }

    cl_context getForeignContext() {
        if (foreignContext == nullptr) {
            cl_int errcode = CL_SUCCESS;
            cl_device_id clDeviceId = clDevice;
            foreignContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
            EXPECT_EQ(CL_SUCCESS, errcode);
        }
        return foreignContext;
    }

    cl_event createUserEventIn(cl_context context) {
        cl_int errcode = CL_SUCCESS;
        auto event = clCreateUserEvent(context, &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        ownedEvents.push_back(event);
        return event;
    }

    static constexpr size_t bufferSize = 128u;

    cl_mem buffer = nullptr;
    cl_context foreignContext = nullptr;
    std::vector<cl_event> ownedEvents;
    std::array<uint8_t, bufferSize> hostData{};
    uint32_t pattern = 0u;
};

TEST_F(EnqueueValidationFixture, givenZeroMemObjectsWhenMigrateMemObjectsThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMigrateMemObjects(getCommandQueue(), 0, nullptr, 0, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNullMemObjectListWithNonZeroCountWhenMigrateMemObjectsThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMigrateMemObjects(getCommandQueue(), 1, nullptr, 0, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNonNullMemObjectListWithZeroCountWhenMigrateMemObjectsThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMigrateMemObjects(getCommandQueue(), 0, &buffer, 0, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenInvalidMemObjectInListWhenMigrateMemObjectsThenReturnsInvalidMemObject) {
    cl_mem memObjects[] = {buffer, nullptr};
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueMigrateMemObjects(getCommandQueue(), 2, memObjects, 0, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenUnknownMigrationFlagWhenMigrateMemObjectsThenReturnsInvalidValue) {
    const cl_mem_migration_flags unknownFlag = 1u << 4;
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMigrateMemObjects(getCommandQueue(), 1, &buffer, unknownFlag, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenSupportedMigrationFlagsWhenMigrateMemObjectsThenBarrierIsAppended) {
    const cl_mem_migration_flags accepted[] = {
        0,
        CL_MIGRATE_MEM_OBJECT_HOST,
        CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED,
        CL_MIGRATE_MEM_OBJECT_HOST | CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED};

    for (auto flags : accepted) {
        capturingCmdList.clearCaptures();
        EXPECT_EQ(CL_SUCCESS, clEnqueueMigrateMemObjects(getCommandQueue(), 1, &buffer, flags, 0, nullptr, nullptr))
            << "rejected flags " << flags;
        EXPECT_EQ(1u, capturingCmdList.appendBarrierArgs.count()) << "flags " << flags;
    }
}

TEST_F(EnqueueValidationFixture, givenValidFlagsCombinedWithUnknownBitWhenMigrateMemObjectsThenReturnsInvalidValue) {
    const cl_mem_migration_flags flags = CL_MIGRATE_MEM_OBJECT_HOST | (1u << 5);
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMigrateMemObjects(getCommandQueue(), 1, &buffer, flags, 0, nullptr, nullptr));
}

TEST_F(EnqueueValidationFixture, givenNonZeroCountAndNullWaitListWhenMarkerWithWaitListThenReturnsInvalidEventWaitList) {
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, clEnqueueMarkerWithWaitList(getCommandQueue(), 1, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenZeroCountAndNonNullWaitListWhenMarkerWithWaitListThenReturnsInvalidEventWaitList) {
    auto event = createUserEventIn(clContext);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, clEnqueueMarkerWithWaitList(getCommandQueue(), 0, &event, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenForeignContextEventWhenMarkerWithWaitListThenReturnsInvalidContext) {
    auto foreignEvent = createUserEventIn(getForeignContext());

    EXPECT_EQ(CL_INVALID_CONTEXT, clEnqueueMarkerWithWaitList(getCommandQueue(), 1, &foreignEvent, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenSameContextEventWhenMarkerWithWaitListThenBarrierIsAppended) {
    auto event = createUserEventIn(clContext);

    EXPECT_EQ(CL_SUCCESS, clEnqueueMarkerWithWaitList(getCommandQueue(), 1, &event, nullptr));
    ASSERT_EQ(1u, capturingCmdList.appendBarrierArgs.count());
    ASSERT_EQ(1u, capturingCmdList.appendBarrierArgs[0].waitEvents.size());
    EXPECT_EQ(castToObject<Event>(event)->getL0Handle(), capturingCmdList.appendBarrierArgs[0].waitEvents[0]);
}

TEST_F(EnqueueValidationFixture, givenForeignContextEventWhenBarrierWithWaitListThenReturnsInvalidContext) {
    auto foreignEvent = createUserEventIn(getForeignContext());

    EXPECT_EQ(CL_INVALID_CONTEXT, clEnqueueBarrierWithWaitList(getCommandQueue(), 1, &foreignEvent, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenMixedContextEventsWhenBarrierWithWaitListThenReturnsInvalidContext) {
    cl_event events[] = {createUserEventIn(clContext), createUserEventIn(getForeignContext())};

    EXPECT_EQ(CL_INVALID_CONTEXT, clEnqueueBarrierWithWaitList(getCommandQueue(), 2, events, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenInvalidEventInWaitListWhenBarrierWithWaitListThenReturnsInvalidEventWaitList) {
    cl_event events[] = {nullptr};

    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, clEnqueueBarrierWithWaitList(getCommandQueue(), 1, events, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenPlainMarkerWhenEnqueuedThenBarrierIsAppendedWithoutWaitEvents) {
    cl_event outEvent = nullptr;

    EXPECT_EQ(CL_SUCCESS, clEnqueueMarker(getCommandQueue(), &outEvent));

    ASSERT_EQ(1u, capturingCmdList.appendBarrierArgs.count());
    EXPECT_TRUE(capturingCmdList.appendBarrierArgs[0].waitEvents.empty());
    ASSERT_NE(nullptr, outEvent);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), castToObject<Event>(outEvent)->getCommandType());

    clReleaseEvent(outEvent);
}

TEST_F(EnqueueValidationFixture, givenPlainBarrierWhenEnqueuedThenBarrierIsAppendedWithoutSignalEvent) {
    EXPECT_EQ(CL_SUCCESS, clEnqueueBarrier(getCommandQueue()));

    ASSERT_EQ(1u, capturingCmdList.appendBarrierArgs.count());
    EXPECT_TRUE(capturingCmdList.appendBarrierArgs[0].waitEvents.empty());
    EXPECT_EQ(nullptr, capturingCmdList.appendBarrierArgs[0].signalEvent);
}

TEST_F(EnqueueValidationFixture, givenTerminatedContextWhenWaitForEventsThenReturnsExecStatusErrorForEventsInWaitList) {
    auto event = createUserEventIn(clContext);
    context->terminateExecution();

    EXPECT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, clEnqueueWaitForEvents(getCommandQueue(), 1, &event));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenInvalidEventWhenWaitForEventsThenReturnsInvalidEventWaitList) {
    cl_event events[] = {nullptr};
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, clEnqueueWaitForEvents(getCommandQueue(), 1, events));
}

TEST_F(EnqueueValidationFixture, givenNullQueueWhenWaitForEventsThenReturnsInvalidCommandQueue) {
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueWaitForEvents(nullptr, 0, nullptr));
}

TEST_F(EnqueueValidationFixture, givenZeroComparisonSizeWhenVerifyMemoryIntelThenArgumentsAreCheckedBeforeQueue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueVerifyMemoryINTEL(nullptr, hostData.data(), hostData.data(), 0, 0));
}

TEST_F(EnqueueValidationFixture, givenNullExpectedDataWhenVerifyMemoryIntelThenArgumentsAreCheckedBeforeQueue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueVerifyMemoryINTEL(nullptr, hostData.data(), nullptr, hostData.size(), 0));
}

TEST_F(EnqueueValidationFixture, givenNullAllocationWhenVerifyMemoryIntelThenArgumentsAreCheckedBeforeQueue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueVerifyMemoryINTEL(nullptr, nullptr, hostData.data(), hostData.size(), 0));
}

TEST_F(EnqueueValidationFixture, givenValidArgumentsAndNullQueueWhenVerifyMemoryIntelThenReturnsInvalidCommandQueue) {
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueVerifyMemoryINTEL(nullptr, hostData.data(), hostData.data(), hostData.size(), 0));
}

TEST_F(EnqueueValidationFixture, givenAnyArgumentsWhenNativeKernelEnqueuedThenReturnsInvalidOperation) {
    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueNativeKernel(getCommandQueue(), nullptr, nullptr, 0, 0, nullptr, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNullKernelWhenTaskEnqueuedThenDelegationReportsInvalidKernel) {
    EXPECT_EQ(CL_INVALID_KERNEL, clEnqueueTask(getCommandQueue(), nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenHostWriteOnlyBufferWhenReadBufferEnqueuedThenReturnsInvalidOperation) {
    auto restricted = createBuffer(bufferSize, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY);

    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueReadBuffer(getCommandQueue(), restricted, CL_FALSE, 0, bufferSize,
                                                        hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());

    clReleaseMemObject(restricted);
}

TEST_F(EnqueueValidationFixture, givenHostReadOnlyBufferWhenWriteBufferEnqueuedThenReturnsInvalidOperation) {
    auto restricted = createBuffer(bufferSize, CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY);

    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueWriteBuffer(getCommandQueue(), restricted, CL_FALSE, 0, bufferSize,
                                                         hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());

    clReleaseMemObject(restricted);
}

TEST_F(EnqueueValidationFixture, givenHostNoAccessBufferWhenReadAndWriteBufferEnqueuedThenBothReturnInvalidOperation) {
    auto restricted = createBuffer(bufferSize, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS);

    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueReadBuffer(getCommandQueue(), restricted, CL_FALSE, 0, bufferSize,
                                                        hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueWriteBuffer(getCommandQueue(), restricted, CL_FALSE, 0, bufferSize,
                                                         hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());

    clReleaseMemObject(restricted);
}

TEST_F(EnqueueValidationFixture, givenHostReadOnlyBufferWhenReadBufferEnqueuedThenReadIsStillAllowed) {
    auto restricted = createBuffer(bufferSize, CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY);

    EXPECT_EQ(CL_SUCCESS, clEnqueueReadBuffer(getCommandQueue(), restricted, CL_FALSE, 0, bufferSize,
                                              hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());

    clReleaseMemObject(restricted);
}

TEST_F(EnqueueValidationFixture, givenNullDestinationWhenMemsetIntelThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMemsetINTEL(getCommandQueue(), nullptr, 0, 16u, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNullPatternWhenMemFillIntelThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMemFillINTEL(getCommandQueue(), hostData.data(), nullptr, sizeof(pattern),
                                                      hostData.size(), 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNullDestinationWhenMemFillIntelThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMemFillINTEL(getCommandQueue(), nullptr, &pattern, sizeof(pattern),
                                                      hostData.size(), 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNullPointersWhenMemcpyIntelThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMemcpyINTEL(getCommandQueue(), CL_FALSE, nullptr, hostData.data(), 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueMemcpyINTEL(getCommandQueue(), CL_FALSE, hostData.data(), nullptr, 16u, 0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNullPatternWhenFillBufferEnqueuedThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueFillBuffer(getCommandQueue(), buffer, nullptr, sizeof(pattern), 0, bufferSize,
                                                    0, nullptr, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenInvalidWaitListWhenEnqueuingBufferCommandsThenNothingIsAppended) {
    cl_event invalidEvents[] = {nullptr};

    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, clEnqueueReadBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize,
                                                              hostData.data(), 1, invalidEvents, nullptr));
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize,
                                                               hostData.data(), 1, invalidEvents, nullptr));
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, clEnqueueFillBuffer(getCommandQueue(), buffer, &pattern, sizeof(pattern), 0,
                                                              bufferSize, 1, invalidEvents, nullptr));
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, clEnqueueCopyBuffer(getCommandQueue(), buffer, buffer, 0, 0, 16u,
                                                              1, invalidEvents, nullptr));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNullCommandQueueWhenEnqueuingThenEveryEntryPointReportsInvalidCommandQueue) {
    size_t origin[3] = {0u, 0u, 0u};
    size_t region[3] = {1u, 1u, 1u};

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueReadBuffer(nullptr, buffer, CL_FALSE, 0, 16u, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueWriteBuffer(nullptr, buffer, CL_FALSE, 0, 16u, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueCopyBuffer(nullptr, buffer, buffer, 0, 0, 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueFillBuffer(nullptr, buffer, &pattern, sizeof(pattern), 0, 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueReadBufferRect(nullptr, buffer, CL_FALSE, origin, origin, region, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueWriteBufferRect(nullptr, buffer, CL_FALSE, origin, origin, region, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueCopyBufferRect(nullptr, buffer, buffer, origin, origin, region, 0, 0, 0, 0, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueMigrateMemObjects(nullptr, 1, &buffer, 0, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueMarkerWithWaitList(nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueBarrierWithWaitList(nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueMarker(nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueBarrier(nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueMemsetINTEL(nullptr, hostData.data(), 0, 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueMemFillINTEL(nullptr, hostData.data(), &pattern, sizeof(pattern), 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueMemcpyINTEL(nullptr, CL_FALSE, hostData.data(), hostData.data(), 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueMigrateMemINTEL(nullptr, hostData.data(), 16u, 0, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueMemAdviseINTEL(nullptr, hostData.data(), 16u, 0, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueSVMMemcpy(nullptr, CL_FALSE, hostData.data(), hostData.data(), 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueSVMMemFill(nullptr, hostData.data(), &pattern, sizeof(pattern), 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueSVMMigrateMem(nullptr, 0, nullptr, nullptr, 0, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueTask(nullptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueValidationFixture, givenNullBufferWhenEnqueuingBufferCommandsThenInvalidMemObjectIsReported) {
    size_t origin[3] = {0u, 0u, 0u};
    size_t region[3] = {1u, 1u, 1u};

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueReadBuffer(getCommandQueue(), nullptr, CL_FALSE, 0, 16u, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueWriteBuffer(getCommandQueue(), nullptr, CL_FALSE, 0, 16u, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueFillBuffer(getCommandQueue(), nullptr, &pattern, sizeof(pattern), 0, 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueCopyBuffer(getCommandQueue(), nullptr, buffer, 0, 0, 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueCopyBuffer(getCommandQueue(), buffer, nullptr, 0, 0, 16u, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueReadBufferRect(getCommandQueue(), nullptr, CL_FALSE, origin, origin, region, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueWriteBufferRect(getCommandQueue(), nullptr, CL_FALSE, origin, origin, region, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
