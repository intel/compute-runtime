/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <cstring>
#include "unit_tests/api/cl_api_tests.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/device/device.h"

using namespace OCLRT;

namespace ULT {

//------------------------------------------------------------------------------
struct GetDeviceInfoP : public api_fixture, public ::testing::TestWithParam<uint32_t /*cl_device_info*/> {
    void SetUp() override {
        param = GetParam();
        api_fixture::SetUp();
    }

    void TearDown() override { api_fixture::TearDown(); }

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
            "cl_khr_3d_image_writes",
            "cl_khr_byte_addressable_store",
            "cl_khr_fp16",
            "cl_khr_depth_images",
            "cl_khr_global_int32_base_atomics",
            "cl_khr_global_int32_extended_atomics",
            "cl_khr_local_int32_base_atomics",
            "cl_khr_local_int32_extended_atomics",
            "cl_intel_subgroups",
            "cl_intel_required_subgroup_size",
            "cl_intel_subgroups_short",
            "cl_khr_spir",
            "cl_intel_accelerator",
            "cl_intel_media_block_io",
            "cl_intel_driver_diagnostics",
            "cl_intel_device_side_avc_motion_estimation",
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
