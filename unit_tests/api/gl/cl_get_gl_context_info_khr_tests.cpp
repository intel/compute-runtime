/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_info.h"
#include "core/helpers/options.h"
#include "runtime/device/device.h"
#include "unit_tests/api/cl_api_tests.h"
#include "unit_tests/os_interface/windows/gl/gl_dll_helper.h"

using namespace NEO;

typedef api_tests clGetGLContextInfoKHR_;

namespace ULT {

TEST_F(clGetGLContextInfoKHR_, success) {
    auto expectedDevice = ::platform()->getDevice(0);
    cl_device_id retDevice = 0;
    size_t retSize = 0;

    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, 0};
    retVal = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, retDevice);
    EXPECT_EQ(sizeof(cl_device_id), retSize);

    retVal = clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, retDevice);
    EXPECT_EQ(sizeof(cl_device_id), retSize);
}

TEST_F(clGetGLContextInfoKHR_, invalidParam) {
    cl_device_id retDevice = 0;
    size_t retSize = 0;
    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, 0};
    retVal = clGetGLContextInfoKHR(properties, 0, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, retDevice);
    EXPECT_EQ(0u, retSize);
}

TEST_F(clGetGLContextInfoKHR_, givenContextFromNoIntelOpenGlDriverWhenCallClGetGLContextInfoKHRThenReturnClInvalidContext) {
    cl_device_id retDevice = 0;
    size_t retSize = 0;
    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, 0};
    glDllHelper setDllParam;
    setDllParam.glSetString("NoIntel", GL_VENDOR);
    retVal = clGetGLContextInfoKHR(properties, 0, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, retDevice);
    EXPECT_EQ(0u, retSize);
}

TEST_F(clGetGLContextInfoKHR_, givenNullVersionFromIntelOpenGlDriverWhenCallClGetGLContextInfoKHRThenReturnClInvalidContext) {
    cl_device_id retDevice = 0;
    size_t retSize = 0;
    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, 0};
    glDllHelper setDllParam;
    setDllParam.glSetString("", GL_VERSION);
    retVal = clGetGLContextInfoKHR(properties, 0, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, retDevice);
    EXPECT_EQ(0u, retSize);
}

TEST_F(clGetGLContextInfoKHR_, GivenIncorrectPropertiesWhenCallclGetGLContextInfoKHRThenReturnClInvalidGlShareGroupRererencKhr) {
    cl_device_id retDevice = 0;
    size_t retSize = 0;
    retVal = clGetGLContextInfoKHR(nullptr, 0, sizeof(cl_device_id), &retDevice, &retSize);
    EXPECT_EQ(CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR, retVal);

    const cl_context_properties propertiesLackOfWglHdcKhr[] = {CL_GL_CONTEXT_KHR, 1, 0};
    retVal = clGetGLContextInfoKHR(propertiesLackOfWglHdcKhr, 0, sizeof(cl_device_id), &retDevice, &retSize);
    EXPECT_EQ(CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR, retVal);

    const cl_context_properties propertiesLackOfCLGlContextKhr[] = {CL_WGL_HDC_KHR, 2, 0};
    retVal = clGetGLContextInfoKHR(propertiesLackOfCLGlContextKhr, 0, sizeof(cl_device_id), &retDevice, &retSize);
    EXPECT_EQ(CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR, retVal);

    const cl_context_properties propertiesWithOutRequiredProperties[] = {CL_CONTEXT_PLATFORM, 3, 0};
    retVal = clGetGLContextInfoKHR(propertiesWithOutRequiredProperties, 0, sizeof(cl_device_id), &retDevice, &retSize);
    EXPECT_EQ(CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR, retVal);
}

} // namespace ULT
