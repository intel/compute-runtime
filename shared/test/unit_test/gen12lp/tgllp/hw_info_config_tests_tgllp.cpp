/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using HwInfoConfigTestTgllp = ::testing::Test;

TGLLPTEST_F(HwInfoConfigTestTgllp, givenHwInfoConfigWhenGettingEvictWhenNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isEvictionWhenNecessaryFlagSupported());
}
