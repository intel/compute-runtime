/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

struct NvlProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMockExtended(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

NVLPTEST_F(NvlProductHelperLinux, WhenConfiguringHwInfoThenZeroIsReturned) {
    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

NVLPTEST_F(NvlProductHelperLinux, given57bAddressSpaceWhenConfiguringHwInfoThenSetFtrFlag) {
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

NVLPTEST_F(NvlProductHelperLinux, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isVmBindPatIndexProgrammingSupported());
}

NVLPTEST_F(NvlProductHelperLinux, givenProductHelperWhenAskedIsPageFaultSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isPageFaultSupported());
}

NVLPTEST_F(NvlProductHelperLinux, givenProductHelperWhenAskedIsKmdMigrationSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isKmdMigrationSupported());
}

NVLPTEST_F(NvlProductHelperLinux, givenProductHelperWhenAskedGetSharedSystemPatIndexThenReturnCorrectValue) {
    EXPECT_EQ(1ull, productHelper->getSharedSystemPatIndex());
}

NVLPTEST_F(NvlProductHelperLinux, givenProductHelperWhenAskedUseSharedSystemUsmThenReturnCorrectValue) {
    EXPECT_FALSE(productHelper->useSharedSystemUsm());
}

using NvlHwInfoLinux = ::testing::Test;

NVLPTEST_F(NvlHwInfoLinux, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo};
    drm.overrideDeviceDescriptor = &device;

    int ret = drm.setupHardwareInfo(0, false);

    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
}
