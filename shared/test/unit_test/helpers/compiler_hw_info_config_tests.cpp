/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using CompilerHwInfoConfigFixture = Test<DeviceFixture>;

HWTEST_F(CompilerHwInfoConfigFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(hwInfo, false);
    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_FALSE(compilerHwInfoConfig->isMidThreadPreemptionSupported(hwInfo));
    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(hwInfo, true);
    EXPECT_TRUE(compilerHwInfoConfig->isMidThreadPreemptionSupported(hwInfo));
}

using IsBeforeXeHpc = IsBeforeGfxCore<IGFX_XE_HPC_CORE>;

HWTEST2_F(CompilerHwInfoConfigFixture, GivenProductBeforeXeHpcWhenIsForceToStatelessRequiredThenFalseIsReturned, IsBeforeXeHpc) {
    auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(productFamily);
    EXPECT_FALSE(compilerHwInfoConfig.isForceToStatelessRequired());
}

using IsAtLeastXeHpc = IsAtLeastGfxCore<IGFX_XE_HPC_CORE>;

HWTEST2_F(CompilerHwInfoConfigFixture, GivenXeHpcAndLaterWhenIsForceToStatelessRequiredThenCorrectResultIsReturned, IsAtLeastXeHpc) {
    DebugManagerStateRestore restorer;
    auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(productFamily);
    EXPECT_TRUE(compilerHwInfoConfig.isForceToStatelessRequired());

    DebugManager.flags.DisableForceToStateless.set(false);
    EXPECT_TRUE(compilerHwInfoConfig.isForceToStatelessRequired());

    DebugManager.flags.DisableForceToStateless.set(true);
    EXPECT_FALSE(compilerHwInfoConfig.isForceToStatelessRequired());
}
