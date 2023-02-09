/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/implicit_scaling_fixture.h"

HWTEST2_F(ImplicitScalingTests, GivenXeHpgWhenCheckingPipeControlStallRequiredThenExpectTrue, IsXeHpgCore) {
    EXPECT_TRUE(ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired());
}
