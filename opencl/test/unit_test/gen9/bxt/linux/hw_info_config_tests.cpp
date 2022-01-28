/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"
using namespace NEO;

struct HwInfoConfigTestLinuxBxt : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();
        drm->storedDeviceID = 0x5A84;

        drm->storedEUVal = 18;
        drm->storedHasPooledEU = 1;
        drm->storedMinEUinPool = 3;
    }
};

BXTTEST_F(HwInfoConfigTestLinuxBxt, WhenConfiguringHwInfoThenConfigIsCorrect) {
    drm->storedDeviceRevID = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->storedDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->storedDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ((unsigned int)drm->storedHasPooledEU, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->storedMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrGttCacheInvalidation);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    //constant sysInfo/ftr flags
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VEBoxInfo.IsValid);

    drm->storedDeviceID = 0x5A85;
    drm->storedMinEUinPool = 6;
    drm->storedDeviceRevID = 4;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->storedDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->storedDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ((unsigned int)drm->storedHasPooledEU, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->storedMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    drm->storedDeviceID = 0x5A85;
    drm->storedMinEUinPool = 9;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->storedDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->storedDeviceRevID, outHwInfo.platform.usRevId);
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
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, GivenUnknownDevIdWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedDeviceID = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, GivenFailedIoctlDevIdWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForDeviceID = -2;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-2, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, GivenFailedIoctlDevRevIdWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForDeviceRevID = -3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-3, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, GivenFailedIoctlEuCountWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->failRetTopology = true;
    drm->storedRetValForEUVal = -4;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, GivenFailingEnabledPoolWhenConfiguringHwInfoThenZeroIsReturned) {
    drm->storedRetValForPooledEU = -1;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, GivenDisabledEnabledPoolWhenConfiguringHwInfoThenZeroIsReturned) {
    drm->storedHasPooledEU = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, GivenFailingMinEuInPoolWhenConfiguringHwInfoThenZeroIsReturned) {
    drm->storedRetValForMinEUinPool = -1;

    drm->storedSSVal = 3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(9u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);

    drm->storedSSVal = 2;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(3u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, GivenInvalidMinEuInPoolWhenConfiguringHwInfoThenZeroIsReturned) {
    drm->storedMinEUinPool = 4;

    drm->storedSSVal = 3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(9u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);

    drm->storedSSVal = 2;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(3u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, WhenConfiguringHwInfoWaFlagsThenZeroIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->storedDeviceRevID = 0;
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
}

template <typename T>
class BxtHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<BXT_1x2x6, BXT_1x3x6> bxtTestTypes;
TYPED_TEST_CASE(BxtHwInfoTests, bxtTestTypes);
TYPED_TEST(BxtHwInfoTests, WhenConfiguringHwInfoThenConfigIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    DeviceDescriptor device = {0, &hwInfo, &TypeParam::setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
