/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/state_base_address_tests.h"

BDWTEST_F(SBATest, givenUsedBindlessBuffersWhenAppendStateBaseAddressParametersIsCalledThenSBACmdHasNotBindingSurfaceStateProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManager.flags.UseBindlessMode.set(1);

    STATE_BASE_ADDRESS stateBaseAddress = {};
    STATE_BASE_ADDRESS stateBaseAddressReference = {};

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        &stateBaseAddress,                     // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        nullptr,                               // gmmHelper
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false                                  // areMultipleSubDevicesInContext
    };

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(0u, ssh.getUsed());
    EXPECT_EQ(0, memcmp(&stateBaseAddressReference, &stateBaseAddress, sizeof(STATE_BASE_ADDRESS)));
}
