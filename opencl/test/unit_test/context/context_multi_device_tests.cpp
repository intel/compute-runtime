/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ContextMultiDevice, GivenSingleDeviceWhenCreatingContextThenContextIsCreated) {
    cl_device_id devices[] = {
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)}};
    auto numDevices = static_cast<cl_uint>(arrayCount(devices));

    auto retVal = CL_SUCCESS;
    auto pContext = Context::create<Context>(nullptr, ClDeviceVector(devices, numDevices),
                                             nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, pContext);

    auto numDevicesReturned = pContext->getNumDevices();
    EXPECT_EQ(numDevices, numDevicesReturned);

    ClDeviceVector ctxDevices;
    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        ctxDevices.push_back(pContext->getDevice(deviceOrdinal));
    }

    delete pContext;

    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        auto pDevice = (ClDevice *)devices[deviceOrdinal];
        ASSERT_NE(nullptr, pDevice);

        EXPECT_EQ(pDevice, ctxDevices[deviceOrdinal]);
        delete pDevice;
    }
}

TEST(ContextMultiDevice, GivenMultipleDevicesWhenCreatingContextThenContextIsCreatedForEachDevice) {
    cl_device_id devices[] = {
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)}};
    auto numDevices = static_cast<cl_uint>(arrayCount(devices));
    ASSERT_EQ(8u, numDevices);

    auto retVal = CL_SUCCESS;
    auto pContext = Context::create<Context>(nullptr, ClDeviceVector(devices, numDevices),
                                             nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, pContext);

    auto numDevicesReturned = pContext->getNumDevices();
    EXPECT_EQ(numDevices, numDevicesReturned);

    ClDeviceVector ctxDevices;
    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        ctxDevices.push_back(pContext->getDevice(deviceOrdinal));
    }

    delete pContext;

    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        auto pDevice = (ClDevice *)devices[deviceOrdinal];
        ASSERT_NE(nullptr, pDevice);

        EXPECT_EQ(pDevice, ctxDevices[deviceOrdinal]);
        delete pDevice;
    }
}
