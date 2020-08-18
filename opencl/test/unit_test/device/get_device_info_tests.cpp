/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "opencl/source/cl_device/cl_device_info_map.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"
#include "test.h"

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

HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceInfoMemCapabilitiesTest, GivenValidParametersWhenGetDeviceInfoIsCalledForBdwPlusThenClSuccessIsReturned) {

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

    retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(16352u, value);
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

    if (device->areOcl21FeaturesSupported()) {
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(sizeof(cl_name_version), paramRetSize);
        EXPECT_EQ(CL_MAKE_VERSION(1u, 2u, 0u), ilsWithVersion->version);
        EXPECT_STREQ("SPIR-V", ilsWithVersion->name);
    } else {
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(0u, paramRetSize);
    }
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

    cl_bool expectedDeviceEnqueueSupport = deviceFactory.rootDevices[0]->isDeviceEnqueueSupported() ? CL_TRUE : CL_FALSE;
    EXPECT_EQ(expectedDeviceEnqueueSupport, deviceEnqueueSupport);
}

TEST(GetDeviceInfo, WhenQueryingDeviceEnqueueCapabilitiesThenProperValueIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};

    cl_device_device_enqueue_capabilities deviceEnqueueCapabilities;
    size_t paramRetSize;
    const auto retVal = deviceFactory.rootDevices[0]->getDeviceInfo(CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES,
                                                                    sizeof(cl_device_device_enqueue_capabilities),
                                                                    &deviceEnqueueCapabilities, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_device_device_enqueue_capabilities), paramRetSize);

    cl_device_device_enqueue_capabilities expectedDeviceEnqueueCapabilities =
        deviceFactory.rootDevices[0]->isDeviceEnqueueSupported()
            ? CL_DEVICE_QUEUE_SUPPORTED | CL_DEVICE_QUEUE_REPLACEABLE_DEFAULT
            : 0u;
    EXPECT_EQ(expectedDeviceEnqueueCapabilities, deviceEnqueueCapabilities);
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

    cl_bool expectedNonUniformGroupSupport = deviceFactory.rootDevices[0]->areOcl21FeaturesSupported() ? CL_TRUE : CL_FALSE;
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
