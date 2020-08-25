/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

using DeviceQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(DeviceQueueGroupTest,
          givenNoBlitterSupportAndNoCCSThenOneQueueGroupIsReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    hwInfo.featureTable.ftrBcsInfo.set(0, false);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 1u);
}

HWTEST2_F(DeviceQueueGroupTest,
          givenBlitterSupportAndNoCCSThenTwoQueueGroupsAreReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 2u);
}

class DeviceCopyQueueGroupTest : public DeviceFixture, public ::testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.EnableBlitterOperationsSupport.set(0);
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

HWTEST2_F(DeviceCopyQueueGroupTest,
          givenBlitterSupportAndEnableBlitterOperationsSupportSetToZeroThenNoCopyEngineIsReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_NE(i, static_cast<uint32_t>(NEO::EngineGroupType::Copy));
    }
}

HWTEST2_F(DeviceQueueGroupTest,
          givenBlitterSupportAndCCSDefaultEngineThenThreeQueueGroupsAreReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 3u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < count; i++) {
        if (i == static_cast<uint32_t>(NEO::EngineGroupType::RenderCompute)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            auto hwInfoConfig = NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
            if (hwInfoConfig->isEvenContextCountRequired()) {
                EXPECT_EQ(properties[i].numQueues, 2u);
            } else {
                EXPECT_EQ(properties[i].numQueues, 1u);
            }
        } else if (i == static_cast<uint32_t>(NEO::EngineGroupType::Compute)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if (i == static_cast<uint32_t>(NEO::EngineGroupType::Copy)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
        }
        EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint32_t));
    }
}

HWTEST2_F(DeviceQueueGroupTest,
          givenBlitterSupportAndCCSDefaultEngineAndOnlyTwoQueueGroupsRequestedThenTwoQueueGroupsAreReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 3u);

    count = 2;
    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(2u, count);

    for (uint32_t i = 0; i < count; i++) {
        if (i == static_cast<uint32_t>(NEO::EngineGroupType::RenderCompute)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            auto hwInfoConfig = NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
            if (hwInfoConfig->isEvenContextCountRequired()) {
                EXPECT_EQ(properties[i].numQueues, 2u);
            } else {
                EXPECT_EQ(properties[i].numQueues, 1u);
            }
        } else if (i == static_cast<uint32_t>(NEO::EngineGroupType::Compute)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if (i == static_cast<uint32_t>(NEO::EngineGroupType::Copy)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
        }
        EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint32_t));
    }
}

HWTEST2_F(DeviceQueueGroupTest,
          givenBlitterSupportAndNoCCSThenTwoQueueGroupsPropertiesAreReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 2u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < count; i++) {
        if (i == static_cast<uint32_t>(NEO::EngineGroupType::RenderCompute)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint32_t));
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if (i == static_cast<uint32_t>(NEO::EngineGroupType::Copy)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint32_t));
            EXPECT_EQ(properties[i].numQueues, 1u);
        }
    }
}

HWTEST2_F(DeviceQueueGroupTest,
          givenQueueGroupsReturnedThenCommandListsAreCreatedCorrectly, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 2u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_context_handle_t hContext;
    ze_context_desc_t desc;
    res = driverHandle->createContext(&desc, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    L0::Context *context = Context::fromHandle(hContext);

    for (uint32_t i = 0; i < count; i++) {
        if (i == static_cast<uint32_t>(NEO::EngineGroupType::RenderCompute)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint32_t));
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if (i == static_cast<uint32_t>(NEO::EngineGroupType::Copy)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint32_t));
            EXPECT_EQ(properties[i].numQueues, 1u);
        }

        ze_command_list_desc_t desc = {};
        desc.commandQueueGroupOrdinal = i;
        ze_command_list_handle_t hCommandList = {};

        res = context->createCommandList(device, &desc, &hCommandList);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        CommandList *commandList = CommandList::fromHandle(hCommandList);
        commandList->destroy();
    }

    context->destroy();
}

} // namespace ult
} // namespace L0
