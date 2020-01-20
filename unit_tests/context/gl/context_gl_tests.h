/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"
#include <GL/gl.h>

namespace NEO {

struct GlContextTest : public PlatformFixture, public ::testing::Test {

    using PlatformFixture::SetUp;

    void SetUp() override {
        PlatformFixture::SetUp();

        properties[0] = CL_CONTEXT_PLATFORM;
        properties[1] = reinterpret_cast<cl_context_properties>(static_cast<cl_platform_id>(pPlatform));
        properties[2] = 0;

        context = Context::create<MockContext>(properties, ClDeviceVector(devices, num_devices), nullptr, nullptr, retVal);
        ASSERT_NE(nullptr, context);
    }

    void TearDown() override {
        delete context;
        PlatformFixture::TearDown();
    }

    void testContextCreation(cl_context_properties contextType) {
        const cl_device_id deviceID = devices[0];
        const auto platformId = reinterpret_cast<cl_context_properties>(static_cast<cl_platform_id>(platform()));

        const cl_context_properties propertiesOneContext[] = {CL_CONTEXT_PLATFORM, platformId, contextType, 0x10000, 0};
        auto context = std::unique_ptr<Context>(Context::create<Context>(propertiesOneContext, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, context.get());
        EXPECT_FALSE(context->getInteropUserSyncEnabled());

        const cl_context_properties propertiesTwoContexts[] = {CL_CONTEXT_PLATFORM, platformId, contextType, 0x10000, contextType, 0x10000, 0};
        context = std::unique_ptr<Context>(Context::create<Context>(propertiesTwoContexts, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, context.get());
        EXPECT_FALSE(context->getInteropUserSyncEnabled());
    }

    cl_int retVal = CL_SUCCESS;
    MockContext *context = nullptr;
    cl_context_properties properties[3] = {};
};

} // namespace NEO
