/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/xe_hpc_core/pvc/product_configs_pvc.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"
#include "shared/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct PvcProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

PVCTEST_F(PvcProductHelperLinux, WhenConfiguringHwInfoThenZeroIsReturned) {

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

PVCTEST_F(PvcProductHelperLinux, WhenGetSvmCpuAlignmentThenProperValueIsReturned) {
    EXPECT_EQ(MemoryConstants::pageSize64k, productHelper->getSvmCpuAlignment());
}

PVCTEST_F(PvcProductHelperLinux, given57bAddressSpaceWhenConfiguringHwInfoThenSetFtrFlag) {

    outHwInfo.featureTable.flags.ftr57bGPUAddressing = false;
    outHwInfo.platform.eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

    outHwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48);
    int ret = productHelper->configureHardwareCustom(&outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.featureTable.flags.ftr57bGPUAddressing);

    outHwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(57);
    ret = productHelper->configureHardwareCustom(&outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    auto value = outHwInfo.featureTable.flags.ftr57bGPUAddressing;
    EXPECT_EQ(1u, value);
}

PVCTEST_F(PvcProductHelperLinux, GivenPvcWhenConfigureHardwareCustomThenKmdNotifyIsEnabled) {

    OSInterface osIface;
    productHelper->configureHardwareCustom(&pInHwInfo, &osIface);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(150ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(20ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnFalse, IGFX_PVC);

PVCTEST_F(PvcProductHelperLinux, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnTrue) {

    EXPECT_TRUE(productHelper->isVmBindPatIndexProgrammingSupported());
}

PVCTEST_F(PvcProductHelperLinux, givenAotConfigWhenSetHwInfoRevisionIdForPvcThenCorrectValueIsSet) {
    for (const auto &config : AOT_PVC::productConfigs) {
        HardwareIpVersion aotConfig = {0};
        aotConfig.value = config;
        CompilerProductHelper::get(pInHwInfo.platform.eProductFamily)->setProductConfigForHwInfo(pInHwInfo, aotConfig);
        EXPECT_EQ(pInHwInfo.platform.usRevId, aotConfig.revision);
    }
}

PVCTEST_F(PvcProductHelperLinux, givenOsInterfaceIsNullWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenReturnZero) {
    EXPECT_EQ(0u, productHelper->getDeviceMemoryPhysicalSizeInBytes(nullptr, 0));
}

PVCTEST_F(PvcProductHelperLinux, givenProductHelperWhenAskedIsBlitSplitEnqueueWARequiredThenReturnTrue) {
    EXPECT_TRUE(productHelper->isBlitSplitEnqueueWARequired(pInHwInfo));
}

PVCTEST_F(PvcProductHelperLinux, givenOsInterfaceIsNullWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturnZero) {

    auto testHwInfo = *defaultHwInfo;
    testHwInfo.platform.usRevId = 0x8;
    EXPECT_EQ(0u, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, nullptr, 0));
}

PVCTEST_F(PvcProductHelperLinux, WhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenReturnSuccess) {

    drm->setPciPath("device");
    drm->storedGetDeviceMemoryPhysicalSizeInBytesStatus = true;
    drm->useBaseGetDeviceMemoryPhysicalSizeInBytes = false;
    EXPECT_EQ(1024u, productHelper->getDeviceMemoryPhysicalSizeInBytes(osInterface, 0));
}

PVCTEST_F(PvcProductHelperLinux, WhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturnSuccess) {

    auto testHwInfo = *defaultHwInfo;
    testHwInfo.platform.usRevId = 0x8;
    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;
    EXPECT_EQ(51200000000u, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, osInterface, 0));
}
