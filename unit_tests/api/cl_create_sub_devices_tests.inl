/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/api/api.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"

#include <memory>

using namespace NEO;

namespace ULT {

struct clCreateSubDevicesTests : ::testing::Test {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> mockDeviceCreateSingleDeviceBackup{&MockDevice::createSingleDevice};
    std::unique_ptr<MockDevice> device;
    cl_device_partition_property properties[3] = {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, CL_DEVICE_AFFINITY_DOMAIN_NUMA, 0};
    cl_uint outDevicesCount = 2;
    cl_device_id outDevices[2];

    void setup(int numberOfDevices) {
        DebugManager.flags.CreateMultipleSubDevices.set(numberOfDevices);
        mockDeviceCreateSingleDeviceBackup = (numberOfDevices == 1);
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
    }
};

TEST_F(clCreateSubDevicesTests, GivenInvalidDeviceWhenCreatingSubDevicesThenInvalidDeviceErrorIsReturned) {
    auto retVal = clCreateSubDevices(
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(retVal, CL_INVALID_DEVICE);
}

TEST_F(clCreateSubDevicesTests, GivenDeviceWithoutSubDevicesWhenCreatingSubDevicesThenInvalidDeviceErrorIsReturned) {
    setup(1);

    auto retVal = clCreateSubDevices(device.get(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreateSubDevicesTests, GivenInvalidOrUnsupportedPropertiesWhenCreatingSubDevicesThenInvalidValueErrorIsReturned) {
    setup(2);

    auto retVal = clCreateSubDevices(device.get(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    properties[0] = 0;
    retVal = clCreateSubDevices(device.get(), properties, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    properties[0] = CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN;
    properties[1] = 0;
    retVal = clCreateSubDevices(device.get(), properties, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    properties[1] = CL_DEVICE_AFFINITY_DOMAIN_NUMA;
    properties[2] = CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN;
    retVal = clCreateSubDevices(device.get(), properties, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateSubDevicesTests, GivenOutDevicesNullWhenCreatingSubDevicesThenSuccessIsReturned) {
    setup(2);

    cl_uint returnedOutDeviceCount = 0;
    auto retVal = clCreateSubDevices(device.get(), properties, 0, nullptr, &returnedOutDeviceCount);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, returnedOutDeviceCount);
}

TEST_F(clCreateSubDevicesTests, GivenOutDevicesTooSmallWhenCreatingSubDevicesThenInvalidValueErrorIsReturned) {
    setup(2);

    outDevicesCount = 1;
    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateSubDevicesTests, GivenValidInputWhenCreatingSubDevicesThenSubDevicesAreReturned) {
    setup(2);

    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(device->getDeviceById(0), outDevices[0]);
    EXPECT_EQ(device->getDeviceById(1), outDevices[1]);
}

} // namespace ULT
