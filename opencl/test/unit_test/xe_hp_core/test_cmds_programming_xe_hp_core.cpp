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

XE_HP_CORE_TEST_F(CmdsProgrammingTestsXeHpCore, givenInterfaceDescriptorDataWhenBSteppingIsDetectedThenTGBatchSizeIsEqualTo3) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = REVISION_B;

    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, pDevice->getHardwareInfo());
    EXPECT_EQ(3u, iddArg.getThreadGroupDispatchSize());
}

using PreambleCfeState = PreambleFixture;

XE_HP_CORE_TEST_F(PreambleCfeState, givenXehpBSteppingWhenCfeIsProgrammedThenOverdispatchIsDisabled) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto backup = defaultHwInfo->platform.usRevId;
    defaultHwInfo->platform.usRevId = REVISION_B;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, 0, 0, AdditionalKernelExecInfo::NotApplicable, streamProperties);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_TRUE(cfeState->getComputeOverdispatchDisable());
    defaultHwInfo->platform.usRevId = backup;
}
