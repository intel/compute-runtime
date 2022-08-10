/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/xe_hpc_core/pvc/product_configs_pvc.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"
#include "shared/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxPvc : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

PVCTEST_F(HwInfoConfigTestLinuxPvc, WhenConfiguringHwInfoThenZeroIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
}

PVCTEST_F(HwInfoConfigTestLinuxPvc, given57bAddressSpaceWhenConfiguringHwInfoThenSetFtrFlag) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    outHwInfo.featureTable.flags.ftr57bGPUAddressing = false;
    outHwInfo.platform.eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

    outHwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48);
    int ret = hwInfoConfig->configureHardwareCustom(&outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.featureTable.flags.ftr57bGPUAddressing);

    outHwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(57);
    ret = hwInfoConfig->configureHardwareCustom(&outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    auto value = outHwInfo.featureTable.flags.ftr57bGPUAddressing;
    EXPECT_EQ(1u, value);
}

PVCTEST_F(HwInfoConfigTestLinuxPvc, GivenPvcWhenConfigureHardwareCustomThenKmdNotifyIsEnabled) {
    HwInfoConfig *hwInfoConfig = HwInfoConfig::get(productFamily);

    OSInterface osIface;
    hwInfoConfig->configureHardwareCustom(&pInHwInfo, &osIface);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(150ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(20ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfPatIndexProgrammingSupportedThenReturnFalse, IGFX_PVC);

PVCTEST_F(HwInfoConfigTestLinuxPvc, givenHwInfoConfigWhenAskedIfPatIndexProgrammingSupportedThenReturnTrue) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isVmBindPatIndexProgrammingSupported());
}

PVCTEST_F(ProductConfigTests, givenAotConfigWhenSetHwInfoRevisionIdForPvcThenCorrectValueIsSet) {
    for (const auto &config : AOT_PVC::productConfigs) {
        AheadOfTimeConfig aotConfig = {0};
        aotConfig.ProductConfig = config;
        CompilerHwInfoConfig::get(hwInfo.platform.eProductFamily)->setProductConfigForHwInfo(hwInfo, aotConfig);
        EXPECT_EQ(hwInfo.platform.usRevId, aotConfig.ProductConfigID.Revision);
    }
}

PVCTEST_F(HwInfoConfigTestLinuxPvc, givenOsInterfaceIsNullWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenReturnZero) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    EXPECT_EQ(0u, hwInfoConfig->getDeviceMemoryPhysicalSizeInBytes(nullptr, 0));
}

PVCTEST_F(HwInfoConfigTestLinuxPvc, givenOsInterfaceIsNullWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturnZero) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    auto testHwInfo = *defaultHwInfo;
    testHwInfo.platform.usRevId = 0x8;
    EXPECT_EQ(0u, hwInfoConfig->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, nullptr, 0));
}

PVCTEST_F(HwInfoConfigTestLinuxPvc, WhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenReturnSuccess) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    drm->setPciPath("device");
    drm->storedGetDeviceMemoryPhysicalSizeInBytesStatus = true;
    drm->useBaseGetDeviceMemoryPhysicalSizeInBytes = false;
    EXPECT_EQ(1024u, hwInfoConfig->getDeviceMemoryPhysicalSizeInBytes(osInterface, 0));
}

PVCTEST_F(HwInfoConfigTestLinuxPvc, WhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturnSuccess) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    auto testHwInfo = *defaultHwInfo;
    testHwInfo.platform.usRevId = 0x8;
    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;
    EXPECT_EQ(51200000000u, hwInfoConfig->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, osInterface, 0));
}
