/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <string>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

using ClDeviceDiagnosticsExtensionTests = Test<OclFixture>;

TEST_F(ClDeviceDiagnosticsExtensionTests, givenLeoDeviceWhenQueryingExtensionsThenDriverDiagnosticsIsNotReported) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    cl_device_id device = devices[0].get();

    size_t extensionsSize = 0u;
    EXPECT_EQ(CL_SUCCESS, clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0u, nullptr, &extensionsSize));
    ASSERT_GT(extensionsSize, 0u);

    std::vector<char> extensions(extensionsSize);
    EXPECT_EQ(CL_SUCCESS, clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, extensionsSize, extensions.data(), nullptr));

    EXPECT_EQ(std::string::npos, std::string(extensions.data()).find("cl_intel_driver_diagnostics"));
}

TEST_F(ClDeviceDiagnosticsExtensionTests, givenLeoPlatformWhenQueryingExtensionsThenDriverDiagnosticsIsNotReported) {
    cl_platform_id platformId = platform;

    size_t extensionsSize = 0u;
    EXPECT_EQ(CL_SUCCESS, clGetPlatformInfo(platformId, CL_PLATFORM_EXTENSIONS, 0u, nullptr, &extensionsSize));
    ASSERT_GT(extensionsSize, 0u);

    std::vector<char> extensions(extensionsSize);
    EXPECT_EQ(CL_SUCCESS, clGetPlatformInfo(platformId, CL_PLATFORM_EXTENSIONS, extensionsSize, extensions.data(), nullptr));

    EXPECT_EQ(std::string::npos, std::string(extensions.data()).find("cl_intel_driver_diagnostics"));
}

TEST_F(ClDeviceDiagnosticsExtensionTests, givenShowDiagnosticsPropertyWhenCreatingContextThenSucceeds) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    cl_device_id device = devices[0].get();

    cl_context_properties properties[] = {
        CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, static_cast<cl_context_properties>(CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL),
        0};

    cl_int errcode = CL_INVALID_VALUE;
    cl_context context = clCreateContext(properties, 1, &device, nullptr, nullptr, &errcode);
    EXPECT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, context);

    EXPECT_EQ(CL_SUCCESS, clReleaseContext(context));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
