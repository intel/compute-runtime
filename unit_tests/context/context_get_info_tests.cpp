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
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct ContextGetInfoTest : public PlatformFixture,
                            public ContextFixture,
                            public ::testing::Test {

    using ContextFixture::SetUp;
    using PlatformFixture::SetUp;

    ContextGetInfoTest() {
    }

    void SetUp() override {
        PlatformFixture::SetUp(numPlatformDevices, platformDevices);
        ContextFixture::SetUp(num_devices, devices);
    }

    void TearDown() override {
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
};

TEST_F(ContextGetInfoTest, Invalid) {
    size_t retSize = 0;

    retVal = pContext->getInfo(
        0,
        0,
        nullptr,
        &retSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ContextGetInfoTest, NumDevices) {
    cl_uint numDevices = 0;
    size_t retSize = 0;

    retVal = pContext->getInfo(
        CL_CONTEXT_NUM_DEVICES,
        sizeof(cl_uint),
        &numDevices,
        nullptr);

    EXPECT_EQ(this->num_devices, numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pContext->getInfo(
        CL_CONTEXT_DEVICES,
        0,
        nullptr,
        &retSize);

    // make sure we get the same answer through a different query
    EXPECT_EQ(numDevices * sizeof(cl_device_id), retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ContextGetInfoTest, Devices) {
    auto devicesReturned = new cl_device_id[this->num_devices];
    retVal = pContext->getInfo(
        CL_CONTEXT_DEVICES,
        this->num_devices * sizeof(cl_device_id),
        devicesReturned,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (size_t deviceOrdinal = 0; deviceOrdinal < this->num_devices; ++deviceOrdinal) {
        EXPECT_EQ(devices[deviceOrdinal], devicesReturned[deviceOrdinal]);
    }

    delete[] devicesReturned;
}

TEST_F(ContextGetInfoTest, ContextProperties) {
    cl_context_properties props;
    size_t size;

    auto retVal = pContext->getInfo(
        CL_CONTEXT_PROPERTIES,
        sizeof(props),
        &props,
        &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, size);
}

TEST_F(ContextGetInfoTest, givenMultipleContextPropertiesWhenTheyAreBeingQueriedThenGetInfoReturnProperProperties) {
    cl_context_properties properties[] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, (cl_context_properties)0xff, 0};
    constexpr auto propertiesCount = sizeof(properties) / sizeof(cl_context_properties);
    auto retValue = CL_SUCCESS;
    auto contextWithProperties = clCreateContext(properties, 1, &this->devices[0], nullptr, nullptr, &retValue);
    EXPECT_EQ(CL_SUCCESS, retValue);

    auto pContextWithProperties = castToObject<Context>(contextWithProperties);

    size_t size = 6;
    cl_context_properties obtainedProperties[propertiesCount] = {0};

    auto retVal = pContextWithProperties->getInfo(
        CL_CONTEXT_PROPERTIES,
        sizeof(properties),
        obtainedProperties,
        &size);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(properties), size);
    for (auto property = 0u; property < propertiesCount; property++) {
        EXPECT_EQ(obtainedProperties[property], properties[property]);
    }

    clReleaseContext(contextWithProperties);
}
