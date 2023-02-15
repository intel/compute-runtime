/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adlp.h"
#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

using namespace NEO;

struct AdlpProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

ADLPTEST_F(AdlpProductHelperLinux, WhenConfiguringHwInfoThenInfoIsSetCorrectly) {

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());

    EXPECT_EQ(0, ret);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedEUVal), outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSSVal), outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);

    EXPECT_FALSE(outHwInfo.featureTable.flags.ftrTileY);
}

ADLPTEST_F(AdlpProductHelperLinux, GivenInvalidDeviceIdWhenConfiguringHwInfoThenErrorIsReturned) {

    drm->failRetTopology = true;
    drm->storedRetValForEUVal = -1;
    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());

    EXPECT_EQ(-1, ret);

    drm->storedRetValForEUVal = 0;
    drm->storedRetValForSSVal = -1;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);
}

ADLPTEST_F(AdlpProductHelperLinux, givenAdlpConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    ADLP::setupHardwareInfoBase(&hwInfo, false);

    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

template <typename T>
using AdlpHwInfoLinux = ::testing::Test;
using adlpConfigTestTypes = ::testing::Types<AdlpHwConfig>;
TYPED_TEST_CASE(AdlpHwInfoLinux, adlpConfigTestTypes);
TYPED_TEST(AdlpHwInfoLinux, givenAdlpConfigWhenSetupHardwareInfoThenGtSystemInfoAndWaAndFtrTablesAreSetCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &TypeParam::hwInfo, &TypeParam::setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    {
        const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;
        const auto &featureTable = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->featureTable;
        const auto &workaroundTable = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->workaroundTable;

        EXPECT_EQ(ret, 0);
        EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
        EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
        EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);

        EXPECT_FALSE(featureTable.flags.ftrL3IACoherency);
        EXPECT_FALSE(featureTable.flags.ftrPPGTT);
        EXPECT_FALSE(featureTable.flags.ftrSVM);
        EXPECT_FALSE(featureTable.flags.ftrIA32eGfxPTEs);
        EXPECT_FALSE(featureTable.flags.ftrStandardMipTailFormat);
        EXPECT_FALSE(featureTable.flags.ftrTranslationTable);
        EXPECT_FALSE(featureTable.flags.ftrUserModeTranslationTable);
        EXPECT_FALSE(featureTable.flags.ftrTileMappedResource);
        EXPECT_FALSE(featureTable.flags.ftrFbc);
        EXPECT_FALSE(featureTable.flags.ftrTileY);
        EXPECT_FALSE(featureTable.flags.ftrAstcHdr2D);
        EXPECT_FALSE(featureTable.flags.ftrAstcLdr2D);

        EXPECT_FALSE(featureTable.flags.ftrGpGpuMidBatchPreempt);
        EXPECT_FALSE(featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);

        EXPECT_FALSE(workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_FALSE(workaroundTable.flags.waUntypedBufferCompression);
    }
    ret = drm.setupHardwareInfo(&device, true);
    {
        const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;
        const auto &featureTable = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->featureTable;
        const auto &workaroundTable = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->workaroundTable;

        EXPECT_EQ(ret, 0);
        EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
        EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
        EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);

        EXPECT_TRUE(featureTable.flags.ftrL3IACoherency);
        EXPECT_TRUE(featureTable.flags.ftrPPGTT);
        EXPECT_TRUE(featureTable.flags.ftrSVM);
        EXPECT_TRUE(featureTable.flags.ftrIA32eGfxPTEs);
        EXPECT_TRUE(featureTable.flags.ftrStandardMipTailFormat);
        EXPECT_TRUE(featureTable.flags.ftrTranslationTable);
        EXPECT_TRUE(featureTable.flags.ftrUserModeTranslationTable);
        EXPECT_TRUE(featureTable.flags.ftrTileMappedResource);
        EXPECT_TRUE(featureTable.flags.ftrFbc);
        EXPECT_FALSE(featureTable.flags.ftrTileY);
        EXPECT_TRUE(featureTable.flags.ftrAstcHdr2D);
        EXPECT_TRUE(featureTable.flags.ftrAstcLdr2D);

        EXPECT_TRUE(featureTable.flags.ftrGpGpuMidBatchPreempt);
        EXPECT_TRUE(featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);

        EXPECT_TRUE(workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_TRUE(workaroundTable.flags.waUntypedBufferCompression);
    }
}

TYPED_TEST(AdlpHwInfoLinux, givenSliceCountZeroWhenSetupHardwareInfoThenNotZeroValuesSetInGtSystemInfo) {
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
