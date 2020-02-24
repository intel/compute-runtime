/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueSVMMigrateMemTests;

namespace ULT {

TEST_F(clEnqueueSVMMigrateMemTests, GivenInvalidCommandQueueWhenMigratingSVMThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueSVMMigrateMem(
        nullptr, // cl_command_queue command_queue
        0,       // cl_uint num_svm_pointers
        nullptr, // const void **svm_pointers
        nullptr, // const size_t *sizes
        0,       // const cl_mem_migration_flags flags
        0,       // cl_uint num_events_in_wait_list
        nullptr, // const cl_event *event_wait_list
        nullptr  // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenNullSvmPointersWhenMigratingSvmThenInvalidValueErrorIsReturned) {
    auto retVal = clEnqueueSVMMigrateMem(
        pCommandQueue, // cl_command_queue command_queue
        1,             // cl_uint num_svm_pointers
        nullptr,       // const void **svm_pointers
        nullptr,       // const size_t *sizes
        0,             // const cl_mem_migration_flags flags
        0,             // cl_uint num_events_in_wait_list
        nullptr,       // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenNumSvmPointersIsZeroWhenMigratingSvmThenInvalidValueErrorIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        const void *svmPtrs[] = {ptrSvm};
        auto retVal = clEnqueueSVMMigrateMem(
            pCommandQueue, // cl_command_queue command_queue
            0,             // cl_uint num_svm_pointers
            svmPtrs,       // const void **svm_pointers
            nullptr,       // const size_t *sizes
            0,             // const cl_mem_migration_flags flags
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenSvmPointerIsHostPtrWhenMigratingSvmThenInvalidValueErrorIsReturned) {
    char *ptrHost = new char[10];
    ASSERT_NE(nullptr, ptrHost);

    const void *svmPtrs[] = {ptrHost};
    auto retVal = clEnqueueSVMMigrateMem(
        pCommandQueue, // cl_command_queue command_queue
        1,             // cl_uint num_svm_pointers
        svmPtrs,       // const void **svm_pointers
        nullptr,       // const size_t *sizes
        0,             // const cl_mem_migration_flags flags
        0,             // cl_uint num_events_in_wait_list
        nullptr,       // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete[] ptrHost;
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenNonZeroSizeIsNotContainedWithinAllocationWhenMigratingSvmThenInvalidValueErrorIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(ptrSvm);
        ASSERT_NE(nullptr, svmData);
        auto svmAlloc = svmData->gpuAllocation;
        EXPECT_NE(nullptr, svmAlloc);
        size_t allocSize = svmAlloc->getUnderlyingBufferSize();

        const void *svmPtrs[] = {ptrSvm};
        const size_t sizes[] = {allocSize + 1};
        auto retVal = clEnqueueSVMMigrateMem(
            pCommandQueue, // cl_command_queue command_queue
            1,             // cl_uint num_svm_pointers
            svmPtrs,       // const void **svm_pointers
            sizes,         // const size_t *sizes
            0,             // const cl_mem_migration_flags flags
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenUnsupportedFlagsWhenMigratingSvmThenInvalidValueErrorIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        const void *svmPtrs[] = {ptrSvm};
        auto retVal = clEnqueueSVMMigrateMem(
            pCommandQueue,      // cl_command_queue command_queue
            1,                  // cl_uint num_svm_pointers
            svmPtrs,            // const void **svm_pointers
            nullptr,            // const size_t *sizes
            0xAA55AA55AA55AA55, // const cl_mem_migration_flags flags
            0,                  // cl_uint num_events_in_wait_list
            nullptr,            // const cl_event *event_wait_list
            nullptr             // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenNullEventWaitListAndNonZeroNumEventsWhenMigratingSvmThenInvalidEventWaitListErrorIsReturned) {
    auto retVal = clEnqueueSVMMigrateMem(
        pCommandQueue, // cl_command_queue command_queue
        0,             // cl_uint num_svm_pointers
        nullptr,       // const void **svm_pointers
        nullptr,       // const size_t *sizes
        0,             // const cl_mem_migration_flags flags
        1,             // cl_uint num_events_in_wait_list
        nullptr,       // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenNonNullEventWaitListAndZeroNumEventsWhenMigratingSvmThenInvalidEventWaitListErrorIsReturned) {
    UserEvent uEvent(pContext);
    cl_event eventWaitList[] = {&uEvent};
    auto retVal = clEnqueueSVMMigrateMem(
        pCommandQueue, // cl_command_queue command_queue
        0,             // cl_uint num_svm_pointers
        nullptr,       // const void **svm_pointers
        nullptr,       // const size_t *sizes
        0,             // const cl_mem_migration_flags flags
        0,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenDifferentContextCommandQueueAndEventsWhenMigratingSvmThenInvalidContextErrorIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        MockContext mockContext;
        UserEvent uEvent(&mockContext);
        cl_event eventWaitList[] = {&uEvent};
        const void *svmPtrs[] = {ptrSvm};
        auto retVal = clEnqueueSVMMigrateMem(
            pCommandQueue, // cl_command_queue command_queue
            1,             // cl_uint num_svm_pointers
            svmPtrs,       // const void **svm_pointers
            nullptr,       // const size_t *sizes
            0,             // const cl_mem_migration_flags flags
            1,             // cl_uint num_events_in_wait_list
            eventWaitList, // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_CONTEXT, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenNullSizesWhenMigratingSvmThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        const void *svmPtrs[] = {ptrSvm};
        auto retVal = clEnqueueSVMMigrateMem(
            pCommandQueue, // cl_command_queue command_queue
            1,             // cl_uint num_svm_pointers
            svmPtrs,       // const void **svm_pointers
            nullptr,       // const size_t *sizes
            0,             // const cl_mem_migration_flags flags
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenSizeZeroWhenMigratingSvmThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        const void *svmPtrs[] = {ptrSvm};
        const size_t sizes[] = {0};
        auto retVal = clEnqueueSVMMigrateMem(
            pCommandQueue, // cl_command_queue command_queue
            1,             // cl_uint num_svm_pointers
            svmPtrs,       // const void **svm_pointers
            sizes,         // const size_t *sizes
            0,             // const cl_mem_migration_flags  flags
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenNonZeroSizeWhenMigratingSvmThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        const void *svmPtrs[] = {ptrSvm};
        const size_t sizes[] = {256};
        auto retVal = clEnqueueSVMMigrateMem(
            pCommandQueue, // cl_command_queue command_queue
            1,             // cl_uint num_svm_pointers
            svmPtrs,       // const void **svm_pointers
            sizes,         // const size_t *sizes
            0,             // const cl_mem_migration_flags  flags
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenSameContextCommandQueueAndEventsWhenMigratingSvmThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        UserEvent uEvent(pContext);
        cl_event eventWaitList[] = {&uEvent};
        const void *svmPtrs[] = {ptrSvm};
        auto retVal = clEnqueueSVMMigrateMem(
            pCommandQueue, // cl_command_queue command_queue
            1,             // cl_uint num_svm_pointers
            svmPtrs,       // const void **svm_pointers
            nullptr,       // const size_t *sizes
            0,             // const cl_mem_migration_flags flags
            1,             // cl_uint num_events_in_wait_list
            eventWaitList, // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}
} // namespace ULT
