/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"
#include "shared/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxCfl : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();
    }
};

CFLTEST_F(HwInfoConfigTestLinuxCfl, WhenConfiguringHwInfoThenInformationIsCorrect) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    //constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.VEBoxInfo.Instances.Bits.VEBox0Enabled);
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VEBoxInfo.IsValid);

    pInHwInfo.platform.usDeviceID = 0x3E90;
    drm->storedSSVal = 3;

    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    pInHwInfo.platform.usDeviceID = 0x3EA5;
    drm->storedSSVal = 6;

    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(2u, outHwInfo.gtSystemInfo.SliceCount);
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

CFLTEST_F(HwInfoConfigTestLinuxCfl, GivenFailedIoctlEuCountWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForEUVal = -4;
    drm->failRetTopology = true;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, GivenFailedIoctlSsCountWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForSSVal = -5;
    drm->failRetTopology = true;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, WhenConfiguringHwInfoThenEdramInformationIsCorrect) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL(0u, outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrEDram);

    pInHwInfo.platform.usDeviceID = 0x3EA8;

    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    pInHwInfo.platform.usDeviceID = 0x3EA6;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);
}

template <typename T>
class CflHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<CflHw1x2x6, CflHw1x3x6, CflHw1x3x8, CflHw2x3x8, CflHw3x3x8> cflTestTypes;
TYPED_TEST_CASE(CflHwInfoTests, cflTestTypes);
TYPED_TEST(CflHwInfoTests, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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
