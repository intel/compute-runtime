/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;
using namespace std;

struct HwInfoConfigTestLinuxBdw : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();
        drm->StoredDeviceID = IBDW_GT2_ULT_MOBL_DEVICE_F0_ID;
        drm->setGtType(GTTYPE_GT2);
    }
};

BDWTEST_F(HwInfoConfigTestLinuxBdw, configureHwInfo) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    drm->StoredSSVal = 3;
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT2, outHwInfo.platform.eGTType);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1_5);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrGT2);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT3);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT4);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTA);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTC);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTX);

    drm->StoredDeviceID = IBDW_GT1_HALO_MOBL_DEVICE_F0_ID;
    drm->setGtType(GTTYPE_GT1);
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT1, outHwInfo.platform.eGTType);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrGT1);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT2);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT3);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT4);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTA);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTC);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTX);

    drm->StoredDeviceID = IBDW_GT3_ULT_MOBL_DEVICE_F0_ID;
    drm->setGtType(GTTYPE_GT3);
    drm->StoredSSVal = 6;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(2u, outHwInfo.gtSystemInfo.SliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT3, outHwInfo.platform.eGTType);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT2);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrGT3);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT4);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTA);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTC);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTX);
}

BDWTEST_F(HwInfoConfigTestLinuxBdw, negativeUnknownDevId) {
    drm->StoredDeviceID = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

BDWTEST_F(HwInfoConfigTestLinuxBdw, negativeFailedIoctlDevId) {
    drm->StoredRetValForDeviceID = -2;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-2, ret);
}

BDWTEST_F(HwInfoConfigTestLinuxBdw, negativeFailedIoctlDevRevId) {
    drm->StoredRetValForDeviceRevID = -3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-3, ret);
}

BDWTEST_F(HwInfoConfigTestLinuxBdw, negativeFailedIoctlEuCount) {
    drm->StoredRetValForEUVal = -4;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

BDWTEST_F(HwInfoConfigTestLinuxBdw, negativeFailedIoctlSsCount) {
    drm->StoredRetValForSSVal = -5;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

BDWTEST_F(HwInfoConfigTestLinuxBdw, configureHwInfoWaFlags) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->StoredDeviceRevID = 0;
    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
}

BDWTEST_F(HwInfoConfigTestLinuxBdw, configureHwInfoEdram) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    int ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL(0u, outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrEDram);

    drm->StoredDeviceID = IBDW_GT3_HALO_MOBL_DEVICE_F0_ID;
    drm->setGtType(GTTYPE_GT3);
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((128u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrEDram);

    drm->StoredDeviceID = IBDW_GT3_SERV_DEVICE_F0_ID;
    ret = hwInfoConfig->configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((128u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrEDram);
}

BDWTEST_F(HwInfoConfigTestLinuxBdw, whenCallAdjustPlatformThenDoNothing) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    outHwInfo = pInHwInfo;
    hwInfoConfig->adjustPlatformForProductFamily(&outHwInfo);

    int ret = memcmp(&outHwInfo.platform, &pInHwInfo.platform, sizeof(PLATFORM));
    EXPECT_EQ(0, ret);
}

template <typename T>
class BdwHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<BDW_1x2x6, BDW_1x3x6, BDW_1x3x8, BDW_2x3x8> bdwTestTypes;
TYPED_TEST_CASE(BdwHwInfoTests, bdwTestTypes);
TYPED_TEST(BdwHwInfoTests, gtSetupIsCorrect) {
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
