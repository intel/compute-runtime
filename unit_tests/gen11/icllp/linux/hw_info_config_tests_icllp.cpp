/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/gtest_helpers.h"
#include "unit_tests/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxIcllp : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm->StoredDeviceID = IICL_LP_GT1_MOB_DEVICE_F0_ID;
        drm->setGtType(GTTYPE_GT1);
    }
};

ICLLPTEST_F(HwInfoConfigTestLinuxIcllp, configureHwInfoIcllp) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT1, outHwInfo.platform.eGTType);
    EXPECT_TRUE(outHwInfo.featureTable.ftrGT1);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGT1_5);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGT2);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGT3);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGT4);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGTA);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGTC);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGTX);
    EXPECT_FALSE(outHwInfo.featureTable.ftrTileY);
}

ICLLPTEST_F(HwInfoConfigTestLinuxIcllp, negative) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->StoredRetValForDeviceID = -1;
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    drm->StoredRetValForDeviceID = 0;
    drm->StoredRetValForDeviceRevID = -1;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    drm->StoredRetValForDeviceRevID = 0;
    drm->StoredRetValForEUVal = -1;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    drm->StoredRetValForEUVal = 0;
    drm->StoredRetValForSSVal = -1;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

template <typename T>
class IcllpHwInfoTests : public ::testing::Test {};
typedef ::testing::Types<ICLLP_1x8x8, ICLLP_1x4x8, ICLLP_1x6x8, ICLLP_1x1x8> icllpTestTypes;
TYPED_TEST_CASE(IcllpHwInfoTests, icllpTestTypes);
TYPED_TEST(IcllpHwInfoTests, gtSetupIsCorrect) {
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
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 5u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
