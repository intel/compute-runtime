/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

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
    EXPECT_EQ(20ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

MTLTEST_F(MtlProductHelperLinux, givenMtlWhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isBlitterForImagesSupported());
}

MTLTEST_F(MtlProductHelperLinux, givenMtlWhenisVmBindPatIndexProgrammingSupportedIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isVmBindPatIndexProgrammingSupported());
}
