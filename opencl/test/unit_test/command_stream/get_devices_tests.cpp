/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/libult/create_command_stream.h"
#include "test.h"

namespace NEO {
bool operator==(const HardwareInfo &hwInfoIn, const HardwareInfo &hwInfoOut) {
    bool result = (0 == memcmp(&hwInfoIn.platform, &hwInfoOut.platform, sizeof(PLATFORM)));
    result &= (0 == memcmp(&hwInfoIn.featureTable, &hwInfoOut.featureTable, sizeof(FeatureTable)));
    result &= (0 == memcmp(&hwInfoIn.workaroundTable, &hwInfoOut.workaroundTable, sizeof(WorkaroundTable)));
    result &= (0 == memcmp(&hwInfoIn.capabilityTable, &hwInfoOut.capabilityTable, sizeof(RuntimeCapabilityTable)));
    return result;
}

struct GetDevicesTest : ::testing::Test {
    void SetUp() override {
        ultHwConfig.useMockedGetDevicesFunc = false;
    }
    void TearDown() override {
    }
    int i = 0;
    size_t numDevices = 0;
    const HardwareInfo *hwInfo = nullptr;

    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    DebugManagerStateRestore stateRestorer;
};

HWTEST_F(GetDevicesTest, givenGetDevicesWhenCsrIsSetToVariousTypesThenTheFunctionReturnsTheExpectedValueOfHardwareInfo) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);
    for (int productFamilyIndex = 0; productFamilyIndex < IGFX_MAX_PRODUCT; productFamilyIndex++) {
        const char *hwPrefix = hardwarePrefix[productFamilyIndex];
        if (hwPrefix == nullptr) {
            continue;
        }
        const std::string productFamily(hwPrefix);

        for (int csrTypes = -1; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
            CommandStreamReceiverType csrType;
            if (csrTypes != -1) {
                csrType = static_cast<CommandStreamReceiverType>(csrTypes);
                DebugManager.flags.SetCommandStreamReceiver.set(csrType);
            } else {
                csrType = CSR_HW;
                DebugManager.flags.SetCommandStreamReceiver.set(-1);
            }

            DebugManager.flags.ProductFamilyOverride.set(productFamily);
            platformsImpl.clear();
            ExecutionEnvironment *exeEnv = constructPlatform()->peekExecutionEnvironment();

            const auto ret = getDevices(numDevices, *exeEnv);
            for (auto i = 0u; i < expectedDevices; i++) {
                hwInfo = exeEnv->rootDeviceEnvironments[i]->getHardwareInfo();

                switch (csrType) {
                case CSR_HW:
                case CSR_HW_WITH_AUB:
                    EXPECT_TRUE(ret);
                    EXPECT_NE(nullptr, hwInfo);
                    EXPECT_EQ(expectedDevices, numDevices);
                    break;
                case CSR_AUB:
                case CSR_TBX:
                case CSR_TBX_WITH_AUB: {
                    EXPECT_TRUE(ret);
                    EXPECT_NE(nullptr, hwInfo);
                    EXPECT_EQ(expectedDevices, numDevices);
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
                    EXPECT_EQ(0, memcmp(&hwInfoFromTable.capabilityTable, &hwInfo->capabilityTable, sizeof(RuntimeCapabilityTable)));

                    EXPECT_STREQ(hardwarePrefix[i], productFamily.c_str());
                    break;
                }
                default:
                    break;
                }
            }
        }
    }
}

HWTEST_F(GetDevicesTest, givenUpperCaseProductFamilyOverrideFlagSetWhenCreatingDevicesThenFindExpectedPlatform) {
    std::string hwPrefix;
    std::string hwPrefixUpperCase;
    PRODUCT_FAMILY productFamily;

    for (int productFamilyIndex = 0; productFamilyIndex < IGFX_MAX_PRODUCT; productFamilyIndex++) {
        if (hardwarePrefix[productFamilyIndex]) {
            hwPrefix = hardwarePrefix[productFamilyIndex];
            productFamily = static_cast<PRODUCT_FAMILY>(productFamilyIndex);
            break;
        }
    }

    EXPECT_NE(0u, hwPrefix.length());
    hwPrefixUpperCase.resize(hwPrefix.length());
    std::transform(hwPrefix.begin(), hwPrefix.end(), hwPrefixUpperCase.begin(), ::toupper);
    EXPECT_NE(hwPrefix, hwPrefixUpperCase);

    DebugManager.flags.ProductFamilyOverride.set(hwPrefixUpperCase);
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_AUB);

    ExecutionEnvironment *exeEnv = platform()->peekExecutionEnvironment();
    bool ret = getDevices(numDevices, *exeEnv);

    EXPECT_TRUE(ret);
    EXPECT_EQ(productFamily, exeEnv->rootDeviceEnvironments[0]->getHardwareInfo()->platform.eProductFamily);
}

HWTEST_F(GetDevicesTest, givenGetDevicesAndUnknownProductFamilyWhenCsrIsSetToValidTypeThenTheFunctionReturnsTheExpectedValueOfHardwareInfo) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);
    for (int csrTypes = 0; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
        CommandStreamReceiverType csrType = static_cast<CommandStreamReceiverType>(csrTypes);
        std::string productFamily("unk");

        DebugManager.flags.SetCommandStreamReceiver.set(csrType);
        DebugManager.flags.ProductFamilyOverride.set(productFamily);
        platformsImpl.clear();
        ExecutionEnvironment *exeEnv = constructPlatform()->peekExecutionEnvironment();

        auto ret = getDevices(numDevices, *exeEnv);
        for (auto i = 0u; i < expectedDevices; i++) {
            hwInfo = exeEnv->rootDeviceEnvironments[i]->getHardwareInfo();

            switch (csrType) {
            case CSR_HW:
            case CSR_HW_WITH_AUB:
                EXPECT_TRUE(ret);
                EXPECT_EQ(expectedDevices, numDevices);
                break;
            case CSR_AUB:
            case CSR_TBX:
            case CSR_TBX_WITH_AUB: {
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);
                EXPECT_EQ(expectedDevices, numDevices);
                for (i = 0; i < IGFX_MAX_PRODUCT; i++) {
                    auto hardwareInfo = hardwareInfoTable[i];
                    if (hardwareInfo == nullptr)
                        continue;
                    if (hardwareInfoTable[i]->platform.eProductFamily == hwInfo->platform.eProductFamily)
                        break;
                }
                EXPECT_TRUE(i < IGFX_MAX_PRODUCT);
                ASSERT_NE(nullptr, hardwarePrefix[i]);
                HardwareInfo defaultHwInfo = DEFAULT_PLATFORM::hwInfo;
                defaultHwInfo.featureTable = {};
                defaultHwInfo.workaroundTable = {};
                defaultHwInfo.gtSystemInfo = {};
                hardwareInfoSetup[defaultHwInfo.platform.eProductFamily](&defaultHwInfo, true, 0x0);
                HwInfoConfig *hwConfig = HwInfoConfig::get(defaultHwInfo.platform.eProductFamily);
                hwConfig->configureHardwareCustom(&defaultHwInfo, nullptr);
                EXPECT_EQ(0, memcmp(&defaultHwInfo.platform, &hwInfo->platform, sizeof(PLATFORM)));
                EXPECT_EQ(0, memcmp(&defaultHwInfo.capabilityTable, &hwInfo->capabilityTable, sizeof(RuntimeCapabilityTable)));
                break;
            }
            default:
                break;
            }
        }
    }
}
} // namespace NEO
