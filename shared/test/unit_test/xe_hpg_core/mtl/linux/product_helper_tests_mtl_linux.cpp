/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

using namespace NEO;

struct MtlProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

MTLTEST_F(MtlProductHelperLinux, WhenConfiguringHwInfoThenZeroIsReturned) {
    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

MTLTEST_F(MtlProductHelperLinux, GivenMtlWhenConfigureHardwareCustomThenKmdNotifyIsEnabled) {
    OSInterface osIface;
    productHelper->configureHardwareCustom(&pInHwInfo, &osIface);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(150ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(28000ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

MTLTEST_F(MtlProductHelperLinux, givenMtlWhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isBlitterForImagesSupported());
}

MTLTEST_F(MtlProductHelperLinux, givenMtlWhenisVmBindPatIndexProgrammingSupportedIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isVmBindPatIndexProgrammingSupported());
}

MTLTEST_F(MtlProductHelperLinux, givenProductHelperWhenAskedIsPageFaultSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isPageFaultSupported());
}

MTLTEST_F(MtlProductHelperLinux, givenProductHelperWhenAskedIsKmdMigrationsSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isKmdMigrationSupported());
}

MTLTEST_F(MtlProductHelperLinux, givenProductHelperWhenAskedIsDisableScratchPagessSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isDisableScratchPagesSupported());
}

MTLTEST_F(MtlProductHelperLinux, whenCheckingIsTimestampWaitSupportedForEventsThenReturnTrue) {
    EXPECT_TRUE(productHelper->isTimestampWaitSupportedForEvents());
}

MTLTEST_F(MtlProductHelperLinux, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned) {
    uint64_t patIndex = 1u;
    bool isUncached = true;
    EXPECT_EQ(0u, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::buffer));
    EXPECT_EQ(3u, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::commandBuffer));

    isUncached = false;
    EXPECT_EQ(0u, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::buffer));
    EXPECT_EQ(3u, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::commandBuffer));
}

MTLTEST_F(MtlProductHelperLinux, givenProductHelperThenCompressionIsForbidden) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isCompressionForbidden(hwInfo));
}
