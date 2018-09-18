/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"

using namespace OCLRT;

typedef api_tests clEnqueueSVMMemFillTests;

namespace ULT {

TEST_F(clEnqueueSVMMemFillTests, invalidCommandQueue) {
    auto retVal = clEnqueueSVMMemFill(
        nullptr, // cl_command_queue command_queue
        nullptr, // void *svm_ptr
        nullptr, // const void *pattern
        0,       // size_t pattern_size
        0,       // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr  // cL_event *event
    );
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueSVMMemFillTests, invalidValue_SvmPtrIsNull) {
    auto retVal = clEnqueueSVMMemFill(
        pCommandQueue, // cl_command_queue command_queue
        nullptr,       // void *svm_ptr
        nullptr,       // const void *pattern
        0,             // size_t pattern_size
        256,           // size_t size
        0,             // cl_uint num_events_in_wait_list
        nullptr,       // cl_evebt *event_wait_list
        nullptr        // cL_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueSVMMemFillTests, invalidValue_SizeIsZero) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clEnqueueSVMMemFill(
            pCommandQueue, // cl_command_queue command_queue
            ptrSvm,        // void *svm_ptr
            nullptr,       // const void *pattern
            0,             // size_t pattern_size
            0,             // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // cl_evebt *event_wait_list
            nullptr        // cL_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMMemFillTests, invalidEventWaitList_EventWaitListIsNullAndNumEventsInWaitListIsGreaterThanZero) {
    auto retVal = clEnqueueSVMMemFill(
        pCommandQueue, // cl_command_queue command_queue
        nullptr,       // void *svm_ptr
        nullptr,       // const void *pattern
        0,             // size_t pattern_size
        0,             // size_t size
        1,             // cl_uint num_events_in_wait_list
        nullptr,       // cl_evebt *event_wait_list
        nullptr        // cL_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMMemFillTests, invalidEventWaitList_EventWaitListIsNotNullAndNumEventsInWaitListIsZero) {
    UserEvent uEvent(pContext);
    cl_event eventWaitList[] = {&uEvent};
    auto retVal = clEnqueueSVMMemFill(
        pCommandQueue, // cl_command_queue command_queue
        nullptr,       // void *svm_ptr
        nullptr,       // const void *pattern
        0,             // size_t pattern_size
        0,             // size_t size
        0,             // cl_uint num_events_in_wait_list
        eventWaitList, // cl_evebt *event_wait_list
        nullptr        // cL_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMMemFillTests, success) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clEnqueueSVMMemFill(
            pCommandQueue, // cl_command_queue command_queue
            ptrSvm,        // void *svm_ptr
            nullptr,       // const void *pattern
            0,             // size_t pattern_size
            256,           // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // cl_evebt *event_wait_list
            nullptr        // cL_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}
} // namespace ULT
