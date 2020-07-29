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
    ze_device_module_properties_t kernelProps;

    device->getKernelProperties(&kernelProps);
    EXPECT_EQ(0u, kernelProps.flags & ZE_DEVICE_MODULE_FLAG_DP4A);
}

using DevicePropertyTest = Test<DeviceFixture>;

HWTEST2_F(DevicePropertyTest, givenReturnedDevicePropertiesThenExpectedPageFaultSupportReturned, IsGen9) {
    ze_device_properties_t deviceProps;

    device->getProperties(&deviceProps);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
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
