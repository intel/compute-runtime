/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/fixtures/implicit_scaling_fixture.h"

HWTEST2_F(ImplicitScalingTests, GivenXeHpgWhenCheckingPipeControlStallRequiredThenExpectTrue, IsXeHpgCore) {
    EXPECT_TRUE(ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired());
}
