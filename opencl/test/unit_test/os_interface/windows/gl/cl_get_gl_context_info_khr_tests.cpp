/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_wddm.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/sharings/gl/gl_dll_helper.h"

using namespace NEO;

using clGetGLContextInfoKhrTest = ApiTests;

namespace ULT {

TEST_F(clGetGLContextInfoKhrTest, GivenDefaultPlatformWhenGettingGlContextThenSuccessIsReturned) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto defaultPlatform = std::make_unique<MockPlatform>();
    defaultPlatform->initializeWithNewDevices();
    (*platformsImpl)[0] = std::move(defaultPlatform);
    auto expectedDevice = ::platform()->getClDevice(0);
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

using clGetGLContextInfoKHRNonDefaultPlatform = ::testing::Test;

TEST_F(clGetGLContextInfoKHRNonDefaultPlatform, GivenNonDefaultPlatformWhenGettingGlContextThenSuccessIsReturned) {
    platformsImpl->clear();

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    cl_int retVal = CL_SUCCESS;

    auto nonDefaultPlatform = std::make_unique<MockPlatform>();
    nonDefaultPlatform->initializeWithNewDevices();
    cl_platform_id nonDefaultPlatformCl = nonDefaultPlatform.get();

    auto expectedDevice = nonDefaultPlatform->getClDevice(0);
    size_t retSize = 0;
    cl_device_id retDevice = 0;

    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(nonDefaultPlatformCl), 0};
    retVal = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, retDevice);
    EXPECT_EQ(sizeof(cl_device_id), retSize);

    retVal = clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, retDevice);
    EXPECT_EQ(sizeof(cl_device_id), retSize);
}

TEST_F(clGetGLContextInfoKhrTest, GivenInvalidParamWhenGettingGlContextThenInvalidValueErrorIsReturned) {
    cl_device_id retDevice = 0;
    size_t retSize = 0;
    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, 0};
    retVal = clGetGLContextInfoKHR(properties, 0, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, retDevice);
    EXPECT_EQ(0u, retSize);
}

TEST_F(clGetGLContextInfoKhrTest, givenContextFromNoIntelOpenGlDriverWhenCallClGetGLContextInfoKHRThenReturnClInvalidContext) {
    cl_device_id retDevice = 0;
    size_t retSize = 0;
    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, 0};
    GlDllHelper setDllParam;
    setDllParam.glSetString("NoIntel", GL_VENDOR);
    retVal = clGetGLContextInfoKHR(properties, 0, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, retDevice);
    EXPECT_EQ(0u, retSize);
}

TEST_F(clGetGLContextInfoKhrTest, givenNullVersionFromIntelOpenGlDriverWhenCallClGetGLContextInfoKHRThenReturnClInvalidContext) {
    cl_device_id retDevice = 0;
    size_t retSize = 0;
    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, 0};
    GlDllHelper setDllParam;
    setDllParam.glSetString("", GL_VERSION);
    retVal = clGetGLContextInfoKHR(properties, 0, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, retDevice);
    EXPECT_EQ(0u, retSize);
}

TEST_F(clGetGLContextInfoKhrTest, GivenIncorrectPropertiesWhenCallclGetGLContextInfoKHRThenReturnClInvalidGlShareGroupRererencKhr) {
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
}

TEST_F(clGetGLContextInfoKHRNonDefaultPlatform, whenVerificationOfAdapterLuidFailsThenInvalidGlReferenceErrorIsReturned) {
    platformsImpl->clear();

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    cl_int retVal = CL_SUCCESS;

    auto nonDefaultPlatform = std::make_unique<MockPlatform>();
    nonDefaultPlatform->initializeWithNewDevices();
    cl_platform_id nonDefaultPlatformCl = nonDefaultPlatform.get();

    auto device = nonDefaultPlatform->getClDevice(0);

    static_cast<WddmMock *>(device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Wddm>())->verifyAdapterLuidReturnValue = false;
    size_t retSize = 0;
    cl_device_id retDevice = 0;

    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(nonDefaultPlatformCl), 0};
    retVal = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR, retVal);
}
TEST_F(clGetGLContextInfoKHRNonDefaultPlatform, whenVerificationOfAdapterLuidFailsForFirstDeviceButSucceedsForSecondOneThenReturnTheSecondDevice) {
    platformsImpl->clear();

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(2);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    cl_int retVal = CL_SUCCESS;

    auto nonDefaultPlatform = std::make_unique<MockPlatform>();
    nonDefaultPlatform->initializeWithNewDevices();
    cl_platform_id nonDefaultPlatformCl = nonDefaultPlatform.get();

    auto device0 = nonDefaultPlatform->getClDevice(0);
    auto device1 = nonDefaultPlatform->getClDevice(0);
    cl_device_id expectedDevice = device1;

    static_cast<WddmMock *>(device0->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Wddm>())->verifyAdapterLuidReturnValue = false;
    static_cast<WddmMock *>(device1->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Wddm>())->verifyAdapterLuidReturnValue = true;
    size_t retSize = 0;
    cl_device_id retDevice = 0;

    const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, 1, CL_WGL_HDC_KHR, 2, CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(nonDefaultPlatformCl), 0};
    retVal = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &retDevice, &retSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, retDevice);
    EXPECT_EQ(sizeof(cl_device_id), retSize);
}

} // namespace ULT
