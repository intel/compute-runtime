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

BDWTEST_F(SbaTest, givenUsedBindlessBuffersWhenAppendStateBaseAddressParametersIsCalledThenSBACmdHasNotBindingSurfaceStateProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManager.flags.UseBindlessMode.set(1);

    STATE_BASE_ADDRESS stateBaseAddress = {};
    STATE_BASE_ADDRESS stateBaseAddressReference = {};

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &stateBaseAddress,                     // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        nullptr,                               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false                                  // overrideSurfaceStateBaseAddress
    };

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(0u, ssh.getUsed());
    EXPECT_EQ(0, memcmp(&stateBaseAddressReference, &stateBaseAddress, sizeof(STATE_BASE_ADDRESS)));
}

BDWTEST_F(SbaTest,
          givenUsedBindlessBuffersAndOverridenSurfaceStateBaseAddressWhenAppendStateBaseAddressParametersIsCalledThenSbaCmdHasCorrectSurfaceStateBaseAddress) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t surfaceStateBaseAddress = 0xBADA550000;

    STATE_BASE_ADDRESS stateBaseAddressCmd = {};

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        surfaceStateBaseAddress,               // surfaceStateBaseAddress
        &stateBaseAddressCmd,                  // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        nullptr,                               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        true                                   // overrideSurfaceStateBaseAddress
    };

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(stateBaseAddressCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(surfaceStateBaseAddress, stateBaseAddressCmd.getSurfaceStateBaseAddress());
}
