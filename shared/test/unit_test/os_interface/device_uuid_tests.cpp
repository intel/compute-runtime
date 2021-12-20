/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

class MockMemoryManagerOsAgnosticContext : public MockMemoryManager {
  public:
    MockMemoryManagerOsAgnosticContext(ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
    OsContext *createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                          const EngineDescriptor &engineDescriptor) {
        auto osContext = new OsContext(0, engineDescriptor);
        osContext->incRefInternal();
        registeredEngines.emplace_back(commandStreamReceiver, osContext);
        return osContext;
    }
};

struct MockDriverModel : NEO::DriverModel {
    PhysicalDevicePciBusInfo pciBusInfo{};
    MockDriverModel() : NEO::DriverModel(NEO::DriverModelType::UNKNOWN) {}
    void setGmmInputArgs(void *args) override {}
    uint32_t getDeviceHandle() const override { return {}; }
    PhysicalDevicePciBusInfo getPciBusInfo() const override { return pciBusInfo; }
    size_t getMaxMemAllocSize() const override {
        return 0;
    }
};

template <PRODUCT_FAMILY gfxProduct>
class MockHwInfoConfigHw : public HwInfoConfigHw<gfxProduct> {
  public:
    bool getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const {
        return false;
    }
};

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
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::InvalidValue,
                                        PhysicalDevicePciBusInfo::InvalidValue,
                                        PhysicalDevicePciBusInfo::InvalidValue,
                                        PhysicalDevicePciBusInfo::InvalidValue);
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
} // namespace NEO