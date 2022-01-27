/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device_info_map.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"
#include "opencl/test/unit_test/helpers/raii_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

TEST(GetDeviceInfo, GivenInvalidParamsWhenGettingDeviceInfoThenInvalidValueErrorIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    auto retVal = device->getDeviceInfo(
        0,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(GetDeviceInfo, GivenInvalidParametersWhenGettingDeviceInfoThenValueSizeRetIsNotUpdated) {
    size_t valueSizeRet = 0x1234;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    auto retVal = device->getDeviceInfo(
        0,
        0,
        nullptr,
        &valueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, valueSizeRet);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceInfoMemCapabilitiesTest, GivenValidParametersWhenGetDeviceInfoIsCalledForBdwAndLaterThenClSuccessIsReturned) {

    std::vector<TestParams> params = {
        {CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL, 0},
        {CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL, 0}};

    check(params);
}

TEST(GetDeviceInfo, GivenPlanarYuvExtensionDisabledAndSupportImageEnabledWhenGettingPlanarYuvMaxWidthHeightThenInvalidValueErrorIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->sharedDeviceInfo.imageSupport = true;
    device->deviceInfo.nv12Extension = false;
    uint32_t value;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL,
        4,
        &value,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL,
        4,
        &value,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(GetDeviceInfo, GivenPlanarYuvExtensionEnabledAndSupportImageEnabledWhenGettingPlanarYuvMaxWidthHeightThenCorrectValuesAreReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->sharedDeviceInfo.imageSupport = true;
    device->deviceInfo.nv12Extension = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(16384u, value);
}

TEST(GetDeviceInfo, GivenPlanarYuvExtensionDisabledAndSupportImageDisabledWhenGettingPlanarYuvMaxWidthHeightThenInvalidValueErrorIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->sharedDeviceInfo.imageSupport = false;
    device->deviceInfo.nv12Extension = false;
    uint32_t value;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL,
        4,
        &value,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL,
        4,
        &value,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(GetDeviceInfo, GivenPlanarYuvExtensionEnabledAndSupportImageDisabledWhenGettingPlanarYuvMaxWidthHeightThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->sharedDeviceInfo.imageSupport = false;
    device->deviceInfo.nv12Extension = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);

    retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportDisabledWhenGettingImage2dMaxWidthHeightThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = false;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE2D_MAX_WIDTH,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);

    retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE2D_MAX_HEIGHT,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportEnabledWhenGettingImage2dMaxWidthHeightThenCorrectValuesAreReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE2D_MAX_WIDTH,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE2D_MAX_HEIGHT,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(GetDeviceInfo, GivenImageSupportDisabledWhenGettingImage3dMaxWidthHeightDepthThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    size_t value = 0;

    device->sharedDeviceInfo.imageSupport = false;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE3D_MAX_WIDTH,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);

    retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE3D_MAX_HEIGHT,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);

    retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE3D_MAX_DEPTH,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportEnabledWhenGettingImage3dMaxWidthHeightDepthThenCorrectValuesAreReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE3D_MAX_WIDTH,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE3D_MAX_HEIGHT,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE3D_MAX_DEPTH,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(GetDeviceInfo, GivenImageSupportDisabledWhenGettingImageMaxArgsThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = false;
    uint32_t value;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_MAX_READ_IMAGE_ARGS,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);

    retVal = device->getDeviceInfo(
        CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);

    retVal = device->getDeviceInfo(
        CL_DEVICE_MAX_WRITE_IMAGE_ARGS,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportEnabledWhenGettingImageMaxArgsThenCorrectValuesAreReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_MAX_READ_IMAGE_ARGS,
        sizeof(cl_uint),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = device->getDeviceInfo(
        CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS,
        sizeof(cl_uint),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = device->getDeviceInfo(
        CL_DEVICE_MAX_WRITE_IMAGE_ARGS,
        sizeof(cl_uint),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(GetDeviceInfo, GivenImageSupportDisabledWhenGettingImageBaseAddressAlignmentThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    size_t value = 0;

    device->sharedDeviceInfo.imageSupport = false;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT,
        sizeof(cl_uint),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportEnabledWhenGettingImageBaseAddressAlignmentThenCorrectValuesAreReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT,
        sizeof(cl_uint),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportDisabledWhenGettingImageMaxArraySizeThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    size_t value = 0;

    device->sharedDeviceInfo.imageSupport = false;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportEnabledWhenGettingImageMaxArraySizeThenCorrectValuesAreReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(GetDeviceInfo, GivenImageSupportDisabledWhenGettingImageMaxBufferSizeThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    size_t value = 0;

    device->sharedDeviceInfo.imageSupport = false;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE_MAX_BUFFER_SIZE,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportEnabledWhenGettingImageMaxBufferSizeThenCorrectValuesAreReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE_MAX_BUFFER_SIZE,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(GetDeviceInfo, GivenImageSupportDisabledWhenGettingImagePitchAlignmentThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    size_t value = 0;

    device->sharedDeviceInfo.imageSupport = false;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE_PITCH_ALIGNMENT,
        sizeof(cl_uint),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, value);
}

TEST(GetDeviceInfo, GivenImageSupportEnabledWhenGettingImagePitchAlignmentThenCorrectValuesAreReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->sharedDeviceInfo.imageSupport = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_IMAGE_PITCH_ALIGNMENT,
        sizeof(cl_uint),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, value);
}

TEST(GetDeviceInfo, GivenNumSimultaneousInteropsWhenGettingDeviceInfoThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->simultaneousInterops = {0};

    cl_uint value = 0;
    size_t size = 0;

    auto retVal = device->getDeviceInfo(CL_DEVICE_NUM_SIMULTANEOUS_INTEROPS_INTEL, sizeof(cl_uint), &value, &size);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0u, size);
    EXPECT_EQ(0u, value);

    device->simultaneousInterops = {1, 2, 3, 0};
    retVal = device->getDeviceInfo(CL_DEVICE_NUM_SIMULTANEOUS_INTEROPS_INTEL, sizeof(cl_uint), &value, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), size);
    EXPECT_EQ(1u, value);
}

TEST(GetDeviceInfo, GivenSimultaneousInteropsWhenGettingDeviceInfoThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->simultaneousInterops = {0};

    cl_uint value[4] = {};
    size_t size = 0;

    auto retVal = device->getDeviceInfo(CL_DEVICE_SIMULTANEOUS_INTEROPS_INTEL, sizeof(cl_uint), &value, &size);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    device->simultaneousInterops = {1, 2, 3, 0};
    retVal = device->getDeviceInfo(CL_DEVICE_SIMULTANEOUS_INTEROPS_INTEL, sizeof(cl_uint) * 4u, &value, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint) * 4u, size);
    EXPECT_TRUE(memcmp(value, &device->simultaneousInterops[0], 4u * sizeof(cl_uint)) == 0);
}

TEST(GetDeviceInfo, GivenMaxGlobalVariableSizeWhenGettingDeviceInfoThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    size_t value = 0;
    size_t size = 0;

    auto retVal = device->getDeviceInfo(CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE, sizeof(size_t), &value, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(size_t), size);
    if (device->areOcl21FeaturesEnabled()) {
        EXPECT_EQ(value, 65536u);
    } else {
        EXPECT_EQ(value, 0u);
    }
}

TEST(GetDeviceInfo, GivenGlobalVariablePreferredTotalSizeWhenGettingDeviceInfoThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    size_t value = 0;
    size_t size = 0;

    auto retVal = device->getDeviceInfo(CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE, sizeof(size_t), &value, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(size_t), size);
    if (device->areOcl21FeaturesEnabled()) {
        EXPECT_EQ(value, static_cast<size_t>(device->getSharedDeviceInfo().maxMemAllocSize));
    } else {
        EXPECT_EQ(value, 0u);
    }
}

TEST(GetDeviceInfo, GivenPreferredInteropsWhenGettingDeviceInfoThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    cl_bool value = 0;
    size_t size = 0;

    auto retVal = device->getDeviceInfo(CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &value, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), size);
    EXPECT_TRUE(value == 1u);
}
TEST(GetDeviceInfo, WhenQueryingIlsWithVersionThenProperValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    cl_name_version ilsWithVersion[1];
    size_t paramRetSize;

    const auto retVal = device->getDeviceInfo(CL_DEVICE_ILS_WITH_VERSION, sizeof(ilsWithVersion), &ilsWithVersion,
                                              &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_name_version), paramRetSize);
    EXPECT_EQ(CL_MAKE_VERSION(1u, 2u, 0u), ilsWithVersion->version);
    EXPECT_STREQ("SPIR-V", ilsWithVersion->name);
}

TEST(GetDeviceInfo, WhenQueryingAtomicMemoryCapabilitiesThenProperValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    cl_device_atomic_capabilities atomicMemoryCapabilities;
    size_t paramRetSize;
    const auto retVal = device->getDeviceInfo(CL_DEVICE_ATOMIC_MEMORY_CAPABILITIES, sizeof(cl_device_atomic_capabilities),
                                              &atomicMemoryCapabilities, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_device_atomic_capabilities), paramRetSize);

    cl_device_atomic_capabilities expectedCapabilities = CL_DEVICE_ATOMIC_ORDER_RELAXED | CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP;
    if (device->areOcl21FeaturesSupported()) {
        expectedCapabilities |= CL_DEVICE_ATOMIC_ORDER_ACQ_REL | CL_DEVICE_ATOMIC_ORDER_SEQ_CST |
                                CL_DEVICE_ATOMIC_SCOPE_ALL_DEVICES | CL_DEVICE_ATOMIC_SCOPE_DEVICE;
    }
    EXPECT_EQ(expectedCapabilities, atomicMemoryCapabilities);
}

TEST(GetDeviceInfo, WhenQueryingAtomicFenceCapabilitiesThenProperValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    cl_device_atomic_capabilities atomicFenceCapabilities;
    size_t paramRetSize;
    const auto retVal = device->getDeviceInfo(CL_DEVICE_ATOMIC_FENCE_CAPABILITIES, sizeof(cl_device_atomic_capabilities),
                                              &atomicFenceCapabilities, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_device_atomic_capabilities), paramRetSize);

    cl_device_atomic_capabilities expectedCapabilities = CL_DEVICE_ATOMIC_ORDER_RELAXED | CL_DEVICE_ATOMIC_ORDER_ACQ_REL |
                                                         CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP;
    if (device->areOcl21FeaturesSupported()) {
        expectedCapabilities |= CL_DEVICE_ATOMIC_ORDER_SEQ_CST | CL_DEVICE_ATOMIC_SCOPE_ALL_DEVICES |
                                CL_DEVICE_ATOMIC_SCOPE_DEVICE | CL_DEVICE_ATOMIC_SCOPE_WORK_ITEM;
    }
    EXPECT_EQ(expectedCapabilities, atomicFenceCapabilities);
}

TEST(GetDeviceInfo, WhenQueryingDeviceEnqueueSupportThenProperValueIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};

    cl_bool deviceEnqueueSupport;
    size_t paramRetSize;
    const auto retVal = deviceFactory.rootDevices[0]->getDeviceInfo(CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES, sizeof(cl_bool),
                                                                    &deviceEnqueueSupport, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), paramRetSize);

    cl_bool expectedDeviceEnqueueSupport = CL_FALSE;
    EXPECT_EQ(expectedDeviceEnqueueSupport, deviceEnqueueSupport);
}

TEST(GetDeviceInfo, WhenQueryingDeviceEnqueueCapabilitiesThenFalseIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};

    cl_device_device_enqueue_capabilities deviceEnqueueCapabilities;
    size_t paramRetSize;
    const auto retVal = deviceFactory.rootDevices[0]->getDeviceInfo(CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES,
                                                                    sizeof(cl_device_device_enqueue_capabilities),
                                                                    &deviceEnqueueCapabilities, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_device_device_enqueue_capabilities), paramRetSize);

    EXPECT_FALSE(deviceEnqueueCapabilities);
}

TEST(GetDeviceInfo, WhenQueryingPipesSupportThenProperValueIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};

    cl_bool pipesSupport;
    size_t paramRetSize;
    const auto retVal = deviceFactory.rootDevices[0]->getDeviceInfo(CL_DEVICE_PIPE_SUPPORT, sizeof(cl_bool),
                                                                    &pipesSupport, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), paramRetSize);

    cl_bool expectedPipesSupport = deviceFactory.rootDevices[0]->arePipesSupported() ? CL_TRUE : CL_FALSE;
    EXPECT_EQ(expectedPipesSupport, pipesSupport);
}

TEST(GetDeviceInfo, WhenQueryingNonUniformWorkGroupSupportThenProperValueIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};

    cl_bool nonUniformGroupSupport;
    size_t paramRetSize;
    const auto retVal = deviceFactory.rootDevices[0]->getDeviceInfo(CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT, sizeof(cl_bool),
                                                                    &nonUniformGroupSupport, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), paramRetSize);

    cl_bool expectedNonUniformGroupSupport = CL_TRUE;
    EXPECT_EQ(expectedNonUniformGroupSupport, nonUniformGroupSupport);
}

TEST(GetDeviceInfo, WhenQueryingWorkGroupCollectiveFunctionsSupportThenProperValueIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};

    cl_bool workGroupCollectiveFunctionsSupport;
    size_t paramRetSize;
    const auto retVal = deviceFactory.rootDevices[0]->getDeviceInfo(CL_DEVICE_WORK_GROUP_COLLECTIVE_FUNCTIONS_SUPPORT, sizeof(cl_bool),
                                                                    &workGroupCollectiveFunctionsSupport, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), paramRetSize);

    cl_bool expectedWorkGroupCollectiveFunctionsSupport =
        deviceFactory.rootDevices[0]->areOcl21FeaturesSupported() ? CL_TRUE : CL_FALSE;
    EXPECT_EQ(expectedWorkGroupCollectiveFunctionsSupport, workGroupCollectiveFunctionsSupport);
}

TEST(GetDeviceInfo, WhenQueryingGenericAddressSpaceSupportThenProperValueIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};

    cl_bool genericAddressSpaceSupport;
    size_t paramRetSize;
    const auto retVal = deviceFactory.rootDevices[0]->getDeviceInfo(CL_DEVICE_GENERIC_ADDRESS_SPACE_SUPPORT, sizeof(cl_bool),
                                                                    &genericAddressSpaceSupport, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), paramRetSize);

    cl_bool expectedGenericAddressSpaceSupport = deviceFactory.rootDevices[0]->areOcl21FeaturesSupported() ? CL_TRUE : CL_FALSE;
    EXPECT_EQ(expectedGenericAddressSpaceSupport, genericAddressSpaceSupport);
}

template <typename GfxFamily, int ccsCount, int bcsCount>
class MockHwHelper : public HwHelperHw<GfxFamily> {
  public:
    const EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const override {
        EngineInstancesContainer result{};
        for (int i = 0; i < ccsCount; i++) {
            result.push_back({aub_stream::ENGINE_CCS, EngineUsage::Regular});
        }
        for (int i = 0; i < bcsCount; i++) {
            result.push_back({aub_stream::ENGINE_BCS, EngineUsage::Regular});
        }
        return result;
    }

    EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
        switch (engineType) {
        case aub_stream::ENGINE_RCS:
            return EngineGroupType::RenderCompute;
        case aub_stream::ENGINE_CCS:
        case aub_stream::ENGINE_CCS1:
        case aub_stream::ENGINE_CCS2:
        case aub_stream::ENGINE_CCS3:
            return EngineGroupType::Compute;
        case aub_stream::ENGINE_BCS:
            return EngineGroupType::Copy;
        default:
            UNRECOVERABLE_IF(true);
        }
    }

    bool isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const override {
        if ((deviceBitfield.to_ulong() == disableEngineSupportOnSubDevice) && (disabledSubDeviceEngineType == engineType)) {
            return false;
        }

        return true;
    }

    static auto overrideHwHelper() {
        return RAIIHwHelperFactory<MockHwHelper<GfxFamily, ccsCount, bcsCount>>{::defaultHwInfo->platform.eRenderCoreFamily};
    }

    uint64_t disableEngineSupportOnSubDevice = -1; // disabled by default
    aub_stream::EngineType disabledSubDeviceEngineType = aub_stream::EngineType::ENGINE_BCS;
};

using GetDeviceInfoQueueFamilyTest = ::testing::Test;

HWTEST_F(GetDeviceInfoQueueFamilyTest, givenSingleDeviceWhenInitializingCapsThenReturnCorrectFamilies) {
    auto raiiHwHelper = MockHwHelper<FamilyType, 3, 1>::overrideHwHelper();
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    UltClDeviceFactory deviceFactory{1, 0};
    ClDevice &clDevice = *deviceFactory.rootDevices[0];
    size_t paramRetSize{};
    cl_int retVal{};

    cl_queue_family_properties_intel families[CommonConstants::engineGroupCount];
    retVal = clDevice.getDeviceInfo(CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL, sizeof(families), families, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, paramRetSize / sizeof(cl_queue_family_properties_intel));

    EXPECT_EQ(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL, families[0].capabilities);
    EXPECT_EQ(3u, families[0].count);
    EXPECT_EQ(clDevice.getDeviceInfo().queueOnHostProperties, families[0].properties);

    EXPECT_EQ(clDevice.getQueueFamilyCapabilities(EngineGroupType::Copy), families[1].capabilities);
    EXPECT_EQ(1u, families[1].count);
    EXPECT_EQ(clDevice.getDeviceInfo().queueOnHostProperties, families[1].properties);
}

HWTEST_F(GetDeviceInfoQueueFamilyTest, givenSubDeviceWhenInitializingCapsThenReturnCorrectFamilies) {
    auto raiiHwHelper = MockHwHelper<FamilyType, 3, 1>::overrideHwHelper();
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    UltClDeviceFactory deviceFactory{1, 2};
    ClDevice &clDevice = *deviceFactory.subDevices[1];
    size_t paramRetSize{};
    cl_int retVal{};

    cl_queue_family_properties_intel families[CommonConstants::engineGroupCount];
    retVal = clDevice.getDeviceInfo(CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL, sizeof(families), families, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, paramRetSize / sizeof(cl_queue_family_properties_intel));

    EXPECT_EQ(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL, families[0].capabilities);
    EXPECT_EQ(3u, families[0].count);
    EXPECT_EQ(clDevice.getDeviceInfo().queueOnHostProperties, families[0].properties);

    EXPECT_EQ(clDevice.getQueueFamilyCapabilities(EngineGroupType::Copy), families[1].capabilities);
    EXPECT_EQ(1u, families[1].count);
    EXPECT_EQ(clDevice.getDeviceInfo().queueOnHostProperties, families[1].properties);
}

HWTEST_F(GetDeviceInfoQueueFamilyTest, givenSubDeviceWithoutSupportedEngineWhenInitializingCapsThenReturnCorrectFamilies) {
    constexpr int bcsCount = 1;

    using MockHwHelperT = MockHwHelper<FamilyType, 3, bcsCount>;

    auto raiiHwHelper = MockHwHelperT::overrideHwHelper();
    MockHwHelperT &mockHwHelper = static_cast<MockHwHelperT &>(raiiHwHelper.mockHwHelper);

    mockHwHelper.disableEngineSupportOnSubDevice = 0b10; // subdevice 1
    mockHwHelper.disabledSubDeviceEngineType = aub_stream::EngineType::ENGINE_BCS;

    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    UltClDeviceFactory deviceFactory{1, 2};
    ClDevice &clDevice0 = *deviceFactory.subDevices[0];
    ClDevice &clDevice1 = *deviceFactory.subDevices[1];
    size_t paramRetSize{};
    cl_int retVal{};

    // subdevice 0
    {
        cl_queue_family_properties_intel families[CommonConstants::engineGroupCount];
        retVal = clDevice0.getDeviceInfo(CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL, sizeof(families), families, &paramRetSize);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(2u, paramRetSize / sizeof(cl_queue_family_properties_intel));

        EXPECT_EQ(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL, families[0].capabilities);
        EXPECT_EQ(3u, families[0].count);
        EXPECT_EQ(clDevice0.getDeviceInfo().queueOnHostProperties, families[0].properties);

        EXPECT_EQ(clDevice0.getQueueFamilyCapabilities(EngineGroupType::Copy), families[1].capabilities);
        EXPECT_EQ(1u, families[1].count);
        EXPECT_EQ(clDevice0.getDeviceInfo().queueOnHostProperties, families[1].properties);
    }

    // subdevice 1
    {
        cl_queue_family_properties_intel families[CommonConstants::engineGroupCount];
        retVal = clDevice1.getDeviceInfo(CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL, sizeof(families), families, &paramRetSize);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(1u, paramRetSize / sizeof(cl_queue_family_properties_intel));

        EXPECT_EQ(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL, families[0].capabilities);
        EXPECT_EQ(3u, families[0].count);
        EXPECT_EQ(clDevice1.getDeviceInfo().queueOnHostProperties, families[0].properties);

        clDevice1.getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;

        MockContext context(&clDevice1);
        MockCommandQueue cmdQ(&context, &clDevice1, nullptr, false);

        EXPECT_EQ(nullptr, cmdQ.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS));
    }
}

HWTEST_F(GetDeviceInfoQueueFamilyTest, givenDeviceRootDeviceWhenInitializingCapsThenReturnDefaultFamily) {
    UltClDeviceFactory deviceFactory{1, 2};
    ClDevice &clDevice = *deviceFactory.rootDevices[0];
    size_t paramRetSize{};
    cl_int retVal{};

    cl_queue_family_properties_intel families[CommonConstants::engineGroupCount];
    retVal = clDevice.getDeviceInfo(CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL, sizeof(families), families, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, paramRetSize / sizeof(cl_queue_family_properties_intel));

    EXPECT_EQ(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL, families[0].capabilities);
    EXPECT_EQ(1u, families[0].count);
    EXPECT_EQ(clDevice.getDeviceInfo().queueOnHostProperties, families[0].properties);
}

struct GetDeviceInfo : public ::testing::TestWithParam<uint32_t /*cl_device_info*/> {
    void SetUp() override {
        param = GetParam();
    }

    cl_device_info param;
};

TEST_P(GetDeviceInfo, GivenValidParamsWhenGettingDeviceInfoThenSuccessIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    size_t sizeReturned = GetInfo::invalidSourceSize;
    auto retVal = device->getDeviceInfo(
        param,
        0,
        nullptr,
        &sizeReturned);
    if (CL_SUCCESS != retVal) {
        ASSERT_EQ(CL_SUCCESS, retVal) << " param = " << param;
    }
    ASSERT_NE(GetInfo::invalidSourceSize, sizeReturned);

    auto *object = new char[sizeReturned];
    retVal = device->getDeviceInfo(
        param,
        sizeReturned,
        object,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] object;
}

// Define new command types to run the parameterized tests
cl_device_info deviceInfoParams[] = {
    CL_DEVICE_ADDRESS_BITS,
    CL_DEVICE_AVAILABLE,
    CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL,
    CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL,
    CL_DEVICE_AVC_ME_VERSION_INTEL,
    CL_DEVICE_BUILT_IN_KERNELS,
    CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION,
    CL_DEVICE_COMPILER_AVAILABLE,
    CL_DEVICE_ILS_WITH_VERSION,
    CL_DEVICE_IL_VERSION,
    //    NOT_SUPPORTED
    //    CL_DEVICE_TERMINATE_CAPABILITY_KHR,
    CL_DEVICE_DOUBLE_FP_CONFIG,
    CL_DEVICE_ENDIAN_LITTLE,
    CL_DEVICE_ERROR_CORRECTION_SUPPORT,
    CL_DEVICE_EXECUTION_CAPABILITIES,
    CL_DEVICE_EXTENSIONS,
    CL_DEVICE_EXTENSIONS_WITH_VERSION,
    CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,
    CL_DEVICE_GLOBAL_MEM_CACHE_SIZE,
    CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,
    CL_DEVICE_GLOBAL_MEM_SIZE,
    CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE,
    CL_DEVICE_HALF_FP_CONFIG,
    CL_DEVICE_HOST_UNIFIED_MEMORY,
    CL_DEVICE_IMAGE_SUPPORT,
    CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED,
    CL_DEVICE_LINKER_AVAILABLE,
    CL_DEVICE_LOCAL_MEM_SIZE,
    CL_DEVICE_LOCAL_MEM_TYPE,
    CL_DEVICE_MAX_CLOCK_FREQUENCY,
    CL_DEVICE_MAX_COMPUTE_UNITS,
    CL_DEVICE_MAX_CONSTANT_ARGS,
    CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,
    CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE,
    CL_DEVICE_MAX_MEM_ALLOC_SIZE,
    CL_DEVICE_MAX_NUM_SUB_GROUPS,
    CL_DEVICE_MAX_ON_DEVICE_EVENTS,
    CL_DEVICE_MAX_ON_DEVICE_QUEUES,
    CL_DEVICE_MAX_PARAMETER_SIZE,
    CL_DEVICE_MAX_PIPE_ARGS,
    CL_DEVICE_MAX_SAMPLERS,
    CL_DEVICE_MAX_WORK_GROUP_SIZE,
    CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
    CL_DEVICE_MAX_WORK_ITEM_SIZES,
    CL_DEVICE_MEM_BASE_ADDR_ALIGN,
    CL_DEVICE_ME_VERSION_INTEL,
    CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,
    CL_DEVICE_NAME,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_INT,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT,
    CL_DEVICE_NUMERIC_VERSION,
    CL_DEVICE_OPENCL_C_ALL_VERSIONS,
    CL_DEVICE_OPENCL_C_FEATURES,
    CL_DEVICE_OPENCL_C_VERSION,
    CL_DEVICE_PARENT_DEVICE,
    CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
    CL_DEVICE_PARTITION_MAX_SUB_DEVICES,
    CL_DEVICE_PARTITION_PROPERTIES,
    CL_DEVICE_PARTITION_TYPE,
    CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS,
    CL_DEVICE_PIPE_MAX_PACKET_SIZE,
    CL_DEVICE_PLATFORM,
    CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_INTEROP_USER_SYNC,
    CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
    CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
    CL_DEVICE_PRINTF_BUFFER_SIZE,
    CL_DEVICE_PROFILE,
    CL_DEVICE_PROFILING_TIMER_RESOLUTION,
    CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL,
    CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE,
    CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,
    CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES,
    CL_DEVICE_QUEUE_ON_HOST_PROPERTIES,
    CL_DEVICE_REFERENCE_COUNT,
    CL_DEVICE_SINGLE_FP_CONFIG,
    CL_DEVICE_SPIR_VERSIONS,
    CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS,
    CL_DEVICE_SUB_GROUP_SIZES_INTEL,
    CL_DEVICE_SVM_CAPABILITIES,
    CL_DEVICE_TYPE,
    CL_DEVICE_VENDOR,
    CL_DEVICE_VENDOR_ID,
    CL_DEVICE_VERSION,
    CL_DRIVER_VERSION,
};

INSTANTIATE_TEST_CASE_P(
    Device_,
    GetDeviceInfo,
    testing::ValuesIn(deviceInfoParams));

TEST(GetDeviceInfoTest, givenDeviceWithSubDevicesWhenGettingNumberOfComputeUnitsThenRootDeviceExposesAllComputeUnits) {
    UltClDeviceFactory deviceFactory{1, 3};
    auto expectedComputeUnitsForSubDevice = deviceFactory.rootDevices[0]->getHardwareInfo().gtSystemInfo.EUCount;

    uint32_t expectedComputeUnitsForRootDevice = 0u;

    for (const auto &subDevice : deviceFactory.rootDevices[0]->subDevices) {
        uint32_t numComputeUnits = 0;
        size_t retSize = 0;
        auto status = clGetDeviceInfo(subDevice.get(), CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(numComputeUnits), &numComputeUnits, &retSize);
        EXPECT_EQ(CL_SUCCESS, status);
        EXPECT_EQ(expectedComputeUnitsForSubDevice, numComputeUnits);
        EXPECT_EQ(sizeof(numComputeUnits), retSize);
        expectedComputeUnitsForRootDevice += numComputeUnits;
    }
    uint32_t numComputeUnits = 0;
    size_t retSize = 0;
    auto status = clGetDeviceInfo(deviceFactory.rootDevices[0], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(numComputeUnits), &numComputeUnits, &retSize);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(expectedComputeUnitsForRootDevice, numComputeUnits);
    EXPECT_EQ(sizeof(numComputeUnits), retSize);
}

TEST(GetDeviceInfoTest, givenPciBusInfoWhenGettingPciBusInfoForDeviceThenPciBusInfoIsReturned) {
    PhysicalDevicePciBusInfo pciBusInfo(0, 1, 2, 3);

    auto driverInfo = new DriverInfoMock();
    driverInfo->setPciBusInfo(pciBusInfo);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->driverInfo.reset(driverInfo);
    device->initializeCaps();

    cl_device_pci_bus_info_khr devicePciBusInfo;

    size_t sizeReturned = 0;
    auto retVal = device->getDeviceInfo(CL_DEVICE_PCI_BUS_INFO_KHR, 0, nullptr, &sizeReturned);

    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_EQ(sizeReturned, sizeof(devicePciBusInfo));

    retVal = device->getDeviceInfo(CL_DEVICE_PCI_BUS_INFO_KHR, sizeof(devicePciBusInfo), &devicePciBusInfo, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(devicePciBusInfo.pci_domain, pciBusInfo.pciDomain);
    EXPECT_EQ(devicePciBusInfo.pci_bus, pciBusInfo.pciBus);
    EXPECT_EQ(devicePciBusInfo.pci_device, pciBusInfo.pciDevice);
    EXPECT_EQ(devicePciBusInfo.pci_function, pciBusInfo.pciFunction);
}

TEST(GetDeviceInfoTest, givenPciBusInfoIsNotAvailableWhenGettingPciBusInfoForDeviceThenInvalidValueIsReturned) {
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue);

    auto driverInfo = new DriverInfoMock();
    driverInfo->setPciBusInfo(pciBusInfo);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->driverInfo.reset(driverInfo);
    device->initializeCaps();

    auto retVal = device->getDeviceInfo(CL_DEVICE_PCI_BUS_INFO_KHR, 0, nullptr, nullptr);

    ASSERT_EQ(retVal, CL_INVALID_VALUE);
}

struct DeviceAttributeQueryTest : public ::testing::TestWithParam<uint32_t /*cl_device_info*/> {
    void SetUp() override {
        param = GetParam();
    }

    void verifyDeviceAttribute(ClDevice &device) {
        size_t sizeReturned = GetInfo::invalidSourceSize;
        auto retVal = device.getDeviceInfo(
            param,
            0,
            nullptr,
            &sizeReturned);
        if (CL_SUCCESS != retVal) {
            ASSERT_EQ(CL_SUCCESS, retVal) << " param = " << param;
        }
        ASSERT_NE(GetInfo::invalidSourceSize, sizeReturned);

        auto object = std::make_unique<char[]>(sizeReturned);
        retVal = device.getDeviceInfo(
            param,
            sizeReturned,
            object.get(),
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        switch (param) {
        case CL_DEVICE_IP_VERSION_INTEL: {
            auto pDeviceIpVersion = reinterpret_cast<cl_version *>(object.get());
            auto &hwInfo = device.getHardwareInfo();
            auto &clHwHelper = NEO::ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);
            EXPECT_EQ(clHwHelper.getDeviceIpVersion(hwInfo), *pDeviceIpVersion);
            EXPECT_EQ(sizeof(cl_version), sizeReturned);
            break;
        }
        case CL_DEVICE_ID_INTEL: {
            auto pDeviceId = reinterpret_cast<cl_uint *>(object.get());
            EXPECT_EQ(device.getHardwareInfo().platform.usDeviceID, *pDeviceId);
            EXPECT_EQ(sizeof(cl_uint), sizeReturned);
            break;
        }
        case CL_DEVICE_NUM_SLICES_INTEL: {
            auto pNumSlices = reinterpret_cast<cl_uint *>(object.get());
            const auto &gtSysInfo = device.getHardwareInfo().gtSystemInfo;
            EXPECT_EQ(gtSysInfo.SliceCount * std::max(device.getNumGenericSubDevices(), 1u), *pNumSlices);
            EXPECT_EQ(sizeof(cl_uint), sizeReturned);
            break;
        }
        case CL_DEVICE_NUM_SUB_SLICES_PER_SLICE_INTEL: {
            auto pNumSubslicesPerSlice = reinterpret_cast<cl_uint *>(object.get());
            const auto &gtSysInfo = device.getHardwareInfo().gtSystemInfo;
            EXPECT_EQ(gtSysInfo.SubSliceCount / gtSysInfo.SliceCount, *pNumSubslicesPerSlice);
            EXPECT_EQ(sizeof(cl_uint), sizeReturned);
            break;
        }
        case CL_DEVICE_NUM_EUS_PER_SUB_SLICE_INTEL: {
            auto pNumEusPerSubslice = reinterpret_cast<cl_uint *>(object.get());
            const auto &gtSysInfo = device.getHardwareInfo().gtSystemInfo;
            EXPECT_EQ(gtSysInfo.MaxEuPerSubSlice, *pNumEusPerSubslice);
            EXPECT_EQ(sizeof(cl_uint), sizeReturned);
            break;
        }
        case CL_DEVICE_NUM_THREADS_PER_EU_INTEL: {
            auto pNumThreadsPerEu = reinterpret_cast<cl_uint *>(object.get());
            const auto &gtSysInfo = device.getHardwareInfo().gtSystemInfo;
            EXPECT_EQ(gtSysInfo.ThreadCount / gtSysInfo.EUCount, *pNumThreadsPerEu);
            EXPECT_EQ(sizeof(cl_uint), sizeReturned);
            break;
        }
        case CL_DEVICE_FEATURE_CAPABILITIES_INTEL: {
            auto pCapabilities = reinterpret_cast<cl_device_feature_capabilities_intel *>(object.get());
            auto &hwInfo = device.getHardwareInfo();
            auto &clHwHelper = ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);
            EXPECT_EQ(clHwHelper.getSupportedDeviceFeatureCapabilities(), *pCapabilities);
            EXPECT_EQ(sizeof(cl_device_feature_capabilities_intel), sizeReturned);
            break;
        }
        default:
            EXPECT_TRUE(false);
            break;
        }
    }

    cl_device_info param;
};

TEST_P(DeviceAttributeQueryTest, givenGetDeviceInfoWhenDeviceAttributeIsQueriedOnClDeviceThenReturnCorrectAttributeValue) {
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ASSERT_EQ(0u, pClDevice->getNumGenericSubDevices());

    verifyDeviceAttribute(*pClDevice);
}

TEST_P(DeviceAttributeQueryTest, givenGetDeviceInfoWhenDeviceAttributeIsQueriedOnRootDeviceAndSubDevicesThenReturnCorrectAttributeValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

    auto pRootClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ASSERT_EQ(2u, pRootClDevice->subDevices.size());

    verifyDeviceAttribute(*pRootClDevice);

    for (const auto &pClSubDevice : pRootClDevice->subDevices) {
        verifyDeviceAttribute(*pClSubDevice);
    }
}

cl_device_info deviceAttributeQueryParams[] = {
    CL_DEVICE_IP_VERSION_INTEL,
    CL_DEVICE_ID_INTEL,
    CL_DEVICE_NUM_SLICES_INTEL,
    CL_DEVICE_NUM_SUB_SLICES_PER_SLICE_INTEL,
    CL_DEVICE_NUM_EUS_PER_SUB_SLICE_INTEL,
    CL_DEVICE_NUM_THREADS_PER_EU_INTEL,
    CL_DEVICE_FEATURE_CAPABILITIES_INTEL};

INSTANTIATE_TEST_CASE_P(
    Device_,
    DeviceAttributeQueryTest,
    testing::ValuesIn(deviceAttributeQueryParams));
