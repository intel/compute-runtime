/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/gtest_helpers.h"
#include "unit_tests/os_interface/linux/hw_info_config_linux_tests.h"

using namespace OCLRT;
using namespace std;

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
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ((unsigned int)drm->StoredHasPooledEU, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->StoredMinEUinPool, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.pSysInfo->EUCount - outHwInfo.pSysInfo->EuCountPerPoolMin), outHwInfo.pSysInfo->EuCountPerPoolMax);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGttCacheInvalidation);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    //constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.pSysInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled);
    EXPECT_TRUE(outHwInfo.pSysInfo->VEBoxInfo.IsValid);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrVEBOX);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrULT);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGpGpuMidBatchPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGpGpuThreadGroupLevelPreempt);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGpGpuMidThreadLevelPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftr3dMidBatchPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftr3dObjectLevelPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPerCtxtPreemptionGranularityControl);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrLCIA);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPPGTT);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrL3IACoherency);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrIA32eGfxPTEs);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrDisplayYTiling);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrTranslationTable);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrUserModeTranslationTable);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrEnableGuC);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbc);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbc2AddressTranslation);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbcBlitterTracking);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbcCpuTracking);

    EXPECT_EQ(GTTYPE_GTA, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = IBXT_P_12EU_3x6_DEVICE_ID;
    drm->setGtType(GTTYPE_GTC); //IBXT_P_12EU_3x6_DEVICE_ID is GTA, but fot test sake make it GTC
    drm->StoredMinEUinPool = 6;
    drm->StoredDeviceRevID = 4;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ((unsigned int)drm->StoredHasPooledEU, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->StoredMinEUinPool, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.pSysInfo->EUCount - outHwInfo.pSysInfo->EuCountPerPoolMin), outHwInfo.pSysInfo->EuCountPerPoolMax);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGttCacheInvalidation);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GTC, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = IBXT_P_12EU_3x6_DEVICE_ID;
    drm->setGtType(GTTYPE_GTX); //IBXT_P_12EU_3x6_DEVICE_ID is GTA, but fot test sake make it GTX
    drm->StoredMinEUinPool = 9;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ((unsigned int)drm->StoredHasPooledEU, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->StoredMinEUinPool, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.pSysInfo->EUCount - outHwInfo.pSysInfo->EuCountPerPoolMin), outHwInfo.pSysInfo->EuCountPerPoolMax);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GTX, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGTX);

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
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeFailedIoctlDevId) {
    drm->StoredRetValForDeviceID = -2;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-2, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeFailedIoctlDevRevId) {
    drm->StoredRetValForDeviceRevID = -3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-3, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeFailedIoctlEuCount) {
    drm->StoredRetValForEUVal = -4;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, negativeFailedIoctlSsCount) {
    drm->StoredRetValForSSVal = -5;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoFailingEnabledPool) {
    drm->StoredRetValForPooledEU = -1;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.pSysInfo->EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoDisabledEnabledPool) {
    drm->StoredHasPooledEU = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.pSysInfo->EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoFailingMinEuInPool) {
    drm->StoredRetValForMinEUinPool = -1;

    drm->StoredSSVal = 3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ(9u, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.pSysInfo->EUCount - outHwInfo.pSysInfo->EuCountPerPoolMin), outHwInfo.pSysInfo->EuCountPerPoolMax);

    ReleaseOutHwInfoStructs();

    drm->StoredSSVal = 2;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ(3u, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.pSysInfo->EUCount - outHwInfo.pSysInfo->EuCountPerPoolMin), outHwInfo.pSysInfo->EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoInvalidMinEuInPool) {
    drm->StoredMinEUinPool = 4;

    drm->StoredSSVal = 3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ(9u, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.pSysInfo->EUCount - outHwInfo.pSysInfo->EuCountPerPoolMin), outHwInfo.pSysInfo->EuCountPerPoolMax);

    ReleaseOutHwInfoStructs();

    drm->StoredSSVal = 2;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ(3u, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.pSysInfo->EUCount - outHwInfo.pSysInfo->EuCountPerPoolMin), outHwInfo.pSysInfo->EuCountPerPoolMax);
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, configureHwInfoWaFlags) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->StoredDeviceRevID = 0;
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads);

    ReleaseOutHwInfoStructs();
}

BXTTEST_F(HwInfoConfigTestLinuxBxt, whenCallAdjustPlatformThenDoNothing) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    hwInfoConfig->adjustPlatformForProductFamily(&testHwInfo);

    int ret = memcmp(testHwInfo.pPlatform, pInHwInfo->pPlatform, sizeof(PLATFORM));
    EXPECT_EQ(0, ret);
}

template <typename T>
class BxtHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<BXT_1x2x6, BXT_1x3x6> bxtTestTypes;
TYPED_TEST_CASE(BxtHwInfoTests, bxtTestTypes);
TYPED_TEST(BxtHwInfoTests, gtSetupIsCorrect) {
    GT_SYSTEM_INFO gtSystemInfo;
    FeatureTable featureTable;
    memset(&gtSystemInfo, 0, sizeof(gtSystemInfo));
    TypeParam::setupHardwareInfo(&gtSystemInfo, &featureTable, false);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
