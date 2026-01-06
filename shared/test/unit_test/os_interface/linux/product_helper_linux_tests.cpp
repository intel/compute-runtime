/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct MockProductHelperTestLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        testPlatform->eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

        raii = new RAIIProductHelperFactory<MockProductHelperHw<IGFX_UNKNOWN>>(*this->executionEnvironment->rootDeviceEnvironments[0]);
        mockProductHelper = raii->mockProductHelper;
    }

    void TearDown() override {
        delete raii;
        ProductHelperTestLinux::TearDown();
    }

    RAIIProductHelperFactory<MockProductHelperHw<IGFX_UNKNOWN>> *raii;

    MockProductHelperHw<IGFX_UNKNOWN> *mockProductHelper;
};

TEST_F(MockProductHelperTestLinux, GivenDummyConfigWhenConfiguringHwInfoThenSucceeds) {
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
}

TEST_F(MockProductHelperTestLinux, GivenDummyConfigThenEdramIsDetected) {
    mockProductHelper->use128MbEdram = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);
}

TEST_F(MockProductHelperTestLinux, givenEnabledPlatformCoherencyWhenConfiguringHwInfoThenIgnoreAndSetAsDisabled) {
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(MockProductHelperTestLinux, givenDisabledPlatformCoherencyWhenConfiguringHwInfoThenSetValidCapability) {
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(MockProductHelperTestLinux, whenFailGettingTopologyThenFallbackToEuCountIoctl) {
    drm->failRetTopology = true;

    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_NE(-1, ret);
}

TEST_F(MockProductHelperTestLinux, givenInvalidTopologyDataWhenConfiguringThenReturnError) {
    auto storedSVal = drm->storedSVal;
    auto storedSSVal = drm->storedSSVal;
    auto storedEUVal = drm->storedEUVal;
    drm->engineInfoQueried = true;
    drm->systemInfoQueried = true;
    {
        // 0 euCount
        drm->storedSVal = storedSVal;
        drm->storedSSVal = storedSSVal;
        drm->storedEUVal = 0;

        DrmQueryTopologyData topologyData = {};
        drm->topologyQueried = false;
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }

    {
        // 0 subSliceCount
        drm->storedSVal = storedSVal;
        drm->storedSSVal = 0;
        drm->storedEUVal = storedEUVal;

        DrmQueryTopologyData topologyData = {};
        drm->topologyQueried = false;
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }

    {
        // 0 sliceCount
        drm->storedSVal = 0;
        drm->storedSSVal = storedSSVal;
        drm->storedEUVal = storedEUVal;

        DrmQueryTopologyData topologyData = {};
        drm->topologyQueried = false;
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }
}

TEST_F(MockProductHelperTestLinux, GivenFailingCustomConfigWhenConfiguringHwInfoThenFails) {
    mockProductHelper->failOnConfigureHardwareCustom = true;

    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(-1, ret);
}

TEST_F(MockProductHelperTestLinux, whenConfigureHwInfoIsCalledThenAreNonPersistentContextsSupportedReturnsTrue) {
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(drm->areNonPersistentContextsSupported());
}

TEST_F(MockProductHelperTestLinux, whenConfigureHwInfoIsCalledAndPersitentContextIsUnsupportedThenAreNonPersistentContextsSupportedReturnsFalse) {
    drm->storedPersistentContextsSupport = 0;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(drm->areNonPersistentContextsSupported());
}

HWTEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledMidThreadOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;

    mockProductHelper->enableMidThreadPreemption = true;

    pInHwInfo.featureTable.flags.ftrWalkerMTP = true;

    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    if (getRootDeviceEnvironment().compilerProductHelper->isMidThreadPreemptionSupported(outHwInfo)) {
        EXPECT_EQ(PreemptionMode::MidThread, outHwInfo.capabilityTable.defaultPreemptionMode);
    }
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledThreadGroupOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableThreadGroupPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, givenDebugFlagSetWhenConfiguringHwInfoThenPrintGetParamIoctlsOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintIoctlEntries.set(true);

    StreamCapture capture;
    capture.captureStdout(); // start capturing
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    std::array<std::string, 1> expectedStrings = {{"DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_HAS_SCHEDULER, output value: 7, retCode: 0"

    }};

    debugManager.flags.PrintIoctlEntries.set(false);
    std::string output = capture.getCapturedStdout(); // stop capturing
    for (const auto &expectedString : expectedStrings) {
        EXPECT_NE(std::string::npos, output.find(expectedString));
    }

    EXPECT_EQ(std::string::npos, output.find("UNKNOWN"));
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledMidBatchOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableMidBatchPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, WhenConfiguringHwInfoThenPreemptionIsSupportedPreemptionDrmEnabledNoPreemptionWhenConfiguringHwInfoThenPreemptionIsNotSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmDisabledAllPreemptionWhenConfiguringHwInfoThenPreemptionIsNotSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport = 0;
    mockProductHelper->enableMidThreadPreemption = true;
    mockProductHelper->enableMidBatchPreemption = true;
    mockProductHelper->enableThreadGroupPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    mockProductHelper->enableMidThreadPreemption = true;
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_FALSE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledAllPreemptionDriverThreadGroupWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableMidBatchPreemption = true;
    mockProductHelper->enableThreadGroupPreemption = true;
    mockProductHelper->enableMidThreadPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledAllPreemptionDriverMidBatchWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidBatch;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableMidBatchPreemption = true;
    mockProductHelper->enableThreadGroupPreemption = true;
    mockProductHelper->enableMidThreadPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenConfigPreemptionDrmEnabledAllPreemptionDriverDisabledWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableMidBatchPreemption = true;
    mockProductHelper->enableThreadGroupPreemption = true;
    mockProductHelper->enableMidThreadPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, givenPointerToHwInfoWhenConfigureHwInfoCalledThenRequiedSurfaceSizeIsSettedProperly) {
    EXPECT_EQ(MemoryConstants::pageSize, pInHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    auto expectedSize = static_cast<size_t>(outHwInfo.gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte);
    auto &rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0];
    auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
    gfxCoreHelper.adjustPreemptionSurfaceSize(expectedSize, *rootDeviceEnvironment);
    EXPECT_EQ(expectedSize, outHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
}

TEST_F(MockProductHelperTestLinux, givenInstrumentationForHardwareIsEnabledOrDisabledWhenConfiguringHwInfoThenOverrideItUsingHaveInstrumentation) {
    int ret;

    pInHwInfo.capabilityTable.instrumentationEnabled = false;
    ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.instrumentationEnabled);

    pInHwInfo.capabilityTable.instrumentationEnabled = true;
    ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    ASSERT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.instrumentationEnabled);
}

TEST_F(MockProductHelperTestLinux, givenGttSizeReturnedWhenInitializingHwInfoThenSetGpuAddressSpace) {
    drm->storedGTTSize = maxNBitValue(40) + 1;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(drm->storedGTTSize - 1, outHwInfo.capabilityTable.gpuAddressSpace);
}

TEST_F(MockProductHelperTestLinux, givenFailingGttSizeIoctlWhenInitializingHwInfoThenSetDefaultValues) {
    drm->storedRetValForGetGttSize = -1;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    EXPECT_NE(0u, outHwInfo.capabilityTable.gpuAddressSpace);
    EXPECT_EQ(pInHwInfo.capabilityTable.gpuAddressSpace, outHwInfo.capabilityTable.gpuAddressSpace);
}

using HwConfigLinux = ::testing::Test;

HWTEST2_F(HwConfigLinux, givenPlatformWithPlatformQuerySupportedWhenItIsCalledThenReturnTrue, IsAtLeastMtl) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    EXPECT_TRUE(productHelper.isPlatformQuerySupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsPlatformQueryNotSupportedThenReturnFalse, IsAtLeastMtl) {

    EXPECT_TRUE(productHelper->isPlatformQuerySupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIsDisableScratchPagesSupportedThenReturnTrue, IsAtLeastXeHpcCore) {
    EXPECT_TRUE(productHelper->isDisableScratchPagesSupported());
}

HWTEST2_F(ProductHelperTestLinux, givenE2ECompressionWhenConfiguringHwInfoDrmThenCompressionFlagsAreCorrectlySet, IsAtMostXeCore) {
    pInHwInfo.featureTable.flags.ftrE2ECompression = true;
    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);

    pInHwInfo.featureTable.flags.ftrE2ECompression = false;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
}

HWTEST2_F(ProductHelperTestLinux, givenXe2CompressionWhenConfiguringHwInfoDrmThenCompressionFlagsAreCorrectlySet, IsAtLeastXe2HpgCore) {
    pInHwInfo.featureTable.flags.ftrXe2Compression = true;
    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);

    pInHwInfo.featureTable.flags.ftrXe2Compression = false;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
}
