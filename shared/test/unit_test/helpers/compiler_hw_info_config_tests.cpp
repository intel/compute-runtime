/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

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
