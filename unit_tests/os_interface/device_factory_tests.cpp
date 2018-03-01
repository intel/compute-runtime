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
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "gtest/gtest.h"

using namespace OCLRT;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo);

struct DeviceFactoryTest : public MemoryManagementFixture,
                           public ::testing::Test {
  public:
    void SetUp() override {
        const HardwareInfo hwInfo = *platformDevices[0];
        mockGdiDll = setAdapterInfo(hwInfo.pPlatform, hwInfo.pSysInfo);
        MemoryManagementFixture::SetUp();
    }

    void TearDown() override {
        MemoryManagementFixture::TearDown();
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
    auto refEnableKmdNotify = hwInfoReference->capabilityTable.enableKmdNotify;
    auto refDelayKmdNotifyMicroseconds = hwInfoReference->capabilityTable.delayKmdNotifyMicroseconds;
    DeviceFactory::releaseDevices();

    DebugManager.flags.OverrideEnableKmdNotify.set(!refEnableKmdNotify);
    DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.set(static_cast<int32_t>(refDelayKmdNotifyMicroseconds) + 10);

    success = DeviceFactory::getDevices(&hwInfoOverriden, numDevices);
    ASSERT_TRUE(success);

    EXPECT_EQ(!refEnableKmdNotify, hwInfoOverriden->capabilityTable.enableKmdNotify);
    EXPECT_EQ(refDelayKmdNotifyMicroseconds + 10, hwInfoOverriden->capabilityTable.delayKmdNotifyMicroseconds);

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