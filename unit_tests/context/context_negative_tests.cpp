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

#include "runtime/context/context.h"
#include "runtime/platform/platform.h"
#include "hw_cmds.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "test.h"
#include "CL/cl_gl.h"
#include "gtest/gtest.h"
#include <memory>

using namespace OCLRT;

////////////////////////////////////////////////////////////////////////////////
typedef Test<MemoryManagementFixture> ContextFailureInjection;

TEST_F(ContextFailureInjection, failedAllocationInjection) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    cl_device_id deviceID = device.get();

    InjectedFunction method = [deviceID](size_t failureIndex) {
        auto retVal = CL_INVALID_VALUE;
        auto context = Context::create<Context>(nullptr, DeviceVector(&deviceID, 1), nullptr,
                                                nullptr, retVal);

        if (nonfailingAllocation == failureIndex) {
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
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    cl_device_id deviceID = device.get();
    auto pPlatform = OCLRT::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;
    cl_context_properties invalidProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], CL_CGL_SHAREGROUP_KHR, 0x10000, 0};
    cl_context_properties invalidProperties2[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], (cl_context_properties)0xdeadbeef, 0x10000, 0};

    cl_int retVal = 0;
    auto context = Context::create<Context>(invalidProperties, DeviceVector(&deviceID, 1), nullptr,
                                            nullptr, retVal);

    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
    EXPECT_EQ(nullptr, context);
    delete context;

    context = Context::create<Context>(invalidProperties2, DeviceVector(&deviceID, 1), nullptr,
                                       nullptr, retVal);

    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
    EXPECT_EQ(nullptr, context);
    delete context;
}
