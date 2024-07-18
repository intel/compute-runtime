/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

using namespace NEO;

struct RklProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

RKLTEST_F(RklProductHelperLinux, WhenConfiguringHwInfoThenConfigIsCorrect) {

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);

    EXPECT_FALSE(outHwInfo.featureTable.flags.ftrTileY);
}

RKLTEST_F(RklProductHelperLinux, GivenIncorrectDataWhenConfiguringHwInfoThenErrorIsReturned) {

    drm->failRetTopology = true;
    drm->storedRetValForEUVal = -1;

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);

    drm->storedRetValForEUVal = 0;
    drm->storedRetValForSSVal = -1;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);
}

using RklHwInfoLinux = ::testing::Test;

RKLTEST_F(RklHwInfoLinux, WhenSettingUpHwInfoThenConfigIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &hwInfo, &RklHwConfig::setupHardwareInfo};

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
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
}
