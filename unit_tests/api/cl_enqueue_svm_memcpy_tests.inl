/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
