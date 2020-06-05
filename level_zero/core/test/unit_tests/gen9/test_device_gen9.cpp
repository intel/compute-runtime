/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

using KernelPropertyTest = Test<DeviceFixture>;

HWTEST2_F(KernelPropertyTest, givenReturnedKernelPropertiesThenExpectedDp4aSupportReturned, IsGen9) {
    ze_device_kernel_properties_t kernelProps;
    kernelProps.version = ZE_DEVICE_KERNEL_PROPERTIES_VERSION_CURRENT;

    device->getKernelProperties(&kernelProps);
    EXPECT_FALSE(kernelProps.dp4aSupported);
}

using DevicePropertyTest = Test<DeviceFixture>;

HWTEST2_F(DevicePropertyTest, givenReturnedDevicePropertiesThenExpectedPageFaultSupportReturned, IsGen9) {
    ze_device_properties_t deviceProps;
    deviceProps.version = ZE_DEVICE_PROPERTIES_VERSION_CURRENT;

    device->getProperties(&deviceProps);
    EXPECT_FALSE(deviceProps.onDemandPageFaultsSupported);
}

} // namespace ult
} // namespace L0
