/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_info.h"
#include "runtime/device/device.h"
#include "unit_tests/api/cl_api_tests.h"

#include <cstring>

using namespace NEO;

namespace ULT {

//------------------------------------------------------------------------------
struct GetDeviceInfoP : public ApiFixture, public ::testing::TestWithParam<uint32_t /*cl_device_info*/> {
    void SetUp() override {
        param = GetParam();
        ApiFixture::SetUp();
    }

    void TearDown() override { ApiFixture::TearDown(); }

    cl_device_info param;
};

typedef GetDeviceInfoP GetDeviceGlInfoStr;

TEST_P(GetDeviceGlInfoStr, StringType) {
    char *paramValue = nullptr;
    size_t paramRetSize = 0;

    cl_int retVal = clGetDeviceInfo(devices[0], param, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    paramValue = new char[paramRetSize];

    retVal = clGetDeviceInfo(devices[0], param, paramRetSize, paramValue, nullptr);

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
            "cl_khr_global_int32_base_atomics ",
            "cl_khr_global_int32_extended_atomics ",
            "cl_khr_icd ",
            "cl_khr_local_int32_base_atomics ",
            "cl_khr_local_int32_extended_atomics ",
            "cl_intel_subgroups ",
            "cl_intel_required_subgroup_size ",
            "cl_intel_subgroups_short ",
            "cl_khr_spir ",
            "cl_intel_accelerator ",
            "cl_intel_driver_diagnostics ",
            "cl_khr_priority_hints ",
            "cl_khr_throttle_hints ",
            "cl_khr_create_command_queue ",
            "cl_khr_gl_depth_images",
            "cl_khr_gl_event",
            "cl_khr_gl_msaa_sharing",
        };

        for (auto element = 0u; element < sizeof(supportedExtensions) / sizeof(supportedExtensions[0]); element++) {
            auto foundOffset = extensionString.find(supportedExtensions[element]);
            EXPECT_TRUE(foundOffset != std::string::npos);
            EXPECT_GE(foundOffset, currentOffset);
            currentOffset = foundOffset;
        }
    }

    delete[] paramValue;
}

// Define new command types to run the parameterized tests
static cl_device_info deviceInfoStrParams[] = {
    CL_DEVICE_BUILT_IN_KERNELS, CL_DEVICE_EXTENSIONS, CL_DEVICE_NAME, CL_DEVICE_OPENCL_C_VERSION,
    CL_DEVICE_PROFILE, CL_DEVICE_VENDOR, CL_DEVICE_VERSION, CL_DRIVER_VERSION};

INSTANTIATE_TEST_CASE_P(api, GetDeviceGlInfoStr, testing::ValuesIn(deviceInfoStrParams));

} // namespace ULT
