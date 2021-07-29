/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/preamble/preamble_fixture.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "test.h"

using namespace NEO;
using CmdsProgrammingTestsXeHpCore = UltCommandStreamReceiverTest;
XE_HP_CORE_TEST_F(CmdsProgrammingTestsXeHpCore, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceStatelessL1CachingPolicy.set(0u);
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), 1, GraphicsAllocation::AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    IndirectHeap indirectHeap(allocation, 1);
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(&sbaCmd, &indirectHeap, true, 0,
                                                                         pDevice->getRootDeviceEnvironment().getGmmHelper(), false,
                                                                         MemoryCompressionState::NotApplicable, true, false, 1u);

    EXPECT_EQ(0u, sbaCmd.getL1CachePolicyL1CacheControl());

    DebugManager.flags.ForceStatelessL1CachingPolicy.set(1u);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(&sbaCmd, &indirectHeap, true, 0,
                                                                         pDevice->getRootDeviceEnvironment().getGmmHelper(), false,
                                                                         MemoryCompressionState::NotApplicable, true, false, 1u);

    EXPECT_EQ(1u, sbaCmd.getL1CachePolicyL1CacheControl());

    memoryManager->freeGraphicsMemory(allocation);
}
