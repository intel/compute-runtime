/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

using namespace NEO;

struct Dg2ProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

DG2TEST_F(Dg2ProductHelperLinux, WhenConfiguringHwInfoThenZeroIsReturned) {

    auto ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

DG2TEST_F(Dg2ProductHelperLinux, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

DG2TEST_F(Dg2ProductHelperLinux, GivenDg2WhenConfigureHardwareCustomThenKmdNotifyIsEnabled) {

    OSInterface osIface;
    productHelper->configureHardwareCustom(&pInHwInfo, &osIface);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(150ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(pInHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(20ll, pInHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

DG2TEST_F(Dg2ProductHelperLinux, whenCheckIsTlbFlushRequiredThenReturnProperValue) {
    EXPECT_EQ(productHelper->isTlbFlushRequired(pInHwInfo, true), !DG2::isG10(pInHwInfo));
    EXPECT_FALSE(productHelper->isTlbFlushRequired(pInHwInfo, false));
}

DG2TEST_F(Dg2ProductHelperLinux, whenForceTlbFlushSetAndCheckIsTlbFlushRequiredThenReturnProperValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceTlbFlush.set(1);
    EXPECT_TRUE(productHelper->isTlbFlushRequired(pInHwInfo, false));
    EXPECT_TRUE(productHelper->isTlbFlushRequired(pInHwInfo, true));
    debugManager.flags.ForceTlbFlush.set(0);
    EXPECT_FALSE(productHelper->isTlbFlushRequired(pInHwInfo, false));
    EXPECT_FALSE(productHelper->isTlbFlushRequired(pInHwInfo, true));
}