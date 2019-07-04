/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/page_fault_manager/cpu_page_fault_manager_tests_fixture.h"
#include "runtime/command_queue/command_queue.h"

#include "gtest/gtest.h"

using namespace NEO;

struct CommandQueueMock : public CommandQueue {
    cl_int enqueueSVMUnmap(void *svmPtr,
                           cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                           cl_event *event, bool externalAppCall) override {
        transferToGpuCalled++;
        return CL_SUCCESS;
    }
    cl_int enqueueSVMMap(cl_bool blockingMap, cl_map_flags mapFlags,
                         void *svmPtr, size_t size,
                         cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                         cl_event *event, bool externalAppCall) override {
        transferToCpuCalled++;
        passedMapFlags = mapFlags;
        return CL_SUCCESS;
    }
    cl_int finish(bool dcFlush) override {
        finishCalled++;
        return CL_SUCCESS;
    }

    int transferToCpuCalled = 0;
    int transferToGpuCalled = 0;
    int finishCalled = 0;
    uint64_t passedMapFlags = 0;
};

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenSynchronizeMemoryThenEnqueueProperCalls) {
    void *alloc = reinterpret_cast<void *>(0x1);
    auto cmdQ = std::make_unique<CommandQueueMock>();

    pageFaultManager->baseCpuTransfer(alloc, 10, cmdQ.get());
    EXPECT_EQ(cmdQ->transferToCpuCalled, 1);
    EXPECT_EQ(cmdQ->transferToGpuCalled, 0);
    EXPECT_EQ(cmdQ->finishCalled, 0);

    pageFaultManager->baseGpuTransfer(alloc, cmdQ.get());
    EXPECT_EQ(cmdQ->transferToCpuCalled, 1);
    EXPECT_EQ(cmdQ->transferToGpuCalled, 1);
    EXPECT_EQ(cmdQ->finishCalled, 1);
}
