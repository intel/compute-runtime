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

DG1TEST_F(Dg1ProductHelperLinux, GivenDG1WhenConfigureHardwareCustomThenKmdNotifyIsEnabled) {
    HardwareInfo hardwareInfo = *defaultHwInfo;

    OSInterface osIface;
    productHelper->configureHardwareCustom(&hardwareInfo, &osIface);
    EXPECT_TRUE(hardwareInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(300ll, hardwareInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
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
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
}
