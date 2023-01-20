/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
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
    void setupMockProductHelper() {
        mockProductHelper.reset(new MockProductHelperHw<gfxProduct>());
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<ProductHelper> mockProductHelper = nullptr;
};

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenValidBdfWithCombinationOfAffinityMaskThenUuidIsCorrectForRootAndSubDevices, MatchAny) {
    setupMockProductHelper<productFamily>();
    VariableBackup<ProductHelper *> backupProductHelper(&productHelperFactory[productFamily], mockProductHelper.get());
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.2,0.3");
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, 4);

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    uint8_t expectedUuid[NEO::ProductHelper::uuidSize] = {};

    uint16_t vendorId = 0x8086; // Intel
    uint16_t deviceId = static_cast<uint16_t>(deviceFactory->rootDevices[0]->getHardwareInfo().platform.usDeviceID);
    uint16_t revisionId = static_cast<uint16_t>(deviceFactory->rootDevices[0]->getHardwareInfo().platform.usRevId);
    uint16_t pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
    uint8_t pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
    uint8_t pciDevice = static_cast<uint8_t>(pciBusInfo.pciDevice);
    uint8_t pciFunction = static_cast<uint8_t>(pciBusInfo.pciFunction);

    memcpy_s(&expectedUuid[0], sizeof(uint16_t), &vendorId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[2], sizeof(uint16_t), &deviceId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[4], sizeof(uint16_t), &revisionId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[6], sizeof(uint16_t), &pciDomain, sizeof(uint16_t));
    memcpy_s(&expectedUuid[8], sizeof(uint8_t), &pciBus, sizeof(uint8_t));
    memcpy_s(&expectedUuid[9], sizeof(uint8_t), &pciDevice, sizeof(uint8_t));
    memcpy_s(&expectedUuid[10], sizeof(uint8_t), &pciFunction, sizeof(uint8_t));

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

    setupMockProductHelper<productFamily>();
    VariableBackup<ProductHelper *> backupProductHelper(&productHelperFactory[productFamily], mockProductHelper.get());
    std::array<uint8_t, 16> uuid;
    const uint32_t numSubDevices = 2;
    uint8_t expectedUuid[NEO::ProductHelper::uuidSize] = {};
    DebugManager.flags.ZE_AFFINITY_MASK.set("default");
    PhysicalDevicePciBusInfo pciBusInfo(0x54ad, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, numSubDevices);

    uint16_t vendorId = 0x8086; // Intel
    uint16_t deviceId = static_cast<uint16_t>(deviceFactory->rootDevices[0]->getHardwareInfo().platform.usDeviceID);
    uint16_t revisionId = static_cast<uint16_t>(deviceFactory->rootDevices[0]->getHardwareInfo().platform.usRevId);
    uint16_t pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
    uint8_t pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
    uint8_t pciDevice = static_cast<uint8_t>(pciBusInfo.pciDevice);
    uint8_t pciFunction = static_cast<uint8_t>(pciBusInfo.pciFunction);

    memcpy_s(&expectedUuid[0], sizeof(uint16_t), &vendorId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[2], sizeof(uint16_t), &deviceId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[4], sizeof(uint16_t), &revisionId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[6], sizeof(uint16_t), &pciDomain, sizeof(uint16_t));
    memcpy_s(&expectedUuid[8], sizeof(uint8_t), &pciBus, sizeof(uint8_t));
    memcpy_s(&expectedUuid[9], sizeof(uint8_t), &pciDevice, sizeof(uint8_t));
    memcpy_s(&expectedUuid[10], sizeof(uint8_t), &pciFunction, sizeof(uint8_t));

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

    setupMockProductHelper<productFamily>();
    VariableBackup<ProductHelper *> backupProductHelper(&productHelperFactory[productFamily], mockProductHelper.get());
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue);
    const auto deviceFactory = createDevices(pciBusInfo, 2);

    std::array<uint8_t, 16> uuid;
    EXPECT_EQ(false, deviceFactory->rootDevices[0]->getUuid(uuid));
}

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenNoSubDevicesInAffinityMaskwhenRetrievingDeviceUuidFromBdfThenUuidOfRootDeviceIsRetrieved, MatchAny) {

    setupMockProductHelper<productFamily>();
    VariableBackup<ProductHelper *> backupProductHelper(&productHelperFactory[productFamily], mockProductHelper.get());
    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid{};
    uint8_t expectedUuid[NEO::ProductHelper::uuidSize] = {};

    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);

    DebugManager.flags.ZE_AFFINITY_MASK.set("0");
    const auto deviceFactory = createDevices(pciBusInfo, 2);

    uint16_t vendorId = 0x8086; // Intel
    uint16_t deviceId = static_cast<uint16_t>(deviceFactory->rootDevices[0]->getHardwareInfo().platform.usDeviceID);
    uint16_t revisionId = static_cast<uint16_t>(deviceFactory->rootDevices[0]->getHardwareInfo().platform.usRevId);
    uint16_t pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
    uint8_t pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
    uint8_t pciDevice = static_cast<uint8_t>(pciBusInfo.pciDevice);
    uint8_t pciFunction = static_cast<uint8_t>(pciBusInfo.pciFunction);

    memcpy_s(&expectedUuid[0], sizeof(uint16_t), &vendorId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[2], sizeof(uint16_t), &deviceId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[4], sizeof(uint16_t), &revisionId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[6], sizeof(uint16_t), &pciDomain, sizeof(uint16_t));
    memcpy_s(&expectedUuid[8], sizeof(uint8_t), &pciBus, sizeof(uint8_t));
    memcpy_s(&expectedUuid[9], sizeof(uint8_t), &pciDevice, sizeof(uint8_t));
    memcpy_s(&expectedUuid[10], sizeof(uint8_t), &pciFunction, sizeof(uint8_t));

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
}

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenValidBdfWithOneBitEnabledInAffinityMaskThenUuidOfRootDeviceIsBasedOnAffinityMask, MatchAny) {

    setupMockProductHelper<productFamily>();
    VariableBackup<ProductHelper *> backupProductHelper(&productHelperFactory[productFamily], mockProductHelper.get());
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.3");
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, 4);

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    uint8_t expectedUuid[NEO::ProductHelper::uuidSize] = {};

    uint16_t vendorId = 0x8086; // Intel
    uint16_t deviceId = static_cast<uint16_t>(deviceFactory->rootDevices[0]->getHardwareInfo().platform.usDeviceID);
    uint16_t revisionId = static_cast<uint16_t>(deviceFactory->rootDevices[0]->getHardwareInfo().platform.usRevId);
    uint16_t pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
    uint8_t pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
    uint8_t pciDevice = static_cast<uint8_t>(pciBusInfo.pciDevice);
    uint8_t pciFunction = static_cast<uint8_t>(pciBusInfo.pciFunction);

    memcpy_s(&expectedUuid[0], sizeof(uint16_t), &vendorId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[2], sizeof(uint16_t), &deviceId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[4], sizeof(uint16_t), &revisionId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[6], sizeof(uint16_t), &pciDomain, sizeof(uint16_t));
    memcpy_s(&expectedUuid[8], sizeof(uint8_t), &pciBus, sizeof(uint8_t));
    memcpy_s(&expectedUuid[9], sizeof(uint8_t), &pciDevice, sizeof(uint8_t));
    memcpy_s(&expectedUuid[10], sizeof(uint8_t), &pciFunction, sizeof(uint8_t));

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    expectedUuid[15] = 4;
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
}

using DeviceUuidEnablementTest = MultipleDeviceBdfUuidTest;

template <PRODUCT_FAMILY gfxProduct>
class MockProductHelperHwUuidEnablementTest : public ProductHelperHw<gfxProduct> {
  public:
    const bool returnStatus;
    MockProductHelperHwUuidEnablementTest(bool returnStatus) : returnStatus(returnStatus) {}
    bool getUuid(Device *device, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const override {
        uuid.fill(255u);
        return returnStatus;
    }
};

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsDefaultWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIstUsed, MatchAny) {

    mockProductHelper.reset(new MockProductHelperHwUuidEnablementTest<productFamily>(true));
    VariableBackup<ProductHelper *> backupProductHelper(&productHelperFactory[productFamily], mockProductHelper.get());
    std::array<uint8_t, 16> uuid, expectedUuid;
    uuid.fill(0u);
    expectedUuid.fill(255u);

    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    const auto deviceFactory = createDevices(pciBusInfo, 2);

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));

    auto &gfxCoreHelper = deviceFactory->rootDevices[0]->getGfxCoreHelper();
    if (gfxCoreHelper.isChipsetUniqueUUIDSupported()) {
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    } else {
        EXPECT_FALSE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    }
}

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsEnabledWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIsUsed, MatchAny) {

    mockProductHelper.reset(new MockProductHelperHwUuidEnablementTest<productFamily>(true));
    VariableBackup<ProductHelper *> backupProductHelper(&productHelperFactory[productFamily], mockProductHelper.get());
    DebugManager.flags.EnableChipsetUniqueUUID.set(1);
    std::array<uint8_t, 16> uuid, expectedUuid;
    uuid.fill(0u);
    expectedUuid.fill(255u);
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);

    const auto deviceFactory = createDevices(pciBusInfo, 2);
    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));

    auto &gfxCoreHelper = deviceFactory->rootDevices[0]->getGfxCoreHelper();
    if (gfxCoreHelper.isChipsetUniqueUUIDSupported()) {
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    } else {
        EXPECT_FALSE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    }
}

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsDisabledWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIsNotUsed, MatchAny) {

    mockProductHelper.reset(new MockProductHelperHwUuidEnablementTest<productFamily>(true));
    VariableBackup<ProductHelper *> backupProductHelper(&productHelperFactory[productFamily], mockProductHelper.get());
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
