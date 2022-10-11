/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
struct MultipleDeviceBdfUuidTest : public ::testing::Test {

    std::unique_ptr<UltDeviceFactory> createDevices(PhysicalDevicePciBusInfo &pciBusInfo, uint32_t numSubDevices) {
        std::unique_ptr<MockDriverModel> mockDriverModel = std::make_unique<MockDriverModel>();
        mockDriverModel->pciBusInfo = pciBusInfo;
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 1);
        executionEnvironment->parseAffinityMask();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface);
        executionEnvironment->memoryManager.reset(new MockMemoryManagerOsAgnosticContext(*executionEnvironment));
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(mockDriverModel));
        return std::make_unique<UltDeviceFactory>(1, numSubDevices, *executionEnvironment);
    }

    template <PRODUCT_FAMILY gfxProduct>
    void setupMockHwInfoConfig() {
        mockHwInfoConfig.reset(new MockHwInfoConfigHw<gfxProduct>());
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<HwInfoConfig> mockHwInfoConfig = nullptr;
};

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenValidBdfWithCombinationOfAffinityMaskThenUuidIsCorrectForRootAndSubDevices, MatchAny) {
    setupMockHwInfoConfig<productFamily>();
    VariableBackup<HwInfoConfig *> backupHwInfoConfig(&hwInfoConfigFactory[productFamily], mockHwInfoConfig.get());
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.2,0.3");
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, 4);

    std::array<uint8_t, 16> uuid;
    uint8_t expectedUuid[16] = {0x00, 0x00, 0x34, 0xab,
                                0xcd, 0x0, 0x0, 0x0,
                                0x0, 0x0, 0x0, 0x0,
                                0x0, 0x0, 0x0, 0x0};

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));

    const auto &subDevices = deviceFactory->rootDevices[0]->getSubDevices();

    ASSERT_EQ(subDevices.size(), 4u);
    expectedUuid[15] = 1;
    ASSERT_NE(subDevices[0], nullptr);
    EXPECT_EQ(true, subDevices[0]->getUuid(uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));

    expectedUuid[15] = 3;
    ASSERT_NE(subDevices[2], nullptr);
    EXPECT_EQ(true, subDevices[2]->getUuid(uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));

    expectedUuid[15] = 4;
    ASSERT_NE(subDevices[3], nullptr);
    EXPECT_EQ(true, subDevices[3]->getUuid(uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
}

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenDefaultAffinityMaskWhenRetrievingDeviceUuidFromBdfThenCorrectUuidIsRetrieved, MatchAny) {

    setupMockHwInfoConfig<productFamily>();
    VariableBackup<HwInfoConfig *> backupHwInfoConfig(&hwInfoConfigFactory[productFamily], mockHwInfoConfig.get());
    std::array<uint8_t, 16> uuid;
    const uint32_t numSubDevices = 2;
    uint8_t expectedUuid[16] = {0xad, 0x54, 0x34, 0xab,
                                0xcd, 0x0, 0x0, 0x0,
                                0x0, 0x0, 0x0, 0x0,
                                0x0, 0x0, 0x0, 0x0};
    DebugManager.flags.ZE_AFFINITY_MASK.set("default");
    PhysicalDevicePciBusInfo pciBusInfo(0x54ad, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, numSubDevices);

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));

    const auto &subDevices = deviceFactory->rootDevices[0]->getSubDevices();
    for (auto i = 0u; i < numSubDevices; i++) {
        expectedUuid[15] = i + 1;
        EXPECT_EQ(true, subDevices[i]->getUuid(uuid));
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
    }
}

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenIncorrectBdfWhenRetrievingDeviceUuidFromBdfThenUuidIsNotRetrieved, MatchAny) {

    setupMockHwInfoConfig<productFamily>();
    VariableBackup<HwInfoConfig *> backupHwInfoConfig(&hwInfoConfigFactory[productFamily], mockHwInfoConfig.get());
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue);
    const auto deviceFactory = createDevices(pciBusInfo, 2);

    std::array<uint8_t, 16> uuid;
    EXPECT_EQ(false, deviceFactory->rootDevices[0]->getUuid(uuid));
}

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenNoSubDevicesInAffinityMaskwhenRetrievingDeviceUuidFromBdfThenUuidOfRootDeviceIsRetrieved, MatchAny) {

    setupMockHwInfoConfig<productFamily>();
    VariableBackup<HwInfoConfig *> backupHwInfoConfig(&hwInfoConfigFactory[productFamily], mockHwInfoConfig.get());
    std::array<uint8_t, 16> uuid;
    uint8_t expectedUuid[16] = {0x00, 0x00, 0x34, 0xab,
                                0xcd, 0x0, 0x0, 0x0,
                                0x0, 0x0, 0x0, 0x0,
                                0x0, 0x0, 0x0, 0x0};

    DebugManager.flags.ZE_AFFINITY_MASK.set("0");
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, 2);

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
}

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenValidBdfWithOneBitEnabledInAffinityMaskThenUuidOfRootDeviceIsBasedOnAffinityMask, MatchAny) {

    setupMockHwInfoConfig<productFamily>();
    VariableBackup<HwInfoConfig *> backupHwInfoConfig(&hwInfoConfigFactory[productFamily], mockHwInfoConfig.get());
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.3");
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, 4);

    std::array<uint8_t, 16> uuid;
    uint8_t expectedUuid[16] = {0x00, 0x00, 0x34, 0xab,
                                0xcd, 0x0, 0x0, 0x0,
                                0x0, 0x0, 0x0, 0x0,
                                0x0, 0x0, 0x0, 0x0};

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    expectedUuid[15] = 4;
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
}

using DeviceUuidEnablementTest = MultipleDeviceBdfUuidTest;

template <PRODUCT_FAMILY gfxProduct>
class MockHwInfoConfigHwUuidEnablementTest : public HwInfoConfigHw<gfxProduct> {
  public:
    const bool returnStatus;
    MockHwInfoConfigHwUuidEnablementTest(bool returnStatus) : returnStatus(returnStatus) {}
    bool getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const override {
        uuid.fill(255u);
        return returnStatus;
    }
};

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsDefaultWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIstUsed, MatchAny) {

    mockHwInfoConfig.reset(new MockHwInfoConfigHwUuidEnablementTest<productFamily>(true));
    VariableBackup<HwInfoConfig *> backupHwInfoConfig(&hwInfoConfigFactory[productFamily], mockHwInfoConfig.get());
    std::array<uint8_t, 16> uuid, expectedUuid;
    uuid.fill(0u);
    expectedUuid.fill(255u);

    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, 2);

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));

    if (HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).isChipsetUniqueUUIDSupported()) {
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    } else {
        EXPECT_FALSE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    }
}

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsEnabledWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIsUsed, MatchAny) {

    mockHwInfoConfig.reset(new MockHwInfoConfigHwUuidEnablementTest<productFamily>(true));
    VariableBackup<HwInfoConfig *> backupHwInfoConfig(&hwInfoConfigFactory[productFamily], mockHwInfoConfig.get());
    DebugManager.flags.EnableChipsetUniqueUUID.set(1);
    std::array<uint8_t, 16> uuid, expectedUuid;
    uuid.fill(0u);
    expectedUuid.fill(255u);
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);

    const auto deviceFactory = createDevices(pciBusInfo, 2);
    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    if (HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).isChipsetUniqueUUIDSupported()) {
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    } else {
        EXPECT_FALSE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    }
}

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsDisabledWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIsNotUsed, MatchAny) {

    mockHwInfoConfig.reset(new MockHwInfoConfigHwUuidEnablementTest<productFamily>(true));
    VariableBackup<HwInfoConfig *> backupHwInfoConfig(&hwInfoConfigFactory[productFamily], mockHwInfoConfig.get());
    DebugManager.flags.EnableChipsetUniqueUUID.set(0);
    std::array<uint8_t, 16> uuid, expectedUuid;
    uuid.fill(0u);
    expectedUuid.fill(255u);

    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, 2);

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    EXPECT_FALSE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
}
} // namespace NEO
