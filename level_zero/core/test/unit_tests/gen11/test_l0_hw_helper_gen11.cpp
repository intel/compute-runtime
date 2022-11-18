/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using L0HwHelperTestGen11 = Test<DeviceFixture>;

GEN11TEST_F(L0HwHelperTestGen11, GivenGen11WhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnFalse) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_FALSE(l0CoreHelper.platformSupportsCmdListHeapSharing());
}

GEN11TEST_F(L0HwHelperTestGen11, GivenGen11WhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_FALSE(l0CoreHelper.platformSupportsStateComputeModeTracking());
}

GEN11TEST_F(L0HwHelperTestGen11, GivenGen11WhenCheckingL0HelperForFrontEndTrackingSupportThenReturnFalse) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_FALSE(l0CoreHelper.platformSupportsFrontEndTracking());
}

GEN11TEST_F(L0HwHelperTestGen11, GivenGen11WhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnFalse) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_FALSE(l0CoreHelper.platformSupportsPipelineSelectTracking());
}

} // namespace ult
} // namespace L0
