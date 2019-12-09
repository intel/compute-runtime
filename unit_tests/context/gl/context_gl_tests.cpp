/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/options.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/os_interface/windows/gl/gl_sharing_os.h"
#include "runtime/sharings/gl/gl_sharing.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"
#include <GL/gl.h>

using namespace NEO;

// use mockGlSharing class for cl-gl sharing tests
typedef bool (*MockGlIoctlFcn)();

struct ContextTest : public PlatformFixture, public ::testing::Test {

    using PlatformFixture::SetUp;

    void SetUp() override {
        PlatformFixture::SetUp();

        cl_platform_id platform = pPlatform;
        properties = new cl_context_properties[3];
        properties[0] = CL_CONTEXT_PLATFORM;
        properties[1] = (cl_context_properties)platform;
        properties[2] = 0;

        context = Context::create<MockContext>(properties, DeviceVector(devices, num_devices), nullptr, nullptr, retVal);
        ASSERT_NE(nullptr, context);
    }

    void TearDown() override {
        delete[] properties;
        delete context;
        PlatformFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockContext *context = nullptr;
    cl_context_properties *properties = nullptr;
};

TEST_F(ContextTest, GlSharingAreIstPresentByDefault) { ASSERT_EQ(context->getSharing<GLSharingFunctions>(), nullptr); }

TEST_F(ContextTest, GivenPropertiesWhenContextIsCreatedThenSuccess) {
    cl_device_id deviceID = devices[0];
    auto pPlatform = NEO::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;

    cl_context_properties validProperties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], CL_GL_CONTEXT_KHR, 0x10000, 0};

    auto context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
    delete context;

    validProperties[2] = CL_EGL_DISPLAY_KHR;
    context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
    delete context;

    validProperties[2] = CL_GLX_DISPLAY_KHR;
    context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
    delete context;

    validProperties[2] = CL_WGL_HDC_KHR;
    context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
    delete context;
}

TEST_F(ContextTest, GivenTwoGlPropertiesWhenContextIsCreatedThenSuccess) {
    cl_device_id deviceID = devices[0];
    auto pPlatform = NEO::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;

    cl_context_properties validProperties[] = {
        CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], CL_GL_CONTEXT_KHR, 0x10000, CL_GL_CONTEXT_KHR, 0x10000, 0};

    validProperties[4] = CL_EGL_DISPLAY_KHR;
    auto context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
    delete context;

    validProperties[4] = CL_GLX_DISPLAY_KHR;
    context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
    delete context;

    validProperties[4] = CL_WGL_HDC_KHR;
    context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
    delete context;
}

TEST_F(ContextTest, GivenGlContextParamWhenCreateContextThenInitSharingFunctions) {
    cl_device_id deviceID = devices[0];
    auto pPlatform = NEO::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], CL_GL_CONTEXT_KHR, 0x10000, 0};
    cl_int retVal = CL_SUCCESS;
    auto ctx = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, ctx);

    //
    auto sharing = ctx->getSharing<GLSharingFunctions>();
    ASSERT_NE(nullptr, sharing);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());

    delete ctx;
}
