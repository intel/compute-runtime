/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/gtest_helpers.h"
#include "shared/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxLkf : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm->storedSSVal = 8;
    }
};

LKFTEST_F(HwInfoConfigTestLinuxLkf, configureHwInfoLkf) {
    auto hwInfoConfig = HwInfoConfigHw<IGFX_LAKEFIELD>::get();
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);

    EXPECT_FALSE(outHwInfo.featureTable.flags.ftrTileY);
}

LKFTEST_F(HwInfoConfigTestLinuxLkf, negative) {
    auto hwInfoConfig = HwInfoConfigHw<IGFX_LAKEFIELD>::get();

    drm->failRetTopology = true;
    drm->storedRetValForEUVal = -1;
    auto ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);

    drm->storedRetValForEUVal = 0;
    drm->storedRetValForSSVal = -1;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

template <typename T>
class LkfHwInfoTests : public ::testing::Test {};
typedef ::testing::Types<LkfHw1x8x8> lkfTestTypes;
TYPED_TEST_CASE(LkfHwInfoTests, lkfTestTypes);
TYPED_TEST(LkfHwInfoTests, gtSetupIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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
