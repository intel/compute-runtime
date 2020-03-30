/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
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

TEST(ContextMultiDevice, WhenGettingSubDeviceByIndexFromContextThenCorrectDeviceIsReturned) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> createSingleDeviceBackup{&MockDevice::createSingleDevice, false};
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createRootDeviceFuncBackup{&DeviceFactory::createRootDeviceFunc};

    DebugManager.flags.CreateMultipleSubDevices.set(2);
    createRootDeviceFuncBackup = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return std::unique_ptr<Device>(MockDevice::create<MockDevice>(&executionEnvironment, rootDeviceIndex));
    };

    auto executionEnvironment = new ExecutionEnvironment;
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    auto pRootDevice = std::make_unique<MockClDevice>(static_cast<MockDevice *>(devices[0].release()));
    auto pSubDevice0 = pRootDevice->subDevices[0].get();
    auto pSubDevice1 = pRootDevice->subDevices[1].get();

    cl_device_id allDevices[3]{};
    cl_device_id onlyRootDevices[1]{};
    cl_device_id onlySubDevices[2]{};

    allDevices[0] = onlyRootDevices[0] = pRootDevice.get();
    allDevices[1] = onlySubDevices[0] = pSubDevice0;
    allDevices[2] = onlySubDevices[1] = pSubDevice1;

    cl_int retVal;
    auto pContextWithAllDevices = std::unique_ptr<Context>(Context::create<Context>(nullptr, ClDeviceVector(allDevices, 3),
                                                                                    nullptr, nullptr, retVal));
    EXPECT_NE(nullptr, pContextWithAllDevices);

    auto pContextWithRootDevices = std::unique_ptr<Context>(Context::create<Context>(nullptr, ClDeviceVector(onlyRootDevices, 1),
                                                                                     nullptr, nullptr, retVal));
    EXPECT_NE(nullptr, pContextWithRootDevices);

    auto pContextWithSubDevices = std::unique_ptr<Context>(Context::create<Context>(nullptr, ClDeviceVector(onlySubDevices, 2),
                                                                                    nullptr, nullptr, retVal));
    EXPECT_NE(nullptr, pContextWithSubDevices);

    EXPECT_EQ(pSubDevice0, pContextWithAllDevices->getSubDeviceByIndex(0));
    EXPECT_EQ(nullptr, pContextWithRootDevices->getSubDeviceByIndex(0));
    EXPECT_EQ(pSubDevice0, pContextWithSubDevices->getSubDeviceByIndex(0));

    EXPECT_EQ(pSubDevice1, pContextWithAllDevices->getSubDeviceByIndex(1));
    EXPECT_EQ(nullptr, pContextWithRootDevices->getSubDeviceByIndex(1));
    EXPECT_EQ(pSubDevice1, pContextWithSubDevices->getSubDeviceByIndex(1));
}
