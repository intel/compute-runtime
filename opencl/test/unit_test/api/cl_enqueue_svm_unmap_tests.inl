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

using ClEnqueueSVMUnmapTests = ApiTests;

namespace ULT {

TEST_F(ClEnqueueSVMUnmapTests, GivenInvalidCommandQueueWhenUnmappingSvmThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueSVMUnmap(
        nullptr, // cl_command_queue  command_queue
        nullptr, // void *svm_ptr
        0,       // cl_uint num_events_in_wait_list
        nullptr, // const cl_event *event_wait_list
        nullptr  // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClEnqueueSVMUnmapTests, GivenNullSvmPtrWhenUnmappingSvmThenInvalidValueErrorIsReturned) {
    auto retVal = clEnqueueSVMUnmap(
        pCommandQueue, // cl_command_queue  command_queue
        nullptr,       // void *svm_ptr
        0,             // cl_uint num_events_in_wait_list
        nullptr,       // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClEnqueueSVMUnmapTests, GivenNullEventListAndNonZeroEventsWhenUnmappingSvmThenInvalidEventWaitListErrorIsReturned) {
    auto retVal = clEnqueueSVMUnmap(
        pCommandQueue, // cl_command_queue  command_queue
        nullptr,       // void *svm_ptr
        1,             // cl_uint num_events_in_wait_list
        nullptr,       // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(ClEnqueueSVMUnmapTests, GivenNonNullEventListAndZeroEventsWhenUnmappingSvmThenInvalidEventWaitListErrorIsReturned) {
    UserEvent uEvent(pContext);
    cl_event eventWaitList[] = {&uEvent};
    auto retVal = clEnqueueSVMUnmap(
        pCommandQueue, // cl_command_queue  command_queue
        nullptr,       // void *svm_ptr
        0,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(ClEnqueueSVMUnmapTests, GivenValidParametersWhenUnmappingSvmThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clEnqueueSVMMap(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_map
            CL_MAP_READ,   // cl_map_flags map_flags
            ptrSvm,        // void *svm_ptr
            256,           // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cL_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clEnqueueSVMUnmap(
            pCommandQueue, // cl_command_queue command_queue
            ptrSvm,        // void *svm_ptr
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cL_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(ClEnqueueSVMUnmapTests, GivenQueueIncapableWhenUnmappingSvmBufferThenInvalidOperationIsReturned) {
    REQUIRE_SVM_OR_SKIP(pDevice);

    void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
    EXPECT_NE(nullptr, ptrSvm);

    auto retVal = clEnqueueSVMMap(
        pCommandQueue, // cl_command_queue command_queue
        CL_FALSE,      // cl_bool blocking_map
        CL_MAP_READ,   // cl_map_flags map_flags
        ptrSvm,        // void *svm_ptr
        256,           // size_t size
        0,             // cl_uint num_events_in_wait_list
        nullptr,       // const cL_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    disableQueueCapabilities(CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL);

    retVal = clEnqueueSVMUnmap(
        pCommandQueue, // cl_command_queue command_queue
        ptrSvm,        // void *svm_ptr
        0,             // cl_uint num_events_in_wait_list
        nullptr,       // const cL_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    clSVMFree(pContext, ptrSvm);
}

} // namespace ULT
