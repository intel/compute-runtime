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

typedef api_tests clEnqueueSVMFreeTests;

namespace ULT {

TEST_F(clEnqueueSVMFreeTests, invalidCommandQueue) {
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

TEST_F(clEnqueueSVMFreeTests, invalidValue_SvmPointersAreNullAndNumSvmPointersIsNotZero) {
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

TEST_F(clEnqueueSVMFreeTests, invalidValue_NumSvmPointersIsZeroAndSvmPointersAreNotNull) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMFreeTests, invalidEventWaitList_EventWaitListIsNullAndNumEventsInWaitListIsGreaterThanZero) {
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

TEST_F(clEnqueueSVMFreeTests, invalidEventWaitList_EventWaitListIsNotNullAndNumEventsInWaitListIsZero) {
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

TEST_F(clEnqueueSVMFreeTests, success_NumSvmPointersIsNonZeroAndSvmPointersAreNotNull) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMFreeTests, success_NumSvmPointersIsZeroAndSvmPointersAreNull) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
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
} // namespace ULT
