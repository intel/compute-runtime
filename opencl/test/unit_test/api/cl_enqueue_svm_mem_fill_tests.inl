/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueSVMMemFillTests;

namespace ULT {

TEST_F(clEnqueueSVMMemFillTests, GivenInvalidCommandQueueWhenFillingSVMMemoryThenInvalidCommandQueueErrorIsReturned) {
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

TEST_F(clEnqueueSVMMemFillTests, GivenNullSVMPtrWhenFillingSVMMemoryThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
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
}

TEST_F(clEnqueueSVMMemFillTests, GivenRegionSizeZeroWhenFillingSVMMemoryThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMMemFillTests, GivenNullEventWaitListAndNonZeroEventsWhenFillingSVMMemoryThenInvalidEventWaitListIsReturned) {
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

TEST_F(clEnqueueSVMMemFillTests, GivenNonNullEventWaitListAndZeroEventsWhenFillingSVMMemoryThenInvalidEventWaitListIsReturned) {
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

TEST_F(clEnqueueSVMMemFillTests, GivenValidParametersWhenFillingSVMMemoryThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
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

TEST_F(clEnqueueSVMMemFillTests, GivenDeviceNotSupportingSvmWhenEnqueuingSVMMemFillThenInvalidOperationErrorIsReturned) {
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.ftrSvm = false;

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    cl_device_id deviceId = pDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
    auto pCommandQueue = std::make_unique<MockCommandQueue>(pContext.get(), pDevice.get(), nullptr);

    auto retVal = clEnqueueSVMMemFill(
        pCommandQueue.get(), // cl_command_queue command_queue
        nullptr,             // void *svm_ptr
        nullptr,             // const void *pattern
        0,                   // size_t pattern_size
        256,                 // size_t size
        0,                   // cl_uint num_events_in_wait_list
        nullptr,             // cl_evebt *event_wait_list
        nullptr              // cL_event *event
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
