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

using ClEnqueueSVMMemcpyTests = ApiTests;

namespace ULT {

TEST_F(ClEnqueueSVMMemcpyTests, GivenInvalidCommandQueueWhenCopyingSVMMemoryThenInvalidCommandQueueErrorIsReturned) {
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

TEST_F(ClEnqueueSVMMemcpyTests, GivenNullDstPtrWhenCopyingSVMMemoryThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
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

TEST_F(ClEnqueueSVMMemcpyTests, GivenNullSrcPtrWhenCopyingSVMMemoryThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
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

TEST_F(ClEnqueueSVMMemcpyTests, GivenNonZeroEventsAndNullEventListWhenCopyingSVMMemoryThenInvalidEventWaitListErrorIsReturned) {
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

TEST_F(ClEnqueueSVMMemcpyTests, GivenZeroEventsAndNonNullEventListWhenCopyingSVMMemoryThenInvalidEventWaitListErrorIsReturned) {
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

TEST_F(ClEnqueueSVMMemcpyTests, GivenNonZeroSizeWhenCopyingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
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

TEST_F(ClEnqueueSVMMemcpyTests, GivenQueueIncapableWhenCopyingSvmBufferThenInvalidOperationIsReturned) {

    disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL);

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
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    clSVMFree(pContext, pDstSvm);
    clSVMFree(pContext, pSrcSvm);
}

TEST_F(ClEnqueueSVMMemcpyTests, GivenZeroSizeWhenCopyingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
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

TEST_F(ClEnqueueSVMMemcpyTests, GivenZeroSizeWhenCopyingSVMMemoryWithEventThenProperCmdTypeIsSet) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *pDstSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pDstSvm);
        void *pSrcSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pSrcSvm);

        cl_event event = nullptr;
        auto retVal = clEnqueueSVMMemcpy(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_copy
            pDstSvm,       // void *dst_ptr
            pSrcSvm,       // const void *src_ptr
            0,             // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            &event         // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        constexpr cl_command_type expectedCmd = CL_COMMAND_SVM_MEMCPY;
        cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
        EXPECT_EQ(expectedCmd, actualCmd);

        clSVMFree(pContext, pDstSvm);
        clSVMFree(pContext, pSrcSvm);
        clReleaseEvent(event);
    }
}

TEST_F(ClEnqueueSVMMemcpyTests, GivenInvalidPtrAndZeroSizeWhenCopyingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        UserEvent uEvent(pContext);
        cl_event eventWaitList[] = {&uEvent};
        void *pDstSvm = reinterpret_cast<int *>(0x100001);
        void *pSrcSvm = reinterpret_cast<int *>(0x100001);

        auto retVal = clEnqueueSVMMemcpy(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_copy
            pDstSvm,       // void *dst_ptr
            pSrcSvm,       // const void *src_ptr
            0,             // size_t size
            1,             // cl_uint num_events_in_wait_list
            eventWaitList, // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_QUEUED, uEvent.peekExecutionStatus());
        EXPECT_TRUE(pCommandQueue->enqueueMarkerWithWaitListCalled);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ClEnqueueSVMMemcpyTests, givenCopyValidForStagingBuffersCopyThenTransferSuccessfull) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *pDstSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, pDstSvm);
        auto pSrc = new unsigned char[256];
        auto retVal = clEnqueueSVMMemcpy(
            pCommandQueue, // cl_command_queue command_queue
            CL_FALSE,      // cl_bool blocking_copy
            pDstSvm,       // void *dst_ptr
            pSrc,          // const void *src_ptr
            256,           // size_t size
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, pDstSvm);
        delete[] pSrc;
    }
}

} // namespace ULT
