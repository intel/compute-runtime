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

HWTEST2_F(DevicePropertyTest, givenReturnedDevicePropertiesThenExpectedPropertiesFlagsSet, IsGen9) {
    ze_device_properties_t deviceProps;

    device->getProperties(&deviceProps);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ECC);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
    EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_INTEGRATED, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

using DeviceQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(DeviceQueueGroupTest, givenCommandQueuePropertiesCallThenCorrectNumberOfGroupsIsReturned, IsGen9) {
    uint32_t count = 0;
    ze_result_t res = device->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto expectedCount = 1u;
    if (device->getHwInfo().featureTable.ftrBcsInfo.test(0)) {
        expectedCount++;
    }
    EXPECT_EQ(expectedCount, count);

    auto properties = std::make_unique<ze_command_queue_group_properties_t[]>(count);
    res = device->getCommandQueueGroupProperties(&count, properties.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_TRUE(properties[0].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
    EXPECT_TRUE(properties[0].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
    EXPECT_TRUE(properties[0].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
    EXPECT_TRUE(properties[0].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
    EXPECT_EQ(properties[0].numQueues, 1u);
    EXPECT_EQ(properties[0].maxMemoryFillPatternSize, sizeof(uint32_t));
}

HWTEST2_F(DeviceQueueGroupTest, givenQueueGroupsReturnedThenCommandListIsCreatedCorrectly, IsGen9) {
    uint32_t count = 0;
    ze_result_t res = device->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto expectedCount = 1u;
    if (device->getHwInfo().featureTable.ftrBcsInfo.test(0)) {
        expectedCount++;
    }
    EXPECT_EQ(expectedCount, count);

    auto properties = std::make_unique<ze_command_queue_group_properties_t[]>(count);
    res = device->getCommandQueueGroupProperties(&count, properties.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_TRUE(properties[0].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
    EXPECT_TRUE(properties[0].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
    EXPECT_TRUE(properties[0].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
    EXPECT_TRUE(properties[0].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
    EXPECT_EQ(properties[0].numQueues, 1u);
    EXPECT_EQ(properties[0].maxMemoryFillPatternSize, sizeof(uint32_t));

    ze_context_handle_t hContext;
    ze_context_desc_t contextDesc;
    res = driverHandle->createContext(&contextDesc, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    L0::Context *context = Context::fromHandle(hContext);
    for (uint32_t i = 0; i < count; i++) {
        ze_command_list_desc_t listDesc = {};
        listDesc.commandQueueGroupOrdinal = i;
        ze_command_list_handle_t hCommandList = {};

        res = context->createCommandList(device, &listDesc, &hCommandList);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        CommandList *commandList = CommandList::fromHandle(hCommandList);
        commandList->destroy();
    }

    context->destroy();
}

} // namespace ult
} // namespace L0
