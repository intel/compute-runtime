/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/test/common/helpers/state_base_address_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

BDWTEST_F(SBATest, givenUsedBindlessBuffersWhenAppendStateBaseAddressParametersIsCalledThenSBACmdHasNotBindingSurfaceStateProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManager.flags.UseBindlessMode.set(1);

    STATE_BASE_ADDRESS stateBaseAddress = {};
    STATE_BASE_ADDRESS stateBaseAddressReference = {};

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(
        &stateBaseAddress,
        &ssh,
        false,
        0,
        nullptr,
        false,
        MemoryCompressionState::NotApplicable,
        true,
        false,
        1u);

    EXPECT_EQ(0u, ssh.getUsed());
    EXPECT_EQ(0, memcmp(&stateBaseAddressReference, &stateBaseAddress, sizeof(STATE_BASE_ADDRESS)));
}
