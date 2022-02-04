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
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;
using CmdsProgrammingTestsDg2 = UltCommandStreamReceiverTest;

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

    auto actualL1CachePolocy = static_cast<uint8_t>(stateBaseAddress->getL1CachePolicyL1CacheControl());

    const uint8_t expectedL1CachePolicy = 0;
    EXPECT_EQ(expectedL1CachePolicy, actualL1CachePolocy);
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

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, sbaCmd.getL1CachePolicyL1CacheControl());

    memoryManager->freeGraphicsMemory(allocation);
}

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

DG2TEST_F(CmdsProgrammingTestsDg2, whenAppendingRssThenProgramWtL1CachePolicy) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    MockContext context(pClDevice);
    auto multiGraphicsAllocation = MultiGraphicsAllocation(pClDevice->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(allocation);

    std::unique_ptr<BufferHw<FamilyType>> buffer(static_cast<BufferHw<FamilyType> *>(
        BufferHw<FamilyType>::create(&context, {}, 0, 0, allocationSize, nullptr, nullptr, multiGraphicsAllocation, false, false, false)));

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

    EXPECT_EQ(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WBP, rssCmd.getL1CachePolicyL1CacheControl());
}

DG2TEST_F(CmdsProgrammingTestsDg2, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferConstPolicy) {
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    const auto flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        ptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    const auto expectedMocs = context.getDevice(0)->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    auto actualL1CachePolocy = static_cast<uint8_t>(surfaceState.getL1CachePolicyL1CacheControl());

    const uint8_t expectedL1CachePolicy = 0;
    EXPECT_EQ(expectedL1CachePolicy, actualL1CachePolocy);

    alignedFree(ptr);
}

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
