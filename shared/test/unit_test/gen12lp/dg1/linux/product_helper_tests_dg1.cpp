/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

using namespace NEO;

struct Dg1ProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

DG1TEST_F(Dg1ProductHelperLinux, GivenDG1WhenConfigureHardwareCustomThenMTPIsNotSet) {
    HardwareInfo hardwareInfo = *defaultHwInfo;

    OSInterface osIface;
    hardwareInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    PreemptionHelper::adjustDefaultPreemptionMode(hardwareInfo.capabilityTable, true, true, true);

    productHelper->configureHardwareCustom(&hardwareInfo, &osIface);
    EXPECT_FALSE(hardwareInfo.featureTable.flags.ftrGpGpuMidThreadLevelPreempt);
}

DG1TEST_F(Dg1ProductHelperLinux, GivenDG1WhenConfigureHardwareCustomThenKmdNotifyIsEnabled) {
    HardwareInfo hardwareInfo = *defaultHwInfo;

    OSInterface osIface;
    productHelper->configureHardwareCustom(&hardwareInfo, &osIface);
    EXPECT_TRUE(hardwareInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(300ll, hardwareInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
}

DG1TEST_F(Dg1ProductHelperLinux, WhenConfiguringHwInfoThenInfoIsSetCorrectly) {
    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedEUVal), outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSSVal), outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);

    EXPECT_FALSE(outHwInfo.featureTable.flags.ftrTileY);
}

DG1TEST_F(Dg1ProductHelperLinux, GivenInvalidDeviceIdWhenConfiguringHwInfoThenErrorIsReturned) {

    drm->failRetTopology = true;
    drm->storedRetValForEUVal = -1;
    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);

    drm->storedRetValForEUVal = 0;
    drm->storedRetValForSSVal = -1;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-1, ret);
}

template <typename T>
class Dg1HwInfoLinux : public ::testing::Test {};
using dg1TestTypes = ::testing::Types<Dg1HwConfig>;
TYPED_TEST_SUITE(Dg1HwInfoLinux, dg1TestTypes);
TYPED_TEST(Dg1HwInfoLinux, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &TypeParam::hwInfo, &TypeParam::setupHardwareInfo};
    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    int ret = drm.setupHardwareInfo(&device, false);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
}
