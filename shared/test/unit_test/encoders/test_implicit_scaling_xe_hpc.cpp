/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/implicit_scaling_fixture.h"

HWTEST2_F(ImplicitScalingTests, GivenXeHpcWhenCheckingPipeControlStallRequiredThenExpectTrue, IsXeHpcCore) {
    EXPECT_TRUE(ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired());
}
