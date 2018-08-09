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
#include "CL/cl_ext.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/base_object.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/mock_context.h"

using namespace OCLRT;

namespace ULT {

struct CommandQueueWithPropertiesTest : public api_fixture,
                                        public ::testing::WithParamInterface<std::tuple<uint64_t, uint32_t, uint32_t, uint32_t>>,
                                        public ::testing::Test {
    CommandQueueWithPropertiesTest()
        : queuePriority(CL_QUEUE_PRIORITY_MED_KHR), queueThrottle(CL_QUEUE_THROTTLE_MED_KHR) {
    }

    void SetUp() override {
        std::tie(commandQueueProperties, queueSize, queuePriority, queueThrottle) = GetParam();
        api_fixture::SetUp();
    }

    void TearDown() override {
        api_fixture::TearDown();
    }

    cl_command_queue_properties commandQueueProperties = 0;
    cl_uint queueSize = 0;
    cl_queue_priority_khr queuePriority;
    cl_queue_throttle_khr queueThrottle;
};

struct clCreateCommandQueueWithPropertiesApi : public api_fixture,
                                               public MemoryManagementFixture,
                                               public ::testing::Test {
    clCreateCommandQueueWithPropertiesApi() {
    }

    void SetUp() override {
        platformImpl.reset();
        MemoryManagementFixture::SetUp();
        api_fixture::SetUp();
    }

    void TearDown() override {
        api_fixture::TearDown();
        MemoryManagementFixture::TearDown();
    }
};

typedef CommandQueueWithPropertiesTest clCreateCommandQueueWithPropertiesTests;

TEST_P(clCreateCommandQueueWithPropertiesTests, returnsSuccessForValidValues) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties[] =
        {
            CL_QUEUE_PROPERTIES, 0,
            CL_QUEUE_SIZE, 0,
            CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_HIGH_KHR,
            CL_QUEUE_THROTTLE_KHR, CL_QUEUE_THROTTLE_MED_KHR,
            0};

    auto minimumCreateDeviceQueueFlags = static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE |
                                                                                  CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    bool queueOnDeviceUsed = false;
    bool priorityHintsUsed = false;
    bool throttleHintsUsed = false;

    cl_queue_properties *pProp = &properties[0];
    if (commandQueueProperties) {
        *pProp++ = CL_QUEUE_PROPERTIES;
        *pProp++ = (cl_queue_properties)commandQueueProperties;
    }
    if ((commandQueueProperties & CL_QUEUE_ON_DEVICE) &&
        queueSize) {
        *pProp++ = CL_QUEUE_SIZE;
        *pProp++ = queueSize;
    }
    if (commandQueueProperties & CL_QUEUE_ON_DEVICE) {
        queueOnDeviceUsed = true;
    }
    if (queuePriority) {
        *pProp++ = CL_QUEUE_PRIORITY_KHR;
        *pProp++ = queuePriority;
        priorityHintsUsed = true;
    }
    if (queueThrottle) {
        *pProp++ = CL_QUEUE_THROTTLE_KHR;
        *pProp++ = queueThrottle;
        throttleHintsUsed = true;
    }
    *pProp++ = 0;

    cmdQ = clCreateCommandQueueWithProperties(
        pContext,
        devices[0],
        properties,
        &retVal);
    if (queueOnDeviceUsed && priorityHintsUsed) {
        EXPECT_EQ(nullptr, cmdQ);
        EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
        return;
    } else if (queueOnDeviceUsed && throttleHintsUsed) {
        EXPECT_EQ(nullptr, cmdQ);
        EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
        return;
    } else {
        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, cmdQ);
    }
    auto deviceQ = static_cast<device_queue>(cmdQ);
    auto deviceQueueObj = castToObject<DeviceQueue>(deviceQ);
    auto commandQueueObj = castToObject<CommandQueue>(cmdQ);

    if ((commandQueueProperties & minimumCreateDeviceQueueFlags) == minimumCreateDeviceQueueFlags) { // created device queue
        ASSERT_NE(deviceQueueObj, nullptr);
        ASSERT_EQ(commandQueueObj, nullptr);
    } else { // created host queue
        ASSERT_EQ(deviceQueueObj, nullptr);
        ASSERT_NE(commandQueueObj, nullptr);
    }

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto icdStoredFunction = icdGlobalDispatchTable.clCreateCommandQueueWithProperties;
    auto pFunction = &clCreateCommandQueueWithProperties;
    EXPECT_EQ(icdStoredFunction, pFunction);
}

static cl_command_queue_properties commandQueueProperties[] =
    {
        0,
        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT,
        CL_QUEUE_PROFILING_ENABLE};

static cl_uint queueSizes[] =
    {
        0, 2000};

cl_queue_priority_khr queuePriorities[] =
    {
        0,
        CL_QUEUE_PRIORITY_LOW_KHR,
        CL_QUEUE_PRIORITY_MED_KHR,
        CL_QUEUE_PRIORITY_HIGH_KHR};

cl_queue_throttle_khr queueThrottles[] =
    {
        0,
        CL_QUEUE_THROTTLE_LOW_KHR,
        CL_QUEUE_THROTTLE_MED_KHR,
        CL_QUEUE_THROTTLE_HIGH_KHR};

INSTANTIATE_TEST_CASE_P(api,
                        clCreateCommandQueueWithPropertiesTests,
                        ::testing::Combine(
                            ::testing::ValuesIn(commandQueueProperties),
                            ::testing::ValuesIn(queueSizes),
                            ::testing::ValuesIn(queuePriorities),
                            ::testing::ValuesIn(queueThrottles)));

TEST_F(clCreateCommandQueueWithPropertiesApi, negativeCase) {
    cl_int retVal = CL_SUCCESS;
    auto cmdQ = clCreateCommandQueueWithProperties(
        nullptr,
        nullptr,
        0,
        &retVal);
    EXPECT_EQ(cmdQ, nullptr);
    EXPECT_NE(retVal, CL_SUCCESS);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, negativeCaseKHR) {
    cl_int retVal = CL_SUCCESS;
    auto cmdQ = clCreateCommandQueueWithPropertiesKHR(
        nullptr,
        nullptr,
        0,
        &retVal);
    EXPECT_EQ(cmdQ, nullptr);
    EXPECT_NE(retVal, CL_SUCCESS);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, givenOoqPropertiesWhenQueueIsCreatedThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[0], ooq, &retVal);
    EXPECT_NE(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseCommandQueue(cmdq);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, givenQueueOnDeviceWithoutOoqPropertiesWhenQueueIsCreatedThenErrorIsReturned) {
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE, 0, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[0], ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnInvalidContext) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(nullptr, devices[0], ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_CONTEXT);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnInvalidDevice) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, nullptr, ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_DEVICE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnInvalidQueuePropertiesWhenSizeExceedsMaxDeviceQueueSize) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, CL_QUEUE_SIZE, (cl_uint)0xffffffff, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[0], ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnInvalidValueWhenQueueOnDeviceIsUsedWithoutOutOfOrderExecModeProperty) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties odq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[0], odq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnInvalidValueWhenDefaultDeviceQueueIsUsedWithoutQueueOnDeviceProperty) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ddq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[0], ddq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnOutOfMemoryWhenNumberOfDeviceQueuesExceedsMaxOnDeviceQueues) {
    cl_int retVal = CL_SUCCESS;
    auto pDevice = castToObject<Device>(devices[0]);
    cl_queue_properties odq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE, 0, 0};

    auto cmdq1 = clCreateCommandQueueWithProperties(pContext, devices[0], odq, &retVal);
    EXPECT_NE(nullptr, cmdq1);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto cmdq2 = clCreateCommandQueueWithProperties(pContext, devices[0], odq, &retVal);
    if (pDevice->getDeviceInfo().maxOnDeviceQueues > 1) {
        EXPECT_NE(nullptr, cmdq2);
        EXPECT_EQ(retVal, CL_SUCCESS);
    } else {
        EXPECT_EQ(nullptr, cmdq2);
        EXPECT_EQ(retVal, CL_OUT_OF_RESOURCES);
    }

    clReleaseCommandQueue(cmdq1);
    if (cmdq2) {
        clReleaseCommandQueue(cmdq2);
    }
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnOutOfMemory) {
    InjectedFunction method = [this](size_t failureIndex) {
        cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0};
        auto retVal = CL_INVALID_VALUE;

        auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[0], ooq, &retVal);

        if (nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, cmdq);
            clReleaseCommandQueue(cmdq);
        } else {
            EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            EXPECT_EQ(nullptr, cmdq);
        }
    };
    injectFailureOnIndex(method, 0);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnErrorOnDeviceWithHighPriority) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_HIGH_KHR, 0, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[0], ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnErrorOnDeviceWithLowPriority) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[0], ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}
TEST_F(clCreateCommandQueueWithPropertiesApi, returnErrorOnDeviceWithMedPriority) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_MED_KHR, 0, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[0], ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, givenInvalidPropertiesWhenQueueIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties properties = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, devices[0], &properties, &retVal);
    EXPECT_EQ(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, givenInvalidPropertiesOnSubsequentTokenWhenQueueIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_DEVICE_PARTITION_EQUALLY, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, devices[0], properties, &retVal);
    EXPECT_EQ(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, givenNullPropertiesWhenQueueIsCreatedThenReturnSuccess) {
    cl_int retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, devices[0], nullptr, &retVal);
    EXPECT_NE(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_SUCCESS);
    clReleaseCommandQueue(commandQueue);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, returnSuccessOnQueueWithPriority) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[0], ondevice, &retVal);
    EXPECT_NE(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = clReleaseCommandQueue(cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

std::pair<uint32_t, QueuePriority> priorityParams[3]{
    std::make_pair(CL_QUEUE_PRIORITY_LOW_KHR, QueuePriority::LOW),
    std::make_pair(CL_QUEUE_PRIORITY_MED_KHR, QueuePriority::MEDIUM),
    std::make_pair(CL_QUEUE_PRIORITY_HIGH_KHR, QueuePriority::HIGH)};

class clCreateCommandQueueWithPropertiesApiPriority : public clCreateCommandQueueWithPropertiesApi,
                                                      public ::testing::WithParamInterface<std::pair<uint32_t, QueuePriority>> {
};

TEST_P(clCreateCommandQueueWithPropertiesApiPriority, givenCreateQueueWithWhenPriorityPropertiesThenSetCorrectPriorityInternally) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PRIORITY_KHR, GetParam().first, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[0], ondevice, &retVal);
    EXPECT_NE(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto commandQueue = castToObject<CommandQueue>(cmdqd);
    EXPECT_EQ(commandQueue->getPriority(), GetParam().second);

    retVal = clReleaseCommandQueue(cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

INSTANTIATE_TEST_CASE_P(AllValidPriorities,
                        clCreateCommandQueueWithPropertiesApiPriority,
                        ::testing::ValuesIn(priorityParams));

std::pair<uint32_t, QueueThrottle> throttleParams[3]{
    std::make_pair(CL_QUEUE_THROTTLE_LOW_KHR, QueueThrottle::LOW),
    std::make_pair(CL_QUEUE_THROTTLE_MED_KHR, QueueThrottle::MEDIUM),
    std::make_pair(CL_QUEUE_THROTTLE_HIGH_KHR, QueueThrottle::HIGH)};

class clCreateCommandQueueWithPropertiesApiThrottle : public clCreateCommandQueueWithPropertiesApi,
                                                      public ::testing::WithParamInterface<std::pair<uint32_t, QueueThrottle>> {
};

TEST_P(clCreateCommandQueueWithPropertiesApiThrottle, givenCreateQueueWithWhenThrottlePropertiesThenSetCorrectThrottleInternally) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_THROTTLE_KHR, GetParam().first, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[0], ondevice, &retVal);
    EXPECT_NE(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto commandQueue = castToObject<CommandQueue>(cmdqd);
    EXPECT_EQ(commandQueue->getThrottle(), GetParam().second);

    retVal = clReleaseCommandQueue(cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

INSTANTIATE_TEST_CASE_P(AllValidThrottleValues,
                        clCreateCommandQueueWithPropertiesApiThrottle,
                        ::testing::ValuesIn(throttleParams));

} // namespace ULT
