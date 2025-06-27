/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

#include <cstring>

using namespace NEO;

namespace ULT {

struct GetDeviceInfoP : public ApiFixture<>, public ::testing::TestWithParam<uint32_t /*cl_device_info*/> {
    void SetUp() override {
        param = GetParam();
        ApiFixture::setUp();
    }

    void TearDown() override { ApiFixture::tearDown(); }

    cl_device_info param;
};

using GetDeviceGlInfoStr = GetDeviceInfoP;

TEST_P(GetDeviceGlInfoStr, WhenGettingDeviceExtensionsThenExtensionsAreReportedCorrectly) {
    char *paramValue = nullptr;
    size_t paramRetSize = 0;
    auto &productHelper = pDevice->getProductHelper();
    bool clGLSharingAllowed = productHelper.isSharingWith3dOrMediaAllowed();

    cl_int retVal = clGetDeviceInfo(testedClDevice, param, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    paramValue = new char[paramRetSize];

    retVal = clGetDeviceInfo(testedClDevice, param, paramRetSize, paramValue, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GE(std::strlen(paramValue), 0u);

    // check for extensions
    if (param == CL_DEVICE_EXTENSIONS) {
        std::string extensionString(paramValue);
        size_t currentOffset = 0u;
        std::string supportedExtensions[] = {
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
        };

        for (auto &element : supportedExtensions) {
            auto foundOffset = extensionString.find(element);
            EXPECT_TRUE(foundOffset != std::string::npos);
            EXPECT_GE(foundOffset, currentOffset);
            currentOffset = foundOffset;
        }

        if (clGLSharingAllowed) {
            std::string optionalSupportedExtensions[] = {
                "cl_khr_gl_depth_images",
                "cl_khr_gl_event",
                "cl_khr_gl_msaa_sharing",
            };

            for (auto &element : optionalSupportedExtensions) {
                auto foundOffset = extensionString.find(element);
                EXPECT_TRUE(foundOffset != std::string::npos);
            }
        }
    }

    delete[] paramValue;
}

// Define new command types to run the parameterized tests
static cl_device_info deviceInfoStrParams[] = {
    CL_DEVICE_BUILT_IN_KERNELS, CL_DEVICE_EXTENSIONS, CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED, CL_DEVICE_NAME,
    CL_DEVICE_OPENCL_C_VERSION, CL_DEVICE_PROFILE, CL_DEVICE_VENDOR, CL_DEVICE_VERSION, CL_DRIVER_VERSION};

INSTANTIATE_TEST_SUITE_P(api, GetDeviceGlInfoStr, testing::ValuesIn(deviceInfoStrParams));

} // namespace ULT
