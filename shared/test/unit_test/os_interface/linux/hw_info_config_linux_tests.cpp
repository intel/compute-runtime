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

struct ProductHelperTestLinux : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        testPlatform->eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;
        hwInfoConfigFactoryBackup = &productHelper;
    }

    void TearDown() override {
        HwInfoConfigTestLinux::TearDown();
    }
    VariableBackup<HwInfoConfig *> hwInfoConfigFactoryBackup{&NEO::hwInfoConfigFactory[static_cast<size_t>(IGFX_UNKNOWN)]};
    MockHwInfoConfigHw<IGFX_UNKNOWN> productHelper;
};

TEST_F(ProductHelperTestLinux, GivenDummyConfigWhenConfiguringHwInfoThenSucceeds) {
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
}

HWTEST2_F(ProductHelperTestLinux, givenDebugFlagSetWhenEnablingBlitterOperationsSupportThenIgnore, IsAtMostGen11) {
    DebugManagerStateRestore restore{};
    HardwareInfo hardwareInfo = *defaultHwInfo;

    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    productHelper.configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_FALSE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

HWTEST2_F(ProductHelperTestLinux, givenUnsupportedChipsetUniqueUUIDWhenGettingUuidThenReturnFalse, IsAtMostGen11) {
    HardwareInfo hardwareInfo = *defaultHwInfo;
    auto hwInfoConfig = HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    std::array<uint8_t, HwInfoConfig::uuidSize> id;
    EXPECT_FALSE(hwInfoConfig->getUuid(nullptr, id));
}

TEST_F(ProductHelperTestLinux, GivenDummyConfigThenEdramIsDetected) {
    productHelper.use128MbEdram = true;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);
}

TEST_F(ProductHelperTestLinux, givenEnabledPlatformCoherencyWhenConfiguringHwInfoThenIgnoreAndSetAsDisabled) {
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(ProductHelperTestLinux, givenDisabledPlatformCoherencyWhenConfiguringHwInfoThenSetValidCapability) {
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(ProductHelperTestLinux, GivenFailGetEuCountWhenConfiguringHwInfoThenFails) {
    drm->storedRetValForEUVal = -4;
    drm->failRetTopology = true;

    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(-4, ret);
}

TEST_F(ProductHelperTestLinux, GivenFailGetSsCountWhenConfiguringHwInfoThenFails) {
    drm->storedRetValForSSVal = -5;
    drm->failRetTopology = true;

    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(-5, ret);
}

TEST_F(ProductHelperTestLinux, whenFailGettingTopologyThenFallbackToEuCountIoctl) {
    drm->failRetTopology = true;

    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_NE(-1, ret);
}

TEST_F(ProductHelperTestLinux, givenInvalidTopologyDataWhenConfiguringThenReturnError) {
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

TEST_F(ProductHelperTestLinux, GivenFailingCustomConfigWhenConfiguringHwInfoThenFails) {
    productHelper.failOnConfigureHardwareCustom = true;

    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(-1, ret);
}

TEST_F(ProductHelperTestLinux, whenConfigureHwInfoIsCalledThenAreNonPersistentContextsSupportedReturnsTrue) {
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(drm->areNonPersistentContextsSupported());
}

TEST_F(ProductHelperTestLinux, whenConfigureHwInfoIsCalledAndPersitentContextIsUnsupportedThenAreNonPersistentContextsSupportedReturnsFalse) {
    drm->storedPersistentContextsSupport = 0;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(drm->areNonPersistentContextsSupported());
}

HWTEST_F(ProductHelperTestLinux, GivenPreemptionDrmEnabledMidThreadOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;

    productHelper.enableMidThreadPreemption = true;

    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(pInHwInfo, true);

    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidThread, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(ProductHelperTestLinux, GivenPreemptionDrmEnabledThreadGroupOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    productHelper.enableThreadGroupPreemption = true;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(ProductHelperTestLinux, givenDebugFlagSetWhenConfiguringHwInfoThenPrintGetParamIoctlsOutput) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintIoctlEntries.set(true);

    testing::internal::CaptureStdout(); // start capturing
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
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

TEST_F(ProductHelperTestLinux, GivenPreemptionDrmEnabledMidBatchOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    productHelper.enableMidBatchPreemption = true;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(ProductHelperTestLinux, WhenConfiguringHwInfoThenPreemptionIsSupportedPreemptionDrmEnabledNoPreemptionWhenConfiguringHwInfoThenPreemptionIsNotSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(ProductHelperTestLinux, GivenPreemptionDrmDisabledAllPreemptionWhenConfiguringHwInfoThenPreemptionIsNotSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport = 0;
    productHelper.enableMidThreadPreemption = true;
    productHelper.enableMidBatchPreemption = true;
    productHelper.enableThreadGroupPreemption = true;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    productHelper.enableMidThreadPreemption = true;
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_FALSE(drm->isPreemptionSupported());
}

TEST_F(ProductHelperTestLinux, GivenPreemptionDrmEnabledAllPreemptionDriverThreadGroupWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    productHelper.enableMidBatchPreemption = true;
    productHelper.enableThreadGroupPreemption = true;
    productHelper.enableMidThreadPreemption = true;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(ProductHelperTestLinux, GivenPreemptionDrmEnabledAllPreemptionDriverMidBatchWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidBatch;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    productHelper.enableMidBatchPreemption = true;
    productHelper.enableThreadGroupPreemption = true;
    productHelper.enableMidThreadPreemption = true;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(ProductHelperTestLinux, GivenConfigPreemptionDrmEnabledAllPreemptionDriverDisabledWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    productHelper.enableMidBatchPreemption = true;
    productHelper.enableThreadGroupPreemption = true;
    productHelper.enableMidThreadPreemption = true;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(ProductHelperTestLinux, givenPlatformEnabledFtrCompressionWhenInitializingThenFlagsAreSet) {
    pInHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    pInHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
}

TEST_F(ProductHelperTestLinux, givenPointerToHwInfoWhenConfigureHwInfoCalledThenRequiedSurfaceSizeIsSettedProperly) {
    EXPECT_EQ(MemoryConstants::pageSize, pInHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    auto expectedSize = static_cast<size_t>(outHwInfo.gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte);
    HwHelper::get(outHwInfo.platform.eRenderCoreFamily).adjustPreemptionSurfaceSize(expectedSize);
    EXPECT_EQ(expectedSize, outHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
}

TEST_F(ProductHelperTestLinux, givenInstrumentationForHardwareIsEnabledOrDisabledWhenConfiguringHwInfoThenOverrideItUsingHaveInstrumentation) {
    int ret;

    pInHwInfo.capabilityTable.instrumentationEnabled = false;
    ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.instrumentationEnabled);

    pInHwInfo.capabilityTable.instrumentationEnabled = true;
    ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    ASSERT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.instrumentationEnabled);
}

TEST_F(ProductHelperTestLinux, givenGttSizeReturnedWhenInitializingHwInfoThenSetSvmFtr) {
    drm->storedGTTSize = MemoryConstants::max64BitAppAddress;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSvm);

    drm->storedGTTSize = MemoryConstants::max64BitAppAddress + 1;
    ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrSvm);
}

TEST_F(ProductHelperTestLinux, givenGttSizeReturnedWhenInitializingHwInfoThenSetGpuAddressSpace) {
    drm->storedGTTSize = maxNBitValue(40) + 1;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(drm->storedGTTSize - 1, outHwInfo.capabilityTable.gpuAddressSpace);
}

TEST_F(ProductHelperTestLinux, givenFailingGttSizeIoctlWhenInitializingHwInfoThenSetDefaultValues) {
    drm->storedRetValForGetGttSize = -1;
    int ret = productHelper.configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
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
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    HardwareInfo outHwInfo;
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    hwInfo.gtSystemInfo.MaxSubSlicesSupported = drm->storedSSVal * 2;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = drm->storedSSVal * 2;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 16;
    hwInfo.gtSystemInfo.MaxSlicesSupported = drm->storedSVal * 4;

    int ret = productHelper.configureHwInfoDrm(&hwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
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

    ret = productHelper.configureHwInfoDrm(&hwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(12u, outHwInfo.gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(6u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice); // MaxEuPerSubslice is preserved
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSVal), outHwInfo.gtSystemInfo.MaxSlicesSupported);

    EXPECT_EQ(hwInfo.gtSystemInfo.MaxDualSubSlicesSupported, outHwInfo.gtSystemInfo.MaxDualSubSlicesSupported);

    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 0;

    ret = productHelper.configureHwInfoDrm(&hwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(8u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice);
}

HWTEST2_F(HwConfigLinux, givenSliceCountWhenConfigureHwInfoDrmThenProperInitializationInSliceInfoEnabled, MatchAny) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    HardwareInfo outHwInfo;
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    uint32_t sliceCount = 4;
    drm->storedSVal = sliceCount;
    hwInfo.gtSystemInfo.SliceCount = sliceCount;

    int ret = productHelper.configureHwInfoDrm(&hwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EXPECT_TRUE(outHwInfo.gtSystemInfo.SliceInfo[i].Enabled);
    }
}
