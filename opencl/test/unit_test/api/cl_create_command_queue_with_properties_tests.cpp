/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/device_queue/device_queue.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/helpers/unit_test_helper.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

#include "CL/cl_ext.h"
#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct CommandQueueWithPropertiesTest : public ApiFixture<>,
                                        public ::testing::WithParamInterface<std::tuple<uint64_t, uint32_t, uint32_t, uint32_t>>,
                                        public ::testing::Test {
    CommandQueueWithPropertiesTest()
        : queuePriority(CL_QUEUE_PRIORITY_MED_KHR), queueThrottle(CL_QUEUE_THROTTLE_MED_KHR) {
    }

    void SetUp() override {
        std::tie(commandQueueProperties, queueSize, queuePriority, queueThrottle) = GetParam();
        ApiFixture::SetUp();
    }

    void TearDown() override {
        ApiFixture::TearDown();
    }

    cl_command_queue_properties commandQueueProperties = 0;
    cl_uint queueSize = 0;
    cl_queue_priority_khr queuePriority;
    cl_queue_throttle_khr queueThrottle;
};

struct clCreateCommandQueueWithPropertiesApi : public ApiFixture<>,
                                               public MemoryManagementFixture,
                                               public ::testing::Test {
    clCreateCommandQueueWithPropertiesApi() {
    }

    void SetUp() override {
        platformsImpl.clear();
        MemoryManagementFixture::SetUp();
        ApiFixture::SetUp();
    }

    void TearDown() override {
        ApiFixture::TearDown();
        MemoryManagementFixture::TearDown();
    }
};

typedef CommandQueueWithPropertiesTest clCreateCommandQueueWithPropertiesTests;

TEST_P(clCreateCommandQueueWithPropertiesTests, GivenPropertiesWhenCreatingCommandQueueThenExpectedResultIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties[] =
        {
            CL_QUEUE_PROPERTIES, 0,
            CL_QUEUE_SIZE, 0,
            CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_HIGH_KHR,
            CL_QUEUE_THROTTLE_KHR, CL_QUEUE_THROTTLE_MED_KHR,
            0};

    const auto minimumCreateDeviceQueueFlags = static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE |
                                                                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    const auto deviceQueueShouldBeCreated = (commandQueueProperties & minimumCreateDeviceQueueFlags) == minimumCreateDeviceQueueFlags;
    if (deviceQueueShouldBeCreated && !castToObject<ClDevice>(this->devices[testedRootDeviceIndex])->getHardwareInfo().capabilityTable.supportsDeviceEnqueue) {
        return;
    }

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
        devices[testedRootDeviceIndex],
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

    if (deviceQueueShouldBeCreated) { // created device queue
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

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenNullContextWhenCreatingCommandQueueWithPropertiesThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto cmdQ = clCreateCommandQueueWithProperties(
        nullptr,
        nullptr,
        0,
        &retVal);
    EXPECT_EQ(cmdQ, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_CONTEXT);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenNullContextWhenCreatingCommandQueueWithPropertiesKHRThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto cmdQ = clCreateCommandQueueWithPropertiesKHR(
        nullptr,
        nullptr,
        0,
        &retVal);
    EXPECT_EQ(cmdQ, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_CONTEXT);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenOoqPropertiesWhenQueueIsCreatedThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ooq, &retVal);
    EXPECT_NE(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseCommandQueue(cmdq);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenQueueOnDeviceWithoutOoqPropertiesWhenQueueIsCreatedThenErrorIsReturned) {
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE, 0, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenNullContextAndOoqPropertiesWhenCreatingCommandQueueWithPropertiesThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(nullptr, devices[testedRootDeviceIndex], ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_CONTEXT);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenNullDeviceWhenCreatingCommandQueueWithPropertiesThenInvalidDeviceErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, nullptr, ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_DEVICE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenSizeWhichExceedsMaxDeviceQueueSizeWhenCreatingCommandQueueWithPropertiesThenInvalidQueuePropertiesErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, CL_QUEUE_SIZE, (cl_uint)0xffffffff, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenQueueOnDeviceWithoutOutOfOrderExecModePropertyWhenCreatingCommandQueueWithPropertiesThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties odq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], odq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenDefaultDeviceQueueWithoutQueueOnDevicePropertyWhenCreatingCommandQueueWithPropertiesThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ddq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ddq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

HWCMDTEST_F(IGFX_GEN8_CORE, clCreateCommandQueueWithPropertiesApi, GivenNumberOfDevicesGreaterThanMaxWhenCreatingCommandQueueWithPropertiesThenOutOfResourcesErrorIsReturned) {
    if (!this->pContext->getDevice(0u)->getHardwareInfo().capabilityTable.supportsDeviceEnqueue) {
        GTEST_SKIP();
    }
    cl_int retVal = CL_SUCCESS;
    auto pDevice = castToObject<ClDevice>(devices[testedRootDeviceIndex]);
    cl_queue_properties odq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE, 0, 0};

    auto cmdq1 = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], odq, &retVal);
    EXPECT_NE(nullptr, cmdq1);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto cmdq2 = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], odq, &retVal);
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

HWCMDTEST_F(IGFX_GEN8_CORE, clCreateCommandQueueWithPropertiesApi, GivenFailedAllocationWhenCreatingCommandQueueWithPropertiesThenOutOfHostMemoryErrorIsReturned) {
    if (!this->pContext->getDevice(0u)->getHardwareInfo().capabilityTable.supportsDeviceEnqueue) {
        GTEST_SKIP();
    }
    InjectedFunction method = [this](size_t failureIndex) {
        cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0};
        auto retVal = CL_INVALID_VALUE;

        auto cmdq = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ooq, &retVal);

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
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

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenHighPriorityWhenCreatingOoqCommandQueueWithPropertiesThenInvalidQueuePropertiesErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_HIGH_KHR, 0, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenLowPriorityWhenCreatingOoqCommandQueueWithPropertiesThenInvalidQueuePropertiesErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}
TEST_F(clCreateCommandQueueWithPropertiesApi, GivenMedPriorityWhenCreatingOoqCommandQueueWithPropertiesThenInvalidQueuePropertiesErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_MED_KHR, 0, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenInvalidPropertiesWhenCreatingOoqCommandQueueWithPropertiesThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties properties = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], &properties, &retVal);
    EXPECT_EQ(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, givenInvalidPropertiesOnSubsequentTokenWhenQueueIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_DEVICE_PARTITION_EQUALLY, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], properties, &retVal);
    EXPECT_EQ(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenNullPropertiesWhenCreatingCommandQueueWithPropertiesThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], nullptr, &retVal);
    EXPECT_NE(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_SUCCESS);
    clReleaseCommandQueue(commandQueue);
}

TEST_F(clCreateCommandQueueWithPropertiesApi, GivenLowPriorityWhenCreatingCommandQueueWithPropertiesThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ondevice, &retVal);
    EXPECT_NE(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = clReleaseCommandQueue(cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

HWTEST_F(clCreateCommandQueueWithPropertiesApi, GivenLowPriorityWhenCreatingCommandQueueThenSelectRcsEngine) {
    cl_queue_properties properties[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], properties, nullptr);

    auto commandQueueObj = castToObject<CommandQueue>(cmdQ);
    auto &osContext = commandQueueObj->getGpgpuCommandStreamReceiver().getOsContext();
    EXPECT_EQ(HwHelperHw<FamilyType>::lowPriorityEngineType, osContext.getEngineType());
    EXPECT_TRUE(osContext.isLowPriority());

    clReleaseCommandQueue(cmdQ);
}

using LowPriorityCommandQueueTest = ::testing::Test;
HWTEST_F(LowPriorityCommandQueueTest, GivenDeviceWithSubdevicesWhenCreatingLowPriorityCommandQueueThenEngineFromFirstSubdeviceIsTaken) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> mockDeviceFlagBackup{&MockDevice::createSingleDevice, false};
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    MockContext context;
    cl_queue_properties properties[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    EXPECT_EQ(2u, context.getDevice(0)->getNumAvailableDevices());
    auto cmdQ = clCreateCommandQueueWithProperties(&context, context.getDevice(0), properties, nullptr);

    auto commandQueueObj = castToObject<CommandQueue>(cmdQ);
    auto subDevice = context.getDevice(0)->getDeviceById(0);
    auto engine = subDevice->getEngine(HwHelperHw<FamilyType>::lowPriorityEngineType, true);

    EXPECT_EQ(engine.commandStreamReceiver, &commandQueueObj->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(engine.osContext, &commandQueueObj->getGpgpuCommandStreamReceiver().getOsContext());
    clReleaseCommandQueue(cmdQ);
}

std::pair<uint32_t, QueuePriority> priorityParams[3]{
    std::make_pair(CL_QUEUE_PRIORITY_LOW_KHR, QueuePriority::LOW),
    std::make_pair(CL_QUEUE_PRIORITY_MED_KHR, QueuePriority::MEDIUM),
    std::make_pair(CL_QUEUE_PRIORITY_HIGH_KHR, QueuePriority::HIGH)};

class clCreateCommandQueueWithPropertiesApiPriority : public clCreateCommandQueueWithPropertiesApi,
                                                      public ::testing::WithParamInterface<std::pair<uint32_t, QueuePriority>> {
};

TEST_P(clCreateCommandQueueWithPropertiesApiPriority, GivenValidPriorityWhenCreatingCommandQueueWithPropertiesThenCorrectPriorityIsSetInternally) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PRIORITY_KHR, GetParam().first, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ondevice, &retVal);
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

TEST_P(clCreateCommandQueueWithPropertiesApiThrottle, GivenThrottlePropertiesWhenCreatingCommandQueueWithPropertiesThenCorrectThrottleIsSetInternally) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_THROTTLE_KHR, GetParam().first, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], ondevice, &retVal);
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
