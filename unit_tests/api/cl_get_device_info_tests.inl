/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstring>
#include "cl_api_tests.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/device/device.h"

using namespace OCLRT;

typedef api_tests clGetDeviceInfoTests;

namespace ULT {

TEST_F(clGetDeviceInfoTests, DeviceType) {
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

TEST_F(clGetDeviceInfoTests, NullDevice) {
    size_t paramRetSize = 0;

    retVal = clGetDeviceInfo(
        nullptr,
        CL_DEVICE_TYPE,
        0,
        nullptr,
        &paramRetSize);

    EXPECT_NE(CL_SUCCESS, retVal);
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

TEST_P(GetDeviceInfoStr, StringType) {
    char *paramValue = nullptr;
    size_t paramRetSize = 0;

    cl_int retVal = clGetDeviceInfo(
        devices[0],
        param,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    paramValue = new char[paramRetSize];

    retVal = clGetDeviceInfo(
        devices[0],
        param,
        paramRetSize,
        paramValue,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GE(std::strlen(paramValue), 0u);

    // check for extensions
    if (param == CL_DEVICE_EXTENSIONS) {
        std::string extensionString(paramValue);
        size_t currentOffset = 0u;
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
            "cl_intel_device_side_avc_motion_estimation ",
        };

        for (auto element = 0u; element < sizeof(supportedExtensions) / sizeof(supportedExtensions[0]); element++) {
            auto foundOffset = extensionString.find(supportedExtensions[element]);
            EXPECT_TRUE(foundOffset != std::string::npos);
            EXPECT_GE(foundOffset, currentOffset);
            currentOffset = foundOffset;
        }
    }

    // check IL versions
    if (param == CL_DEVICE_IL_VERSION) {
        Device *pDevice = castToObject<Device>(devices[0]);

        // do not test if not supported
        if (pDevice->getSupportedClVersion() >= 21) {
            std::string versionString(paramValue);
            size_t currentOffset = 0u;
            std::string supportedVersions[] = {
                "SPIR-V_1.0 ",
            };

            for (auto element = 0u; element < sizeof(supportedVersions) / sizeof(supportedVersions[0]); element++) {
                auto foundOffset = versionString.find(supportedVersions[element]);
                EXPECT_TRUE(foundOffset != std::string::npos);
                EXPECT_GE(foundOffset, currentOffset);
                currentOffset = foundOffset;
            }
        }
    }

    delete[] paramValue;
}

static cl_device_info deviceInfoStrParams[] =
    {
        //        CL_DEVICE_VERSION
        CL_DEVICE_BUILT_IN_KERNELS,
        CL_DEVICE_EXTENSIONS,
        CL_DEVICE_IL_VERSION, // OpenCL 2.1
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

TEST_P(GetDeviceInfoVectorWidth, GreatherThanZero) {
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

cl_device_info devicePrefferredVector[] = {
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT};

INSTANTIATE_TEST_CASE_P(
    api,
    GetDeviceInfoVectorWidth,
    testing::ValuesIn(devicePrefferredVector));

} // namespace ULT
