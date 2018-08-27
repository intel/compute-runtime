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

#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/api/cl_api_tests.h"

using namespace OCLRT;

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
