/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueSVMMemcpyTests;

namespace ULT {

TEST_F(clEnqueueSVMMemcpyTests, GivenInvalidCommandQueueWhenCopyingSVMMemoryThenInvalidCommandQueueErrorIsReturned) {
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

TEST_F(clEnqueueSVMMemcpyTests, GivenNullDstPtrWhenCopyingSVMMemoryThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMMemcpyTests, GivenNullSrcPtrWhenCopyingSVMMemoryThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMMemcpyTests, GivenNonZeroEventsAndNullEventListWhenCopyingSVMMemoryThenInvalidEventWaitListErrorIsReturned) {
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

TEST_F(clEnqueueSVMMemcpyTests, GivenZeroEventsAndNonNullEventListWhenCopyingSVMMemoryThenInvalidEventWaitListErrorIsReturned) {
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

TEST_F(clEnqueueSVMMemcpyTests, GivenNonZeroSizeWhenCopyingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMMemcpyTests, GivenZeroSizeWhenCopyingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMMemcpyTests, GivenDeviceNotSupportingSvmWhenEnqueuingSVMMemcpyThenInvalidOperationErrorIsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrSvm = false;

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    cl_device_id deviceId = pDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
    auto pCommandQueue = std::make_unique<MockCommandQueue>(pContext.get(), pDevice.get(), nullptr);

    auto retVal = clEnqueueSVMMemcpy(
        pCommandQueue.get(), // cl_command_queue command_queue
        CL_FALSE,            // cl_bool blocking_copy
        nullptr,             // void *dst_ptr
        nullptr,             // const void *src_ptr
        0,                   // size_t size
        0,                   // cl_uint num_events_in_wait_list
        nullptr,             // const cl_event *event_wait_list
        nullptr              // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
