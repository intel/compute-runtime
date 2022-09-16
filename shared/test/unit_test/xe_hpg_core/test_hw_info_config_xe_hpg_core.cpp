/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds_xe_hpg_core_base.h"

using namespace NEO;

using TestXeHpgHwInfoConfig = Test<DeviceFixture>;

XE_HPG_CORETEST_F(TestXeHpgHwInfoConfig, givenHwInfoConfigWhenIsSystolicModeConfigurabledThenTrueIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isSystolicModeConfigurable(hwInfo));
}
