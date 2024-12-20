/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_ov_comp_wa.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "gtest/gtest.h"

namespace NEO {
using AILTests = ::testing::Test;

TEST(AILTests, WhenCallingApplyOpenvinoCompatibilityWaIfNeededIsCalledThenReturnFalse) {
    HardwareInfo hwInfo = {};
    EXPECT_FALSE(NEO::applyOpenVinoCompatibilityWaIfNeeded(hwInfo));
}

} // namespace NEO
