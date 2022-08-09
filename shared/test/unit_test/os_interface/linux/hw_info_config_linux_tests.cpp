/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include <cstring>

using namespace NEO;

struct HwInfoConfigTestLinuxDummy : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        testPlatform->eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;
        hwInfoConfigFactoryBackup = &hwConfig;
    }

    void TearDown() override {
        HwInfoConfigTestLinux::TearDown();
    }
    VariableBackup<HwInfoConfig *> hwInfoConfigFactoryBackup{&NEO::hwInfoConfigFactory[static_cast<size_t>(IGFX_UNKNOWN)]};
    MockHwInfoConfigHw<IGFX_UNKNOWN> hwConfig;
};

TEST_F(HwInfoConfigTestLinuxDummy, GivenDummyConfigWhenConfiguringHwInfoThenSucceeds) {
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
}
using HwInfoConfigCommonLinuxTest = ::testing::Test;

HWTEST2_F(HwInfoConfigCommonLinuxTest, givenDebugFlagSetWhenEnablingBlitterOperationsSupportThenIgnore, IsAtMostGen11) {
    DebugManagerStateRestore restore{};
    HardwareInfo hardwareInfo = *defaultHwInfo;

    auto hwInfoConfig = HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    hwInfoConfig->configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_FALSE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenDummyConfigThenEdramIsDetected) {
    hwConfig.use128MbEdram = true;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenEnabledPlatformCoherencyWhenConfiguringHwInfoThenIgnoreAndSetAsDisabled) {
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenDisabledPlatformCoherencyWhenConfiguringHwInfoThenSetValidCapability) {
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenFailGetEuCountWhenConfiguringHwInfoThenFails) {
    drm->storedRetValForEUVal = -4;
    drm->failRetTopology = true;

    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenFailGetSsCountWhenConfiguringHwInfoThenFails) {
    drm->storedRetValForSSVal = -5;
    drm->failRetTopology = true;

    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, whenFailGettingTopologyThenFallbackToEuCountIoctl) {
    drm->failRetTopology = true;

    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_NE(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenInvalidTopologyDataWhenConfiguringThenReturnError) {
    auto storedSVal = drm->storedSVal;
    auto storedSSVal = drm->storedSSVal;
    auto storedEUVal = drm->storedEUVal;

    {
        // 0 euCount
        drm->storedSVal = storedSVal;
        drm->storedSSVal = storedSSVal;
        drm->storedEUVal = 0;

        Drm::QueryTopologyData topologyData = {};
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }

    {
        // 0 subSliceCount
        drm->storedSVal = storedSVal;
        drm->storedSSVal = 0;
        drm->storedEUVal = storedEUVal;

        Drm::QueryTopologyData topologyData = {};
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }

    {
        // 0 sliceCount
        drm->storedSVal = 0;
        drm->storedSSVal = storedSSVal;
        drm->storedEUVal = storedEUVal;

        Drm::QueryTopologyData topologyData = {};
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenFailingCustomConfigWhenConfiguringHwInfoThenFails) {
    hwConfig.failOnConfigureHardwareCustom = true;

    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, whenConfigureHwInfoIsCalledThenAreNonPersistentContextsSupportedReturnsTrue) {
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(drm->areNonPersistentContextsSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, whenConfigureHwInfoIsCalledAndPersitentContextIsUnsupportedThenAreNonPersistentContextsSupportedReturnsFalse) {
    drm->storedPersistentContextsSupport = 0;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(drm->areNonPersistentContextsSupported());
}

HWTEST_F(HwInfoConfigTestLinuxDummy, GivenPreemptionDrmEnabledMidThreadOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;

    hwConfig.enableMidThreadPreemption = true;

    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(pInHwInfo, true);

    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidThread, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenPreemptionDrmEnabledThreadGroupOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    hwConfig.enableThreadGroupPreemption = true;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, givenDebugFlagSetWhenConfiguringHwInfoThenPrintGetParamIoctlsOutput) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintIoctlEntries.set(true);

    testing::internal::CaptureStdout(); // start capturing
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    std::array<std::string, 1> expectedStrings = {{"DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_HAS_SCHEDULER, output value: 7, retCode: 0"

    }};

    DebugManager.flags.PrintIoctlEntries.set(false);
    std::string output = testing::internal::GetCapturedStdout(); // stop capturing
    for (const auto &expectedString : expectedStrings) {
        EXPECT_NE(std::string::npos, output.find(expectedString));
    }

    EXPECT_EQ(std::string::npos, output.find("UNKNOWN"));
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenPreemptionDrmEnabledMidBatchOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    hwConfig.enableMidBatchPreemption = true;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, WhenConfiguringHwInfoThenPreemptionIsSupportedPreemptionDrmEnabledNoPreemptionWhenConfiguringHwInfoThenPreemptionIsNotSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenPreemptionDrmDisabledAllPreemptionWhenConfiguringHwInfoThenPreemptionIsNotSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport = 0;
    hwConfig.enableMidThreadPreemption = true;
    hwConfig.enableMidBatchPreemption = true;
    hwConfig.enableThreadGroupPreemption = true;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    hwConfig.enableMidThreadPreemption = true;
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_FALSE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenPreemptionDrmEnabledAllPreemptionDriverThreadGroupWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    hwConfig.enableMidBatchPreemption = true;
    hwConfig.enableThreadGroupPreemption = true;
    hwConfig.enableMidThreadPreemption = true;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenPreemptionDrmEnabledAllPreemptionDriverMidBatchWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidBatch;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    hwConfig.enableMidBatchPreemption = true;
    hwConfig.enableThreadGroupPreemption = true;
    hwConfig.enableMidThreadPreemption = true;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, GivenConfigPreemptionDrmEnabledAllPreemptionDriverDisabledWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    hwConfig.enableMidBatchPreemption = true;
    hwConfig.enableThreadGroupPreemption = true;
    hwConfig.enableMidThreadPreemption = true;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, givenPlatformEnabledFtrCompressionWhenInitializingThenFlagsAreSet) {
    pInHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    pInHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenPointerToHwInfoWhenConfigureHwInfoCalledThenRequiedSurfaceSizeIsSettedProperly) {
    EXPECT_EQ(MemoryConstants::pageSize, pInHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    auto expectedSize = static_cast<size_t>(outHwInfo.gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte);
    HwHelper::get(outHwInfo.platform.eRenderCoreFamily).adjustPreemptionSurfaceSize(expectedSize);
    EXPECT_EQ(expectedSize, outHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenInstrumentationForHardwareIsEnabledOrDisabledWhenConfiguringHwInfoThenOverrideItUsingHaveInstrumentation) {
    int ret;

    pInHwInfo.capabilityTable.instrumentationEnabled = false;
    ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.instrumentationEnabled);

    pInHwInfo.capabilityTable.instrumentationEnabled = true;
    ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    ASSERT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.instrumentationEnabled);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenGttSizeReturnedWhenInitializingHwInfoThenSetSvmFtr) {
    drm->storedGTTSize = MemoryConstants::max64BitAppAddress;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSvm);

    drm->storedGTTSize = MemoryConstants::max64BitAppAddress + 1;
    ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrSvm);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenGttSizeReturnedWhenInitializingHwInfoThenSetGpuAddressSpace) {
    drm->storedGTTSize = maxNBitValue(40) + 1;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(drm->storedGTTSize - 1, outHwInfo.capabilityTable.gpuAddressSpace);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenFailingGttSizeIoctlWhenInitializingHwInfoThenSetDefaultValues) {
    drm->storedRetValForGetGttSize = -1;
    int ret = hwConfig.configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_TRUE(outHwInfo.capabilityTable.ftrSvm);
    EXPECT_NE(0u, outHwInfo.capabilityTable.gpuAddressSpace);
    EXPECT_EQ(pInHwInfo.capabilityTable.gpuAddressSpace, outHwInfo.capabilityTable.gpuAddressSpace);
}

using HwConfigLinux = ::testing::Test;

HWTEST2_F(HwConfigLinux, GivenDifferentValuesFromTopologyQueryWhenConfiguringHwInfoThenMaxSlicesSupportedSetToAvailableCountInGtSystemInfo, MatchAny) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    HardwareInfo outHwInfo;
    auto hwConfig = HwInfoConfigHw<productFamily>::get();

    hwInfo.gtSystemInfo.MaxSubSlicesSupported = drm->storedSSVal * 2;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = drm->storedSSVal * 2;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 16;
    hwInfo.gtSystemInfo.MaxSlicesSupported = drm->storedSVal * 4;

    int ret = hwConfig->configureHwInfoDrm(&hwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(static_cast<uint32_t>(drm->storedSSVal * 2), outHwInfo.gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSSVal * 2), outHwInfo.gtSystemInfo.MaxDualSubSlicesSupported);
    EXPECT_EQ(16u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSVal), outHwInfo.gtSystemInfo.MaxSlicesSupported);

    drm->storedSVal = 3;
    drm->storedSSVal = 12;
    drm->storedEUVal = 12 * 8;

    hwInfo.gtSystemInfo.MaxSubSlicesSupported = drm->storedSSVal / 2;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = drm->storedSSVal / 2;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 6;
    hwInfo.gtSystemInfo.MaxSlicesSupported = drm->storedSVal / 2;

    ret = hwConfig->configureHwInfoDrm(&hwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(12u, outHwInfo.gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(6u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice); // MaxEuPerSubslice is preserved
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSVal), outHwInfo.gtSystemInfo.MaxSlicesSupported);

    EXPECT_EQ(hwInfo.gtSystemInfo.MaxDualSubSlicesSupported, outHwInfo.gtSystemInfo.MaxDualSubSlicesSupported);

    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 0;

    ret = hwConfig->configureHwInfoDrm(&hwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(8u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice);
}

HWTEST2_F(HwConfigLinux, givenSliceCountWhenConfigureHwInfoDrmThenProperInitializationInSliceInfoEnabled, MatchAny) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    HardwareInfo outHwInfo;
    auto hwConfig = HwInfoConfigHw<productFamily>::get();
    uint32_t sliceCount = 4;
    drm->storedSVal = sliceCount;
    hwInfo.gtSystemInfo.SliceCount = sliceCount;

    int ret = hwConfig->configureHwInfoDrm(&hwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(0, ret);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EXPECT_TRUE(outHwInfo.gtSystemInfo.SliceInfo[i].Enabled);
    }
}
