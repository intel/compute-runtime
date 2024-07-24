/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "hw_cmds.h"

using namespace NEO;

struct BxtProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm->storedEUVal = 18;
        drm->storedHasPooledEU = 1;
        drm->storedMinEUinPool = 3;
    }
};

BXTTEST_F(BxtProductHelperLinux, WhenConfiguringHwInfoThenConfigIsCorrect) {
    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned int)drm->storedHasPooledEU, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->storedMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    // constant sysInfo/ftr flags
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VEBoxInfo.IsValid);

    pInHwInfo.platform.usDeviceID = 0x5A85;
    drm->storedMinEUinPool = 6;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned int)drm->storedHasPooledEU, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->storedMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    pInHwInfo.platform.usDeviceID = 0x5A85;
    drm->storedMinEUinPool = 9;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned int)drm->storedHasPooledEU, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ((uint32_t)drm->storedMinEUinPool, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ((outHwInfo.gtSystemInfo.EUCount - outHwInfo.gtSystemInfo.EuCountPerPoolMin), outHwInfo.gtSystemInfo.EuCountPerPoolMax);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    auto &outKmdNotifyProperties = outHwInfo.capabilityTable.kmdNotifyProperties;
    EXPECT_TRUE(outKmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(50000, outKmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(5000, outKmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(200000, outKmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}

BXTTEST_F(BxtProductHelperLinux, GivenDisabledEnabledPoolWhenConfiguringHwInfoThenZeroIsReturned) {
    drm->storedHasPooledEU = 0;

    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrPooledEuEnabled);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMin);
    EXPECT_EQ(0u, outHwInfo.gtSystemInfo.EuCountPerPoolMax);
}

template <typename T>
class BxtHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<BxtHw1x2x6, BxtHw1x3x6> bxtTestTypes;
TYPED_TEST_SUITE(BxtHwInfoTests, bxtTestTypes);
TYPED_TEST(BxtHwInfoTests, WhenConfiguringHwInfoThenConfigIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &TypeParam::hwInfo, &TypeParam::setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);

    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);
}
