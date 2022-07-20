/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

namespace NEO {
struct PrepareDeviceEnvironmentsTest : ::testing::Test {
    void SetUp() override {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        productConfigHelper = std::make_unique<ProductConfigHelper>();

        auto aotInfos = productConfigHelper->getDeviceAotInfo();
        for (const auto &aotInfo : aotInfos) {
            if (aotInfo.hwInfo->platform.eProductFamily == productFamily && !aotInfo.acronyms.empty()) {
                deviceAot = aotInfo;
                break;
            }
        }
        auto deprecatedAcronyms = productConfigHelper->getDeprecatedAcronyms();

        for (const auto &acronym : deprecatedAcronyms) {
            auto acronymStr = acronym.str();
            if (0 == strcmp(acronymStr.c_str(), hardwarePrefix[productFamily])) {
                deprecatedAcronym = acronymStr;
                break;
            }
        }
    }
    void TearDown() override {
    }

    DeviceAotInfo deviceAot;
    std::string product, deprecatedAcronym;
    const HardwareInfo *hwInfo = nullptr;
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    DebugManagerStateRestore stateRestorer;
    std::unique_ptr<ProductConfigHelper> productConfigHelper;
};

TEST_F(PrepareDeviceEnvironmentsTest, givenPrepareDeviceEnvironmentsWithPciPathWhenCsrIsSetToVariousTypesThenExpectedValuesOfHardwareInfoAreSet) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);

    if (deviceAot.acronyms.empty()) {
        GTEST_SKIP();
    }
    auto product = deviceAot.acronyms.front().str();

    for (int csrTypes = -1; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
        CommandStreamReceiverType csrType;
        if (csrTypes != -1) {
            csrType = static_cast<CommandStreamReceiverType>(csrTypes);
            DebugManager.flags.SetCommandStreamReceiver.set(csrType);
        } else {
            csrType = CSR_HW;
            DebugManager.flags.SetCommandStreamReceiver.set(-1);
        }

        DebugManager.flags.ProductFamilyOverride.set(product);
        MockExecutionEnvironment exeEnv;
        exeEnv.prepareRootDeviceEnvironments(expectedDevices);

        std::string pciPath = "0000:00:02.0";
        exeEnv.rootDeviceEnvironments.resize(1u);
        const auto ret = prepareDeviceEnvironment(exeEnv, pciPath, 0u);
        EXPECT_EQ(expectedDevices, exeEnv.rootDeviceEnvironments.size());
        for (auto i = 0u; i < expectedDevices; i++) {
            hwInfo = exeEnv.rootDeviceEnvironments[i]->getHardwareInfo();

            switch (csrType) {
            case CSR_HW:
            case CSR_HW_WITH_AUB:
            case CSR_TYPES_NUM:
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);
                break;
            default:
                EXPECT_FALSE(ret);
                break;
            }
        }
    }
}

TEST_F(PrepareDeviceEnvironmentsTest, givenPrepareDeviceEnvironmentsWithPciPathForDeprecatedNamesWhenCsrIsSetToVariousTypesThenExpectedValuesOfHardwareInfoAreSet) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);

    if (deprecatedAcronym.empty()) {
        GTEST_SKIP();
    }

    for (int csrTypes = -1; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
        CommandStreamReceiverType csrType;
        if (csrTypes != -1) {
            csrType = static_cast<CommandStreamReceiverType>(csrTypes);
            DebugManager.flags.SetCommandStreamReceiver.set(csrType);
        } else {
            csrType = CSR_HW;
            DebugManager.flags.SetCommandStreamReceiver.set(-1);
        }

        DebugManager.flags.ProductFamilyOverride.set(deprecatedAcronym);
        MockExecutionEnvironment exeEnv;
        exeEnv.prepareRootDeviceEnvironments(expectedDevices);

        std::string pciPath = "0000:00:02.0";
        exeEnv.rootDeviceEnvironments.resize(1u);
        const auto ret = prepareDeviceEnvironment(exeEnv, pciPath, 0u);
        EXPECT_EQ(expectedDevices, exeEnv.rootDeviceEnvironments.size());
        for (auto i = 0u; i < expectedDevices; i++) {
            hwInfo = exeEnv.rootDeviceEnvironments[i]->getHardwareInfo();

            switch (csrType) {
            case CSR_HW:
            case CSR_HW_WITH_AUB:
            case CSR_TYPES_NUM:
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);
                break;
            default:
                EXPECT_FALSE(ret);
                break;
            }
        }
    }
}

HWTEST_F(PrepareDeviceEnvironmentsTest, givenPrepareDeviceEnvironmentsForDeprecatedNamesWhenCsrIsSetToVariousTypesThenExpectedValuesOfHardwareInfoAreSet) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);

    if (deprecatedAcronym.empty()) {
        GTEST_SKIP();
    }

    for (int csrTypes = -1; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
        CommandStreamReceiverType csrType;
        if (csrTypes != -1) {
            csrType = static_cast<CommandStreamReceiverType>(csrTypes);
            DebugManager.flags.SetCommandStreamReceiver.set(csrType);
        } else {
            csrType = CSR_HW;
            DebugManager.flags.SetCommandStreamReceiver.set(-1);
        }

        DebugManager.flags.ProductFamilyOverride.set(deprecatedAcronym);
        MockExecutionEnvironment exeEnv;
        exeEnv.prepareRootDeviceEnvironments(expectedDevices);

        const auto ret = prepareDeviceEnvironments(exeEnv);
        EXPECT_EQ(expectedDevices, exeEnv.rootDeviceEnvironments.size());
        for (auto i = 0u; i < expectedDevices; i++) {
            hwInfo = exeEnv.rootDeviceEnvironments[i]->getHardwareInfo();

            switch (csrType) {
            case CSR_HW:
            case CSR_HW_WITH_AUB:
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);
                break;
            case CSR_AUB:
            case CSR_TBX:
            case CSR_TBX_WITH_AUB: {
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);

                for (i = 0; i < IGFX_MAX_PRODUCT; i++) {
                    auto hardwareInfo = hardwareInfoTable[i];
                    if (hardwareInfo == nullptr)
                        continue;
                    if (hardwareInfoTable[i]->platform.eProductFamily == hwInfo->platform.eProductFamily)
                        break;
                }

                EXPECT_TRUE(i < IGFX_MAX_PRODUCT);
                ASSERT_NE(nullptr, hardwarePrefix[i]);

                HardwareInfo hwInfoFromTable = *hardwareInfoTable[i];
                hwInfoFromTable.featureTable = {};
                hwInfoFromTable.workaroundTable = {};
                hwInfoFromTable.gtSystemInfo = {};
                hardwareInfoSetup[hwInfoFromTable.platform.eProductFamily](&hwInfoFromTable, true, 0x0);
                HwInfoConfig *hwConfig = HwInfoConfig::get(hwInfoFromTable.platform.eProductFamily);
                hwConfig->configureHardwareCustom(&hwInfoFromTable, nullptr);
                EXPECT_EQ(0, memcmp(&hwInfoFromTable.platform, &hwInfo->platform, sizeof(PLATFORM)));

                EXPECT_STREQ(hardwarePrefix[i], deprecatedAcronym.c_str());
                break;
            }
            default:
                break;
            }
        }
    }
}

HWTEST_F(PrepareDeviceEnvironmentsTest, givenPrepareDeviceEnvironmentsWhenCsrIsSetToVariousTypesThenExpectedValuesOfHardwareInfoAreSet) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);

    if (deviceAot.acronyms.empty()) {
        GTEST_SKIP();
    }
    auto product = deviceAot.acronyms.front().str();

    for (int csrTypes = -1; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
        CommandStreamReceiverType csrType;
        if (csrTypes != -1) {
            csrType = static_cast<CommandStreamReceiverType>(csrTypes);
            DebugManager.flags.SetCommandStreamReceiver.set(csrType);
        } else {
            csrType = CSR_HW;
            DebugManager.flags.SetCommandStreamReceiver.set(-1);
        }

        DebugManager.flags.ProductFamilyOverride.set(product);
        MockExecutionEnvironment exeEnv;
        exeEnv.prepareRootDeviceEnvironments(expectedDevices);

        const auto ret = prepareDeviceEnvironments(exeEnv);
        EXPECT_EQ(expectedDevices, exeEnv.rootDeviceEnvironments.size());
        for (auto i = 0u; i < expectedDevices; i++) {
            hwInfo = exeEnv.rootDeviceEnvironments[i]->getHardwareInfo();

            switch (csrType) {
            case CSR_HW:
            case CSR_HW_WITH_AUB:
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);
                break;
            case CSR_AUB:
            case CSR_TBX:
            case CSR_TBX_WITH_AUB: {
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);

                HardwareInfo expectedHwInfo = *deviceAot.hwInfo;
                expectedHwInfo.featureTable = {};
                expectedHwInfo.workaroundTable = {};
                expectedHwInfo.gtSystemInfo = {};
                hardwareInfoSetup[expectedHwInfo.platform.eProductFamily](&expectedHwInfo, true, 0x0);

                HwInfoConfig *hwConfig = HwInfoConfig::get(expectedHwInfo.platform.eProductFamily);
                hwConfig->configureHardwareCustom(&expectedHwInfo, nullptr);

                const auto &compilerHwInfoConfig = *NEO::CompilerHwInfoConfig::get(expectedHwInfo.platform.eProductFamily);
                compilerHwInfoConfig.setProductConfigForHwInfo(expectedHwInfo, deviceAot.aotConfig);
                expectedHwInfo.platform.usDeviceID = deviceAot.deviceIds->front();

                EXPECT_EQ(0, memcmp(&expectedHwInfo.platform, &hwInfo->platform, sizeof(PLATFORM)));
                break;
            }
            default:
                break;
            }
        }
    }
}

HWTEST_F(PrepareDeviceEnvironmentsTest, givenUpperCaseDeprecatedAcronymsToProductFamilyOverrideFlagWhenCreatingDevicesThenFindExpectedPlatform) {
    if (deprecatedAcronym.empty()) {
        GTEST_SKIP();
    }

    std::transform(deprecatedAcronym.begin(), deprecatedAcronym.end(), deprecatedAcronym.begin(), ::toupper);
    DebugManager.flags.ProductFamilyOverride.set(deprecatedAcronym);
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_AUB);
    MockExecutionEnvironment exeEnv;
    bool ret = prepareDeviceEnvironments(exeEnv);

    EXPECT_TRUE(ret);
    EXPECT_EQ(productFamily, exeEnv.rootDeviceEnvironments[0]->getHardwareInfo()->platform.eProductFamily);
}

HWTEST_F(PrepareDeviceEnvironmentsTest, givenUpperCaseProductFamilyOverrideFlagSetWhenCreatingDevicesThenFindExpectedPlatform) {
    if (deviceAot.acronyms.empty()) {
        GTEST_SKIP();
    }
    auto product = deviceAot.acronyms.front().str();
    std::transform(product.begin(), product.end(), product.begin(), ::toupper);

    DebugManager.flags.ProductFamilyOverride.set(product);
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_AUB);

    MockExecutionEnvironment exeEnv;
    bool ret = prepareDeviceEnvironments(exeEnv);

    EXPECT_TRUE(ret);
    EXPECT_EQ(deviceAot.hwInfo->platform.eProductFamily, exeEnv.rootDeviceEnvironments[0]->getHardwareInfo()->platform.eProductFamily);
}

HWTEST_F(PrepareDeviceEnvironmentsTest, givenPrepareDeviceEnvironmentsAndUnknownProductFamilyWhenCsrIsSetToValidTypeThenTheFunctionReturnsTheExpectedValueOfHardwareInfo) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);
    for (int csrTypes = 0; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
        CommandStreamReceiverType csrType = static_cast<CommandStreamReceiverType>(csrTypes);
        std::string productFamily("unk");

        DebugManager.flags.SetCommandStreamReceiver.set(csrType);
        DebugManager.flags.ProductFamilyOverride.set(productFamily);
        MockExecutionEnvironment exeEnv;
        exeEnv.prepareRootDeviceEnvironments(expectedDevices);

        auto ret = prepareDeviceEnvironments(exeEnv);
        EXPECT_EQ(expectedDevices, exeEnv.rootDeviceEnvironments.size());
        for (auto i = 0u; i < expectedDevices; i++) {
            hwInfo = exeEnv.rootDeviceEnvironments[i]->getHardwareInfo();

            switch (csrType) {
            case CSR_HW:
            case CSR_HW_WITH_AUB:
                EXPECT_TRUE(ret);
                break;
            case CSR_AUB:
            case CSR_TBX:
            case CSR_TBX_WITH_AUB: {
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);
                for (i = 0; i < IGFX_MAX_PRODUCT; i++) {
                    auto hardwareInfo = hardwareInfoTable[i];
                    if (hardwareInfo == nullptr)
                        continue;
                    if (hardwareInfoTable[i]->platform.eProductFamily == hwInfo->platform.eProductFamily)
                        break;
                }
                EXPECT_TRUE(i < IGFX_MAX_PRODUCT);
                ASSERT_NE(nullptr, hardwarePrefix[i]);
                HardwareInfo baseHwInfo = *defaultHwInfo;
                baseHwInfo.featureTable = {};
                baseHwInfo.workaroundTable = {};
                baseHwInfo.gtSystemInfo = {};
                hardwareInfoSetup[baseHwInfo.platform.eProductFamily](&baseHwInfo, true, 0x0);
                HwInfoConfig *hwConfig = HwInfoConfig::get(baseHwInfo.platform.eProductFamily);
                hwConfig->configureHardwareCustom(&baseHwInfo, nullptr);
                EXPECT_EQ(0, memcmp(&baseHwInfo.platform, &hwInfo->platform, sizeof(PLATFORM)));
                break;
            }
            default:
                break;
            }
        }
    }
}

} // namespace NEO
