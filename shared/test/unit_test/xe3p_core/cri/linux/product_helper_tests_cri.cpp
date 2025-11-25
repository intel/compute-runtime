/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

struct CriProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMockExtended(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

CRITEST_F(CriProductHelperLinux, WhenConfiguringHwInfoThenZeroIsReturned) {

    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

CRITEST_F(CriProductHelperLinux, given57bAddressSpaceWhenConfiguringHwInfoThenSetFtrFlag) {

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

CRITEST_F(CriProductHelperLinux, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isVmBindPatIndexProgrammingSupported());
}

CRITEST_F(CriProductHelperLinux, givenProductHelperWhenAskedIsPageFaultSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isPageFaultSupported());
}

CRITEST_F(CriProductHelperLinux, givenProductHelperAndEnableRecoverablePageFaultsDebugFlagWhenAskedIsPageFaultSupportedThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableRecoverablePageFaults.set(0);
    EXPECT_FALSE(productHelper->isPageFaultSupported());
    debugManager.flags.EnableRecoverablePageFaults.set(1);
    EXPECT_TRUE(productHelper->isPageFaultSupported());
}

CRITEST_F(CriProductHelperLinux, givenProductHelperWhenAskedGetSharedSystemPatIndexThenReturnCorrectValue) {
    EXPECT_EQ(0ull, productHelper->getSharedSystemPatIndex());
}

CRITEST_F(CriProductHelperLinux, givenProductHelperWhenAskedUseSharedSystemUsmThenReturnCorrectValue) {
    EXPECT_FALSE(productHelper->useSharedSystemUsm());
}

using CriHwInfoLinux = ::testing::Test;

CRITEST_F(CriHwInfoLinux, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &CriHwConfig::hwInfo, &CriHwConfig::setupHardwareInfo};
    drm.overrideDeviceDescriptor = &device;

    int ret = drm.setupHardwareInfo(0, false);

    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
}
