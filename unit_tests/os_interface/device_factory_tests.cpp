/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/os_library.h"
#include "runtime/memory_manager/memory_constants.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "gtest/gtest.h"

using namespace OCLRT;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo);

struct DeviceFactoryTest : public ::testing::Test {
  public:
    void SetUp() override {
        const HardwareInfo hwInfo = *platformDevices[0];
        mockGdiDll = setAdapterInfo(hwInfo.pPlatform, hwInfo.pSysInfo);
    }

    void TearDown() override {
        delete mockGdiDll;
    }

  protected:
    OsLibrary *mockGdiDll;
};

TEST_F(DeviceFactoryTest, GetDevices_Expect_True_If_Returned) {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(&hwInfo, numDevices);

    EXPECT_TRUE((numDevices > 0) ? success : !success);
    DeviceFactory::releaseDevices();
}

TEST_F(DeviceFactoryTest, GetDevices_Check_HwInfo_Null) {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(&hwInfo, numDevices);
    EXPECT_TRUE((numDevices > 0) ? success : !success);

    if (numDevices > 0) {
        ASSERT_NE(hwInfo, nullptr);
        EXPECT_NE(hwInfo->pPlatform, nullptr);
        EXPECT_NE(hwInfo->pSkuTable, nullptr);
        EXPECT_NE(hwInfo->pSysInfo, nullptr);
        EXPECT_NE(hwInfo->pWaTable, nullptr);
    }
    DeviceFactory::releaseDevices();
}

TEST_F(DeviceFactoryTest, GetDevices_Check_HwInfo_Platform) {
    HardwareInfo *hwInfo = nullptr;
    const HardwareInfo *refHwinfo = *platformDevices;
    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(&hwInfo, numDevices);
    EXPECT_TRUE((numDevices > 0) ? success : !success);

    if (numDevices > 0) {
        ASSERT_NE(hwInfo, nullptr);
        EXPECT_NE(hwInfo->pPlatform, nullptr);

        EXPECT_EQ(refHwinfo->pPlatform->eDisplayCoreFamily, hwInfo->pPlatform->eDisplayCoreFamily);
    }
    DeviceFactory::releaseDevices();
}

TEST_F(DeviceFactoryTest, overrideKmdNotifySettings) {
    DebugManagerStateRestore stateRestore;

    HardwareInfo *hwInfoReference = nullptr;
    HardwareInfo *hwInfoOverriden = nullptr;
    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(&hwInfoReference, numDevices);
    ASSERT_TRUE(success);
    auto refEnableKmdNotify = hwInfoReference->capabilityTable.kmdNotifyProperties.enableKmdNotify;
    auto refDelayKmdNotifyMicroseconds = hwInfoReference->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;
    auto refEnableQuickKmdSleep = hwInfoReference->capabilityTable.kmdNotifyProperties.enableQuickKmdSleep;
    auto refDelayQuickKmdSleepMicroseconds = hwInfoReference->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    auto refEnableQuickKmdSleepForSporadicWaits = hwInfoReference->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits;
    auto refDelayQuickKmdSleepForSporadicWaitsMicroseconds = hwInfoReference->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds;
    DeviceFactory::releaseDevices();

    DebugManager.flags.OverrideEnableKmdNotify.set(!refEnableKmdNotify);
    DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.set(static_cast<int32_t>(refDelayKmdNotifyMicroseconds) + 10);

    DebugManager.flags.OverrideEnableQuickKmdSleep.set(!refEnableQuickKmdSleep);
    DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepMicroseconds) + 11);

    DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.set(!refEnableQuickKmdSleepForSporadicWaits);
    DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepForSporadicWaitsMicroseconds) + 12);

    success = DeviceFactory::getDevices(&hwInfoOverriden, numDevices);
    ASSERT_TRUE(success);

    EXPECT_EQ(!refEnableKmdNotify, hwInfoOverriden->capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(refDelayKmdNotifyMicroseconds + 10, hwInfoOverriden->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);

    EXPECT_EQ(!refEnableQuickKmdSleep, hwInfoOverriden->capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(refDelayQuickKmdSleepMicroseconds + 11, hwInfoOverriden->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);

    EXPECT_EQ(!refEnableQuickKmdSleepForSporadicWaits,
              hwInfoOverriden->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(refDelayQuickKmdSleepForSporadicWaitsMicroseconds + 12,
              hwInfoOverriden->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);

    DeviceFactory::releaseDevices();
}

TEST_F(DeviceFactoryTest, getEngineTypeDebugOverride) {
    DebugManagerStateRestore dbgRestorer;
    int32_t debugEngineType = 2;
    DebugManager.flags.NodeOrdinal.set(debugEngineType);
    HardwareInfo *hwInfoOverriden = nullptr;
    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(&hwInfoOverriden, numDevices);
    ASSERT_TRUE(success);
    ASSERT_NE(nullptr, hwInfoOverriden);
    int32_t actualEngineType = static_cast<int32_t>(hwInfoOverriden->capabilityTable.defaultEngineType);
    EXPECT_EQ(debugEngineType, actualEngineType);

    DeviceFactory::releaseDevices();
}

TEST_F(DeviceFactoryTest, givenPointerToHwInfoWhenGetDevicedCalledThenRequiedSurfaceSizeIsSettedProperly) {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(&hwInfo, numDevices);
    ASSERT_TRUE(success);

    EXPECT_EQ(hwInfo->pSysInfo->CsrSizeInMb * MemoryConstants::megaByte, hwInfo->capabilityTable.requiredPreemptionSurfaceSize);
    DeviceFactory::releaseDevices();
}

TEST_F(DeviceFactoryTest, givenCreateMultipleDevicesDebugFlagWhenGetDevicesIsCalledThenNumberOfReturnedDevicesIsEqualToDebugVariable) {
    DebugManagerStateRestore stateRestore;
    auto requiredDeviceCount = 2u;
    DebugManager.flags.CreateMultipleDevices.set(requiredDeviceCount);
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(&hwInfo, numDevices);
    ASSERT_NE(nullptr, hwInfo);

    for (auto deviceIndex = 0u; deviceIndex < requiredDeviceCount; deviceIndex++) {
        EXPECT_NE(nullptr, hwInfo[deviceIndex].pPlatform);
        EXPECT_NE(nullptr, hwInfo[deviceIndex].pSkuTable);
        EXPECT_NE(nullptr, hwInfo[deviceIndex].pSysInfo);
        EXPECT_NE(nullptr, hwInfo[deviceIndex].pWaTable);
    }

    EXPECT_EQ(hwInfo[0].pPlatform->eDisplayCoreFamily, hwInfo[1].pPlatform->eDisplayCoreFamily);

    ASSERT_TRUE(success);
    EXPECT_EQ(requiredDeviceCount, numDevices);
    DeviceFactory::releaseDevices();
}
