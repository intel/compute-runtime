/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxBxt : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();
        drm->StoredDeviceID = IBXT_P_3x6_DEVICE_ID;
        drm->setGtType(GTTYPE_GTA);
        drm->StoredEUVal = 18;
        drm->StoredHasPooledEU = 1;
        drm->StoredMinEUinPool = 3;
    }
};

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfo) {
    drm->StoredDeviceRevID = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ((unsigned int)drm->StoredHasPooledEU, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->StoredMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGttCacheInvalidation);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    //constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.VEBoxInfo.Instances.Bits.VEBox0Enabled);
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VEBoxInfo.IsValid);

    EXPECT_EQ(GTTYPE_GTA, outHwInfo.platform.eGTType);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT2);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT3);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT4);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrGTA);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTC);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTX);

    drm->StoredDeviceID = IBXT_P_12EU_3x6_DEVICE_ID;
    drm->setGtType(GTTYPE_GTC); //IBXT_P_12EU_3x6_DEVICE_ID is GTA, but fot test sake make it GTC
    drm->StoredMinEUinPool = 6;
    drm->StoredDeviceRevID = 4;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ((unsigned int)drm->StoredHasPooledEU, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->StoredMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GTC, outHwInfo.platform.eGTType);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT2);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT3);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT4);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTA);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrGTC);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTX);

    drm->StoredDeviceID = IBXT_P_12EU_3x6_DEVICE_ID;
    drm->setGtType(GTTYPE_GTX); //IBXT_P_12EU_3x6_DEVICE_ID is GTA, but fot test sake make it GTX
    drm->StoredMinEUinPool = 9;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ((unsigned int)drm->StoredHasPooledEU, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->StoredMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GTX, outHwInfo.platform.eGTType);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT2);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT3);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT4);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTA);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTC);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrGTX);

    auto &outKmdNotifyProperties = outHwInfo.capabilityTable.kmdNotifyProperties;
    EXPECT_TRUE(outKmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(50000, outKmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(5000, outKmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(200000, outKmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeUnknownDevId) {
    drm->StoredDeviceID = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeFailedIoctlDevId) {
    drm->StoredRetValForDeviceID = -2;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-2, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeFailedIoctlDevRevId) {
    drm->StoredRetValForDeviceRevID = -3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-3, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeFailedIoctlEuCount) {
    drm->StoredRetValForEUVal = -4;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeFailedIoctlSsCount) {
    drm->StoredRetValForSSVal = -5;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoFailingEnabledPool) {
    drm->StoredRetValForPooledEU = -1;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoDisabledEnabledPool) {
    drm->StoredHasPooledEU = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoFailingMinEuInPool) {
    drm->StoredRetValForMinEUinPool = -1;

    drm->StoredSSVal = 3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ(9u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);

    drm->StoredSSVal = 2;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ(3u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoInvalidMinEuInPool) {
    drm->StoredMinEUinPool = 4;

    drm->StoredSSVal = 3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ(9u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);

    drm->StoredSSVal = 2;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.featureTable.ftrPooledEuEnabled);
    EXPECT_EQ(3u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoWaFlags) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->StoredDeviceRevID = 0;
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, whenCallAdjustPlatformThenDoNothing) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    outHwInfo = pInHwInfo;
    hwInfoConfig->adjustPlatformForProductFamily(&outHwInfo);

    int ret = memcmp(&outHwInfo.platform, &pInHwInfo.platform, sizeof(PLATFORM));
    EXPECT_EQ(0, ret);
}

template <typename T>
class BxtHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<BXT_1x2x6, BXT_1x3x6> bxtTestTypes;
TYPED_TEST_CASE(BxtHwInfoTests, bxtTestTypes);
TYPED_TEST(BxtHwInfoTests, gtSetupIsCorrect) {
    HardwareInfo hwInfo;
    DrmMock drm;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    DeviceDescriptor device = {0, &hwInfo, &TypeParam::setupHardwareInfo, GTTYPE_GT1};

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
