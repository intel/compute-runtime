/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

typedef Test<DeviceFixture> HwHelperTest;

HWTEST_F(HwHelperTest, GivenHwInfoWithEnabledBliterWhenCheckCopyEnginesCountThenReturnedOne) {
    HardwareInfo hwInfo{};
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    EXPECT_EQ(HwHelper::getCopyEnginesCount(hwInfo), 1u);
}

HWTEST_F(HwHelperTest, GivenHwInfoWithDisabledBliterWhenCheckCopyEnginesCountThenReturnedZero) {
    HardwareInfo hwInfo{};
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    EXPECT_EQ(HwHelper::getCopyEnginesCount(hwInfo), 0u);
}
