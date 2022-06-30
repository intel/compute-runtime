/*
 * Copyright (C) 2018-2021 Intel Corporation
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
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
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
}

TEST_F(clEnqueueSVMMapTests, GivenRegionSizeZeroWhenMappingSVMThenInvalidValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
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
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
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

TEST_F(clEnqueueSVMMapTests, GivenQueueIncapableWhenMappingSvmBufferThenInvalidOperationIsReturned) {
    REQUIRE_SVM_OR_SKIP(pDevice);

    disableQueueCapabilities(CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL);

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
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    clSVMFree(pContext, ptrSvm);
}

TEST_F(clEnqueueSVMMapTests, GivenDeviceNotSupportingSvmWhenEnqueuingSVMMapThenInvalidOperationErrorIsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrSvm = false;

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    cl_device_id deviceId = pDevice.get();
    auto pContext = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
    auto pCommandQueue = std::make_unique<MockCommandQueue>(pContext.get(), pDevice.get(), nullptr, false);

    auto retVal = clEnqueueSVMMap(
        pCommandQueue.get(), // cl_command_queue command_queue
        CL_FALSE,            // cl_bool blocking_map
        CL_MAP_READ,         // cl_map_flags map_flags
        nullptr,             // void *svm_ptr
        256,                 // size_t size
        0,                   // cl_uint num_events_in_wait_list
        nullptr,             // const cL_event *event_wait_list
        nullptr              // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
