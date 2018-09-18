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

struct HwInfoConfigTestLinuxGlk : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm->StoredDeviceID = IGLK_GT2_ULT_12EU_DEVICE_F0_ID;
        drm->setGtType(GTTYPE_GTA);
        drm->StoredEUVal = 18;
        drm->StoredHasPooledEU = 1;
        drm->StoredMinEUinPool = 3;
    }
};

GLKTEST_F(HwInfoConfigTestLinuxGlk, configureHwInfo) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GTA, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    //constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.pSysInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled);
    EXPECT_TRUE(outHwInfo.pSysInfo->VEBoxInfo.IsValid);
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
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrTranslationTable);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrUserModeTranslationTable);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrEnableGuC);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrTileMappedResource);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrULT);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrAstcHdr2D);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrAstcLdr2D);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = IGLK_GT2_ULT_18EU_DEVICE_F0_ID;
    drm->setGtType(GTTYPE_GTC); //IGLK_GT2_ULT_18EU_DEVICE_F0_ID is GTA, but fot test sake make it GTC
    drm->StoredMinEUinPool = 6;
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

    drm->StoredDeviceID = IGLK_GT2_ULT_12EU_DEVICE_F0_ID;
    drm->setGtType(GTTYPE_GTX); //IGLK_GT2_ULT_18EU_DEVICE_F0_ID is GTA, but fot test sake make it GTX
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

GLKTEST_F(HwInfoConfigTestLinuxGlk, negative) {
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

    ReleaseOutHwInfoStructs();
}

GLKTEST_F(HwInfoConfigTestLinuxGlk, configureHwInfoFailingEnabledPool) {
    drm->StoredRetValForPooledEU = -1;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.pSysInfo->EuCountPerPoolMax);
}

GLKTEST_F(HwInfoConfigTestLinuxGlk, configureHwInfoDisabledEnabledPool) {
    drm->StoredHasPooledEU = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.pSysInfo->EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.pSysInfo->EuCountPerPoolMax);
}

GLKTEST_F(HwInfoConfigTestLinuxGlk, configureHwInfoFailingMinEuInPool) {
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

GLKTEST_F(HwInfoConfigTestLinuxGlk, configureHwInfoInvalidMinEuInPool) {
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

GLKTEST_F(HwInfoConfigTestLinuxGlk, configureHwInfoWaFlags) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->StoredDeviceRevID = 0;
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads);

    ReleaseOutHwInfoStructs();
}

GLKTEST_F(HwInfoConfigTestLinuxGlk, whenCallAdjustPlatformThenDoNothing) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    hwInfoConfig->adjustPlatformForProductFamily(&testHwInfo);

    int ret = memcmp(testHwInfo.pPlatform, pInHwInfo->pPlatform, sizeof(PLATFORM));
    EXPECT_EQ(0, ret);
}

template <typename T>
class GlkHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<GLK_1x3x6, GLK_1x2x6> glkTestTypes;
TYPED_TEST_CASE(GlkHwInfoTests, glkTestTypes);
TYPED_TEST(GlkHwInfoTests, gtSetupIsCorrect) {
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
