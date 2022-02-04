/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {
bool operator==(const HardwareInfo &hwInfoIn, const HardwareInfo &hwInfoOut) {
    bool result = (0 == memcmp(&hwInfoIn.platform, &hwInfoOut.platform, sizeof(PLATFORM)));
    result &= (hwInfoIn.featureTable.asHash() == hwInfoOut.featureTable.asHash());
    result &= (hwInfoIn.workaroundTable.asHash() == hwInfoOut.workaroundTable.asHash());
    result &= (hwInfoIn.capabilityTable == hwInfoOut.capabilityTable);
    return result;
}

TEST(PrepareDeviceEnvironmentTest, givenPrepareDeviceEnvironmentWhenCsrIsSetToVariousTypesThenFunctionReturnsExpectedValueOfHardwareInfo) {
    const HardwareInfo *hwInfo = nullptr;
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    DebugManagerStateRestore stateRestorer;
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);
    for (int productFamilyIndex = 0; productFamilyIndex < IGFX_MAX_PRODUCT; productFamilyIndex++) {
        const char *hwPrefix = hardwarePrefix[productFamilyIndex];
        auto hwInfoConfig = hwInfoConfigFactory[productFamilyIndex];
        if (hwPrefix == nullptr || hwInfoConfig == nullptr) {
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
            platformsImpl->clear();
            ExecutionEnvironment *exeEnv = constructPlatform()->peekExecutionEnvironment();

            std::string pciPath = "0000:00:02.0";
            exeEnv->rootDeviceEnvironments.resize(1u);
            const auto ret = prepareDeviceEnvironment(*exeEnv, pciPath, 0u);
            EXPECT_EQ(expectedDevices, exeEnv->rootDeviceEnvironments.size());
            for (auto i = 0u; i < expectedDevices; i++) {
                hwInfo = exeEnv->rootDeviceEnvironments[i]->getHardwareInfo();

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
}

struct PrepareDeviceEnvironmentsTest : ::testing::Test {
    void SetUp() override {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    }
    void TearDown() override {
    }
    int i = 0;
    const HardwareInfo *hwInfo = nullptr;

    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    DebugManagerStateRestore stateRestorer;
};

HWTEST_F(PrepareDeviceEnvironmentsTest, givenPrepareDeviceEnvironmentsWhenCsrIsSetToVariousTypesThenTheFunctionReturnsTheExpectedValueOfHardwareInfo) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);
    for (int productFamilyIndex = 0; productFamilyIndex < IGFX_MAX_PRODUCT; productFamilyIndex++) {
        const char *hwPrefix = hardwarePrefix[productFamilyIndex];
        auto hwInfoConfig = hwInfoConfigFactory[productFamilyIndex];
        if (hwPrefix == nullptr || hwInfoConfig == nullptr) {
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
            platformsImpl->clear();
            ExecutionEnvironment *exeEnv = constructPlatform()->peekExecutionEnvironment();

            const auto ret = prepareDeviceEnvironments(*exeEnv);
            EXPECT_EQ(expectedDevices, exeEnv->rootDeviceEnvironments.size());
            for (auto i = 0u; i < expectedDevices; i++) {
                hwInfo = exeEnv->rootDeviceEnvironments[i]->getHardwareInfo();

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

HWTEST_F(PrepareDeviceEnvironmentsTest, givenUpperCaseProductFamilyOverrideFlagSetWhenCreatingDevicesThenFindExpectedPlatform) {
    std::string hwPrefix;
    std::string hwPrefixUpperCase;
    PRODUCT_FAMILY productFamily;

    for (int productFamilyIndex = 0; productFamilyIndex < IGFX_MAX_PRODUCT; productFamilyIndex++) {
        if (hardwarePrefix[productFamilyIndex] && hwInfoConfigFactory[productFamilyIndex]) {
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
    bool ret = prepareDeviceEnvironments(*exeEnv);

    EXPECT_TRUE(ret);
    EXPECT_EQ(productFamily, exeEnv->rootDeviceEnvironments[0]->getHardwareInfo()->platform.eProductFamily);
}

HWTEST_F(PrepareDeviceEnvironmentsTest, givenPrepareDeviceEnvironmentsAndUnknownProductFamilyWhenCsrIsSetToValidTypeThenTheFunctionReturnsTheExpectedValueOfHardwareInfo) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleRootDevices.set(expectedDevices);
    for (int csrTypes = 0; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
        CommandStreamReceiverType csrType = static_cast<CommandStreamReceiverType>(csrTypes);
        std::string productFamily("unk");

        DebugManager.flags.SetCommandStreamReceiver.set(csrType);
        DebugManager.flags.ProductFamilyOverride.set(productFamily);
        platformsImpl->clear();
        ExecutionEnvironment *exeEnv = constructPlatform()->peekExecutionEnvironment();

        auto ret = prepareDeviceEnvironments(*exeEnv);
        EXPECT_EQ(expectedDevices, exeEnv->rootDeviceEnvironments.size());
        for (auto i = 0u; i < expectedDevices; i++) {
            hwInfo = exeEnv->rootDeviceEnvironments[i]->getHardwareInfo();

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

TEST(MultiDeviceTests, givenCreateMultipleRootDevicesAndLimitAmountOfReturnedDevicesFlagWhenClGetDeviceIdsIsCalledThenLowerValueIsReturned) {
    platformsImpl->clear();
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    ultHwConfig.forceOsAgnosticMemoryManager = false;
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CreateMultipleRootDevices.set(2);
    DebugManager.flags.LimitAmountOfReturnedDevices.set(1);
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numDevices);
}
} // namespace NEO
