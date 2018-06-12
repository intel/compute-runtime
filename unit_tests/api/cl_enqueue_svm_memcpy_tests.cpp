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
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device/device.h"

using namespace OCLRT;

typedef api_tests clEnqueueSVMMemcpyTests;

namespace ULT {

TEST_F(clEnqueueSVMMemcpyTests, invalidCommandQueue) {
    auto retVal = clEnqueueSVMMemcpy(
        nullptr,  // cl_command_queue command_queue
        CL_FALSE, // cl_bool blocking_copy
        nullptr,  // void *dst_ptr
        nullptr,  // const void *src_ptr
        0,        // size_t size
        0,        // cl_uint num_events_in_wait_list
        nullptr,  // const cl_event *event_wait_list
        nullptr   // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueSVMMemcpyTests, invalidValueDstPtrIsNull) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *pSrcSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pSrcSvm);

        auto retVal = clEnqueueSVMMemcpy(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_copy
            nullptr,       // void *dst_ptr
            pSrcSvm,       // const void *src_ptr
            256,           // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        clSVMFree(pContext, pSrcSvm);
    }
}

TEST_F(clEnqueueSVMMemcpyTests, invalidValueSrcPtrIsNull) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *pDstSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pDstSvm);

        auto retVal = clEnqueueSVMMemcpy(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_copy
            pDstSvm,       // void *dst_ptr
            nullptr,       // const void *src_ptr
            256,           // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        clSVMFree(pContext, pDstSvm);
    }
}

TEST_F(clEnqueueSVMMemcpyTests, invalidEventWaitListEventWaitListIsNullAndNumEventsInWaitListIsGreaterThanZero) {
    auto retVal = clEnqueueSVMMemcpy(
        pCommandQueue, // cl_command_queue command_queue
        CL_FALSE,      // cl_bool blocking_copy
        nullptr,       // void *dst_ptr
        nullptr,       // const void *src_ptr
        0,             // size_t size
        1,             // cl_uint num_events_in_wait_list
        nullptr,       // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMMemcpyTests, invalidEventWaitListEventWaitListIsNotNullAndNumEventsInWaitListIsZero) {
    UserEvent uEvent(pContext);
    cl_event eventWaitList[] = {&uEvent};
    auto retVal = clEnqueueSVMMemcpy(
        pCommandQueue, // cl_command_queue command_queue
        CL_FALSE,      // cl_bool blocking_copy
        nullptr,       // void *dst_ptr
        nullptr,       // const void *src_ptr
        0,             // size_t size
        0,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMMemcpyTests, successSizeIsNonZero) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *pDstSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pDstSvm);
        void *pSrcSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pSrcSvm);

        auto retVal = clEnqueueSVMMemcpy(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_copy
            pDstSvm,       // void *dst_ptr
            pSrcSvm,       // const void *src_ptr
            256,           // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, pDstSvm);
        clSVMFree(pContext, pSrcSvm);
    }
}

TEST_F(clEnqueueSVMMemcpyTests, successSizeIsZero) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *pDstSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pDstSvm);
        void *pSrcSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pSrcSvm);

        auto retVal = clEnqueueSVMMemcpy(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_copy
            pDstSvm,       // void *dst_ptr
            pSrcSvm,       // const void *src_ptr
            0,             // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, pDstSvm);
        clSVMFree(pContext, pSrcSvm);
    }
}
} // namespace ULT
