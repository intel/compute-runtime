/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/xe_hpg_core/hw_cmds_arl.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

using namespace NEO;

struct ArlProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMockExtended(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

ARLTEST_F(ArlProductHelperLinux, configureHwInfo) {
    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

ARLTEST_F(ArlProductHelperLinux, GivenArlWhenConfigureHardwareCustomThenKmdNotifyIsEnabled) {
    OSInterface osIface;
    productHelper->configureHardwareCustom(&pInHwInfo, &osIface);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(150ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(28000ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

ARLTEST_F(ArlProductHelperLinux, givenArlWhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isBlitterForImagesSupported());
}

ARLTEST_F(ArlProductHelperLinux, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isVmBindPatIndexProgrammingSupported());
}

ARLTEST_F(ArlProductHelperLinux, givenProductHelperWhenAskedIsKmdMigrationSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isKmdMigrationSupported());
}

ARLTEST_F(ArlProductHelperLinux, givenProductHelperWhenAskedIsDisableScratchPagesSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isDisableScratchPagesSupported());
}

ARLTEST_F(ArlProductHelperLinux, givenProductHelperWhenAskedIsPageFaultSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isPageFaultSupported());
}

using ArlHwInfoLinux = ::testing::Test;
ARLTEST_F(ArlHwInfoLinux, whenSetupHardwareInfoThenGtSetupIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo;

    gtSystemInfo = {};

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &ArlHwConfig::hwInfo, &ArlHwConfig::setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MaxSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MaxSubSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MaxEuPerSubSlice, 0u);
}

ARLTEST_F(ArlProductHelperLinux, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned) {
    uint64_t patIndex = 1u;
    bool isUncached = true;
    EXPECT_EQ(0u, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::buffer));
    EXPECT_EQ(3u, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::commandBuffer));

    isUncached = false;
    EXPECT_EQ(0u, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::buffer));
    EXPECT_EQ(3u, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::commandBuffer));
}

ARLTEST_F(ArlProductHelperLinux, givenProductHelperThenCompressionIsForbidden) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isCompressionForbidden(hwInfo));
}
