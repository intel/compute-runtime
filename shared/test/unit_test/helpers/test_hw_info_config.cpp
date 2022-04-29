/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using HwInfoConfigTest = Test<DeviceFixture>;

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsAdjustProgrammableIdPreferredSlmSizeRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isAdjustProgrammableIdPreferredSlmSizeRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsComputeDispatchAllWalkerEnableInCfeStateRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isComputeDispatchAllWalkerEnableInCfeStateRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsComputeDispatchAllWalkerEnableInComputeWalkerRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isComputeDispatchAllWalkerEnableInComputeWalkerRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsGlobalFenceInCommandStreamRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isGlobalFenceInCommandStreamRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsSpecialPipelineSelectModeChangedThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isSpecialPipelineSelectModeChanged(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsSystolicModeConfigurabledThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isSystolicModeConfigurable(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenGetThreadEuRatioForScratchThen8IsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(8u, hwInfoConfig.getThreadEuRatioForScratch(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenGetProductConfigThenCorrectMatchIsFound) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_NE(hwInfoConfig.getProductConfigFromHwInfo(*defaultHwInfo), UNKNOWN_ISA);
}

HWTEST_F(HwInfoConfigTest, whenIsGrfNumReportedWithScmIsQueriedThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isGrfNumReportedWithScm());
}

HWTEST_F(HwInfoConfigTest, givenForceGrfNumProgrammingWithScmFlagSetWhenIsGrfNumReportedWithScmIsQueriedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(0);
    EXPECT_FALSE(hwInfoConfig.isGrfNumReportedWithScm());

    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(1);
    EXPECT_TRUE(hwInfoConfig.isGrfNumReportedWithScm());
}
