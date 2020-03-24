/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"
#include "hw_device_id.h"

#include <set>

using namespace NEO;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace);

struct DeviceFactoryTest : public ::testing::Test {
  public:
    void SetUp() override {
        const HardwareInfo *hwInfo = defaultHwInfo.get();
        executionEnvironment = platform()->peekExecutionEnvironment();
        mockGdiDll = setAdapterInfo(&hwInfo->platform, &hwInfo->gtSystemInfo, hwInfo->capabilityTable.gpuAddressSpace);
    }

    void TearDown() override {
        delete mockGdiDll;
    }

  protected:
    OsLibrary *mockGdiDll;
    ExecutionEnvironment *executionEnvironment;
};

TEST_F(DeviceFactoryTest, PrepareDeviceEnvironments_Check_HwInfo_Platform) {
    const HardwareInfo *refHwinfo = defaultHwInfo.get();

    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    EXPECT_TRUE(success);
    const HardwareInfo *hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
        EXPECT_EQ(refHwinfo->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
}

TEST_F(DeviceFactoryTest, overrideKmdNotifySettings) {
    DebugManagerStateRestore stateRestore;

    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    ASSERT_TRUE(success);
    auto refEnableKmdNotify = hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify;
    auto refDelayKmdNotifyMicroseconds = hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;
    auto refEnableQuickKmdSleep = hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleep;
    auto refDelayQuickKmdSleepMicroseconds = hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    auto refEnableQuickKmdSleepForSporadicWaits = hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits;
    auto refDelayQuickKmdSleepForSporadicWaitsMicroseconds = hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds;

    DebugManager.flags.OverrideEnableKmdNotify.set(!refEnableKmdNotify);
    DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.set(static_cast<int32_t>(refDelayKmdNotifyMicroseconds) + 10);

    DebugManager.flags.OverrideEnableQuickKmdSleep.set(!refEnableQuickKmdSleep);
    DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepMicroseconds) + 11);

    DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.set(!refEnableQuickKmdSleepForSporadicWaits);
    DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepForSporadicWaitsMicroseconds) + 12);

    platformsImpl.clear();
    executionEnvironment = constructPlatform()->peekExecutionEnvironment();
    success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    ASSERT_TRUE(success);
    hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();

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
    DebugManagerStateRestore dbgRestorer;
    int32_t debugEngineType = 2;
    DebugManager.flags.NodeOrdinal.set(debugEngineType);

    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    ASSERT_TRUE(success);
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();

    int32_t actualEngineType = static_cast<int32_t>(hwInfo->capabilityTable.defaultEngineType);
    EXPECT_EQ(debugEngineType, actualEngineType);
}

TEST_F(DeviceFactoryTest, givenPointerToHwInfoWhenGetDevicedCalledThenRequiedSurfaceSizeIsSettedProperly) {
    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    ASSERT_TRUE(success);
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();

    EXPECT_EQ(hwInfo->gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte, hwInfo->capabilityTable.requiredPreemptionSurfaceSize);
}

TEST_F(DeviceFactoryTest, givenCreateMultipleRootDevicesDebugFlagWhenPrepareDeviceEnvironmentsIsCalledThenNumberOfReturnedDevicesIsEqualToDebugVariable) {
    DebugManagerStateRestore stateRestore;
    auto requiredDeviceCount = 2u;
    DebugManager.flags.CreateMultipleRootDevices.set(requiredDeviceCount);

    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);

    ASSERT_TRUE(success);
    EXPECT_EQ(requiredDeviceCount, executionEnvironment->rootDeviceEnvironments.size());
}

TEST_F(DeviceFactoryTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsIsCalledThenOverrideGpuAddressSpace) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideGpuAddressSpace.set(12);

    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(maxNBitValue(12), executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.gpuAddressSpace);
}

TEST_F(DeviceFactoryTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideGpuAddressSpace) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideGpuAddressSpace.set(12);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(*executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(maxNBitValue(12), executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.gpuAddressSpace);
}

TEST_F(DeviceFactoryTest, whenPrepareDeviceEnvironmentsIsCalledThenAllRootDeviceEnvironmentMembersAreInitialized) {
    DebugManagerStateRestore stateRestore;
    auto requiredDeviceCount = 2u;
    DebugManager.flags.CreateMultipleRootDevices.set(requiredDeviceCount);

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, requiredDeviceCount);

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    ASSERT_TRUE(success);

    std::set<MemoryOperationsHandler *> memoryOperationHandlers;
    std::set<OSInterface *> osInterfaces;
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < requiredDeviceCount; rootDeviceIndex++) {
        auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get());

        auto memoryOperationInterface = rootDeviceEnvironment->memoryOperationsInterface.get();
        EXPECT_NE(nullptr, memoryOperationInterface);
        EXPECT_EQ(memoryOperationHandlers.end(), memoryOperationHandlers.find(memoryOperationInterface));
        memoryOperationHandlers.insert(memoryOperationInterface);

        auto osInterface = rootDeviceEnvironment->osInterface.get();
        EXPECT_NE(nullptr, osInterface);
        EXPECT_EQ(osInterfaces.end(), osInterfaces.find(osInterface));
        osInterfaces.insert(osInterface);
    }
}

TEST_F(DeviceFactoryTest, givenInvalidHwConfigStringPrepareDeviceEnvironmentsForProductFamilyOverrideReturnsFalse) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.HardwareInfoOverride.set("1x3");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_FALSE(success);
}

TEST_F(DeviceFactoryTest, givenValidHwConfigStringPrepareDeviceEnvironmentsForProductFamilyOverrideReturnsTrue) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.HardwareInfoOverride.set("1x1x1");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    EXPECT_ANY_THROW(DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment));
}

TEST_F(DeviceFactoryTest, givenPrepareDeviceEnvironmentsCallWhenItIsDoneThenOsInterfaceIsAllocated) {
    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, executionEnvironment->rootDeviceEnvironments[0]->osInterface);
}

TEST(DeviceFactory, givenHwModeSelectedWhenIsHwModeSelectedIsCalledThenTrueIsReturned) {
    DebugManagerStateRestore stateRestore;
    constexpr int32_t hwModes[] = {-1, CommandStreamReceiverType::CSR_HW, CommandStreamReceiverType::CSR_HW_WITH_AUB};
    for (const auto &hwMode : hwModes) {
        DebugManager.flags.SetCommandStreamReceiver.set(hwMode);
        EXPECT_TRUE(DeviceFactory::isHwModeSelected());
    }
}

TEST(DeviceFactory, givenNonHwModeSelectedWhenIsHwModeSelectedIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore stateRestore;
    constexpr int32_t nonHwModes[] = {CommandStreamReceiverType::CSR_AUB, CommandStreamReceiverType::CSR_TBX, CommandStreamReceiverType::CSR_TBX_WITH_AUB};
    for (const auto &nonHwMode : nonHwModes) {
        DebugManager.flags.SetCommandStreamReceiver.set(nonHwMode);
        EXPECT_FALSE(DeviceFactory::isHwModeSelected());
    }
}

TEST(DiscoverDevices, whenDiscoverDevicesAndForceDeviceIdIsDifferentFromTheExistingDeviceThenReturnNullptr) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.ForceDeviceId.set("invalid");
    ExecutionEnvironment executionEnviornment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnviornment);
    EXPECT_TRUE(hwDeviceIds.empty());
}

TEST(DiscoverDevices, whenDiscoverDevicesAndForceDeviceIdIsDifferentFromTheExistingDeviceThenPrepareDeviceEnvironmentsReturnsFalse) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.ForceDeviceId.set("invalid");
    ExecutionEnvironment executionEnviornment;

    auto result = DeviceFactory::prepareDeviceEnvironments(executionEnviornment);
    EXPECT_FALSE(result);
}
