/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "hw_cmds.h"

using namespace NEO;

struct LkfProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm->storedSSVal = 8;
    }
};

LKFTEST_F(LkfProductHelperLinux, GivenLkfThenHwInfoIsCorrect) {

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);

    EXPECT_FALSE(outHwInfo.featureTable.flags.ftrTileY);
}

LKFTEST_F(LkfProductHelperLinux, GivenInvalidDeviceIdWhenConfiguringHwInfoThenNegativeOneReturned) {

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());

    drm->failRetTopology = true;
    drm->storedRetValForEUVal = -1;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);

    drm->storedRetValForEUVal = 0;
    drm->storedRetValForSSVal = -1;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);
}

template <typename T>
class LkfHwInfoTests : public ::testing::Test {};
using lkfTestTypes = ::testing::Types<LkfHw1x8x8>;
TYPED_TEST_SUITE(LkfHwInfoTests, lkfTestTypes);
TYPED_TEST(LkfHwInfoTests, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &hwInfo, &TypeParam::setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);

    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
