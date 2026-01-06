/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "encode_surface_state_args.h"

using namespace NEO;
using CmdsProgrammingTestsDg2 = UltCommandStreamReceiverTest;

DG2TEST_F(CmdsProgrammingTestsDg2, givenL3ToL1DebugFlagWhenStatelessMocsIsProgrammedThenItHasL1CachingOn) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(1u);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);

    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);

    EXPECT_EQ(stateBaseAddress->getL1CacheControlCachePolicy(), STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB);
}

DG2TEST_F(CmdsProgrammingTestsDg2, givenL3ToL1DebugFlagAndDebuggerInitializedWhenStatelessMocsIsProgrammedThenItHasCorrectL1CachingOn) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(1u);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    pDevice->getL0Debugger()->initialize();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);

    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);

    EXPECT_EQ(stateBaseAddress->getL1CacheControlCachePolicy(), STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP);
}

DG2TEST_F(CmdsProgrammingTestsDg2, whenAppendingRssThenProgramWBL1CachePolicyUnlessDebuggerIsActive) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::buffer, pDevice->getDeviceBitfield());
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
    EXPECT_EQ(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB, rssCmd.getL1CacheControlCachePolicy());

    args.isDebuggerActive = true;
    EncodeSurfaceState<FamilyType>::encodeBuffer(args);
    EXPECT_EQ(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP, rssCmd.getL1CacheControlCachePolicy());
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
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false);

    const auto expectedMocs = context.getDevice(0)->getGmmHelper()->getL1EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    EXPECT_EQ(surfaceState.getL1CacheControlCachePolicy(), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB);

    alignedFree(ptr);
}

DG2TEST_F(CmdsProgrammingTestsDg2, givenAlignedCacheableReadOnlyBufferAndDebuggerInitializedWhenBufferCreateThenChoseOclBufferConstPolicy) {
    MockContext context;
    auto clDevice = context.getDevice(0);
    clDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->initDebuggerL0(&clDevice->getDevice());

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
    buffer->setArgStateful(&surfaceState, false, false, false, false, clDevice->getDevice(), false);

    const auto expectedMocs = clDevice->getGmmHelper()->getL1EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    EXPECT_EQ(surfaceState.getL1CacheControlCachePolicy(), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP);

    alignedFree(ptr);
}

DG2TEST_F(CmdsProgrammingTestsDg2, givenDG2WithBSteppingWhenFlushingTaskThenAdditionalStateBaseAddressCommandIsPresent) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &productHelper = pDevice->getProductHelper();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.doubleSbaWa = productHelper.isAdditionalStateBaseAddressWARequired(hwInfo);

    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);

    stateBaseAddressItor++;
    stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(stateBaseAddressItor, cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}
