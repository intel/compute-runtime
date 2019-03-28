/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/gtest_helpers.h"
#include "unit_tests/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;
using namespace std;

struct HwInfoConfigTestLinuxIcllp : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm->StoredDeviceID = IICL_LP_GT1_MOB_DEVICE_F0_ID;
        drm->setGtType(GTTYPE_GT1);
    }
};

ICLLPTEST_F(HwInfoConfigTestLinuxIcllp, configureHwInfo) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.pSysInfo->SliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

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
}

ICLLPTEST_F(HwInfoConfigTestLinuxIcllp, negative) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

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

using IcllpHwInfoTests = ::testing::Test;

ICLLPTEST_F(IcllpHwInfoTests, icllp1x8x8systemInfo) {
    GT_SYSTEM_INFO requestedGtSystemInfo = {};
    GT_SYSTEM_INFO expectedGtSystemInfo = {};
    FeatureTable featureTable = {};
    WorkaroundTable pWaTable;
    PLATFORM pPlatform;
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &requestedGtSystemInfo;
    hwInfo.pSkuTable = &featureTable;
    hwInfo.pWaTable = &pWaTable;
    hwInfo.pPlatform = &pPlatform;

    expectedGtSystemInfo.EUCount = 63;
    expectedGtSystemInfo.ThreadCount = 63 * ICLLP::threadsPerEu;
    expectedGtSystemInfo.SliceCount = 1;
    expectedGtSystemInfo.SubSliceCount = 8;
    expectedGtSystemInfo.L3CacheSizeInKb = 3072;
    expectedGtSystemInfo.L3BankCount = 8;
    expectedGtSystemInfo.MaxFillRate = 16;
    expectedGtSystemInfo.TotalVsThreads = 336;
    expectedGtSystemInfo.TotalHsThreads = 336;
    expectedGtSystemInfo.TotalDsThreads = 336;
    expectedGtSystemInfo.TotalGsThreads = 336;
    expectedGtSystemInfo.TotalPsThreadsWindowerRange = 64;
    expectedGtSystemInfo.CsrSizeInMb = 8;
    expectedGtSystemInfo.MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    expectedGtSystemInfo.MaxSlicesSupported = ICLLP::maxSlicesSupported;
    expectedGtSystemInfo.MaxSubSlicesSupported = ICLLP::maxSubslicesSupported;
    expectedGtSystemInfo.IsL3HashModeEnabled = false;
    expectedGtSystemInfo.IsDynamicallyPopulated = false;

    ICLLP_1x8x8::setupHardwareInfo(&hwInfo, false);
    EXPECT_TRUE(memcmp(&requestedGtSystemInfo, &expectedGtSystemInfo, sizeof(GT_SYSTEM_INFO)) == 0);
}

ICLLPTEST_F(IcllpHwInfoTests, icllp1x4x8systemInfo) {
    GT_SYSTEM_INFO requestedGtSystemInfo = {};
    GT_SYSTEM_INFO expectedGtSystemInfo = {};
    FeatureTable featureTable = {};
    WorkaroundTable pWaTable;
    PLATFORM pPlatform;
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &requestedGtSystemInfo;
    hwInfo.pSkuTable = &featureTable;
    hwInfo.pWaTable = &pWaTable;
    hwInfo.pPlatform = &pPlatform;

    expectedGtSystemInfo.EUCount = 31;
    expectedGtSystemInfo.ThreadCount = 31 * ICLLP::threadsPerEu;
    expectedGtSystemInfo.SliceCount = 1;
    expectedGtSystemInfo.SubSliceCount = 4;
    expectedGtSystemInfo.L3CacheSizeInKb = 2304;
    expectedGtSystemInfo.L3BankCount = 6;
    expectedGtSystemInfo.MaxFillRate = 8;
    expectedGtSystemInfo.TotalVsThreads = 364;
    expectedGtSystemInfo.TotalHsThreads = 224;
    expectedGtSystemInfo.TotalDsThreads = 364;
    expectedGtSystemInfo.TotalGsThreads = 224;
    expectedGtSystemInfo.TotalPsThreadsWindowerRange = 128;
    expectedGtSystemInfo.CsrSizeInMb = 5;
    expectedGtSystemInfo.MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    expectedGtSystemInfo.MaxSlicesSupported = 1;
    expectedGtSystemInfo.MaxSubSlicesSupported = 4;
    expectedGtSystemInfo.IsL3HashModeEnabled = false;
    expectedGtSystemInfo.IsDynamicallyPopulated = false;

    ICLLP_1x4x8::setupHardwareInfo(&hwInfo, false);
    EXPECT_TRUE(memcmp(&requestedGtSystemInfo, &expectedGtSystemInfo, sizeof(GT_SYSTEM_INFO)) == 0);
}

ICLLPTEST_F(IcllpHwInfoTests, icllp1x6x8systemInfo) {
    GT_SYSTEM_INFO requestedGtSystemInfo = {};
    GT_SYSTEM_INFO expectedGtSystemInfo = {};
    FeatureTable featureTable = {};
    WorkaroundTable pWaTable;
    PLATFORM pPlatform;
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &requestedGtSystemInfo;
    hwInfo.pSkuTable = &featureTable;
    hwInfo.pWaTable = &pWaTable;
    hwInfo.pPlatform = &pPlatform;

    expectedGtSystemInfo.EUCount = 47;
    expectedGtSystemInfo.ThreadCount = 47 * ICLLP::threadsPerEu;
    expectedGtSystemInfo.SliceCount = 1;
    expectedGtSystemInfo.SubSliceCount = 6;
    expectedGtSystemInfo.L3CacheSizeInKb = 2304;
    expectedGtSystemInfo.L3BankCount = 6;
    expectedGtSystemInfo.MaxFillRate = 8;
    expectedGtSystemInfo.TotalVsThreads = 364;
    expectedGtSystemInfo.TotalHsThreads = 224;
    expectedGtSystemInfo.TotalDsThreads = 364;
    expectedGtSystemInfo.TotalGsThreads = 224;
    expectedGtSystemInfo.TotalPsThreadsWindowerRange = 128;
    expectedGtSystemInfo.CsrSizeInMb = 5;
    expectedGtSystemInfo.MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    expectedGtSystemInfo.MaxSlicesSupported = 1;
    expectedGtSystemInfo.MaxSubSlicesSupported = 4;
    expectedGtSystemInfo.IsL3HashModeEnabled = false;
    expectedGtSystemInfo.IsDynamicallyPopulated = false;

    ICLLP_1x6x8::setupHardwareInfo(&hwInfo, false);
    EXPECT_TRUE(memcmp(&requestedGtSystemInfo, &expectedGtSystemInfo, sizeof(GT_SYSTEM_INFO)) == 0);
}

ICLLPTEST_F(IcllpHwInfoTests, icllp1x1x8systemInfo) {
    GT_SYSTEM_INFO requestedGtSystemInfo = {};
    GT_SYSTEM_INFO expectedGtSystemInfo = {};
    FeatureTable featureTable = {};
    WorkaroundTable pWaTable;
    PLATFORM pPlatform;
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &requestedGtSystemInfo;
    hwInfo.pSkuTable = &featureTable;
    hwInfo.pWaTable = &pWaTable;
    hwInfo.pPlatform = &pPlatform;

    expectedGtSystemInfo.EUCount = 8;
    expectedGtSystemInfo.ThreadCount = 8 * ICLLP::threadsPerEu;
    expectedGtSystemInfo.SliceCount = 1;
    expectedGtSystemInfo.SubSliceCount = 1;
    expectedGtSystemInfo.L3CacheSizeInKb = 2304;
    expectedGtSystemInfo.L3BankCount = 6;
    expectedGtSystemInfo.MaxFillRate = 8;
    expectedGtSystemInfo.TotalVsThreads = 364;
    expectedGtSystemInfo.TotalHsThreads = 224;
    expectedGtSystemInfo.TotalDsThreads = 364;
    expectedGtSystemInfo.TotalGsThreads = 224;
    expectedGtSystemInfo.TotalPsThreadsWindowerRange = 128;
    expectedGtSystemInfo.CsrSizeInMb = 5;
    expectedGtSystemInfo.MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    expectedGtSystemInfo.MaxSlicesSupported = 1;
    expectedGtSystemInfo.MaxSubSlicesSupported = 4;
    expectedGtSystemInfo.IsL3HashModeEnabled = false;
    expectedGtSystemInfo.IsDynamicallyPopulated = false;

    ICLLP_1x1x8::setupHardwareInfo(&hwInfo, false);
    EXPECT_TRUE(memcmp(&requestedGtSystemInfo, &expectedGtSystemInfo, sizeof(GT_SYSTEM_INFO)) == 0);
}
