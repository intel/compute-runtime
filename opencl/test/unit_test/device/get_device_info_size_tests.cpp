/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/string.h"

#include "opencl/source/cl_device/cl_device_info_map.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {
extern const char *latestConformanceVersionPassed;
} // namespace NEO

using namespace NEO;

struct GetDeviceInfoSize : public ::testing::TestWithParam<std::pair<uint32_t /*cl_device_info*/, size_t>> {
    void SetUp() override {
        param = GetParam();
    }

    std::pair<uint32_t, size_t> param;
};

TEST_P(GetDeviceInfoSize, GivenParamWhenGettingDeviceInfoThenSizeIsValid) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    size_t sizeReturned = GetInfo::invalidSourceSize;
    auto retVal = device->getDeviceInfo(
        param.first,
        0,
        nullptr,
        &sizeReturned);
    if (CL_SUCCESS != retVal) {
        ASSERT_EQ(CL_SUCCESS, retVal) << " param = " << param.first;
    }
    ASSERT_NE(GetInfo::invalidSourceSize, sizeReturned);
    EXPECT_EQ(param.second, sizeReturned);
}

std::pair<uint32_t, size_t> deviceInfoParams2[] = {
    {CL_DEVICE_ADDRESS_BITS, sizeof(cl_uint)},
    {CL_DEVICE_AVAILABLE, sizeof(cl_bool)},
    //    {CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION, sizeof(cl_name_version[])},
    {CL_DEVICE_COMPILER_AVAILABLE, sizeof(cl_bool)},
    {CL_DEVICE_DOUBLE_FP_CONFIG, sizeof(cl_device_fp_config)},
    {CL_DEVICE_ENDIAN_LITTLE, sizeof(cl_bool)},
    {CL_DEVICE_ERROR_CORRECTION_SUPPORT, sizeof(cl_bool)},
    {CL_DEVICE_EXECUTION_CAPABILITIES, sizeof(cl_device_exec_capabilities)},
    //    {CL_DEVICE_EXTENSIONS, sizeof(char[])},
    //    {CL_DEVICE_EXTENSIONS_WITH_VERSION, sizeof(cl_name_version[])},
    {CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(cl_ulong)},
    {CL_DEVICE_GLOBAL_MEM_CACHE_TYPE, sizeof(cl_device_mem_cache_type)},
    {CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, sizeof(cl_uint)},
    {CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong)},
    {CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE, sizeof(size_t)},
    {CL_DEVICE_ILS_WITH_VERSION, sizeof(cl_name_version[1])},
    {CL_DEVICE_IL_VERSION, sizeof(char[12])},
    {CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool)},
    {CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED, strlen(latestConformanceVersionPassed) + 1},
    {CL_DEVICE_LINKER_AVAILABLE, sizeof(cl_bool)},
    {CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong)},
    {CL_DEVICE_LOCAL_MEM_TYPE, sizeof(cl_device_local_mem_type)},
    {CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint)},
    {CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_CONSTANT_ARGS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong)},
    {CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE, sizeof(size_t)},
    {CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong)},
    {CL_DEVICE_MAX_NUM_SUB_GROUPS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_ON_DEVICE_EVENTS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_ON_DEVICE_QUEUES, sizeof(cl_uint)},
    {CL_DEVICE_MAX_PARAMETER_SIZE, sizeof(size_t)},
    {CL_DEVICE_MAX_PIPE_ARGS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_SAMPLERS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t)},
    {CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t[3])},
    {CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(cl_uint)},
    //    {CL_DEVICE_NAME, sizeof(char[])},
    {CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR, sizeof(cl_uint)},
    {CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT, sizeof(cl_uint)},
    {CL_DEVICE_NATIVE_VECTOR_WIDTH_INT, sizeof(cl_uint)},
    {CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG, sizeof(cl_uint)},
    {CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT, sizeof(cl_uint)},
    {CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE, sizeof(cl_uint)},
    {CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF, sizeof(cl_uint)},
    {CL_DEVICE_NUMERIC_VERSION, sizeof(cl_version)},
    //    {CL_DEVICE_OPENCL_C_ALL_VERSIONS, sizeof(cl_name_version[])},
    //    {CL_DEVICE_OPENCL_C_FEATURES, sizeof(cl_name_version[])},
    //    {CL_DEVICE_OPENCL_C_VERSION, sizeof(char[])},
    {CL_DEVICE_PARENT_DEVICE, sizeof(cl_device_id)},
    {CL_DEVICE_PARTITION_AFFINITY_DOMAIN, sizeof(cl_device_affinity_domain)},
    {CL_DEVICE_PARTITION_MAX_SUB_DEVICES, sizeof(cl_uint)},
    {CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS, sizeof(cl_uint)},
    {CL_DEVICE_PIPE_MAX_PACKET_SIZE, sizeof(cl_uint)},
    {CL_DEVICE_PLATFORM, sizeof(cl_platform_id)},
    {CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool)},
    {CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF, sizeof(cl_uint)},
    {CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t)},
    {CL_DEVICE_PRINTF_BUFFER_SIZE, sizeof(size_t)},
    //    {CL_DEVICE_PROFILE, sizeof(char[])},
    {CL_DEVICE_PROFILING_TIMER_RESOLUTION, sizeof(size_t)},
    {CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE, sizeof(cl_uint)},
    {CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE, sizeof(cl_uint)},
    {CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES, sizeof(cl_command_queue_properties)},
    {CL_DEVICE_QUEUE_ON_HOST_PROPERTIES, sizeof(cl_command_queue_properties)},
    {CL_DEVICE_REFERENCE_COUNT, sizeof(cl_uint)},
    {CL_DEVICE_SINGLE_FP_CONFIG, sizeof(cl_device_fp_config)},
    //    {CL_DEVICE_SPIR_VERSIONS, sizeof(char[])},
    {CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS, sizeof(cl_bool)},
    {CL_DEVICE_SVM_CAPABILITIES, sizeof(cl_device_svm_capabilities)},
    //    {CL_DEVICE_TERMINATE_CAPABILITY_KHR, sizeof(cl_device_terminate_capability_khr)},
    {CL_DEVICE_TYPE, sizeof(cl_device_type)},
    //    {CL_DEVICE_VENDOR, sizeof(char[])},
    {CL_DEVICE_VENDOR_ID, sizeof(cl_uint)},
    //    {CL_DEVICE_VERSION, sizeof(char[])},
    //    {CL_DRIVER_VERSION, sizeof(char[])},
};

INSTANTIATE_TEST_CASE_P(
    Device_,
    GetDeviceInfoSize,
    testing::ValuesIn(deviceInfoParams2));

struct GetDeviceInfoForImage : public GetDeviceInfoSize {};

TEST_P(GetDeviceInfoForImage, GivenParamWhenGettingDeviceInfoThenSizeIsValid) {
    auto device = std::make_unique<ClDevice>(*MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr), platform());
    if (!device->getSharedDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }
    size_t sizeReturned = 0;
    auto retVal = device->getDeviceInfo(
        param.first,
        0,
        nullptr,
        &sizeReturned);
    if (CL_SUCCESS != retVal) {
        ASSERT_EQ(CL_SUCCESS, retVal) << " param = " << param.first;
    }
    ASSERT_NE(0u, sizeReturned);
    EXPECT_EQ(param.second, sizeReturned);
}

TEST_P(GetDeviceInfoForImage, whenImageAreNotSupportedThenClSuccessAndSizeofCluintIsReturned) {
    auto device = std::make_unique<ClDevice>(*MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr), platform());
    if (device->getSharedDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }
    size_t sizeReturned = 0;
    auto retVal = device->getDeviceInfo(
        param.first,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(param.second, sizeReturned);
}

TEST_P(GetDeviceInfoForImage, givenInfoImageParamsWhenCallGetDeviceInfoForImageThenSizeIsValidAndTrueReturned) {
    auto device = std::make_unique<ClDevice>(*MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr), platform());
    size_t srcSize = 0;
    size_t retSize = 0;
    const void *src = nullptr;
    auto retVal = device->getDeviceInfoForImage(
        param.first,
        src,
        srcSize,
        retSize);
    EXPECT_TRUE(retVal);
    ASSERT_NE(0u, srcSize);
    EXPECT_EQ(param.second, srcSize);
    EXPECT_EQ(param.second, retSize);
}

TEST(GetDeviceInfoForImage, givenNotImageParamWhenCallGetDeviceInfoForImageThenSizeIsNotValidAndFalseReturned) {
    auto device = std::make_unique<ClDevice>(*MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr), platform());
    size_t srcSize = 0;
    size_t retSize = 0;
    const void *src = nullptr;
    cl_device_info notImageParam = CL_DEVICE_ADDRESS_BITS;
    size_t paramSize = sizeof(cl_uint);
    auto retVal = device->getDeviceInfoForImage(
        notImageParam,
        src,
        srcSize,
        retSize);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(0u, srcSize);
    EXPECT_NE(paramSize, srcSize);
    EXPECT_NE(paramSize, retSize);
}

std::pair<uint32_t, size_t> deviceInfoImageParams[] = {
    {CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(size_t)},
    {CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(size_t)},
    {CL_DEVICE_IMAGE3D_MAX_DEPTH, sizeof(size_t)},
    {CL_DEVICE_IMAGE3D_MAX_HEIGHT, sizeof(size_t)},
    {CL_DEVICE_IMAGE3D_MAX_WIDTH, sizeof(size_t)},
    {CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT, sizeof(cl_uint)},
    {CL_DEVICE_IMAGE_MAX_ARRAY_SIZE, sizeof(size_t)},
    {CL_DEVICE_IMAGE_MAX_BUFFER_SIZE, sizeof(size_t)},
    {CL_DEVICE_IMAGE_PITCH_ALIGNMENT, sizeof(cl_uint)},
    {CL_DEVICE_MAX_READ_IMAGE_ARGS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS, sizeof(cl_uint)},
    {CL_DEVICE_MAX_WRITE_IMAGE_ARGS, sizeof(cl_uint)},
};

INSTANTIATE_TEST_CASE_P(
    Device_,
    GetDeviceInfoForImage,
    testing::ValuesIn(deviceInfoImageParams));

TEST(DeviceInfoTests, givenDefaultDeviceWhenQueriedForDeviceVersionThenProperSizeIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    size_t sizeReturned = 0;
    auto retVal = device->getDeviceInfo(
        CL_DEVICE_VERSION,
        0,
        nullptr,
        &sizeReturned);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(16u, sizeReturned);
    std::unique_ptr<char[]> deviceVersion(new char[sizeReturned]);

    retVal = device->getDeviceInfo(
        CL_DEVICE_VERSION,
        sizeReturned,
        deviceVersion.get(),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(DeviceInfoTests, givenDefaultDeviceWhenQueriedForBuiltInKernelsWithVersionThenProperSizeIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    size_t sizeReturned = 0;
    auto retVal = pClDevice->getDeviceInfo(
        CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION,
        0,
        nullptr,
        &sizeReturned);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pClDevice->getDeviceInfo().builtInKernelsWithVersion.size() * sizeof(cl_name_version), sizeReturned);
}

TEST(DeviceInfoTests, givenDefaultDeviceWhenQueriedForOpenclCAllVersionsThenProperSizeIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    size_t sizeReturned = 0;
    auto retVal = pClDevice->getDeviceInfo(
        CL_DEVICE_OPENCL_C_ALL_VERSIONS,
        0,
        nullptr,
        &sizeReturned);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pClDevice->getDeviceInfo().openclCAllVersions.size() * sizeof(cl_name_version), sizeReturned);
}

TEST(DeviceInfoTests, givenDefaultDeviceWhenQueriedForOpenclCFeaturesThenProperSizeIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    size_t sizeReturned = 0;
    auto retVal = pClDevice->getDeviceInfo(
        CL_DEVICE_OPENCL_C_FEATURES,
        0,
        nullptr,
        &sizeReturned);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pClDevice->getDeviceInfo().openclCFeatures.size() * sizeof(cl_name_version), sizeReturned);
}

TEST(DeviceInfoTests, givenDefaultDeviceWhenQueriedForExtensionsWithVersionThenProperSizeIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    size_t sizeReturned = 0;
    auto retVal = pClDevice->getDeviceInfo(
        CL_DEVICE_EXTENSIONS_WITH_VERSION,
        0,
        nullptr,
        &sizeReturned);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pClDevice->getDeviceInfo().extensionsWithVersion.size() * sizeof(cl_name_version), sizeReturned);
}
