/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/helpers/options.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"

using namespace OCLRT;

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

HWTEST_P(GetDevicesTest, givenGetDevicesWhenCsrIsSetToValidTypeThenTheFuntionReturnsTheExpectedValueOfHardwareInfo) {
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
    auto ret = getDevices(&hwInfo, numDevices);

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
            EXPECT_EQ(hardwareInfoTable[i], hwInfo);
            EXPECT_STREQ(hardwarePrefix[i], productFamily.c_str());
        } else {
            auto defaultHwInfo = *platformDevices;
            EXPECT_EQ(defaultHwInfo, hwInfo);
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
