/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

using namespace NEO;

struct GetCommandQueueInfoTest : public ClDeviceFixture,
                                 public ContextFixture,
                                 public CommandQueueFixture,
                                 ::testing::TestWithParam<uint64_t /*cl_command_queue_properties*/> {
    using CommandQueueFixture::setUp;
    using ContextFixture::setUp;

    GetCommandQueueInfoTest() {
    }

    void SetUp() override {
        properties = GetParam();
        ClDeviceFixture::setUp();

        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        CommandQueueFixture::setUp(pContext, pClDevice, properties);
    }

    void TearDown() override {
        CommandQueueFixture::tearDown();
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    const HardwareInfo *pHwInfo = nullptr;
    cl_command_queue_properties properties;
};

TEST_P(GetCommandQueueInfoTest, GivenClQueueContextWhenGettingCommandQueueInfoThenSuccessIsReturned) {
    cl_context contextReturned = nullptr;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_CONTEXT,
        sizeof(contextReturned),
        &contextReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((cl_context)pContext, contextReturned);
}

TEST_P(GetCommandQueueInfoTest, GivenClQueueDeviceWhenGettingCommandQueueInfoThenSuccessIsReturned) {
    cl_device_id deviceExpected = pClDevice;

    cl_device_id deviceReturned = nullptr;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_DEVICE,
        sizeof(deviceReturned),
        &deviceReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(deviceExpected, deviceReturned);
}

TEST_P(GetCommandQueueInfoTest, GivenClQueuePropertiesWhenGettingCommandQueueInfoThenSuccessIsReturned) {
    cl_command_queue_properties cmdqPropertiesReturned = 0;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_PROPERTIES,
        sizeof(cmdqPropertiesReturned),
        &cmdqPropertiesReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(properties, cmdqPropertiesReturned);
}

TEST_P(GetCommandQueueInfoTest, givenNonDeviceQueueWhenQueryingQueueSizeThenInvalidCommandQueueErrorIsReturned) {
    cl_uint queueSize = 0;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_SIZE,
        sizeof(queueSize),
        &queueSize,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_P(GetCommandQueueInfoTest, GivenClQueueDeviceDefaultWhenGettingCommandQueueInfoThenSuccessIsReturned) {
    cl_command_queue commandQueueReturned = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(0x1234));
    size_t sizeReturned = 0u;
    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_DEVICE_DEFAULT,
        sizeof(commandQueueReturned),
        &commandQueueReturned,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, commandQueueReturned);
    EXPECT_EQ(sizeof(cl_command_queue), sizeReturned);
}

TEST_P(GetCommandQueueInfoTest, GivenInvalidParameterWhenGettingCommandQueueInfoThenInvalidValueIsReturned) {
    cl_uint parameterReturned = 0;
    cl_command_queue_info invalidParameter = 0xdeadbeef;

    auto retVal = pCmdQ->getCommandQueueInfo(
        invalidParameter,
        sizeof(parameterReturned),
        &parameterReturned,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

INSTANTIATE_TEST_SUITE_P(
    GetCommandQueueInfoTest,
    GetCommandQueueInfoTest,
    ::testing::ValuesIn(defaultCommandQueueProperties));

using GetCommandQueueFamilyInfoTests = ::testing::Test;

TEST_F(GetCommandQueueFamilyInfoTests, givenQueueFamilyNotSelectedWhenGettingFamilyAndQueueIndexThenValuesAreReturned) {
    MockContext context{};
    MockCommandQueue queue{context};
    queue.queueFamilySelected = false;
    queue.queueFamilyIndex = 12u;
    cl_int retVal{};

    const auto &hwInfo = context.getDevice(0)->getHardwareInfo();
    const auto &gfxCoreHelper = context.getDevice(0)->getGfxCoreHelper();
    const auto engineGroupType = gfxCoreHelper.getEngineGroupType(context.getDevice(0)->getDefaultEngine().getEngineType(),
                                                                  context.getDevice(0)->getDefaultEngine().getEngineUsage(), hwInfo);
    const auto expectedFamilyIndex = context.getDevice(0)->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);

    cl_uint familyIndex{};
    retVal = queue.getCommandQueueInfo(
        CL_QUEUE_FAMILY_INTEL,
        sizeof(cl_uint),
        &familyIndex,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedFamilyIndex, familyIndex);

    cl_uint queueIndex{};
    retVal = queue.getCommandQueueInfo(
        CL_QUEUE_INDEX_INTEL,
        sizeof(cl_uint),
        &queueIndex,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, queueIndex);
}

TEST_F(GetCommandQueueFamilyInfoTests, givenQueueFamilySelectedWhenGettingFamilyAndQueueIndexThenValuesAreReturned) {
    MockCommandQueue queue;
    queue.queueFamilySelected = true;
    queue.queueFamilyIndex = 12u;
    queue.queueIndexWithinFamily = 1432u;
    cl_int retVal{};

    cl_uint familyIndex{};
    retVal = queue.getCommandQueueInfo(
        CL_QUEUE_FAMILY_INTEL,
        sizeof(cl_uint),
        &familyIndex,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(queue.queueFamilyIndex, familyIndex);

    cl_uint queueIndex{};
    retVal = queue.getCommandQueueInfo(
        CL_QUEUE_INDEX_INTEL,
        sizeof(cl_uint),
        &queueIndex,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(queue.queueIndexWithinFamily, queueIndex);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GetCommandQueueFamilyInfoTests, givenFamilyIdWhenGettingCommandQueueInfoThenCorrectValueIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0, MockClDevice::prepareExecutionEnvironment(&hwInfo, 0)};
    MockClDevice &mockClDevice = *deviceFactory.rootDevices[0];
    const cl_device_id deviceId = &mockClDevice;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, nullptr);
    auto ccsFamily = mockClDevice.getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
    cl_command_queue_properties properties[] = {CL_QUEUE_FAMILY_INTEL, ccsFamily, CL_QUEUE_INDEX_INTEL, 0, 0};
    EXPECT_EQ(0u, mockClDevice.getNumGenericSubDevices());
    auto commandQueue = clCreateCommandQueueWithProperties(context, deviceId, properties, nullptr);
    auto neoQueue = castToObject<CommandQueue>(commandQueue);

    cl_uint familyParameter;
    auto retVal = neoQueue->getCommandQueueInfo(
        CL_QUEUE_FAMILY_INTEL,
        sizeof(familyParameter),
        &familyParameter,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(ccsFamily, familyParameter);

    cl_uint indexParameter;
    retVal = neoQueue->getCommandQueueInfo(
        CL_QUEUE_INDEX_INTEL,
        sizeof(indexParameter),
        &indexParameter,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, indexParameter);

    clReleaseCommandQueue(commandQueue);
    clReleaseContext(context);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GetCommandQueueFamilyInfoTests, givenNonZeroFamilyIdWhenCreatingCommandQueueForRootDeviceWithMultipleSubDevicesThenInvalidValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    initPlatform();

    auto rootDevice = platform()->getClDevice(0);
    const cl_device_id deviceId = rootDevice;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, nullptr);

    cl_command_queue_properties properties[] = {CL_QUEUE_FAMILY_INTEL, 1u, CL_QUEUE_INDEX_INTEL, 0, 0};
    EXPECT_EQ(2u, rootDevice->getNumGenericSubDevices());
    cl_int retVal;
    auto commandQueue = clCreateCommandQueueWithProperties(context, rootDevice, properties, &retVal);

    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
    EXPECT_EQ(nullptr, commandQueue);

    clReleaseContext(context);
}

using MultiEngineQueueHwTests = ::testing::Test;
HWCMDTEST_F(IGFX_XE_HP_CORE, MultiEngineQueueHwTests, givenLimitedNumberOfCcsWhenCreatingCmdQueueThenFailOnNotSupportedCcs) {
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.gtSystemInfo.CCSInfo.IsValid = true;
    localHwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
    localHwInfo.gtSystemInfo.CCSInfo.Instances.CCSEnableMask = 0b1111;
    localHwInfo.featureTable.flags.ftrCCSNode = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    MockContext context(device.get());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;

    const uint32_t ccsCount = 4;

    auto ccsEngine = device->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
    cl_queue_properties properties[5] = {CL_QUEUE_FAMILY_INTEL, ccsEngine, CL_QUEUE_INDEX_INTEL, 0, 0};

    auto mutableHwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();

    for (uint32_t i = 0; i < ccsCount; i++) {
        properties[3] = i;
        mutableHwInfo->gtSystemInfo.CCSInfo.Instances.CCSEnableMask = (1 << i);

        cl_int retVal = CL_SUCCESS;
        cl_command_queue clCommandQueue = clCreateCommandQueueWithProperties(&context, device.get(), properties, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        clReleaseCommandQueue(clCommandQueue);
    }
}
