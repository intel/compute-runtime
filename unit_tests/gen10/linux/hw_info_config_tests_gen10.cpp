/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/helpers/gtest_helpers.h"
#include "unit_tests/os_interface/linux/hw_info_config_linux_tests.h"

using namespace OCLRT;
using namespace std;

struct HwInfoConfigTestLinuxCnl : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm->StoredDeviceID = ICNL_5x8_SUPERSKU_DEVICE_F0_ID;
        drm->setGtType(GTTYPE_GT2);
        drm->StoredSSVal = 3;
    }
};

CNLTEST_F(HwInfoConfigTestLinuxCnl, configureHwInfo) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.pSysInfo->SliceCount);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT2, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    //constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGpGpuMidBatchPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGpGpuThreadGroupLevelPreempt);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGpGpuMidThreadLevelPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftr3dMidBatchPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftr3dObjectLevelPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftr3dMidBatchPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGpGpuThreadGroupLevelPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPerCtxtPreemptionGranularityControl);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPPGTT);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrSVM);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrL3IACoherency);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrIA32eGfxPTEs);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrStandardMipTailFormat);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrDisplayYTiling);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrTranslationTable);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrUserModeTranslationTable);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrTileMappedResource);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrEnableGuC);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbc);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbc2AddressTranslation);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbcBlitterTracking);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbcCpuTracking);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrAstcHdr2D);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrAstcLdr2D);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = ICNL_3x8_ULT_DEVICE_F0_ID;
    drm->StoredSSVal = 4;
    drm->setGtType(GTTYPE_GT1);
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(2u, outHwInfo.pSysInfo->SliceCount);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT1, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = ICNL_3x8_ULT_DEVICE_F0_ID;
    drm->StoredSSVal = 6;
    drm->setGtType(GTTYPE_GT2_5);
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(3u, outHwInfo.pSysInfo->SliceCount);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT2_5, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);

    ReleaseOutHwInfoStructs();
}

CNLTEST_F(HwInfoConfigTestLinuxCnl, negative) {
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

CNLTEST_F(HwInfoConfigTestLinuxCnl, configureHwInfoWaFlags) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->StoredDeviceRevID = 0;
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waFbcLinearSurfaceStride);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waEncryptedEdramOnlyPartials);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceRevID = 1;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waFbcLinearSurfaceStride);
    EXPECT_EQ(0u, outHwInfo.pWaTable->waEncryptedEdramOnlyPartials);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceRevID = 2;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, outHwInfo.pWaTable->waFbcLinearSurfaceStride);
    EXPECT_EQ(0u, outHwInfo.pWaTable->waEncryptedEdramOnlyPartials);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads);
}

CNLTEST_F(HwInfoConfigTestLinuxCnl, configureHwInfoIsL3HashModeEnabled) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    uint64_t oldL3CacheSizeInKb = testSysInfo.L3CacheSizeInKb;
    uint32_t oldL3BankCount = testSysInfo.L3BankCount;

    testSysInfo.IsL3HashModeEnabled = true;

    drm->StoredSSVal = 9;
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((oldL3BankCount - 1), outHwInfo.pSysInfo->L3BankCount);
    EXPECT_EQ_VAL((oldL3CacheSizeInKb - 256), outHwInfo.pSysInfo->L3CacheSizeInKb);
}

template <typename T>
class CnlHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<CNL_2x5x8, CNL_2x4x8, CNL_1x3x8, CNL_1x2x8, CNL_4x9x8> cnlTestTypes;
TYPED_TEST_CASE(CnlHwInfoTests, cnlTestTypes);
TYPED_TEST(CnlHwInfoTests, gtSetupIsCorrect) {
    GT_SYSTEM_INFO gtSystemInfo;
    memset(&gtSystemInfo, 0, sizeof(gtSystemInfo));
    TypeParam::setupGtSystemInfo(&gtSystemInfo);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
