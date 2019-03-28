/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/gtest_helpers.h"
#include "unit_tests/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxLkf : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm->StoredDeviceID = ILKF_1x8x8_DESK_DEVICE_F0_ID;
        drm->setGtType(GTTYPE_GT1);
        drm->StoredSSVal = 8;
    }
};

TEST_F(HwInfoConfigTestLinuxLkf, configureHwInfoLkf) {
    auto hwInfoConfig = HwInfoConfigHw<IGFX_LAKEFIELD>::get();
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.pSysInfo->SliceCount);

    EXPECT_EQ(GTTYPE_GT1, outHwInfo.pPlatform->eGTType);
    EXPECT_TRUE(outHwInfo.pSkuTable->ftrGT1);
    EXPECT_FALSE(outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_FALSE(outHwInfo.pSkuTable->ftrGT2);
    EXPECT_FALSE(outHwInfo.pSkuTable->ftrGT3);
    EXPECT_FALSE(outHwInfo.pSkuTable->ftrGT4);
    EXPECT_FALSE(outHwInfo.pSkuTable->ftrGTA);
    EXPECT_FALSE(outHwInfo.pSkuTable->ftrGTC);
    EXPECT_FALSE(outHwInfo.pSkuTable->ftrGTX);
    EXPECT_FALSE(outHwInfo.pSkuTable->ftrTileY);

    ReleaseOutHwInfoStructs();
}

TEST_F(HwInfoConfigTestLinuxLkf, negative) {
    auto hwInfoConfig = HwInfoConfigHw<IGFX_LAKEFIELD>::get();

    drm->StoredRetValForDeviceID = -1;
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    ReleaseOutHwInfoStructs();

    drm->StoredRetValForDeviceID = 0;
    drm->StoredRetValForDeviceRevID = -1;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    ReleaseOutHwInfoStructs();

    drm->StoredRetValForDeviceRevID = 0;
    drm->StoredRetValForEUVal = -1;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    ReleaseOutHwInfoStructs();

    drm->StoredRetValForEUVal = 0;
    drm->StoredRetValForSSVal = -1;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

template <typename T>
class LkfHwInfoTests : public ::testing::Test {};
typedef ::testing::Types<LKF_1x8x8> lkfTestTypes;
TYPED_TEST_CASE(LkfHwInfoTests, lkfTestTypes);
TYPED_TEST(LkfHwInfoTests, gtSetupIsCorrect) {
    GT_SYSTEM_INFO gtSystemInfo;
    FeatureTable featureTable = {};
    WorkaroundTable pWaTable;
    PLATFORM pPlatform;
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &gtSystemInfo;
    hwInfo.pSkuTable = &featureTable;
    hwInfo.pWaTable = &pWaTable;
    hwInfo.pPlatform = &pPlatform;
    memset(&gtSystemInfo, 0, sizeof(gtSystemInfo));
    TypeParam::setupHardwareInfo(&hwInfo, false);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
