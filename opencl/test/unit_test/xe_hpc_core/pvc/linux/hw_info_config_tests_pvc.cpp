/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxPvc : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

        drm->storedDeviceID = 0x0BD0;
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
