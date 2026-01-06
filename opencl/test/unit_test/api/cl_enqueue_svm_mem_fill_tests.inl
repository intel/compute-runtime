/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueSVMMemFillTests = ApiTests;

namespace ULT {

TEST_F(ClEnqueueSVMMemFillTests, GivenInvalidCommandQueueWhenFillingSVMMemoryThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueSVMMemFill(
        nullptr, // cl_command_queue command_queue
        nullptr, // void *svm_ptr
        nullptr, // const void *pattern
        0,       // size_t pattern_size
        0,       // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_event *event_wait_list
        nullptr  // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClEnqueueSVMMemFillTests, GivenNullSVMPtrWhenFillingSVMMemoryThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_uint pattern = 0;
        auto retVal = clEnqueueSVMMemFill(
            pCommandQueue,   // cl_command_queue command_queue
            nullptr,         // void *svm_ptr
            &pattern,        // const void *pattern
            sizeof(pattern), // size_t pattern_size
            256,             // size_t size
            0,               // cl_uint num_events_in_wait_list
            nullptr,         // cl_event *event_wait_list
            nullptr          // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);
    }
}

TEST_F(ClEnqueueSVMMemFillTests, GivenRegionSizeZeroWhenFillingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        cl_uint pattern = 0;
        auto retVal = clEnqueueSVMMemFill(
            pCommandQueue,   // cl_command_queue command_queue
            ptrSvm,          // void *svm_ptr
            &pattern,        // const void *pattern
            sizeof(pattern), // size_t pattern_size
            0,               // size_t size
            0,               // cl_uint num_events_in_wait_list
            nullptr,         // cl_event *event_wait_list
            nullptr          // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(ClEnqueueSVMMemFillTests, GivenNullSVMPtrWithRegionSizeZeroWhenFillingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_uint pattern = 0;
        auto retVal = clEnqueueSVMMemFill(
            pCommandQueue,   // cl_command_queue command_queue
            nullptr,         // void *svm_ptr
            &pattern,        // const void *pattern
            sizeof(pattern), // size_t pattern_size
            0,               // size_t size
            0,               // cl_uint num_events_in_wait_list
            nullptr,         // cl_event *event_wait_list
            nullptr          // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ClEnqueueSVMMemFillTests, GivenNullEventWaitListAndNonZeroEventsWhenFillingSVMMemoryThenInvalidEventWaitListIsReturned) {
    cl_uint pattern = 0;
    auto retVal = clEnqueueSVMMemFill(
        pCommandQueue,   // cl_command_queue command_queue
        nullptr,         // void *svm_ptr
        &pattern,        // const void *pattern
        sizeof(pattern), // size_t pattern_size
        0,               // size_t size
        1,               // cl_uint num_events_in_wait_list
        nullptr,         // cl_event *event_wait_list
        nullptr          // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(ClEnqueueSVMMemFillTests, GivenNonNullEventWaitListAndZeroEventsWhenFillingSVMMemoryThenInvalidEventWaitListIsReturned) {
    cl_uint pattern = 0;
    UserEvent uEvent(pContext);
    cl_event eventWaitList[] = {&uEvent};
    auto retVal = clEnqueueSVMMemFill(
        pCommandQueue,   // cl_command_queue command_queue
        nullptr,         // void *svm_ptr
        &pattern,        // const void *pattern
        sizeof(pattern), // size_t pattern_size
        0,               // size_t size
        0,               // cl_uint num_events_in_wait_list
        eventWaitList,   // cl_event *event_wait_list
        nullptr          // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(ClEnqueueSVMMemFillTests, GivenValidParametersWhenFillingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        cl_uint pattern = 0;
        auto retVal = clEnqueueSVMMemFill(
            pCommandQueue,   // cl_command_queue command_queue
            ptrSvm,          // void *svm_ptr
            &pattern,        // const void *pattern
            sizeof(pattern), // size_t pattern_size
            256,             // size_t size
            0,               // cl_uint num_events_in_wait_list
            nullptr,         // cl_event *event_wait_list
            nullptr          // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(ClEnqueueSVMMemFillTests, GivenQueueIncapableWhenFillingSvmBufferThenInvalidOperationIsReturned) {

    disableQueueCapabilities(CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL);

    void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
    EXPECT_NE(nullptr, ptrSvm);

    cl_uint pattern = 0;
    auto retVal = clEnqueueSVMMemFill(
        pCommandQueue,   // cl_command_queue command_queue
        ptrSvm,          // void *svm_ptr
        &pattern,        // const void *pattern
        sizeof(pattern), // size_t pattern_size
        256,             // size_t size
        0,               // cl_uint num_events_in_wait_list
        nullptr,         // cl_event *event_wait_list
        nullptr          // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    clSVMFree(pContext, ptrSvm);
}

} // namespace ULT
