/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/context/gl/context_gl_tests.h"

#include "runtime/sharings/gl/gl_sharing.h"

namespace NEO {

TEST_F(GlContextTest, GivenDefaultContextThenGlSharingIsDisabled) {
    ASSERT_EQ(context->getSharing<GLSharingFunctions>(), nullptr);
}

TEST_F(GlContextTest, GivenGlContextParamWhenCreateContextThenInitSharingFunctions) {
    cl_device_id deviceID = devices[0];
    auto pPlatform = NEO::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], CL_GL_CONTEXT_KHR, 0x10000, 0};
    cl_int retVal = CL_SUCCESS;
    auto ctx = Context::create<Context>(validProperties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, ctx);

    auto sharing = ctx->getSharing<GLSharingFunctions>();
    ASSERT_NE(nullptr, sharing);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());

    delete ctx;
}
} // namespace NEO
