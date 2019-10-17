/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/memory_constants.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/os_library.h"
#include "runtime/platform/platform.h"

#include "gtest/gtest.h"

using namespace NEO;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace);

struct DeviceFactoryTest : public ::testing::Test {
  public:
    void SetUp() override {
        const HardwareInfo *hwInfo = platformDevices[0];
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        mockGdiDll = setAdapterInfo(&hwInfo->platform, &hwInfo->gtSystemInfo, hwInfo->capabilityTable.gpuAddressSpace);
    }

    void TearDown() override {
        delete mockGdiDll;
    }

  protected:
    OsLibrary *mockGdiDll;
    ExecutionEnvironment *executionEnvironment;
};

TEST_F(DeviceFactoryTest, GetDevices_Expect_True_If_Returned) {
    DeviceFactoryCleaner cleaner;
    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(numDevices, *executionEnvironment);

    EXPECT_TRUE((numDevices > 0) ? success : !success);
}

TEST_F(DeviceFactoryTest, GetDevices_Check_HwInfo_Null) {
    DeviceFactoryCleaner cleaner;
    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(numDevices, *executionEnvironment);
    EXPECT_TRUE((numDevices > 0) ? success : !success);
}

TEST_F(DeviceFactoryTest, GetDevices_Check_HwInfo_Platform) {
    DeviceFactoryCleaner cleaner;
    const HardwareInfo *refHwinfo = *platformDevices;
    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(numDevices, *executionEnvironment);
    const HardwareInfo *hwInfo = executionEnvironment->getHardwareInfo();

    EXPECT_TRUE((numDevices > 0) ? success : !success);

    if (numDevices > 0) {

        EXPECT_EQ(refHwinfo->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
    }
}

TEST_F(DeviceFactoryTest, overrideKmdNotifySettings) {
    DeviceFactoryCleaner cleaner;
    DebugManagerStateRestore stateRestore;

    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(numDevices, *executionEnvironment);
    auto hwInfo = executionEnvironment->getHardwareInfo();
    ASSERT_TRUE(success);
    auto refEnableKmdNotify = hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify;
    auto refDelayKmdNotifyMicroseconds = hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;
    auto refEnableQuickKmdSleep = hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleep;
    auto refDelayQuickKmdSleepMicroseconds = hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    auto refEnableQuickKmdSleepForSporadicWaits = hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits;
    auto refDelayQuickKmdSleepForSporadicWaitsMicroseconds = hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds;
    DeviceFactory::releaseDevices();

    DebugManager.flags.OverrideEnableKmdNotify.set(!refEnableKmdNotify);
    DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.set(static_cast<int32_t>(refDelayKmdNotifyMicroseconds) + 10);

    DebugManager.flags.OverrideEnableQuickKmdSleep.set(!refEnableQuickKmdSleep);
    DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepMicroseconds) + 11);

    DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.set(!refEnableQuickKmdSleepForSporadicWaits);
    DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepForSporadicWaitsMicroseconds) + 12);

    success = DeviceFactory::getDevices(numDevices, *executionEnvironment);
    ASSERT_TRUE(success);
    hwInfo = executionEnvironment->getHardwareInfo();

    EXPECT_EQ(!refEnableKmdNotify, hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(refDelayKmdNotifyMicroseconds + 10, hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);

    EXPECT_EQ(!refEnableQuickKmdSleep, hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(refDelayQuickKmdSleepMicroseconds + 11, hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);

    EXPECT_EQ(!refEnableQuickKmdSleepForSporadicWaits,
              hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(refDelayQuickKmdSleepForSporadicWaitsMicroseconds + 12,
              hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}

TEST_F(DeviceFactoryTest, getEngineTypeDebugOverride) {
    DeviceFactoryCleaner cleaner;
    DebugManagerStateRestore dbgRestorer;
    int32_t debugEngineType = 2;
    DebugManager.flags.NodeOrdinal.set(debugEngineType);

    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(numDevices, *executionEnvironment);
    ASSERT_TRUE(success);
    auto hwInfo = executionEnvironment->getHardwareInfo();

    int32_t actualEngineType = static_cast<int32_t>(hwInfo->capabilityTable.defaultEngineType);
    EXPECT_EQ(debugEngineType, actualEngineType);
}

TEST_F(DeviceFactoryTest, givenPointerToHwInfoWhenGetDevicedCalledThenRequiedSurfaceSizeIsSettedProperly) {
    DeviceFactoryCleaner cleaner;
    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(numDevices, *executionEnvironment);
    ASSERT_TRUE(success);
    auto hwInfo = executionEnvironment->getHardwareInfo();

    EXPECT_EQ(hwInfo->gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte, hwInfo->capabilityTable.requiredPreemptionSurfaceSize);
}

TEST_F(DeviceFactoryTest, givenCreateMultipleRootDevicesDebugFlagWhenGetDevicesIsCalledThenNumberOfReturnedDevicesIsEqualToDebugVariable) {
    DeviceFactoryCleaner cleaner;
    DebugManagerStateRestore stateRestore;
    auto requiredDeviceCount = 2u;
    DebugManager.flags.CreateMultipleRootDevices.set(requiredDeviceCount);

    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(numDevices, *executionEnvironment);

    ASSERT_TRUE(success);
    EXPECT_EQ(requiredDeviceCount, numDevices);
}

TEST_F(DeviceFactoryTest, givenCreateMultipleRootDevicesDebugFlagWhenGetDevicesForProductFamilyOverrideIsCalledThenNumberOfReturnedDevicesIsEqualToDebugVariable) {
    DeviceFactoryCleaner cleaner;
    DebugManagerStateRestore stateRestore;
    auto requiredDeviceCount = 2u;
    DebugManager.flags.CreateMultipleRootDevices.set(requiredDeviceCount);

    size_t numDevices = 0;
    bool success = DeviceFactory::getDevicesForProductFamilyOverride(numDevices, *executionEnvironment);

    ASSERT_TRUE(success);
    EXPECT_EQ(requiredDeviceCount, numDevices);
}

TEST_F(DeviceFactoryTest, givenGetDevicesCallWhenItIsDoneThenOsInterfaceIsAllocated) {
    DeviceFactoryCleaner cleaner;

    size_t numDevices = 0;
    bool success = DeviceFactory::getDevices(numDevices, *executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, executionEnvironment->osInterface);
}