/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"
#include "runtime/helpers/hw_info.h"

#include "cl_api_tests.h"

#include <cstring>

using namespace NEO;

using clGetDeviceInfoTests = api_tests;

namespace ULT {

TEST_F(clGetDeviceInfoTests, givenNeoDeviceWhenAskedForSliceCountThenNumberOfSlicesIsReturned) {
    cl_device_info paramName = 0;
    size_t paramSize = 0;
    void *paramValue = nullptr;
    size_t paramRetSize = 0;

    size_t numSlices = 0;
    paramName = CL_DEVICE_SLICE_COUNT_INTEL;

    retVal = clGetDeviceInfo(
        devices[0],
        paramName,
        0,
        nullptr,
        &paramRetSize);

    EXPECT_EQ(sizeof(size_t), paramRetSize);
    paramSize = paramRetSize;
    paramValue = &numSlices;

    retVal = clGetDeviceInfo(
        devices[0],
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);

    EXPECT_EQ(platformDevices[0]->gtSystemInfo.SliceCount, numSlices);
}

TEST_F(clGetDeviceInfoTests, GivenGpuDeviceWhenGettingDeviceInfoThenDeviceTypeGpuIsReturned) {
    cl_device_info paramName = 0;
    size_t paramSize = 0;
    void *paramValue = nullptr;
    size_t paramRetSize = 0;

    cl_device_type deviceType = CL_DEVICE_TYPE_CPU; // set to wrong value
    paramName = CL_DEVICE_TYPE;
    paramSize = sizeof(cl_device_type);
    paramValue = &deviceType;

    retVal = clGetDeviceInfo(
        devices[0],
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_device_type>(CL_DEVICE_TYPE_GPU), deviceType);
}

TEST_F(clGetDeviceInfoTests, GivenNullDeviceWhenGettingDeviceInfoThenInvalidDeviceErrorIsReturned) {
    size_t paramRetSize = 0;

    retVal = clGetDeviceInfo(
        nullptr,
        CL_DEVICE_TYPE,
        0,
        nullptr,
        &paramRetSize);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clGetDeviceInfoTests, givenOpenCLDeviceWhenAskedForSupportedSvmTypeCorrectValueIsReturned) {

    cl_device_svm_capabilities svmCaps;

    retVal = clGetDeviceInfo(
        devices[0],
        CL_DEVICE_SVM_CAPABILITIES,
        sizeof(cl_device_svm_capabilities),
        &svmCaps,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const HardwareInfo &hwInfo = pPlatform->getDevice(0)->getHardwareInfo();

    cl_device_svm_capabilities expectedCaps = 0;
    if (hwInfo.capabilityTable.ftrSvm != 0) {
        if (hwInfo.capabilityTable.ftrSupportsCoherency != 0) {
            expectedCaps = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER | CL_DEVICE_SVM_FINE_GRAIN_BUFFER | CL_DEVICE_SVM_ATOMICS;
        } else {
            expectedCaps = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;
        }
    }
    EXPECT_EQ(svmCaps, expectedCaps);
}

TEST_F(clGetDeviceInfoTests, givenNeoDeviceWhenAskedForDriverVersionThenNeoIsReturned) {
    cl_device_info paramName = 0;
    size_t paramSize = 0;
    void *paramValue = nullptr;
    size_t paramRetSize = 0;

    cl_uint driverVersion = 0;
    paramName = CL_DEVICE_DRIVER_VERSION_INTEL;

    retVal = clGetDeviceInfo(
        devices[0],
        paramName,
        0,
        nullptr,
        &paramRetSize);

    EXPECT_EQ(sizeof(cl_uint), paramRetSize);
    paramSize = paramRetSize;
    paramValue = &driverVersion;

    retVal = clGetDeviceInfo(
        devices[0],
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);

    EXPECT_EQ((cl_uint)CL_DEVICE_DRIVER_VERSION_INTEL_NEO1, driverVersion);
}

TEST_F(clGetDeviceInfoTests, GivenClDeviceExtensionsParamWhenGettingDeviceInfoThenAllExtensionsAreListed) {
    size_t paramRetSize = 0;

    cl_int retVal = clGetDeviceInfo(
        devices[0],
        CL_DEVICE_EXTENSIONS,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    auto paramValue = std::make_unique<char[]>(paramRetSize);

    retVal = clGetDeviceInfo(
        devices[0],
        CL_DEVICE_EXTENSIONS,
        paramRetSize,
        paramValue.get(),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    std::string extensionString(paramValue.get());
    std::string supportedExtensions[] = {
        "cl_khr_3d_image_writes ",
        "cl_khr_byte_addressable_store ",
        "cl_khr_fp16 ",
        "cl_khr_depth_images ",
        "cl_khr_global_int32_base_atomics ",
        "cl_khr_global_int32_extended_atomics ",
        "cl_khr_local_int32_base_atomics ",
        "cl_khr_local_int32_extended_atomics ",
        "cl_intel_subgroups ",
        "cl_intel_required_subgroup_size ",
        "cl_intel_subgroups_short ",
        "cl_khr_spir ",
        "cl_intel_accelerator ",
        "cl_intel_media_block_io ",
        "cl_intel_driver_diagnostics ",
    };

    for (auto element = 0u; element < sizeof(supportedExtensions) / sizeof(supportedExtensions[0]); element++) {
        auto foundOffset = extensionString.find(supportedExtensions[element]);
        EXPECT_TRUE(foundOffset != std::string::npos);
    }
}

TEST_F(clGetDeviceInfoTests, GivenClDeviceIlVersionParamAndOcl21WhenGettingDeviceInfoThenSpirv12IsReturned) {
    size_t paramRetSize = 0;

    Device *pDevice = castToObject<Device>(devices[0]);

    if (pDevice->getSupportedClVersion() < 21)
        return;

    cl_int retVal = clGetDeviceInfo(
        devices[0],
        CL_DEVICE_IL_VERSION,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    auto paramValue = std::make_unique<char[]>(paramRetSize);

    retVal = clGetDeviceInfo(
        devices[0],
        CL_DEVICE_IL_VERSION,
        paramRetSize,
        paramValue.get(),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_STREQ("SPIR-V_1.2 ", paramValue.get());
}

//------------------------------------------------------------------------------
struct GetDeviceInfoP : public api_fixture,
                        public ::testing::TestWithParam<uint32_t /*cl_device_info*/> {
    void SetUp() override {
        param = GetParam();
        api_fixture::SetUp();
    }

    void TearDown() override {
        api_fixture::TearDown();
    }

    cl_device_info param;
};

typedef GetDeviceInfoP GetDeviceInfoStr;

TEST_P(GetDeviceInfoStr, GivenStringTypeParamWhenGettingDeviceInfoThenSuccessIsReturned) {
    size_t paramRetSize = 0;

    cl_int retVal = clGetDeviceInfo(
        devices[0],
        param,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    auto paramValue = std::make_unique<char[]>(paramRetSize);

    retVal = clGetDeviceInfo(
        devices[0],
        param,
        paramRetSize,
        paramValue.get(),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

static cl_device_info deviceInfoStrParams[] =
    {
        CL_DEVICE_BUILT_IN_KERNELS,
        CL_DEVICE_NAME,
        CL_DEVICE_OPENCL_C_VERSION,
        CL_DEVICE_PROFILE,
        CL_DEVICE_VENDOR,
        CL_DEVICE_VERSION,
        CL_DRIVER_VERSION};

INSTANTIATE_TEST_CASE_P(
    api,
    GetDeviceInfoStr,
    testing::ValuesIn(deviceInfoStrParams));

typedef GetDeviceInfoP GetDeviceInfoVectorWidth;

TEST_P(GetDeviceInfoVectorWidth, GivenParamTypeVectorWhenGettingDeviceInfoThenSizeIsGreaterThanZeroAndValueIsGreaterThanZero) {
    cl_uint paramValue = 0;
    size_t paramRetSize = 0;

    auto retVal = clGetDeviceInfo(
        devices[0],
        param,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), paramRetSize);

    retVal = clGetDeviceInfo(
        devices[0],
        param,
        paramRetSize,
        &paramValue,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(paramValue, 0u);
}

cl_device_info devicePreferredVector[] = {
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT};

INSTANTIATE_TEST_CASE_P(
    api,
    GetDeviceInfoVectorWidth,
    testing::ValuesIn(devicePreferredVector));

} // namespace ULT
