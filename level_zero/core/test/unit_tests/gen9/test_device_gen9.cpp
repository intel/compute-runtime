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

using DeviceQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(DeviceQueueGroupTest, givenCommandQueuePropertiesCallThenCorrectNumberOfGroupsIsReturned, IsGen9) {
    uint32_t count = 0;
    ze_result_t res = device->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(1u, count);

    ze_command_queue_group_properties_t properties;
    res = device->getCommandQueueGroupProperties(&count, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_TRUE(properties.flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
    EXPECT_TRUE(properties.flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
    EXPECT_TRUE(properties.flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
    EXPECT_TRUE(properties.flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
    EXPECT_EQ(properties.numQueues, 1u);
    EXPECT_EQ(properties.maxMemoryFillPatternSize, sizeof(uint32_t));
}

} // namespace ult
} // namespace L0
