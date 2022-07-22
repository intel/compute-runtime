/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;
using CmdsProgrammingTestsDg2 = UltCommandStreamReceiverTest;

HWTEST_EXCLUDE_PRODUCT(CmdsProgrammingTestsXeHpgCore, givenL3ToL1DebugFlagWhenStatelessMocsIsProgrammedThenItHasL1CachingOn, IGFX_XE_HPG_CORE);
DG2TEST_F(CmdsProgrammingTestsDg2, givenL3ToL1DebugFlagWhenStatelessMocsIsProgrammedThenItHasL1CachingOn) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(1u);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);

    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);

    auto actualL1CachePolicy = static_cast<uint8_t>(stateBaseAddress->getL1CachePolicyL1CacheControl());

    const uint8_t expectedL1CachePolicy = FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB;
    EXPECT_EQ(expectedL1CachePolicy, actualL1CachePolicy);
}

DG2TEST_F(CmdsProgrammingTestsDg2, givenSpecificProductFamilyWhenAppendingSbaThenProgramWtL1CachePolicy) {

    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), 1, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    IndirectHeap indirectHeap(allocation, 1);
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(&sbaCmd, &indirectHeap, true, 0,
                                                                         pDevice->getRootDeviceEnvironment().getGmmHelper(), false,
                                                                         MemoryCompressionState::NotApplicable, true, false, 1u);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, sbaCmd.getL1CachePolicyL1CacheControl());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_EXCLUDE_PRODUCT(CmdsProgrammingTestsXeHpgCore, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue, IGFX_XE_HPG_CORE);
DG2TEST_F(CmdsProgrammingTestsDg2, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceStatelessL1CachingPolicy.set(0u);
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), 1, AllocationType::BUFFER, pDevice->getDeviceBitfield());
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

HWTEST_EXCLUDE_PRODUCT(CmdsProgrammingTestsXeHpgCore, givenXeHpgCoreWhenAppendingRssThenProgramWBPL1CachePolicy, IGFX_XE_HPG_CORE);
DG2TEST_F(CmdsProgrammingTestsDg2, givenDG2whenAppendingRssThenProgramWBL1CachePolicy) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    MockContext context(pClDevice);
    auto multiGraphicsAllocation = MultiGraphicsAllocation(pClDevice->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(allocation);

    std::unique_ptr<BufferHw<FamilyType>> buffer(static_cast<BufferHw<FamilyType> *>(
        BufferHw<FamilyType>::create(&context, {}, 0, 0, allocationSize, nullptr, nullptr, std::move(multiGraphicsAllocation), false, false, false)));

    NEO::EncodeSurfaceStateArgs args;
    args.outMemory = &rssCmd;
    args.graphicsAddress = allocation->getGpuAddress();
    args.size = allocation->getUnderlyingBufferSize();
    args.mocs = buffer->getMocsValue(false, false, pClDevice->getRootDeviceIndex());
    args.numAvailableDevices = pClDevice->getNumGenericSubDevices();
    args.allocation = allocation;
    args.gmmHelper = pClDevice->getGmmHelper();
    args.areMultipleSubDevicesInContext = true;

    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB, rssCmd.getL1CachePolicyL1CacheControl());
}

HWTEST_EXCLUDE_PRODUCT(CmdsProgrammingTestsXeHpgCore, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferConstPolicy, IGFX_XE_HPG_CORE);
DG2TEST_F(CmdsProgrammingTestsDg2, givenDG2WithBSteppingWhenFlushingTaskThenAdditionalStateBaseAddressCommandIsPresent) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);

    stateBaseAddressItor++;
    stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(stateBaseAddressItor, cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_EXCLUDE_PRODUCT(CmdsProgrammingTestsXeHpgCore, givenSpecificProductFamilyWhenAppendingSbaThenProgramWBPL1CachePolicy, IGFX_XE_HPG_CORE);
DG2TEST_F(CmdsProgrammingTestsDg2, givenDG2WhenAppendingSbaThenProgramWBL1CachePolicy) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), 1, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    IndirectHeap indirectHeap(allocation, 1);
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(&sbaCmd, &indirectHeap, true, 0,
                                                                         pDevice->getRootDeviceEnvironment().getGmmHelper(), false,
                                                                         MemoryCompressionState::NotApplicable, true, false, 1u);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, sbaCmd.getL1CachePolicyL1CacheControl());

    memoryManager->freeGraphicsMemory(allocation);
}
