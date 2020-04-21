/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/state_base_address_tests.h"

BDWTEST_F(SBATest, givenUsedBindlessBuffersWhenAppendStateBaseAddressParametersIsCalledThenSBACmdHasNotBindingSurfaceStateProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManager.flags.UseBindlessBuffers.set(1);

    STATE_BASE_ADDRESS stateBaseAddress = {};
    STATE_BASE_ADDRESS stateBaseAddressReference = {};

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(
        &stateBaseAddress,
        &ssh,
        false,
        0,
        nullptr,
        false);

    EXPECT_EQ(0u, ssh.getUsed());
    EXPECT_EQ(0, memcmp(&stateBaseAddressReference, &stateBaseAddress, sizeof(STATE_BASE_ADDRESS)));
}
