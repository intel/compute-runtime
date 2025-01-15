/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "hw_cmds_xe3_core.h"

using namespace NEO;

struct Xe3CoreProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMockExtended(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

XE3_CORETEST_F(Xe3CoreProductHelperLinux, WhenConfiguringHwInfoThenZeroIsReturned) {
    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

XE3_CORETEST_F(Xe3CoreProductHelperLinux, given57bAddressSpaceWhenConfiguringHwInfoThenSetFtrFlag) {
    outHwInfo.featureTable.flags.ftr57bGPUAddressing = false;
    outHwInfo.platform.eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

    outHwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48);
    int ret = productHelper->configureHardwareCustom(&outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.featureTable.flags.ftr57bGPUAddressing);

    outHwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(57);
    ret = productHelper->configureHardwareCustom(&outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.featureTable.flags.ftr57bGPUAddressing);
}

XE3_CORETEST_F(Xe3CoreProductHelperLinux, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isVmBindPatIndexProgrammingSupported());
}

XE3_CORETEST_F(Xe3CoreProductHelperLinux, givenProductHelperWhenAskedIsPageFaultSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isPageFaultSupported());
}

XE3_CORETEST_F(Xe3CoreProductHelperLinux, givenProductHelperWhenAskedIsKmdMigrationSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isKmdMigrationSupported());
}

XE3_CORETEST_F(Xe3CoreProductHelperLinux, givenProductHelperWhenAskedIsDisableScratchPagesSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isDisableScratchPagesSupported());
}
