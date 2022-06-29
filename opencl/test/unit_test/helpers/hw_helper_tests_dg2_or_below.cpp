/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using HwHelperDg2OrBelowTests = Test<ClDeviceFixture>;

using isDG2OrBelow = IsAtMostProduct<IGFX_DG2>;
HWTEST2_F(HwHelperDg2OrBelowTests, WhenGettingIsKmdMigrationSupportedThenFalseIsReturned, isDG2OrBelow) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.isKmdMigrationSupported(hardwareInfo));
}
