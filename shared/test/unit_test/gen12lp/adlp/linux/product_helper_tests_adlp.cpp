/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adlp.h"
#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/helpers/compiler_product_helper.h"
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
    ADLP::setupHardwareInfoBase(&hwInfo, false, nullptr);

    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

template <typename T>
using AdlpHwInfoLinux = ::testing::Test;
using adlpConfigTestTypes = ::testing::Types<AdlpHwConfig>;
TYPED_TEST_SUITE(AdlpHwInfoLinux, adlpConfigTestTypes);

TYPED_TEST(AdlpHwInfoLinux, givenSliceCountZeroWhenSetupHardwareInfoThenNotZeroValuesSetInGtSystemInfo) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo = {0};

    TypeParam::setupHardwareInfo(&hwInfo, false, nullptr);

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
