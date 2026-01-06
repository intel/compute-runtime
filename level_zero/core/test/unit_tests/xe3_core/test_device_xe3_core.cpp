/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

using DeviceXe3CoreTest = Test<DeviceFixture>;

HWTEST2_F(DeviceXe3CoreTest, whenCallingGetMemoryPropertiesWithNonNullPtrThenPropertiesAreReturned, IsXe3Core) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    EXPECT_EQ(memProperties.maxClockRate, 0u);
    EXPECT_EQ(memProperties.maxBusWidth, this->neoDevice->getDeviceInfo().addressBits);
    EXPECT_EQ(memProperties.totalSize, this->neoDevice->getDeviceInfo().globalMemSize);
}

using CommandQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(CommandQueueGroupTest, givenNoBlitterSupportAndNoCCSThenOneQueueGroupIsReturned, IsXe3Core) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 1u);
}

HWTEST2_F(CommandQueueGroupTest, givenNoBlitterSupportAndCCSThenTwoQueueGroupsAreReturned, IsXe3Core) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 2u);
}

HWTEST2_F(CommandQueueGroupTest, givenBlitterSupportAndCCSThenFourQueueGroupsAreReturned, IsXe3Core) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 4u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::linkedCopy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, hwInfo.featureTable.ftrBcsInfo.count() - 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        }
    }
}

HWTEST2_F(CommandQueueGroupTest, givenBlitterSupportCCSAndLinkedBcsDisabledThenThreeQueueGroupsAreReturned, IsXe3Core) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 3u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        }
    }
}

HWTEST2_F(CommandQueueGroupTest, givenBlitterDisabledAndAllBcsSetThenTwoQueueGroupsAreReturned, IsXe3Core) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableBlitterOperationsSupport.set(0);
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 2u);
}

class DeviceCopyQueueGroupXe3CoreFixture : public DeviceFixture {
  public:
    void setUp() {
        debugManager.flags.EnableBlitterOperationsSupport.set(0);
        DeviceFixture::setUp();
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

using DeviceCopyQueueGroupXe3CoreTest = Test<DeviceCopyQueueGroupXe3CoreFixture>;

HWTEST2_F(DeviceCopyQueueGroupXe3CoreTest,
          givenBlitterSupportAndEnableBlitterOperationsSupportSetToZeroThenNoCopyEngineIsReturned, IsXe3Core) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (auto &engineGroup : neoMockDevice->getRegularEngineGroups()) {
        EXPECT_NE(NEO::EngineGroupType::copy, engineGroup.engineGroupType);
    }
}

class CommandQueueGroupTestXe3Core : public DeviceFixture, public testing::TestWithParam<uint32_t> {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

HWTEST2_P(CommandQueueGroupTestXe3Core, givenVaryingBlitterSupportAndCCSThenBCSGroupContainsCorrectNumberOfEngines, IsXe3Core) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(2);
    hwInfo.featureTable.ftrBcsInfo.set(GetParam());
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 3u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::linkedCopy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, hwInfo.featureTable.ftrBcsInfo.count() - 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    CommandQueueGroupTestXe3CoreValues,
    CommandQueueGroupTestXe3Core,
    testing::Values(0, 1, 2, 3));

HWTEST2_F(DeviceXe3CoreTest, givenReturnedDevicePropertiesThenExpectedPageFaultSupportReturned, IsXe3Core) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    EXPECT_NE(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}
} // namespace ult
} // namespace L0
