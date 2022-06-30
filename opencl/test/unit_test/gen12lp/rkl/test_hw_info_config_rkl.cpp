/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include <array>

using namespace NEO;

using RklHwInfoConfig = ::testing::Test;

RKLTEST_F(RklHwInfoConfig, givenA0OrBSteppingAndRklPlatformWhenAskingIfWAIsRequiredThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    std::array<std::pair<uint32_t, bool>, 3> revisions = {
        {{REVISION_A0, true},
         {REVISION_B, true},
         {REVISION_C, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = hwInfoConfig->getHwRevIdFromStepping(revision, hwInfo);

        hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, hwInfoConfig->isForceEmuInt32DivRemSPWARequired(hwInfo));
    }
}

RKLTEST_F(RklHwInfoConfig, givenHwInfoConfigWhenAskedIf3DPipelineSelectWAIsRequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.is3DPipelineSelectWARequired());
}

using CompilerHwInfoConfigHelperTestsRkl = ::testing::Test;
RKLTEST_F(CompilerHwInfoConfigHelperTestsRkl, givenRklWhenIsForceEmuInt32DivRemSPRequiredIsCalledThenReturnsTrue) {
    EXPECT_TRUE(CompilerHwInfoConfig::get(productFamily)->isForceEmuInt32DivRemSPRequired());
}
