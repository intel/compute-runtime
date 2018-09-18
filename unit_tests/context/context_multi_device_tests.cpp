/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
