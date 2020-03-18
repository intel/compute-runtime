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
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

#include "cl_api_tests.h"

#include <memory>

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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
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
}

TEST_F(clEnqueueSVMMigrateMemTests, GivenNumSvmPointersIsZeroWhenMigratingSvmThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMMigrateMemTests, GivenDeviceNotSupportingSvmWhenEnqueuingSVMMigrateMemThenInvalidOperationErrorIsReturned) {
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.ftrSvm = false;

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    cl_device_id deviceId = pDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
    auto pCommandQueue = std::make_unique<MockCommandQueue>(pContext.get(), pDevice.get(), nullptr);

    auto retVal = clEnqueueSVMMigrateMem(
        pCommandQueue.get(), // cl_command_queue command_queue
        1,                   // cl_uint num_svm_pointers
        nullptr,             // const void **svm_pointers
        nullptr,             // const size_t *sizes
        0,                   // const cl_mem_migration_flags flags
        0,                   // cl_uint num_events_in_wait_list
        nullptr,             // const cl_event *event_wait_list
        nullptr              // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
