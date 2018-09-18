/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/helpers/options.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"

using namespace OCLRT;

namespace OCLRT {
bool operator==(const HardwareInfo &hwInfoIn, const HardwareInfo &hwInfoOut) {
    bool result = (0 == memcmp(hwInfoIn.pPlatform, hwInfoOut.pPlatform, sizeof(PLATFORM)));
    result &= (0 == memcmp(hwInfoIn.pSkuTable, hwInfoOut.pSkuTable, sizeof(FeatureTable)));
    result &= (0 == memcmp(hwInfoIn.pWaTable, hwInfoOut.pWaTable, sizeof(WorkaroundTable)));
    result &= (0 == memcmp(&hwInfoIn.capabilityTable, &hwInfoOut.capabilityTable, sizeof(RuntimeCapabilityTable)));
    return result;
}
} // namespace OCLRT

struct GetDevicesTest : ::testing::TestWithParam<std::tuple<CommandStreamReceiverType, const char *>> {
    void SetUp() override {
        overrideDeviceWithDefaultHardwareInfo = false;
        gtSystemInfo = *platformDevices[0]->pSysInfo;
    }
    void TearDown() override {
        overrideDeviceWithDefaultHardwareInfo = true;
        memcpy(const_cast<GT_SYSTEM_INFO *>(platformDevices[0]->pSysInfo), &gtSystemInfo, sizeof(GT_SYSTEM_INFO));
    }
    GT_SYSTEM_INFO gtSystemInfo;
};

HWTEST_P(GetDevicesTest, givenGetDevicesWhenCsrIsSetToValidTypeThenTheFunctionReturnsTheExpectedValueOfHardwareInfo) {
    DebugManagerStateRestore stateRestorer;
    CommandStreamReceiverType csrType = std::get<0>(GetParam());
    const char *hwPrefix = std::get<1>(GetParam());
    std::string productFamily = "unk";
    if (hwPrefix != nullptr) {
        productFamily = hwPrefix;
    }

    DebugManager.flags.SetCommandStreamReceiver.set(csrType);
    DebugManager.flags.ProductFamilyOverride.set(productFamily);

    int i = 0;
    size_t numDevices = 0;
    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment executionEnvironment;
    auto ret = getDevices(&hwInfo, numDevices, executionEnvironment);

    switch (csrType) {
    case CSR_HW:
    case CSR_HW_WITH_AUB:
        EXPECT_TRUE(ret);
        EXPECT_NE(nullptr, hwInfo);
        EXPECT_EQ(1u, numDevices);
        DeviceFactory::releaseDevices();
        break;
    case CSR_AUB:
    case CSR_TBX:
    case CSR_TBX_WITH_AUB:
        EXPECT_TRUE(ret);
        EXPECT_NE(nullptr, hwInfo);
        EXPECT_EQ(1u, numDevices);
        for (i = 0; i < IGFX_MAX_PRODUCT; i++) {
            auto hardwareInfo = hardwareInfoTable[i];
            if (hardwareInfo == nullptr)
                continue;
            if (hardwareInfoTable[i]->pPlatform->eProductFamily == hwInfo->pPlatform->eProductFamily)
                break;
        }
        EXPECT_TRUE(i < IGFX_MAX_PRODUCT);
        ASSERT_NE(nullptr, hardwarePrefix[i]);
        if (hwPrefix != nullptr) {
            EXPECT_EQ(*hardwareInfoTable[i], *hwInfo);
            EXPECT_STREQ(hardwarePrefix[i], productFamily.c_str());
        } else {
            auto defaultHwInfo = *platformDevices;
            EXPECT_EQ(*defaultHwInfo, *hwInfo);
        }
        break;
    default:
        EXPECT_FALSE(ret);
        EXPECT_EQ(nullptr, hwInfo);
        EXPECT_EQ(0u, numDevices);
        break;
    }
}

static CommandStreamReceiverType commandStreamReceiverTypes[] = {
    CSR_HW,
    CSR_AUB,
    CSR_TBX,
    CSR_HW_WITH_AUB,
    CSR_TBX_WITH_AUB,
    CSR_TYPES_NUM};

INSTANTIATE_TEST_CASE_P(
    GetDevicesTest_Create,
    GetDevicesTest,
    ::testing::Combine(
        ::testing::ValuesIn(commandStreamReceiverTypes),
        ::testing::ValuesIn(hardwarePrefix, &hardwarePrefix[IGFX_MAX_PRODUCT])));
