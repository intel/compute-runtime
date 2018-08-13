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
#include "hw_cmds.h"
#include "runtime/helpers/array_count.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(ContextMultiDevice, singleDevice) {
    cl_device_id devices[] = {
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    auto numDevices = static_cast<cl_uint>(arrayCount(devices));

    auto retVal = CL_SUCCESS;
    auto pContext = Context::create<Context>(nullptr, DeviceVector(devices, numDevices),
                                             nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, pContext);

    auto numDevicesReturned = pContext->getNumDevices();
    EXPECT_EQ(numDevices, numDevicesReturned);

    DeviceVector ctxDevices;
    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        ctxDevices.push_back(pContext->getDevice(deviceOrdinal));
    }

    delete pContext;

    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        auto pDevice = (Device *)devices[deviceOrdinal];
        ASSERT_NE(nullptr, pDevice);

        EXPECT_EQ(pDevice, ctxDevices[deviceOrdinal]);
        delete pDevice;
    }
}

TEST(ContextMultiDevice, eightDevices) {
    cl_device_id devices[] = {
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr),
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr),
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr),
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr),
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr),
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr),
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr),
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    auto numDevices = static_cast<cl_uint>(arrayCount(devices));
    ASSERT_EQ(8u, numDevices);

    auto retVal = CL_SUCCESS;
    auto pContext = Context::create<Context>(nullptr, DeviceVector(devices, numDevices),
                                             nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, pContext);

    auto numDevicesReturned = pContext->getNumDevices();
    EXPECT_EQ(numDevices, numDevicesReturned);

    DeviceVector ctxDevices;
    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        ctxDevices.push_back(pContext->getDevice(deviceOrdinal));
    }

    delete pContext;

    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        auto pDevice = (Device *)devices[deviceOrdinal];
        ASSERT_NE(nullptr, pDevice);

        EXPECT_EQ(pDevice, ctxDevices[deviceOrdinal]);
        delete pDevice;
    }
}
