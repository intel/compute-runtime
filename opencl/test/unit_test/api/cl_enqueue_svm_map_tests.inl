/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueSVMMapTests;

namespace ULT {

TEST_F(clEnqueueSVMMapTests, GivenInvalidCommandQueueWhenMappingSVMThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueSVMMap(
        nullptr,     // cl_command_queue command_queue
        CL_FALSE,    // cl_bool blocking_map
        CL_MAP_READ, // cl_map_flags map_flags
        nullptr,     // void *svm_ptr
        0,           // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // const cL_event *event_wait_list
        nullptr      // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueSVMMapTests, GivenNullSVMPointerWhenMappingSVMThenInvalidValueErrorIsReturned) {
    auto retVal = clEnqueueSVMMap(
        pCommandQueue, // cl_command_queue command_queue
        CL_FALSE,      // cl_bool blocking_map
        CL_MAP_READ,   // cl_map_flags map_flags
        nullptr,       // void *svm_ptr
        256,           // size_t size
        0,             // cl_uint num_events_in_wait_list
        nullptr,       // const cL_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueSVMMapTests, GivenRegionSizeZeroWhenMappingSVMThenInvalidValueErrorIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clEnqueueSVMMap(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_map
            CL_MAP_READ,   // cl_map_flags map_flags
            ptrSvm,        // void *svm_ptr
            0,             // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cL_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMapTests, GivenNullEventWaitListAndNonZeroNumEventsWhenMappingSVMThenInvalidEventWaitListErrorIsReturned) {
    auto retVal = clEnqueueSVMMap(
        pCommandQueue, // cl_command_queue command_queue
        CL_FALSE,      // cl_bool blocking_map
        CL_MAP_READ,   // cl_map_flags map_flags
        nullptr,       // void *svm_ptr
        0,             // size_t size
        1,             // cl_uint num_events_in_wait_list
        nullptr,       // const cL_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMMapTests, GivenNonNullEventWaitListAndZeroNumEventsWhenMappingSVMThenInvalidEventWaitListErrorIsReturned) {
    UserEvent uEvent(pContext);
    cl_event eventWaitList[] = {&uEvent};
    auto retVal = clEnqueueSVMMap(
        pCommandQueue, // cl_command_queue command_queue
        CL_FALSE,      // cl_bool blocking_map
        CL_MAP_READ,   // cl_map_flags map_flags
        nullptr,       // void *svm_ptr
        0,             // size_t size
        0,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cL_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMMapTests, GivenValidParametersWhenMappingSVMThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
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

        clSVMFree(pContext, ptrSvm);
    }
}
} // namespace ULT
