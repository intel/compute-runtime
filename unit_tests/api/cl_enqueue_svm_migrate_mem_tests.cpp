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

#include "cl_api_tests.h"
#include "unit_tests/mocks/mock_context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device/device.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/svm_memory_manager.h"

using namespace OCLRT;

typedef api_tests clEnqueueSVMMigrateMemTests;

namespace ULT {

TEST_F(clEnqueueSVMMigrateMemTests, invalidCommandQueue) {
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

TEST_F(clEnqueueSVMMigrateMemTests, invalidValue_SvmPointersAreNull) {
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

TEST_F(clEnqueueSVMMigrateMemTests, invalidValue_NumSvmPointersIsZero) {
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

TEST_F(clEnqueueSVMMigrateMemTests, invalidValue_SvmPointerIsHostPtr) {
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

TEST_F(clEnqueueSVMMigrateMemTests, invalidValue_NonZeroSizeIsNotContainedWithinAllocation) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        ASSERT_NE(nullptr, ptrSvm);

        auto svmAlloc = pContext->getSVMAllocsManager()->getSVMAlloc(ptrSvm);
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

TEST_F(clEnqueueSVMMigrateMemTests, invalidValue_FlagsAreNeitherZeroNorSupported) {
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

TEST_F(clEnqueueSVMMigrateMemTests, invalidEventWaitList_EventWaitListIsNullAndNumEventsInWaitListIsGreaterThanZero) {
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

TEST_F(clEnqueueSVMMigrateMemTests, invalidEventWaitList_EventWaitListIsNotNullAndNumEventsInWaitListIsZero) {
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

TEST_F(clEnqueueSVMMigrateMemTests, invalidEventWaitList_CommandQueueAndEventsContextsAreNotTheSame) {
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

TEST_F(clEnqueueSVMMigrateMemTests, success_SizesAreNull) {
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

TEST_F(clEnqueueSVMMigrateMemTests, success_SizesContainZeroSize) {
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

TEST_F(clEnqueueSVMMigrateMemTests, success_SizesContainNonZeroSize) {
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

TEST_F(clEnqueueSVMMigrateMemTests, success_CommandQueueAndEventsContextsAreTheSame) {
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
