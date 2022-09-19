/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

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

TEST_F(DeviceFactoryTest, WhenDeviceEnvironemntIsPreparedThenItIsInitializedCorrectly) {
    const HardwareInfo *refHwinfo = defaultHwInfo.get();

    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    EXPECT_TRUE(success);
    const HardwareInfo *hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    EXPECT_EQ(refHwinfo->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
}

TEST_F(DeviceFactoryTest, WhenOverridingUsingDebugManagerThenOverridesAreAppliedCorrectly) {
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
    auto refEnableQuickKmdSleepForDirectSubmission = hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission;
    auto refDelayQuickKmdSleepForDirectSubmissionMicroseconds = hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds;

    DebugManager.flags.OverrideEnableKmdNotify.set(!refEnableKmdNotify);
    DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.set(static_cast<int32_t>(refDelayKmdNotifyMicroseconds) + 10);

    DebugManager.flags.OverrideEnableQuickKmdSleep.set(!refEnableQuickKmdSleep);
    DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepMicroseconds) + 11);

    DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.set(!refEnableQuickKmdSleepForSporadicWaits);
    DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepForSporadicWaitsMicroseconds) + 12);

    DebugManager.flags.OverrideEnableQuickKmdSleepForDirectSubmission.set(!refEnableQuickKmdSleepForDirectSubmission);
    DebugManager.flags.OverrideDelayQuickKmdSleepForDirectSubmissionMicroseconds.set(static_cast<int32_t>(refDelayQuickKmdSleepForDirectSubmissionMicroseconds) + 15);

    platformsImpl->clear();
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

    EXPECT_EQ(!refEnableQuickKmdSleepForDirectSubmission,
              hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(refDelayQuickKmdSleepForDirectSubmissionMicroseconds + 15,
              hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

TEST_F(DeviceFactoryTest, givenDebugFlagSetWhenCreatingDevicesThenForceImagesSupport) {
    DebugManagerStateRestore stateRestore;

    for (int32_t flag : {0, 1}) {
        DebugManager.flags.ForceImagesSupport.set(flag);

        MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get());

        auto success = DeviceFactory::prepareDeviceEnvironments(mockExecutionEnvironment);
        ASSERT_TRUE(success);
        auto hwInfo = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();

        EXPECT_EQ(!!flag, hwInfo->capabilityTable.supportsImages);
    }
}

TEST_F(DeviceFactoryTest, givenZeAffinityMaskSetWhenCreateDevicesThenProperNumberOfDevicesIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(5);
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    DebugManager.flags.ZE_AFFINITY_MASK.set("1.0,2.3,2.1,1.3,0,2.0,4.0,4.2,4.3,4.1");
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto devices = DeviceFactory::createDevices(*executionEnvironment);

    EXPECT_EQ(devices.size(), 4u);
    EXPECT_EQ(devices[0]->getNumSubDevices(), 4u);
    EXPECT_EQ(devices[1]->getNumSubDevices(), 2u);
    EXPECT_EQ(devices[2]->getNumSubDevices(), 3u);
    EXPECT_EQ(devices[3]->getNumSubDevices(), 4u);
}

TEST_F(DeviceFactoryTest, givenZeAffinityMaskSetToGreaterRootDeviceThanAvailableWhenCreateDevicesThenProperNumberOfDevicesIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(2);
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    DebugManager.flags.ZE_AFFINITY_MASK.set("0,92,1.1");
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto devices = DeviceFactory::createDevices(*executionEnvironment);

    EXPECT_EQ(devices.size(), 2u);
    EXPECT_EQ(devices[0]->getNumSubDevices(), 4u);
    EXPECT_EQ(devices[0]->getNumGenericSubDevices(), 4u);
    EXPECT_EQ(devices[1]->getNumGenericSubDevices(), 0u);
}

TEST_F(DeviceFactoryTest, givenZeAffinityMaskSetToGreaterSubDeviceThanAvailableWhenCreateDevicesThenProperNumberOfDevicesIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(2);
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    DebugManager.flags.ZE_AFFINITY_MASK.set("0,1.54");
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto devices = DeviceFactory::createDevices(*executionEnvironment);

    EXPECT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0]->getNumSubDevices(), 4u);
}

TEST_F(DeviceFactoryTest, givenZeAffinityMaskSetToRootDevicesOnlyWhenCreateDevicesThenProperNumberOfDevicesIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(2);
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    DebugManager.flags.ZE_AFFINITY_MASK.set("0,1");
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto devices = DeviceFactory::createDevices(*executionEnvironment);

    EXPECT_EQ(devices.size(), 2u);
    EXPECT_EQ(devices[0]->getNumSubDevices(), 4u);
    EXPECT_EQ(devices[1]->getNumSubDevices(), 4u);
}

TEST_F(DeviceFactoryTest, WhenOverridingEngineTypeThenDebugEngineIsReported) {
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

    const auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    auto expextedSize = static_cast<size_t>(hwInfo->gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte);
    hwHelper.adjustPreemptionSurfaceSize(expextedSize);

    EXPECT_EQ(expextedSize, hwInfo->capabilityTable.requiredPreemptionSurfaceSize);
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

TEST_F(DeviceFactoryTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsIsCalledThenOverrideRevision) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideRevision.set(3);

    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(3u, executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.usRevId);
}

TEST_F(DeviceFactoryTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideRevision) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideRevision.set(3);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(*executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(3u, executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.usRevId);
}

TEST_F(DeviceFactoryTest, givenDebugFlagWithoutZeroXWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideDeviceIdToHexValue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceDeviceId.set("1234");

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(*executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(0x1234u, executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID);
}

TEST_F(DeviceFactoryTest, givenDebugFlagWithZeroXWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideDeviceIdToHexValue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceDeviceId.set("0x1234");

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(*executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(0x1234u, executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID);
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

TEST_F(DeviceFactoryTest, givenInvalidHwConfigStringWhenPreparingDeviceEnvironmentsForProductFamilyOverrideThenFalseIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.HardwareInfoOverride.set("1x3");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_FALSE(success);
}

TEST_F(DeviceFactoryTest, givenPrepareDeviceEnvironmentsCallWhenItIsDoneThenOsInterfaceIsAllocated) {
    bool success = DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, executionEnvironment->rootDeviceEnvironments[0]->osInterface);
}

TEST(DeviceFactory, givenCreateMultipleRootDevicesWhenCreateDevicesIsCalledThenVectorReturnedWouldContainFirstDiscreteDevicesThenIntegratedDevices) {
    uint32_t numRootDevices = 8u;
    NEO::HardwareInfo hwInfo[8];
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        hwInfo[i] = *NEO::defaultHwInfo.get();
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo[i]);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment->rootDeviceEnvironments[1]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment->rootDeviceEnvironments[2]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment->rootDeviceEnvironments[3]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    executionEnvironment->rootDeviceEnvironments[4]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    executionEnvironment->rootDeviceEnvironments[5]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment->rootDeviceEnvironments[6]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment->rootDeviceEnvironments[7]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    for (auto iterator = 0u; iterator < 3; iterator++) {
        EXPECT_FALSE(devices[iterator]->getHardwareInfo().capabilityTable.isIntegratedDevice); // Initial entries would be for discrete devices
    }
    for (auto iterator = 3u; iterator < 8u; iterator++) {
        EXPECT_TRUE(devices[iterator]->getHardwareInfo().capabilityTable.isIntegratedDevice); // Later entries would be for integrated
    }
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

TEST(DiscoverDevices, whenDiscoverDevicesAndFilterDifferentFromTheExistingDeviceThenReturnNullptr) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.FilterDeviceId.set("invalid");
    DebugManager.flags.FilterBdfPath.set("invalid");
    ExecutionEnvironment executionEnviornment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnviornment);
    EXPECT_TRUE(hwDeviceIds.empty());
}

TEST(DiscoverDevices, whenDiscoverDevicesAndFilterDifferentFromTheExistingDeviceThenPrepareDeviceEnvironmentsReturnsFalse) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.FilterDeviceId.set("invalid");
    DebugManager.flags.FilterBdfPath.set("invalid");
    ExecutionEnvironment executionEnviornment;

    auto result = DeviceFactory::prepareDeviceEnvironments(executionEnviornment);
    EXPECT_FALSE(result);
}

using UltDeviceFactoryTest = DeviceFactoryTest;

TEST_F(UltDeviceFactoryTest, givenExecutionEnvironmentWhenCreatingUltDeviceFactoryThenMockMemoryManagerIsAllocated) {
    executionEnvironment->rootDeviceEnvironments.clear();
    executionEnvironment->memoryManager.reset();
    UltDeviceFactory ultDeviceFactory{2, 0, *executionEnvironment};

    EXPECT_EQ(2u, executionEnvironment->rootDeviceEnvironments.size());
    EXPECT_NE(nullptr, executionEnvironment->memoryManager.get());
    EXPECT_EQ(true, executionEnvironment->memoryManager->isInitialized());
}
