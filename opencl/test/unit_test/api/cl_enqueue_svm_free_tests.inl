/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueSVMFreeTests;

namespace ULT {

TEST_F(clEnqueueSVMFreeTests, GivenInvalidCommandQueueWhenFreeingSVMThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueSVMFree(
        nullptr, // cl_command_queue command_queue
        0,       // cl_uint num_svm_pointers
        nullptr, // void *svm_pointers[]
        nullptr, // (CL_CALLBACK  *pfn_free_func) ( cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        nullptr, // void *user_data
        0,       // cl_uint num_events_in_wait_list
        nullptr, // const cl_event *event_wait_list
        nullptr  // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueSVMFreeTests, GivenNonZeroNumOfSVMPointersAndNullSVMPointersWhenFreeingSVMThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto retVal = clEnqueueSVMFree(
            pCommandQueue, // cl_command_queue command_queue
            1,             // cl_uint num_svm_pointers
            nullptr,       // void *svm_pointers[]
            nullptr,       // (CL_CALLBACK  *pfn_free_func) ( cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
            nullptr,       // void *user_data
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);
    }
}

TEST_F(clEnqueueSVMFreeTests, GivenZeroNumOfSVMPointersAndNonNullSVMPointersWhenFreeingSVMThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        void *svmPtrs[] = {ptrSvm};
        auto retVal = clEnqueueSVMFree(
            pCommandQueue, // cl_command_queue command_queue
            0,             // cl_uint num_svm_pointers
            svmPtrs,       // void *svm_pointers[]
            nullptr,       // (CL_CALLBACK  *pfn_free_func) ( cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
            nullptr,       // void *user_data
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMFreeTests, GivenNonZeroNumOfEventsAndNullEventListWhenFreeingSVMThenInvalidEventWaitListErrorIsReturned) {
    auto retVal = clEnqueueSVMFree(
        pCommandQueue, // cl_command_queue command_queue
        0,             // cl_uint num_svm_pointers
        nullptr,       // void *svm_pointers[]
        nullptr,       // (CL_CALLBACK  *pfn_free_func) ( cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        nullptr,       // void *user_data
        1,             // cl_uint num_events_in_wait_list
        nullptr,       // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMFreeTests, GivenZeroNumOfEventsAndNonNullEventListWhenFreeingSVMThenInvalidEventWaitListErrorIsReturned) {
    UserEvent uEvent(pContext);
    cl_event eventWaitList[] = {&uEvent};
    auto retVal = clEnqueueSVMFree(
        pCommandQueue, // cl_command_queue command_queue
        0,             // cl_uint num_svm_pointers
        nullptr,       // void *svm_pointers[]
        nullptr,       // (CL_CALLBACK  *pfn_free_func) ( cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        nullptr,       // void *user_data
        0,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(clEnqueueSVMFreeTests, GivenNonZeroNumOfSVMPointersAndNonNullSVMPointersWhenFreeingSVMThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        void *svmPtrs[] = {ptrSvm};
        auto retVal = clEnqueueSVMFree(
            pCommandQueue, // cl_command_queue command_queue
            1,             // cl_uint num_svm_pointers
            svmPtrs,       // void *svm_pointers[]
            nullptr,       // (CL_CALLBACK  *pfn_free_func) ( cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
            nullptr,       // void *user_data
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clEnqueueSVMFreeTests, GivenZeroNumOfSVMPointersAndNullSVMPointersWhenFreeingSVMThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto retVal = clEnqueueSVMFree(
            pCommandQueue, // cl_command_queue command_queue
            0,             // cl_uint num_svm_pointers
            nullptr,       // void *svm_pointers[]
            nullptr,       // (CL_CALLBACK  *pfn_free_func) ( cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
            nullptr,       // void *user_data
            0,             // cl_uint num_events_in_wait_list
            nullptr,       // const cl_event *event_wait_list
            nullptr        // cl_event *event
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(clEnqueueSVMFreeTests, GivenDeviceNotSupportingSvmWhenEnqueuingSVMFreeThenInvalidOperationErrorIsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrSvm = false;

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    cl_device_id deviceId = pDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
    auto pCommandQueue = std::make_unique<MockCommandQueue>(pContext.get(), pDevice.get(), nullptr);

    auto retVal = clEnqueueSVMFree(
        pCommandQueue.get(), // cl_command_queue command_queue
        0,                   // cl_uint num_svm_pointers
        nullptr,             // void *svm_pointers[]
        nullptr,             // (CL_CALLBACK  *pfn_free_func) ( cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        nullptr,             // void *user_data
        0,                   // cl_uint num_events_in_wait_list
        nullptr,             // const cl_event *event_wait_list
        nullptr              // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
