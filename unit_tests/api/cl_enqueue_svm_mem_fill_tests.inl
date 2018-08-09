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
