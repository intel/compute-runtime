/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/sharings/va/va_sharing.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

using namespace NEO;

struct VAContextTest : public PlatformFixture,
                       public ::testing::Test {

    using PlatformFixture::SetUp;

    VAContextTest() {
    }

    void SetUp() override {
        PlatformFixture::SetUp();

        cl_platform_id platform = pPlatform;
        properties = new cl_context_properties[3];
        properties[0] = CL_CONTEXT_PLATFORM;
        properties[1] = (cl_context_properties)platform;
        properties[2] = 0;

        context = Context::create<Context>(properties, ClDeviceVector(devices, num_devices), nullptr, nullptr, retVal);
        ASSERT_NE(nullptr, context);
    }

    void TearDown() override {
        delete[] properties;
        delete context;
        PlatformFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    Context *context = nullptr;
    cl_context_properties *properties = nullptr;
};

TEST_F(VAContextTest, GivenDefaultThenSharingNotPresent) {
    ASSERT_EQ(context->getSharing<VASharingFunctions>(), nullptr);
}

TEST_F(VAContextTest, GivenVaContextParamWhenCreateContextThenReturnError) {
    cl_device_id deviceID = devices[0];
    auto pPlatform = NEO::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableVaLibCalls.set(false); // avoid libva calls on initialization

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0],
                                                CL_CONTEXT_VA_API_DISPLAY_INTEL, 0x10000, 0};
    cl_int retVal = CL_SUCCESS;
    auto ctx = Context::create<Context>(validProperties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal);

    // not supported by default
    // use MockVaSharing to test va-sharing functionality
    EXPECT_EQ(CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL, retVal);
    EXPECT_EQ(nullptr, ctx);
}
