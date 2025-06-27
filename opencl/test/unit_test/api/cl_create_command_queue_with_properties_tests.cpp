/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

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
        ApiFixture::setUp();
    }

    void TearDown() override {
        ApiFixture::tearDown();
    }

    cl_command_queue_properties commandQueueProperties = 0;
    cl_uint queueSize = 0;
    cl_queue_priority_khr queuePriority;
    cl_queue_throttle_khr queueThrottle;
};

struct ClCreateCommandQueueWithPropertiesApi : public ApiFixture<>,
                                               public MemoryManagementFixture,
                                               public ::testing::Test {
    ClCreateCommandQueueWithPropertiesApi() {
    }

    void SetUp() override {
        platformsImpl->clear();
        MemoryManagementFixture::setUp();
        ApiFixture::setUp();
    }

    void TearDown() override {
        ApiFixture::tearDown();
        MemoryManagementFixture::tearDown();
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
        testedClDevice,
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
    auto commandQueueObj = castToObject<CommandQueue>(cmdQ);

    ASSERT_NE(commandQueueObj, nullptr);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto icdStoredFunction = icdGlobalDispatchTable.clCreateCommandQueueWithProperties;
    auto pFunction = &clCreateCommandQueueWithProperties;
    EXPECT_EQ(icdStoredFunction, pFunction);
}

static cl_command_queue_properties commandQueueProperties[] =
    {
        0,
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

INSTANTIATE_TEST_SUITE_P(api,
                         clCreateCommandQueueWithPropertiesTests,
                         ::testing::Combine(
                             ::testing::ValuesIn(commandQueueProperties),
                             ::testing::ValuesIn(queueSizes),
                             ::testing::ValuesIn(queuePriorities),
                             ::testing::ValuesIn(queueThrottles)));

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenNullContextWhenCreatingCommandQueueWithPropertiesThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto cmdQ = clCreateCommandQueueWithProperties(
        nullptr,
        nullptr,
        0,
        &retVal);
    EXPECT_EQ(cmdQ, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_CONTEXT);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenNullContextWhenCreatingCommandQueueWithPropertiesKHRThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto cmdQ = clCreateCommandQueueWithPropertiesKHR(
        nullptr,
        nullptr,
        0,
        &retVal);
    EXPECT_EQ(cmdQ, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_CONTEXT);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenOoqPropertiesWhenQueueIsCreatedThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, testedClDevice, ooq, &retVal);
    EXPECT_NE(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseCommandQueue(cmdq);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenQueueOnDeviceWithoutOoqPropertiesWhenQueueIsCreatedThenErrorIsReturned) {
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, testedClDevice, ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenNullContextAndOoqPropertiesWhenCreatingCommandQueueWithPropertiesThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, 0};
    auto cmdq = clCreateCommandQueueWithProperties(nullptr, testedClDevice, ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_CONTEXT);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenNullDeviceWhenCreatingCommandQueueWithPropertiesThenInvalidDeviceErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, nullptr, ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_DEVICE);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenDeviceNotAssociatedWithContextWhenCreatingCommandQueueWithPropertiesThenInvalidDeviceErrorIsReturned) {
    cl_int retVal = CL_OUT_OF_HOST_MEMORY;
    UltClDeviceFactory deviceFactory{1, 0};
    EXPECT_FALSE(pContext->isDeviceAssociated(*deviceFactory.rootDevices[0]));
    auto cmdq = clCreateCommandQueueWithProperties(pContext, deviceFactory.rootDevices[0], nullptr, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_DEVICE);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenSizeWhichExceedsMaxDeviceQueueSizeWhenCreatingCommandQueueWithPropertiesThenInvalidQueuePropertiesErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT, CL_QUEUE_SIZE, (cl_uint)0xffffffff, 0, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, testedClDevice, ooq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenQueueOnDeviceWithoutOutOfOrderExecModePropertyWhenCreatingCommandQueueWithPropertiesThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties odq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, testedClDevice, odq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenDefaultDeviceQueueWithoutQueueOnDevicePropertyWhenCreatingCommandQueueWithPropertiesThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ddq[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE_DEFAULT, 0};
    auto cmdq = clCreateCommandQueueWithProperties(pContext, testedClDevice, ddq, &retVal);
    EXPECT_EQ(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenHighPriorityWhenCreatingOoqCommandQueueWithPropertiesThenInvalidQueuePropertiesErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_HIGH_KHR, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, testedClDevice, ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenLowPriorityWhenCreatingOoqCommandQueueWithPropertiesThenInvalidQueuePropertiesErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, testedClDevice, ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}
TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenMedPriorityWhenCreatingOoqCommandQueueWithPropertiesThenInvalidQueuePropertiesErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_MED_KHR, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, testedClDevice, ondevice, &retVal);
    EXPECT_EQ(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenDeviceQueuePropertiesWhenCreatingCommandQueueWithPropertiesThenNullQueueAndInvalidQueuePropertiesErrorIsReturned) {
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context{pClDevice.get()};

    cl_int retVal = CL_SUCCESS;
    cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto pCmdQ = clCreateCommandQueueWithProperties(&context, pClDevice.get(), queueProperties, &retVal);
    EXPECT_EQ(nullptr, pCmdQ);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenDefaultDeviceQueuePropertiesWhenCreatingCommandQueueWithPropertiesThenNullQueueAndInvalidQueuePropertiesErrorIsReturned) {
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context{pClDevice.get()};

    cl_int retVal = CL_SUCCESS;
    cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto pCmdQ = clCreateCommandQueueWithProperties(&context, pClDevice.get(), queueProperties, &retVal);
    EXPECT_EQ(nullptr, pCmdQ);
    EXPECT_EQ(retVal, CL_INVALID_QUEUE_PROPERTIES);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenInvalidPropertiesWhenCreatingOoqCommandQueueWithPropertiesThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties properties = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, &properties, &retVal);
    EXPECT_EQ(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenInvalidPropertiesOnSubsequentTokenWhenQueueIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_DEVICE_PARTITION_EQUALLY, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, properties, &retVal);
    EXPECT_EQ(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenNullPropertiesWhenCreatingCommandQueueWithPropertiesThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, nullptr, &retVal);
    EXPECT_NE(nullptr, commandQueue);
    EXPECT_EQ(retVal, CL_SUCCESS);
    clReleaseCommandQueue(commandQueue);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenLowPriorityWhenCreatingCommandQueueWithPropertiesThenSuccessIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, testedClDevice, ondevice, &retVal);
    EXPECT_NE(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = clReleaseCommandQueue(cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

HWTEST_F(ClCreateCommandQueueWithPropertiesApi, GivenLowPriorityWhenCreatingCommandQueueThenSelectRcsEngine) {
    cl_queue_properties properties[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ = clCreateCommandQueueWithProperties(pContext, testedClDevice, properties, nullptr);

    auto commandQueueObj = castToObject<CommandQueue>(cmdQ);
    auto &osContext = commandQueueObj->getGpgpuCommandStreamReceiver().getOsContext();
    EXPECT_EQ(getChosenEngineType(pDevice->getHardwareInfo()), osContext.getEngineType());
    EXPECT_TRUE(osContext.isLowPriority());

    clReleaseCommandQueue(cmdQ);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenCommandQueueCreatedWithNullPropertiesWhenQueryingPropertiesArrayThenNothingIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, nullptr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(nullptr, commandQueue);

    size_t propertiesArraySize;
    retVal = clGetCommandQueueInfo(commandQueue, CL_QUEUE_PROPERTIES_ARRAY, 0, nullptr, &propertiesArraySize);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(0u, propertiesArraySize);

    clReleaseCommandQueue(commandQueue);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, GivenCommandQueueCreatedWithVariousPropertiesWhenQueryingPropertiesArrayThenCorrectValuesAreReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties propertiesArray[3];
    size_t propertiesArraySize;

    std::vector<std::vector<uint64_t>> propertiesToTest{
        {0},
        {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0},
        {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0}};

    for (auto properties : propertiesToTest) {
        auto commandQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, properties.data(), &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clGetCommandQueueInfo(commandQueue, CL_QUEUE_PROPERTIES_ARRAY,
                                       sizeof(propertiesArray), propertiesArray, &propertiesArraySize);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(properties.size() * sizeof(cl_queue_properties), propertiesArraySize);
        for (size_t i = 0; i < properties.size(); i++) {
            EXPECT_EQ(properties[i], propertiesArray[i]);
        }

        clReleaseCommandQueue(commandQueue);
    }
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenQueueFamilySelectedAndNotIndexWhenCreatingQueueThenFail) {
    cl_queue_properties queueProperties[] = {
        CL_QUEUE_FAMILY_INTEL,
        0,
        0,
    };

    auto queue = clCreateCommandQueueWithProperties(pContext, testedClDevice, queueProperties, &retVal);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenQueueIndexSelectedAndNotFamilyWhenCreatingQueueThenFail) {
    cl_queue_properties queueProperties[] = {
        CL_QUEUE_INDEX_INTEL,
        0,
        0,
    };

    auto queue = clCreateCommandQueueWithProperties(pContext, testedClDevice, queueProperties, &retVal);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenValidFamilyAndIndexSelectedWhenCreatingQueueThenReturnSuccess) {
    cl_queue_properties queueProperties[] = {
        CL_QUEUE_FAMILY_INTEL,
        0,
        CL_QUEUE_INDEX_INTEL,
        0,
        0,
    };

    auto queue = clCreateCommandQueueWithProperties(pContext, testedClDevice, queueProperties, &retVal);
    EXPECT_NE(nullptr, queue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(queue));
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenInvalidQueueFamilySelectedWhenCreatingQueueThenFail) {
    cl_queue_properties queueProperties[] = {
        CL_QUEUE_FAMILY_INTEL,
        CommonConstants::engineGroupCount,
        CL_QUEUE_INDEX_INTEL,
        0,
        0,
    };

    auto queue = clCreateCommandQueueWithProperties(pContext, testedClDevice, queueProperties, &retVal);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenInvalidQueueIndexSelectedWhenCreatingQueueThenFail) {
    cl_queue_properties queueProperties[] = {
        CL_QUEUE_FAMILY_INTEL,
        0,
        CL_QUEUE_INDEX_INTEL,
        50,
        0,
    };

    auto queue = clCreateCommandQueueWithProperties(pContext, testedClDevice, queueProperties, &retVal);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(ClCreateCommandQueueWithPropertiesApi, givenDeprecatedClSetCommandQueuePropertiesCalledThenCorrectErrorIsReturned) {
    cl_queue_properties properties = 0, oldProperties = 0, newProperties = CL_QUEUE_PROFILING_ENABLE;

    cl_int retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, &properties, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(nullptr, commandQueue);

    EXPECT_EQ(CL_INVALID_OPERATION, clSetCommandQueueProperty(commandQueue, newProperties, CL_TRUE, &oldProperties));
    clReleaseCommandQueue(commandQueue);
}

using LowPriorityCommandQueueTest = ::testing::Test;
HWTEST_F(LowPriorityCommandQueueTest, GivenDeviceWithSubdevicesWhenCreatingLowPriorityCommandQueueThenEngineFromFirstSubdeviceIsTaken) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> mockDeviceFlagBackup{&MockDevice::createSingleDevice, false};
    debugManager.flags.CreateMultipleSubDevices.set(2);
    MockContext context;
    cl_queue_properties properties[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    EXPECT_EQ(2u, context.getDevice(0)->getNumGenericSubDevices());
    auto cmdQ = clCreateCommandQueueWithProperties(&context, context.getDevice(0), properties, nullptr);

    auto commandQueueObj = castToObject<CommandQueue>(cmdQ);
    auto subDevice = context.getDevice(0)->getSubDevice(0);
    auto &engine = subDevice->getEngine(getChosenEngineType(subDevice->getHardwareInfo()), EngineUsage::lowPriority);

    EXPECT_EQ(engine.commandStreamReceiver, &commandQueueObj->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(engine.osContext, &commandQueueObj->getGpgpuCommandStreamReceiver().getOsContext());
    clReleaseCommandQueue(cmdQ);
}

std::pair<uint32_t, QueuePriority> priorityParams[3]{
    std::make_pair(CL_QUEUE_PRIORITY_LOW_KHR, QueuePriority::low),
    std::make_pair(CL_QUEUE_PRIORITY_MED_KHR, QueuePriority::medium),
    std::make_pair(CL_QUEUE_PRIORITY_HIGH_KHR, QueuePriority::high)};

class ClCreateCommandQueueWithPropertiesApiPriority : public ClCreateCommandQueueWithPropertiesApi,
                                                      public ::testing::WithParamInterface<std::pair<uint32_t, QueuePriority>> {
};

TEST_P(ClCreateCommandQueueWithPropertiesApiPriority, GivenValidPriorityWhenCreatingCommandQueueWithPropertiesThenCorrectPriorityIsSetInternally) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_PRIORITY_KHR, GetParam().first, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, testedClDevice, ondevice, &retVal);
    EXPECT_NE(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto commandQueue = castToObject<CommandQueue>(cmdqd);
    EXPECT_EQ(commandQueue->getPriority(), GetParam().second);

    retVal = clReleaseCommandQueue(cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(AllValidPriorities,
                         ClCreateCommandQueueWithPropertiesApiPriority,
                         ::testing::ValuesIn(priorityParams));

std::pair<uint32_t, QueueThrottle> throttleParams[3]{
    std::make_pair(CL_QUEUE_THROTTLE_LOW_KHR, QueueThrottle::LOW),
    std::make_pair(CL_QUEUE_THROTTLE_MED_KHR, QueueThrottle::MEDIUM),
    std::make_pair(CL_QUEUE_THROTTLE_HIGH_KHR, QueueThrottle::HIGH)};

class ClCreateCommandQueueWithPropertiesApiThrottle : public ClCreateCommandQueueWithPropertiesApi,
                                                      public ::testing::WithParamInterface<std::pair<uint32_t, QueueThrottle>> {
};

TEST_P(ClCreateCommandQueueWithPropertiesApiThrottle, GivenThrottlePropertiesWhenCreatingCommandQueueWithPropertiesThenCorrectThrottleIsSetInternally) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ondevice[] = {CL_QUEUE_THROTTLE_KHR, GetParam().first, 0};
    auto cmdqd = clCreateCommandQueueWithProperties(pContext, testedClDevice, ondevice, &retVal);
    EXPECT_NE(nullptr, cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto commandQueue = castToObject<CommandQueue>(cmdqd);
    EXPECT_EQ(commandQueue->getThrottle(), GetParam().second);

    retVal = clReleaseCommandQueue(cmdqd);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(AllValidThrottleValues,
                         ClCreateCommandQueueWithPropertiesApiThrottle,
                         ::testing::ValuesIn(throttleParams));

} // namespace ULT
