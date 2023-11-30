/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
struct MultipleDeviceBdfUuidTest : public ::testing::Test {

    std::unique_ptr<UltDeviceFactory> createDevices(PhysicalDevicePciBusInfo &pciBusInfo, uint32_t numSubDevices, MockExecutionEnvironment *executionEnvironment) {
        std::unique_ptr<MockDriverModel> mockDriverModel = std::make_unique<MockDriverModel>();
        mockDriverModel->pciBusInfo = pciBusInfo;
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        executionEnvironment->parseAffinityMask();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface);
        executionEnvironment->memoryManager.reset(new MockMemoryManagerOsAgnosticContext(*executionEnvironment));
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(mockDriverModel));
        return std::make_unique<UltDeviceFactory>(1, numSubDevices, *executionEnvironment);
    }
    DebugManagerStateRestore restorer;
};

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenValidBdfWithCombinationOfAffinityMaskThenUuidIsCorrectForRootAndSubDevices, MatchAny) {

    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);

    debugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.2,0.3");
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    deviceFactory = createDevices(pciBusInfo, 4, mockExecutionEnvironment.release());

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

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    std::array<uint8_t, 16> uuid;
    const uint32_t numSubDevices = 2;
    uint8_t expectedUuid[NEO::ProductHelper::uuidSize] = {};
    debugManager.flags.ZE_AFFINITY_MASK.set("default");
    PhysicalDevicePciBusInfo pciBusInfo(0x54ad, 0x34, 0xab, 0xcd);
    deviceFactory = createDevices(pciBusInfo, numSubDevices, mockExecutionEnvironment.release());

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

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenDefaultAffinityMaskWhenRetrievingDeviceUuidFromBdfFromOneDeviceThenCorrectUuidIsRetrieved, MatchAny) {

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    std::array<uint8_t, 16> uuid;
    const uint32_t numSubDevices = 1;
    uint8_t expectedUuid[NEO::ProductHelper::uuidSize] = {};
    debugManager.flags.ZE_AFFINITY_MASK.set("default");
    PhysicalDevicePciBusInfo pciBusInfo(0x54ad, 0x34, 0xab, 0xcd);
    deviceFactory = createDevices(pciBusInfo, numSubDevices, mockExecutionEnvironment.release());

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

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenIncorrectBdfWhenRetrievingDeviceUuidFromBdfThenUuidIsNotRetrieved, MatchAny) {

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue,
                                        PhysicalDevicePciBusInfo::invalidValue);
    deviceFactory = createDevices(pciBusInfo, 2, mockExecutionEnvironment.release());

    std::array<uint8_t, 16> uuid;
    EXPECT_EQ(false, deviceFactory->rootDevices[0]->getUuid(uuid));
}

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenNoSubDevicesInAffinityMaskwhenRetrievingDeviceUuidFromBdfThenUuidOfRootDeviceIsRetrieved, MatchAny) {
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid{};
    uint8_t expectedUuid[NEO::ProductHelper::uuidSize] = {};

    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);

    debugManager.flags.ZE_AFFINITY_MASK.set("0");
    deviceFactory = createDevices(pciBusInfo, 2, mockExecutionEnvironment.release());

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

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenValidBdfWithOneBitEnabledInAffinityMaskSetToRootDeviceThenUuidOfRootDeviceIsBasedOnAffinityMask, MatchAny) {

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    debugManager.flags.ZE_AFFINITY_MASK.set("0");
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    deviceFactory = createDevices(pciBusInfo, 4, mockExecutionEnvironment.release());

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
}

HWTEST2_F(MultipleDeviceBdfUuidTest, GivenValidBdfWithOneBitEnabledInAffinityMaskSetToSubDeviceThenUuidOfRootDeviceIsBasedOnAffinityMask, MatchAny) {

    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    debugManager.flags.ZE_AFFINITY_MASK.set("0.3");
    uint32_t subDeviceIndex = 3;
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    deviceFactory = createDevices(pciBusInfo, 4, mockExecutionEnvironment.release());

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
    expectedUuid[15] = subDeviceIndex + 1;
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
}

using DeviceUuidEnablementTest = MultipleDeviceBdfUuidTest;

template <PRODUCT_FAMILY gfxProduct>
class MockProductHelperHwUuidEnablementTest : public ProductHelperHw<gfxProduct> {
  public:
    bool returnStatus;
    MockProductHelperHwUuidEnablementTest() {}
    bool getUuid(NEO::DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const override {
        uuid.fill(255u);
        return returnStatus;
    }
};

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsDefaultWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIstUsed, MatchAny) {

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHwUuidEnablementTest<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    raii.mockProductHelper->returnStatus = true;

    std::array<uint8_t, 16> uuid, expectedUuid;
    uuid.fill(0u);
    expectedUuid.fill(255u);

    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    deviceFactory = createDevices(pciBusInfo, 2, mockExecutionEnvironment.release());

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));

    auto &gfxCoreHelper = deviceFactory->rootDevices[0]->getGfxCoreHelper();
    if (gfxCoreHelper.isChipsetUniqueUUIDSupported()) {
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    } else {
        EXPECT_FALSE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    }
}

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsEnabledWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIsUsed, MatchAny) {

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHwUuidEnablementTest<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    raii.mockProductHelper->returnStatus = true;

    debugManager.flags.EnableChipsetUniqueUUID.set(1);
    std::array<uint8_t, 16> uuid, expectedUuid;
    uuid.fill(0u);
    expectedUuid.fill(255u);
    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);

    deviceFactory = createDevices(pciBusInfo, 2, mockExecutionEnvironment.release());
    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));

    auto &gfxCoreHelper = deviceFactory->rootDevices[0]->getGfxCoreHelper();
    if (gfxCoreHelper.isChipsetUniqueUUIDSupported()) {
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    } else {
        EXPECT_FALSE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
    }
}

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsDisabledWhenDeviceIsCreatedThenChipsetUniqueUuidUsingTelemetryIsNotUsed, MatchAny) {

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHwUuidEnablementTest<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    raii.mockProductHelper->returnStatus = true;

    debugManager.flags.EnableChipsetUniqueUUID.set(0);
    std::array<uint8_t, 16> uuid, expectedUuid;
    uuid.fill(0u);
    expectedUuid.fill(255u);

    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    deviceFactory = createDevices(pciBusInfo, 2, mockExecutionEnvironment.release());

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
    EXPECT_FALSE(0 == std::memcmp(uuid.data(), expectedUuid.data(), 16));
}

HWTEST2_F(DeviceUuidEnablementTest, GivenEnableChipsetUniqueUUIDIsTrueWhenDeviceIsCreatedAndGetHelperReturnsFalseThenGetUuidReturnsFalse, IsPVC) {

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1);
    RAIIProductHelperFactory<MockProductHelperHwUuidEnablementTest<productFamily>> raii(*mockExecutionEnvironment->rootDeviceEnvironments[0]);
    raii.mockProductHelper->returnStatus = false;

    debugManager.flags.EnableChipsetUniqueUUID.set(1);
    std::array<uint8_t, 16> uuid;
    uuid.fill(0u);

    PhysicalDevicePciBusInfo pciBusInfo(0x00, 0x34, 0xab, 0xcd);
    deviceFactory = createDevices(pciBusInfo, 2, mockExecutionEnvironment.release());

    EXPECT_EQ(true, deviceFactory->rootDevices[0]->getUuid(uuid));
}
} // namespace NEO
