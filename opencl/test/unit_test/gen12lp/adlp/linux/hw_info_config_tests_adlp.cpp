/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "opencl/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxAdlp : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

        drm->storedDeviceID = IGFX_ALDERLAKE_P;
        drm->setGtType(GTTYPE_GT2);
    }
};

ADLPTEST_F(HwInfoConfigTestLinuxAdlp, WhenConfiguringHwInfoThenInfoIsSetCorrectly) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(static_cast<unsigned short>(drm->storedDeviceID), outHwInfo.platform.usDeviceID);
    EXPECT_EQ(static_cast<unsigned short>(drm->storedDeviceRevID), outHwInfo.platform.usRevId);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedEUVal), outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSSVal), outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);

    EXPECT_EQ(GTTYPE_GT2, outHwInfo.platform.eGTType);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGT1);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGT1_5);
    EXPECT_TRUE(outHwInfo.featureTable.ftrGT2);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGT3);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGT4);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGTA);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGTC);
    EXPECT_FALSE(outHwInfo.featureTable.ftrGTX);
    EXPECT_FALSE(outHwInfo.featureTable.ftrTileY);
}

ADLPTEST_F(HwInfoConfigTestLinuxAdlp, GivenInvalidDeviceIdWhenConfiguringHwInfoThenErrorIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->storedRetValForDeviceID = -1;
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    drm->storedRetValForDeviceID = 0;
    drm->storedRetValForDeviceRevID = -1;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    drm->storedRetValForDeviceRevID = 0;
    drm->failRetTopology = true;
    drm->storedRetValForEUVal = -1;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    drm->storedRetValForEUVal = 0;
    drm->storedRetValForSSVal = -1;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

template <typename T>
using AdlpConfigHwInfoTests = ::testing::Test;
using adlpConfigTestTypes = ::testing::Types<ADLP_CONFIG>;
TYPED_TEST_CASE(AdlpConfigHwInfoTests, adlpConfigTestTypes);
TYPED_TEST(AdlpConfigHwInfoTests, givenAdlpConfigWhenSetupHardwareInfoThenGtSystemInfoAndWaAndFtrTablesAreSetCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    auto &featureTable = hwInfo.featureTable;
    auto &workaroundTable = hwInfo.workaroundTable;
    DeviceDescriptor device = {0, &hwInfo, &TypeParam::setupHardwareInfo, GTTYPE_GT1};

    int ret = drm.setupHardwareInfo(&device, false);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);

    EXPECT_FALSE(featureTable.ftrL3IACoherency);
    EXPECT_FALSE(featureTable.ftrPPGTT);
    EXPECT_FALSE(featureTable.ftrSVM);
    EXPECT_FALSE(featureTable.ftrIA32eGfxPTEs);
    EXPECT_FALSE(featureTable.ftrStandardMipTailFormat);
    EXPECT_FALSE(featureTable.ftrTranslationTable);
    EXPECT_FALSE(featureTable.ftrUserModeTranslationTable);
    EXPECT_FALSE(featureTable.ftrTileMappedResource);
    EXPECT_FALSE(featureTable.ftrEnableGuC);
    EXPECT_FALSE(featureTable.ftrFbc);
    EXPECT_FALSE(featureTable.ftrFbc2AddressTranslation);
    EXPECT_FALSE(featureTable.ftrFbcBlitterTracking);
    EXPECT_FALSE(featureTable.ftrFbcCpuTracking);
    EXPECT_FALSE(featureTable.ftrTileY);
    EXPECT_FALSE(featureTable.ftrAstcHdr2D);
    EXPECT_FALSE(featureTable.ftrAstcLdr2D);
    EXPECT_FALSE(featureTable.ftr3dMidBatchPreempt);
    EXPECT_FALSE(featureTable.ftrGpGpuMidBatchPreempt);
    EXPECT_FALSE(featureTable.ftrGpGpuThreadGroupLevelPreempt);
    EXPECT_FALSE(featureTable.ftrPerCtxtPreemptionGranularityControl);

    EXPECT_FALSE(workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
    EXPECT_FALSE(workaroundTable.waEnablePreemptionGranularityControlByUMD);
    EXPECT_FALSE(workaroundTable.waUntypedBufferCompression);

    ret = drm.setupHardwareInfo(&device, true);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);

    EXPECT_TRUE(featureTable.ftrL3IACoherency);
    EXPECT_TRUE(featureTable.ftrPPGTT);
    EXPECT_TRUE(featureTable.ftrSVM);
    EXPECT_TRUE(featureTable.ftrIA32eGfxPTEs);
    EXPECT_TRUE(featureTable.ftrStandardMipTailFormat);
    EXPECT_TRUE(featureTable.ftrTranslationTable);
    EXPECT_TRUE(featureTable.ftrUserModeTranslationTable);
    EXPECT_TRUE(featureTable.ftrTileMappedResource);
    EXPECT_TRUE(featureTable.ftrEnableGuC);
    EXPECT_TRUE(featureTable.ftrFbc);
    EXPECT_TRUE(featureTable.ftrFbc2AddressTranslation);
    EXPECT_TRUE(featureTable.ftrFbcBlitterTracking);
    EXPECT_TRUE(featureTable.ftrFbcCpuTracking);
    EXPECT_FALSE(featureTable.ftrTileY);
    EXPECT_TRUE(featureTable.ftrAstcHdr2D);
    EXPECT_TRUE(featureTable.ftrAstcLdr2D);
    EXPECT_TRUE(featureTable.ftr3dMidBatchPreempt);
    EXPECT_TRUE(featureTable.ftrGpGpuMidBatchPreempt);
    EXPECT_TRUE(featureTable.ftrGpGpuThreadGroupLevelPreempt);
    EXPECT_TRUE(featureTable.ftrPerCtxtPreemptionGranularityControl);

    EXPECT_TRUE(workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
    EXPECT_TRUE(workaroundTable.waEnablePreemptionGranularityControlByUMD);
    EXPECT_TRUE(workaroundTable.waUntypedBufferCompression);
}

TYPED_TEST(AdlpConfigHwInfoTests, givenSliceCountZeroWhenSetupHardwareInfoThenNotZeroValuesSetInGtSystemInfo) {
    HardwareInfo hwInfo = {};
    hwInfo.gtSystemInfo = {0};

    TypeParam::setupHardwareInfo(&hwInfo, false);

    EXPECT_NE(0u, hwInfo.gtSystemInfo.SliceCount);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.EUCount);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.MaxEuPerSubSlice);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.MaxSlicesSupported);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_TRUE(hwInfo.gtSystemInfo.CCSInfo.IsValid);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
}
