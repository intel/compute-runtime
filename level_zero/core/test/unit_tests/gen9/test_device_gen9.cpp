/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

using DevicePropertyTest = Test<DeviceFixture>;

HWTEST2_F(DevicePropertyTest, givenReturnedDevicePropertiesThenExpectedPropertiesFlagsSet, IsGen9) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ECC);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
    EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_INTEGRATED, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

using CommandQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(CommandQueueGroupTest, givenCommandQueuePropertiesCallThenCorrectNumberOfGroupsIsReturned, IsGen9) {
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
    EXPECT_EQ(properties.maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
}

HWTEST2_F(CommandQueueGroupTest, givenQueueGroupsReturnedThenCommandListIsCreatedCorrectly, IsGen9) {
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
    EXPECT_EQ(properties.maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());

    ze_context_handle_t hContext;
    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    res = driverHandle->createContext(&contextDesc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    L0::Context *context = Context::fromHandle(hContext);

    ze_command_list_desc_t listDesc = {};
    listDesc.commandQueueGroupOrdinal = 0;
    ze_command_list_handle_t hCommandList = {};

    res = context->createCommandList(device, &listDesc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    CommandList *commandList = CommandList::fromHandle(hCommandList);
    commandList->destroy();

    context->destroy();
}

} // namespace ult
} // namespace L0
