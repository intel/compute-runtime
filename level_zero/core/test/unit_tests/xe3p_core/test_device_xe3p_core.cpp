/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

using DeviceXe3pCoreTest = Test<DeviceFixture>;

XE3P_CORETEST_F(DeviceXe3pCoreTest, whenCallingGetMemoryPropertiesWithNonNullPtrThenPropertiesAreReturned) {
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

XE3P_CORETEST_F(CommandQueueGroupTest, givenNoBlitterSupportAndNoCCSThenOneQueueGroupIsReturned) {
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

XE3P_CORETEST_F(CommandQueueGroupTest, givenNoBlitterSupportAndCCSThenOneQueueGroupIsReturned) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 1u);
}

XE3P_CORETEST_F(CommandQueueGroupTest, givenBlitterSupportAndCCSThenThreeQueueGroupsAreReturned) {
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
    EXPECT_GE(count, 3u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_FALSE(engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute);

        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
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

XE3P_CORETEST_F(CommandQueueGroupTest, givenBlitterSupportCCSAndLinkedBcsDisabledThenTwoQueueGroupsAreReturned) {
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
    EXPECT_GE(count, 2u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_FALSE(engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute);

        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
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

XE3P_CORETEST_F(CommandQueueGroupTest, givenBlitterDisabledAndAllBcsSetThenOneQueueGroupIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableBlitterOperationsSupport.set(0);
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.flags.ftrRcsNode = false;

    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 1u);
    for (const auto &engineGroup : neoMockDevice->getRegularEngineGroups()) {
        EXPECT_NE(NEO::EngineGroupType::copy, engineGroup.engineGroupType);
    }
}

XE3P_CORETEST_F(CommandQueueGroupTest, givenContextGroupSupportAndCCSEngineEnabledWhenGettingCsrForOrdinalAndIndexThenSecondaryEnginesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableBlitterOperationsSupport.set(0);
    debugManager.flags.ContextGroupSize.set(8);
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    const NEO::GfxCoreHelper &gfxCoreHelper = neoMockDevice->getGfxCoreHelper();

    auto &secondaryEngines = neoMockDevice->secondaryEngines[aub_stream::EngineType::ENGINE_CCS];

    auto contextCount = gfxCoreHelper.getContextGroupContextsCount();
    const auto regularContextCount = secondaryEngines.regularEnginesTotal;
    const auto highPriorityContextCount = secondaryEngines.highPriorityEnginesTotal;
    EXPECT_EQ(4u, regularContextCount);
    EXPECT_EQ(4u, highPriorityContextCount);

    std::vector<CommandStreamReceiver *> csrs;
    csrs.resize(regularContextCount);

    for (size_t i = 0; i < regularContextCount; i++) {
        ze_result_t res = deviceImp.getCsrForOrdinalAndIndex(&csrs[i], 0, 0, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        EXPECT_EQ(secondaryEngines.engines[i].commandStreamReceiver, csrs[i]);
    }

    for (size_t i = regularContextCount; i < contextCount; i++) {
        EXPECT_TRUE(secondaryEngines.engines[i].commandStreamReceiver->getOsContext().getEngineUsage() == NEO::EngineUsage::highPriority);
    }

    CommandStreamReceiver *lowPriorityCsr = nullptr;
    ze_result_t res = deviceImp.getCsrForLowPriority(&lowPriorityCsr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ASSERT_NE(nullptr, lowPriorityCsr);
    EXPECT_FALSE(lowPriorityCsr->getOsContext().isPartOfContextGroup());
    EXPECT_EQ(nullptr, lowPriorityCsr->getOsContext().getPrimaryContext());
}

XE3P_CORETEST_F(CommandQueueGroupTest, givenContextGroupSupportAndBcsEngineEnabledWhenGettingCsrForOrdinalAndIndexThenSecondaryEnginesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableBlitterOperationsSupport.set(1);
    debugManager.flags.ContextGroupSize.set(8);
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0b111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    const NEO::GfxCoreHelper &gfxCoreHelper = neoMockDevice->getGfxCoreHelper();

    uint32_t bcsEngineCount = 0;

    auto &regularEngineGroups = neoMockDevice->getRegularEngineGroups();

    for (uint32_t ordinal = 0; ordinal < regularEngineGroups.size(); ordinal++) {
        if (regularEngineGroups[ordinal].engineGroupType != EngineGroupType::copy && regularEngineGroups[ordinal].engineGroupType != EngineGroupType::linkedCopy) {
            continue;
        }

        for (uint32_t index = 0; index < regularEngineGroups[ordinal].engines.size(); index++) {
            bcsEngineCount++;

            auto &secondaryEngines = neoMockDevice->secondaryEngines[regularEngineGroups[ordinal].engines[index].getEngineType()];

            auto contextCount = gfxCoreHelper.getContextGroupContextsCount();
            auto hpContextCount = gfxCoreHelper.getContextGroupHpContextsCount(regularEngineGroups[ordinal].engineGroupType, true);
            const auto regularContextCount = secondaryEngines.regularEnginesTotal;
            const auto highPriorityContextCount = secondaryEngines.highPriorityEnginesTotal;

            EXPECT_EQ(8u, regularContextCount);
            EXPECT_EQ(0u, highPriorityContextCount);
            EXPECT_EQ(0u, hpContextCount);

            std::vector<CommandStreamReceiver *> csrs;
            csrs.resize(regularContextCount);

            for (size_t i = 0; i < regularContextCount; i++) {
                ze_result_t res = deviceImp.getCsrForOrdinalAndIndex(&csrs[i], ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt, false);
                EXPECT_EQ(ZE_RESULT_SUCCESS, res);

                EXPECT_EQ(secondaryEngines.engines[i].commandStreamReceiver, csrs[i]);
            }

            for (size_t i = regularContextCount; i < contextCount; i++) {
                EXPECT_TRUE(secondaryEngines.engines[i].commandStreamReceiver->getOsContext().getEngineUsage() == NEO::EngineUsage::highPriority);
            }
        }
    }

    EXPECT_NE(0u, bcsEngineCount);
}

XE3P_CORETEST_F(CommandQueueGroupTest, givenVaryingBlitterSupportAndCCSThenBCSGroupContainsCorrectNumberOfEngines) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(2);
    for (const auto ftrBcsInfoValue : {0, 1, 2, 3}) {
        hwInfo.featureTable.ftrBcsInfo.set(ftrBcsInfoValue);
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
}

XE3P_CORETEST_F(DeviceXe3pCoreTest, givenReturnedDevicePropertiesThenExpectedPageFaultSupportReturned) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    EXPECT_NE(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}
} // namespace ult
} // namespace L0
