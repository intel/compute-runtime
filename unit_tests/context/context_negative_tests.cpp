/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"

#include "CL/cl_gl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

////////////////////////////////////////////////////////////////////////////////
typedef Test<MemoryManagementFixture> ContextFailureInjection;

TEST_F(ContextFailureInjection, GivenFailedAllocationInjectionWhenCreatingContextThenOutOfHostMemoryErrorIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    cl_device_id deviceID = device.get();

    InjectedFunction method = [deviceID](size_t failureIndex) {
        auto retVal = CL_INVALID_VALUE;
        auto context = Context::create<Context>(nullptr, ClDeviceVector(&deviceID, 1), nullptr,
                                                nullptr, retVal);

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, context);
        } else {
            EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            EXPECT_EQ(nullptr, context);
        }

        delete context;
        context = nullptr;
    };
    injectFailures(method);
}

TEST(InvalidPropertyContextTest, GivenInvalidPropertiesWhenContextIsCreatedThenErrorIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    cl_device_id deviceID = device.get();
    auto pPlatform = NEO::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;
    cl_context_properties invalidProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], CL_CGL_SHAREGROUP_KHR, 0x10000, 0};
    cl_context_properties invalidProperties2[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], (cl_context_properties)0xdeadbeef, 0x10000, 0};

    cl_int retVal = 0;
    auto context = Context::create<Context>(invalidProperties, ClDeviceVector(&deviceID, 1), nullptr,
                                            nullptr, retVal);

    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
    EXPECT_EQ(nullptr, context);
    delete context;

    context = Context::create<Context>(invalidProperties2, ClDeviceVector(&deviceID, 1), nullptr,
                                       nullptr, retVal);

    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
    EXPECT_EQ(nullptr, context);
    delete context;
}
