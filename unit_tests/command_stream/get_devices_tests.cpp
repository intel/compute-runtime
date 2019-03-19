/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/libult/create_command_stream.h"

namespace OCLRT {
bool operator==(const HardwareInfo &hwInfoIn, const HardwareInfo &hwInfoOut) {
    bool result = (0 == memcmp(hwInfoIn.pPlatform, hwInfoOut.pPlatform, sizeof(PLATFORM)));
    result &= (0 == memcmp(hwInfoIn.pSkuTable, hwInfoOut.pSkuTable, sizeof(FeatureTable)));
    result &= (0 == memcmp(hwInfoIn.pWaTable, hwInfoOut.pWaTable, sizeof(WorkaroundTable)));
    result &= (0 == memcmp(&hwInfoIn.capabilityTable, &hwInfoOut.capabilityTable, sizeof(RuntimeCapabilityTable)));
    return result;
}

struct GetDevicesTest : ::testing::Test {
    void SetUp() override {
        overrideDeviceWithDefaultHardwareInfo = false;
        gtSystemInfo = *platformDevices[0]->pSysInfo;
    }
    void TearDown() override {
        overrideDeviceWithDefaultHardwareInfo = true;
        memcpy(const_cast<GT_SYSTEM_INFO *>(platformDevices[0]->pSysInfo), &gtSystemInfo, sizeof(GT_SYSTEM_INFO));
    }
    GT_SYSTEM_INFO gtSystemInfo;
    int i = 0;
    size_t numDevices = 0;
    HardwareInfo *hwInfo = nullptr;
    DebugManagerStateRestore stateRestorer;
};

HWTEST_F(GetDevicesTest, givenGetDevicesWhenCsrIsSetToVariousTypesThenTheFunctionReturnsTheExpectedValueOfHardwareInfo) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleDevices.set(expectedDevices);
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
            ExecutionEnvironment *exeEnv = platformImpl->peekExecutionEnvironment();

            const auto ret = getDevices(&hwInfo, numDevices, *exeEnv);

            switch (csrType) {
            case CSR_HW:
            case CSR_HW_WITH_AUB:
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);
                EXPECT_EQ(expectedDevices, numDevices);
                DeviceFactory::releaseDevices();
                break;
            case CSR_AUB:
            case CSR_TBX:
            case CSR_TBX_WITH_AUB:
                EXPECT_TRUE(ret);
                EXPECT_NE(nullptr, hwInfo);
                EXPECT_EQ(expectedDevices, numDevices);
                for (i = 0; i < IGFX_MAX_PRODUCT; i++) {
                    auto hardwareInfo = hardwareInfoTable[i];
                    if (hardwareInfo == nullptr)
                        continue;
                    if (hardwareInfoTable[i]->pPlatform->eProductFamily == hwInfo->pPlatform->eProductFamily)
                        break;
                }
                EXPECT_TRUE(i < IGFX_MAX_PRODUCT);
                ASSERT_NE(nullptr, hardwarePrefix[i]);
                EXPECT_EQ(0, memcmp(hardwareInfoTable[i]->pPlatform, hwInfo->pPlatform, sizeof(PLATFORM)));
                EXPECT_EQ(0, memcmp(&hardwareInfoTable[i]->capabilityTable, &hwInfo->capabilityTable, sizeof(RuntimeCapabilityTable)));
                EXPECT_EQ(0, memcmp(hardwareInfoTable[i]->pWaTable, hwInfo->pWaTable, sizeof(WorkaroundTable)));
                EXPECT_STREQ(hardwarePrefix[i], productFamily.c_str());
                DeviceFactory::releaseDevices();
                break;
            default:
                break;
            }
        }
    }
}

HWTEST_F(GetDevicesTest, givenGetDevicesAndUnknownProductFamilyWhenCsrIsSetToValidTypeThenTheFunctionReturnsTheExpectedValueOfHardwareInfo) {
    uint32_t expectedDevices = 1;
    DebugManager.flags.CreateMultipleDevices.set(expectedDevices);
    for (int csrTypes = 0; csrTypes <= CSR_TYPES_NUM; csrTypes++) {
        CommandStreamReceiverType csrType = static_cast<CommandStreamReceiverType>(csrTypes);
        std::string productFamily("unk");

        DebugManager.flags.SetCommandStreamReceiver.set(csrType);
        DebugManager.flags.ProductFamilyOverride.set(productFamily);
        ExecutionEnvironment *exeEnv = platformImpl->peekExecutionEnvironment();

        auto ret = getDevices(&hwInfo, numDevices, *exeEnv);

        switch (csrType) {
        case CSR_HW:
        case CSR_HW_WITH_AUB:
            EXPECT_TRUE(ret);
            EXPECT_NE(nullptr, hwInfo);
            EXPECT_EQ(expectedDevices, numDevices);
            DeviceFactory::releaseDevices();
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
                if (hardwareInfoTable[i]->pPlatform->eProductFamily == hwInfo->pPlatform->eProductFamily)
                    break;
            }
            EXPECT_TRUE(i < IGFX_MAX_PRODUCT);
            ASSERT_NE(nullptr, hardwarePrefix[i]);
            auto defaultHwInfo = *platformDevices;
            EXPECT_EQ(0, memcmp(defaultHwInfo->pPlatform, hwInfo->pPlatform, sizeof(PLATFORM)));
            EXPECT_EQ(0, memcmp(&defaultHwInfo->capabilityTable, &hwInfo->capabilityTable, sizeof(RuntimeCapabilityTable)));
            EXPECT_EQ(0, memcmp(defaultHwInfo->pWaTable, hwInfo->pWaTable, sizeof(WorkaroundTable)));
            DeviceFactory::releaseDevices();
            break;
        }
        default:
            break;
        }
    }
}
} // namespace OCLRT
