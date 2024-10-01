/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

using namespace NEO;

struct TgllpProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

template <typename T>
class TgllpHwInfoLinux : public ::testing::Test {};
typedef ::testing::Types<TgllpHw1x6x16> tgllpTestTypes;
TYPED_TEST_SUITE(TgllpHwInfoLinux, tgllpTestTypes);
TYPED_TEST(TgllpHwInfoLinux, gtSetupIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &TypeParam::hwInfo, &TypeParam::setupHardwareInfo};

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
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
}
