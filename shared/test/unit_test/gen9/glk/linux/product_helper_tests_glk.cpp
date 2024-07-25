/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "hw_cmds.h"

using namespace NEO;

struct GlkProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm->storedEUVal = 18;
        drm->storedHasPooledEU = 1;
        drm->storedMinEUinPool = 3;
    }
};

GLKTEST_F(GlkProductHelperLinux, WhenConfiguringHwInfoThenInformationIsCorrect) {
    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    // constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.VEBoxInfo.Instances.Bits.VEBox0Enabled);
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VEBoxInfo.IsValid);

    pInHwInfo.platform.usDeviceID = 0x3184;

    drm->storedMinEUinPool = 6;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ((unsigned int)drm->storedHasPooledEU, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->storedMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    pInHwInfo.platform.usDeviceID = 0x3185;

    drm->storedMinEUinPool = 9;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ((unsigned int)drm->storedHasPooledEU, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->storedMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    auto &outKmdNotifyProperties = outHwInfo.capabilityTable.kmdNotifyProperties;
    EXPECT_TRUE(outKmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(50000, outKmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(5000, outKmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(200000, outKmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    EXPECT_FALSE(outKmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(0, outKmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

GLKTEST_F(GlkProductHelperLinux, GivenInvalidInputWhenConfiguringHwInfoThenErrorIsReturned) {

    drm->failRetTopology = true;
    drm->storedRetValForEUVal = -1;

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);

    drm->storedRetValForEUVal = 0;
    drm->storedRetValForSSVal = -1;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);
}

GLKTEST_F(GlkProductHelperLinux, GivenFailingEnabledPoolWhenConfiguringHwInfoThenZeroIsSet) {
    drm->storedRetValForPooledEU = -1;

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

GLKTEST_F(GlkProductHelperLinux, GivenDisabledEnabledPoolWhenConfiguringHwInfoThenZeroIsSet) {
    drm->storedHasPooledEU = 0;

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

GLKTEST_F(GlkProductHelperLinux, GivenFailingMinEuInPoolWhenConfiguringHwInfoThenCorrectValueSet) {
    drm->storedRetValForMinEUinPool = -1;
    drm->storedSSVal = 3;

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(9u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);

    drm->storedSSVal = 2;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(3u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

GLKTEST_F(GlkProductHelperLinux, GivenInvalidMinEuInPoolWhenConfiguringHwInfoThenCorrectValueSet) {
    drm->storedMinEUinPool = 4;
    drm->storedSSVal = 3;

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(9u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);

    drm->storedSSVal = 2;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(3u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

GLKTEST_F(GlkProductHelperLinux, GivenWaFlagsWhenConfiguringHwInfoThenInformationIsCorrect) {
    pInHwInfo.platform.usRevId = 0;
    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

template <typename T>
class GlkHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<GlkHw1x3x6, GlkHw1x2x6> glkTestTypes;
TYPED_TEST_SUITE(GlkHwInfoTests, glkTestTypes);
TYPED_TEST(GlkHwInfoTests, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &TypeParam::hwInfo, &TypeParam::setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);

    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
