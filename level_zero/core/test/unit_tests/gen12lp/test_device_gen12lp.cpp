/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

using DeviceFixtureGen12LP = Test<DeviceFixture>;

HWTEST2_F(DeviceFixtureGen12LP, GivenTargetGen12LPaWhenGettingMemoryPropertiesThenMemoryNameComesAsDDR, IsGen12LP) {
    ze_device_memory_properties_t memProperties = {};
    uint32_t pCount = 1u;

    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMemoryProperties(&pCount, &memProperties));
    EXPECT_EQ(0, strcmp(memProperties.name, "DDR"));
    EXPECT_EQ(0u, memProperties.maxClockRate);
}

using CommandQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(CommandQueueGroupTest,
          givenNoBlitterSupportAndNoCCSThenOneQueueGroupIsReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
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

HWTEST2_F(CommandQueueGroupTest,
          givenBlitterSupportAndNoCCSThenTwoQueueGroupsAreReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
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

class DeviceCopyQueueGroupFixture : public DeviceFixture {
  public:
    void SetUp() {
        DebugManager.flags.EnableBlitterOperationsSupport.set(0);
        DeviceFixture::SetUp();
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

using DeviceCopyQueueGroupTest = Test<DeviceCopyQueueGroupFixture>;

HWTEST2_F(DeviceCopyQueueGroupTest,
          givenBlitterSupportAndEnableBlitterOperationsSupportSetToZeroThenNoCopyEngineIsReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
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

    for (auto &engineGroup : neoMockDevice->getRegularEngineGroups()) {
        EXPECT_NE(NEO::EngineGroupType::Copy, engineGroup.engineGroupType);
    }
}

HWTEST2_F(CommandQueueGroupTest,
          givenBlitterSupportAndCCSDefaultEngineThenThreeQueueGroupsAreReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
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

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::RenderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::Compute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::Copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, 4 * sizeof(uint32_t));
        }
    }
}

HWTEST2_F(CommandQueueGroupTest,
          givenBlitterSupportAndCCSDefaultEngineAndOnlyTwoQueueGroupsRequestedThenTwoQueueGroupsAreReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
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

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::RenderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::Compute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::Copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, 4 * sizeof(uint32_t));
        }
    }
}

HWTEST2_F(CommandQueueGroupTest,
          givenBlitterSupportAndNoCCSThenTwoQueueGroupsPropertiesAreReturned, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
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

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::RenderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::Copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, 4 * sizeof(uint32_t));
            EXPECT_EQ(properties[i].numQueues, 1u);
        }
    }
}

HWTEST2_F(CommandQueueGroupTest,
          givenQueueGroupsReturnedThenCommandListsAreCreatedCorrectly, IsGen12LP) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
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
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    L0::Context *context = Context::fromHandle(hContext);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::RenderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::Copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, 4 * sizeof(uint32_t));
            EXPECT_EQ(properties[i].numQueues, 1u);
        }

        ze_command_list_desc_t desc = {};
        desc.commandQueueGroupOrdinal = i;
        ze_command_list_handle_t hCommandList = {};

        res = context->createCommandList(&deviceImp, &desc, &hCommandList);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        CommandList *commandList = CommandList::fromHandle(hCommandList);
        commandList->destroy();
    }

    context->destroy();
}

} // namespace ult
} // namespace L0
