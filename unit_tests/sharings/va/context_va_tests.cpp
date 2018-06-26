/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "gtest/gtest.h"
#include "runtime/sharings/va/va_sharing.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_context.h"

using namespace OCLRT;

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

        context = Context::create<Context>(properties, DeviceVector(devices, num_devices), nullptr, nullptr, retVal);
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

TEST_F(VAContextTest, sharingAreNotPresentByDefault) {
    ASSERT_EQ(context->getSharing<VASharingFunctions>(), nullptr);
}

TEST_F(VAContextTest, GivenVaContextParamWhenCreateContextThenReturnError) {
    cl_device_id deviceID = devices[0];
    auto pPlatform = OCLRT::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableVaLibCalls.set(false); // avoid libva calls on initialization

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0],
                                                CL_CONTEXT_VA_API_DISPLAY_INTEL, 0x10000, 0};
    cl_int retVal = CL_SUCCESS;
    auto ctx = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);

    // not supported by default
    // use MockVaSharing to test va-sharing functionality
    EXPECT_EQ(CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL, retVal);
    EXPECT_EQ(nullptr, ctx);
}
