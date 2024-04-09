/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

namespace L0 {
namespace ult {

using AppendMemoryCopy = Test<DeviceFixture>;

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenTwoNewAllocationAreAddedToHostMapPtr, IsAtLeastSkl) {
    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_EQ(cmdList.hostPtrMap.size(), 2u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndUnalignedHostPointersWhenMemoryCopyRegion2DCalledThenSrcDstPointersArePageAligned, IsAtLeastSkl) {
    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 0};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    auto sshAlignmentMask = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignmentMask();
    EXPECT_TRUE(cmdList.srcAlignedPtr == (cmdList.srcAlignedPtr & sshAlignmentMask));
    EXPECT_TRUE(cmdList.dstAlignedPtr == (cmdList.dstAlignedPtr & sshAlignmentMask));
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndUnalignedHostPointersWhenMemoryCopyRegion3DCalledThenSrcDstPointersArePageAligned, IsAtLeastSkl) {
    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    auto sshAlignmentMask = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignmentMask();
    EXPECT_TRUE(cmdList.srcAlignedPtr == (cmdList.srcAlignedPtr & sshAlignmentMask));
    EXPECT_TRUE(cmdList.dstAlignedPtr == (cmdList.dstAlignedPtr & sshAlignmentMask));
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndUnalignedHostPointersWhenBlitMemoryCopyRegion2DCalledThenSrcDstNotZeroOffsetsArePassed, IsAtLeastSkl) {
    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1233);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 0};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_GT(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_GT(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndUnalignedHostPointersWhenBlitMemoryCopyRegion3DCalledThenSrcDstNotZeroOffsetsArePassed, IsAtLeastSkl) {
    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1233);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_GT(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_GT(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndAlignedHostPointersWhenBlitMemoryCopyRegion3DCalledThenSrcDstZeroOffsetsArePassed, IsAtLeastSkl) {
    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = alignDown(reinterpret_cast<void *>(0x1233), NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    void *dstPtr = alignDown(reinterpret_cast<void *>(0x2345), NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_EQ(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_EQ(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListAndDestinationPtrOffsetWhenMemoryCopyRegionToSameUsmHostAllocationThenDestinationBlitCopyRegionHasOffset, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = buffer;
    void *dstPtr = ptrOffset(buffer, offset);
    ze_copy_region_t dstRegion = {0, 0, 0, 8, 4, 0};
    ze_copy_region_t srcRegion = {0, 0, 0, 8, 4, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(srcPtr));
    EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(dstPtr));

    context->freeMem(buffer);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListAndSourcePtrOffsetWhenMemoryCopyRegionToSameUsmHostAllocationThenSourceBlitCopyRegionHasOffset, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = ptrOffset(buffer, offset);
    void *dstPtr = buffer;
    ze_copy_region_t dstRegion = {0, 0, 0, 8, 4, 0};
    ze_copy_region_t srcRegion = {0, 0, 0, 8, 4, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(srcPtr));
    EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(dstPtr));

    context->freeMem(buffer);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListAndDestinationPtrOffsetWhenMemoryCopyRegionToSameUsmSharedAllocationThenDestinationBlitCopyRegionHasOffset, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = buffer;
    void *dstPtr = ptrOffset(buffer, offset);
    ze_copy_region_t dstRegion = {0, 0, 0, 8, 4, 0};
    ze_copy_region_t srcRegion = {0, 0, 0, 8, 4, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(srcPtr));
    EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(dstPtr));

    context->freeMem(buffer);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListAndSourcePtrOffsetWhenMemoryCopyRegionToSameUsmSharedAllocationThenSourceBlitCopyRegionHasOffset, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = ptrOffset(buffer, offset);
    void *dstPtr = buffer;
    ze_copy_region_t dstRegion = {0, 0, 0, 8, 4, 0};
    ze_copy_region_t srcRegion = {0, 0, 0, 8, 4, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(srcPtr));
    EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(dstPtr));

    context->freeMem(buffer);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListAndDestinationPtrOffsetWhenMemoryCopyRegionToSameUsmDeviceAllocationThenDestinationBlitCopyRegionHasOffset, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = buffer;
    void *dstPtr = ptrOffset(buffer, offset);
    ze_copy_region_t dstRegion = {0, 0, 0, 8, 4, 0};
    ze_copy_region_t srcRegion = {0, 0, 0, 8, 4, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(srcPtr));
    EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(dstPtr));

    context->freeMem(buffer);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListAndSourcePtrOffsetWhenMemoryCopyRegionToSameUsmDeviceAllocationThenSourceBlitCopyRegionHasOffset, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = ptrOffset(buffer, offset);
    void *dstPtr = buffer;
    ze_copy_region_t dstRegion = {0, 0, 0, 8, 4, 0};
    ze_copy_region_t srcRegion = {0, 0, 0, 8, 4, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(srcPtr));
    EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(dstPtr));

    context->freeMem(buffer);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenPipeControlWithDcFlushAdded, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);

    auto &commandContainer = cmdList.commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto pc = genCmdCast<PIPE_CONTROL *>(*genCmdList.rbegin());

    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment())) {
        EXPECT_NE(nullptr, pc);
        EXPECT_TRUE(pc->getDcFlushEnable());
    } else {
        EXPECT_EQ(nullptr, pc);
    }
}

HWTEST2_F(AppendMemoryCopy, givenImmediateCommandListWhenAppendingMemoryCopyThenSuccessIsReturned, IsAtLeastSkl) {
    Mock<CommandQueue> cmdQueue;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, cmdQueue.executeCommandListsCalled);
    EXPECT_EQ(1u, cmdQueue.synchronizeCalled);

    commandList->cmdQImmediate = nullptr;
}

HWTEST2_F(AppendMemoryCopy, givenImmediateCommandListWhenAppendingMemoryCopyWithInvalidEventThenInvalidArgumentErrorIsReturned, IsAtLeastSkl) {
    Mock<CommandQueue> cmdQueue;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 1, nullptr, false, false);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    commandList->cmdQImmediate = nullptr;
}

HWTEST2_F(AppendMemoryCopy, givenAsyncImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineThenSuccessIsReturned, IsAtLeastSkl) {
    Mock<CommandQueue> cmdQueue;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, cmdQueue.executeCommandListsCalled);
    EXPECT_EQ(0u, cmdQueue.synchronizeCalled);
    EXPECT_EQ(0u, commandList->commandContainer.getResidencyContainer().size());
    commandList->cmdQImmediate = nullptr;
    commandList->csr->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST2_F(AppendMemoryCopy, givenAsyncImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineThenProgramCmdStreamWithFlushTask, IsAtLeastSkl) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    auto cmdQueue = std::make_unique<Mock<CommandQueue>>();
    cmdQueue->csr = ultCsr;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->isFlushTaskSubmissionEnabled = true;
    commandList->device = device;
    commandList->isSyncModeQueue = false;
    commandList->cmdQImmediate = cmdQueue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->csr = ultCsr;

    // Program CSR state on first submit

    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(0u, ultCsr->getCS(0).getUsed());

    bool hwContextProgrammingRequired = (ultCsr->getCmdsSizeForHardwareContext() > 0);

    size_t expectedSize = 0;
    if (hwContextProgrammingRequired) {
        expectedSize = alignUp(ultCsr->getCmdsSizeForHardwareContext() + sizeof(typename FamilyType::MI_BATCH_BUFFER_START), MemoryConstants::cacheLineSize);
    }

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false));

    EXPECT_EQ(expectedSize, ultCsr->getCS(0).getUsed());
    EXPECT_TRUE(ultCsr->isMadeResident(commandList->commandContainer.getCommandStream()->getGraphicsAllocation()));

    size_t offset = 0;
    if constexpr (FamilyType::isUsingMiMemFence) {
        if (ultCsr->globalFenceAllocation) {
            using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
            auto sysMemFence = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(ultCsr->getCS(0).getCpuBase());
            ASSERT_NE(nullptr, sysMemFence);
            EXPECT_EQ(ultCsr->globalFenceAllocation->getGpuAddress(), sysMemFence->getSystemMemoryFenceAddress());
            offset += sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS);
        }
    }

    if (hwContextProgrammingRequired) {
        auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(ultCsr->getCS(0).getCpuBase(), offset));
        ASSERT_NE(nullptr, bbStartCmd);

        EXPECT_EQ(commandList->commandContainer.getCommandStream()->getGpuBase(), bbStartCmd->getBatchBufferStartAddress());
    }

    auto findTagUpdate = [](void *streamBase, size_t sizeUsed, uint64_t tagAddress) -> bool {
        GenCmdList genCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, streamBase, sizeUsed));

        auto itor = find<MI_FLUSH_DW *>(genCmdList.begin(), genCmdList.end());
        bool found = false;

        while (itor != genCmdList.end()) {
            auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
            if (cmd && cmd->getDestinationAddress() == tagAddress) {
                found = true;
                break;
            }
            itor++;
        }

        return found;
    };

    EXPECT_FALSE(findTagUpdate(commandList->commandContainer.getCommandStream()->getCpuBase(),
                               commandList->commandContainer.getCommandStream()->getUsed(),
                               ultCsr->getTagAllocation()->getGpuAddress()));

    // Dont program CSR state on next submit
    size_t csrOfffset = ultCsr->getCS(0).getUsed();
    size_t cmdListOffset = commandList->commandContainer.getCommandStream()->getUsed();

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false));

    EXPECT_EQ(csrOfffset, ultCsr->getCS(0).getUsed());

    EXPECT_FALSE(findTagUpdate(ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), cmdListOffset),
                               commandList->commandContainer.getCommandStream()->getUsed() - cmdListOffset,
                               ultCsr->getTagAllocation()->getGpuAddress()));
}

HWTEST2_F(AppendMemoryCopy, givenSyncImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineThenProgramCmdStreamWithFlushTask, IsAtLeastSkl) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    auto cmdQueue = std::make_unique<Mock<CommandQueue>>();
    cmdQueue->csr = ultCsr;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->isFlushTaskSubmissionEnabled = true;
    commandList->device = device;
    commandList->isSyncModeQueue = true;
    commandList->cmdQImmediate = cmdQueue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->csr = ultCsr;

    // Program CSR state on first submit

    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(0u, ultCsr->getCS(0).getUsed());

    bool hwContextProgrammingRequired = (ultCsr->getCmdsSizeForHardwareContext() > 0);

    size_t expectedSize = 0;
    if (hwContextProgrammingRequired) {
        expectedSize = alignUp(ultCsr->getCmdsSizeForHardwareContext() + sizeof(typename FamilyType::MI_BATCH_BUFFER_START), MemoryConstants::cacheLineSize);
    }

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false));

    EXPECT_EQ(expectedSize, ultCsr->getCS(0).getUsed());
    EXPECT_TRUE(ultCsr->isMadeResident(commandList->commandContainer.getCommandStream()->getGraphicsAllocation()));

    size_t offset = 0;
    if constexpr (FamilyType::isUsingMiMemFence) {
        if (ultCsr->globalFenceAllocation) {
            using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
            auto sysMemFence = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(ultCsr->getCS(0).getCpuBase());
            ASSERT_NE(nullptr, sysMemFence);
            EXPECT_EQ(ultCsr->globalFenceAllocation->getGpuAddress(), sysMemFence->getSystemMemoryFenceAddress());
            offset += sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS);
        }
    }

    if (hwContextProgrammingRequired) {
        auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(ultCsr->getCS(0).getCpuBase(), offset));
        ASSERT_NE(nullptr, bbStartCmd);
        EXPECT_EQ(commandList->commandContainer.getCommandStream()->getGpuBase(), bbStartCmd->getBatchBufferStartAddress());
    }

    auto findTagUpdate = [](void *streamBase, size_t sizeUsed, uint64_t tagAddress) -> bool {
        GenCmdList genCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, streamBase, sizeUsed));

        auto itor = find<MI_FLUSH_DW *>(genCmdList.begin(), genCmdList.end());
        bool found = false;

        while (itor != genCmdList.end()) {
            auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
            if (cmd && cmd->getDestinationAddress() == tagAddress) {
                found = true;
                break;
            }
            itor++;
        }

        return found;
    };

    EXPECT_TRUE(findTagUpdate(commandList->commandContainer.getCommandStream()->getCpuBase(),
                              commandList->commandContainer.getCommandStream()->getUsed(),
                              ultCsr->getTagAllocation()->getGpuAddress()));

    // Dont program CSR state on next submit
    size_t csrOfffset = ultCsr->getCS(0).getUsed();
    size_t cmdListOffset = commandList->commandContainer.getCommandStream()->getUsed();

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false));

    EXPECT_EQ(csrOfffset, ultCsr->getCS(0).getUsed());

    EXPECT_TRUE(findTagUpdate(ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), cmdListOffset),
                              commandList->commandContainer.getCommandStream()->getUsed() - cmdListOffset,
                              ultCsr->getTagAllocation()->getGpuAddress()));
}

HWTEST2_F(AppendMemoryCopy, givenSyncModeImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineThenSuccessIsReturned, IsAtLeastSkl) {
    Mock<CommandQueue> cmdQueue;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->isSyncModeQueue = true;
    commandList->csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, cmdQueue.executeCommandListsCalled);
    EXPECT_EQ(1u, cmdQueue.synchronizeCalled);

    commandList->cmdQImmediate = nullptr;
    commandList->csr->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST2_F(AppendMemoryCopy, givenImmediateCommandListWhenAppendingMemoryCopyThenNBlitsIsSuccessfullyCalculated, IsAtLeastSkl) {
    Mock<CommandQueue> cmdQueue;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->isCopyOnly());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->cmdQImmediate = nullptr;
    commandList->csr->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAdded, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto &commandContainer = cmdList.commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();

    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));
    auto itor = find<PIPE_CONTROL *>(genCmdList.begin(), genCmdList.end());
    PIPE_CONTROL *cmd = nullptr;
    uint32_t dcFlushPipeControl = 0;
    while (itor != genCmdList.end()) {
        cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        if (cmd->getDcFlushEnable()) {
            dcFlushPipeControl++;
        }
        itor++;
    }
    uint32_t expectedDcFlushPipeControl =
        NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()) ? 1 : 0;
    EXPECT_EQ(expectedDcFlushPipeControl, dcFlushPipeControl);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    MockCommandListCoreFamily<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr, false, false);
    EXPECT_EQ(commandList.appendMemoryCopyBlitCalled, 1u);
    EXPECT_EQ(1u, event->getPacketsInUse());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::globalTimestampLdw);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::gpThreadTimeRegAddressOffsetLow);

    itor = find<MI_FLUSH_DW *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::globalTimestampLdw);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListWhenForcingTlbFlushBeforeCopyThenMiFlushProgrammedAndTlbFlushDetected, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.FlushTlbBeforeCopy.set(1);
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    MockCommandListCoreFamily<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr, false, false);
    EXPECT_EQ(commandList.appendMemoryCopyBlitCalled, 1u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));

    bool tlbFlushFound = false;
    for (auto cmdIterator = cmdList.begin(); cmdIterator != cmdList.end(); cmdIterator++) {
        auto miFlushCmd = genCmdCast<MI_FLUSH_DW *>(*cmdIterator);
        if (miFlushCmd && miFlushCmd->getTlbInvalidate()) {
            tlbFlushFound = true;
            break;
        }
    }
    EXPECT_TRUE(tlbFlushFound);
}

using SupportedPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_DG1>;
HWTEST2_F(AppendMemoryCopy,
          givenCommandListUsesTimestampPassedToMemoryCopyWhenTwoKernelsAreUsedThenAppendProfilingCalledForSinglePacket, SupportedPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;

    MockCommandListCoreFamily<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    uint64_t globalStartAddress = event->getGpuAddress(device) + event->getGlobalStartOffset();
    uint64_t contextStartAddress = event->getGpuAddress(device) + event->getContextStartOffset();
    uint64_t globalEndAddress = event->getGpuAddress(device) + event->getGlobalEndOffset();
    uint64_t contextEndAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr, false, false);
    EXPECT_EQ(2u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(1u, event->getPacketsInUse());
    EXPECT_EQ(1u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    auto begin = cmdList.begin();
    ASSERT_EQ(2u, itorWalkers.size());
    auto secondWalker = itorWalkers[1];

    validateTimestampRegisters<FamilyType>(cmdList,
                                           begin,
                                           RegisterOffsets::globalTimestampLdw, globalStartAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextStartAddress,
                                           false);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           secondWalker,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           false);
}

HWTEST2_F(AppendMemoryCopy,
          givenCommandListUsesTimestampPassedToMemoryCopyWhenThreeKernelsAreUsedThenAppendProfilingCalledForSinglePacket, SupportedPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;

    MockCommandListCoreFamily<gfxCoreFamily> commandList;
    commandList.appendMemoryCopyKernelWithGACallBase = true;

    commandList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1231);
    void *dstPtr = reinterpret_cast<void *>(0x200002345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    uint64_t globalStartAddress = event->getGpuAddress(device) + event->getGlobalStartOffset();
    uint64_t contextStartAddress = event->getGpuAddress(device) + event->getContextStartOffset();
    uint64_t globalEndAddress = event->getGpuAddress(device) + event->getGlobalEndOffset();
    uint64_t contextEndAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100002345, event->toHandle(), 0, nullptr, false, false);
    EXPECT_EQ(3u, commandList.appendMemoryCopyKernelWithGACalled);
    EXPECT_EQ(0u, commandList.appendMemoryCopyBlitCalled);
    EXPECT_EQ(1u, event->getPacketsInUse());
    EXPECT_EQ(1u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    auto begin = cmdList.begin();
    ASSERT_EQ(3u, itorWalkers.size());
    auto thirdWalker = itorWalkers[2];

    validateTimestampRegisters<FamilyType>(cmdList,
                                           begin,
                                           RegisterOffsets::globalTimestampLdw, globalStartAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextStartAddress,
                                           false);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           thirdWalker,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           false);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListImmediateWithDummyBlitWaWhenCopyMemoryRegionThenDummyBlitIsNotProgrammedButIsRequiredForNextFlushProgramming, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.ForceDummyBlitWa.set(1);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.isFlushTaskSubmissionEnabled = true;
    cmdList.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = ptrOffset(buffer, offset);
    void *dstPtr = buffer;
    const auto numSlices = 32u;
    ze_copy_region_t dstRegion = {0, 0, 0, 8, 4, numSlices};
    ze_copy_region_t srcRegion = {0, 0, 0, 8, 4, numSlices};

    constexpr auto dstPitch = 32u;
    constexpr auto dstSlicePitch = 1024u;
    constexpr auto srcPitch = 64u;
    constexpr auto srcSlicePitch = 2048u;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, dstPitch, dstSlicePitch, srcPtr, &srcRegion, srcPitch, srcSlicePitch, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itors = findAll<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());

    EXPECT_EQ(numSlices, itors.size());
    for (auto i = 0u; i < numSlices; i++) {

        auto itor = itors[i];
        ASSERT_NE(genCmdList.end(), itor);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
        EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(ptrOffset(srcPtr, srcSlicePitch * i)));
        EXPECT_EQ(bltCmd->getSourcePitch(), srcPitch);
        EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(ptrOffset(dstPtr, dstSlicePitch * i)));
        EXPECT_EQ(bltCmd->getDestinationPitch(), dstPitch);
    }

    if constexpr (IsPVC::isMatched<productFamily>()) {
        EXPECT_EQ(itors.back(), find<typename GfxFamily::MEM_SET *>(genCmdList.begin(), itors.back()));
    }
    EXPECT_EQ(itors.back(), find<XY_COLOR_BLT *>(genCmdList.begin(), itors.back()));

    EXPECT_TRUE(cmdList.dummyBlitWa.isWaRequired);

    context->freeMem(buffer);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListWithDummyBlitWaWhenCopyMemoryRegionThenDummyBlitIsNotProgrammedButIsRequiredForNextFlushProgramming, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.ForceDummyBlitWa.set(1);

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.isFlushTaskSubmissionEnabled = true;
    cmdList.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = ptrOffset(buffer, offset);
    void *dstPtr = buffer;
    const auto numSlices = 32u;
    ze_copy_region_t dstRegion = {0, 0, 0, 8, 4, numSlices};
    ze_copy_region_t srcRegion = {0, 0, 0, 8, 4, numSlices};

    constexpr auto dstPitch = 32u;
    constexpr auto dstSlicePitch = 1024u;
    constexpr auto srcPitch = 64u;
    constexpr auto srcSlicePitch = 2048u;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, dstPitch, dstSlicePitch, srcPtr, &srcRegion, srcPitch, srcSlicePitch, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itors = findAll<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());

    EXPECT_EQ(numSlices, itors.size());
    for (auto i = 0u; i < numSlices; i++) {

        auto itor = itors[i];
        ASSERT_NE(genCmdList.end(), itor);

        auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
        EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(ptrOffset(srcPtr, srcSlicePitch * i)));
        EXPECT_EQ(bltCmd->getSourcePitch(), srcPitch);
        EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(ptrOffset(dstPtr, dstSlicePitch * i)));
        EXPECT_EQ(bltCmd->getDestinationPitch(), dstPitch);
    }

    if constexpr (IsPVC::isMatched<productFamily>()) {
        EXPECT_EQ(itors.back(), find<typename GfxFamily::MEM_SET *>(genCmdList.begin(), itors.back()));
    }
    EXPECT_EQ(itors.back(), find<XY_COLOR_BLT *>(genCmdList.begin(), itors.back()));

    EXPECT_TRUE(cmdList.dummyBlitWa.isWaRequired);

    context->freeMem(buffer);
}

struct AppendMemoryCopyFenceTest : public AppendMemoryCopy {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        AppendMemoryCopy::SetUp();
    }
    DebugManagerStateRestore restore;
};

HWTEST2_F(AppendMemoryCopyFenceTest, givenDeviceToHostCopyWhenProgrammingThenAddFence, IsAtLeastXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    using MI_MEM_FENCE = typename GfxFamily::MI_MEM_FENCE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename GfxFamily::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device));
    auto regularEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename GfxFamily::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.isFlushTaskSubmissionEnabled = true;
    cmdList.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    constexpr size_t allocSize = 1;
    void *hostBuffer = nullptr;
    void *deviceBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocHostMem(&hostDesc, allocSize, allocSize, &hostBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocSize, allocSize, &deviceBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_copy_region_t dstRegion = {0, 0, 0, 1, 1, 1};
    ze_copy_region_t srcRegion = {0, 0, 0, 1, 1, 1};

    auto cmdStream = cmdList.getCmdContainer().getCommandStream();
    size_t offset = 0;

    auto verify = [&](bool expected) {
        expected &= device->getProductHelper().isDeviceToHostCopySignalingFenceRequired();

        GenCmdList genCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
        itor = find<MI_MEM_FENCE *>(itor, genCmdList.end());

        EXPECT_EQ(expected, genCmdList.end() != itor);

        return !::testing::Test::HasFailure();
    };

    // device to host - host visible event
    {
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, false, false);

        EXPECT_TRUE(verify(true));
    }

    // device to host - regular event
    {
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, regularEvent->toHandle(), 0, nullptr, false, false);

        EXPECT_TRUE(verify(false));
    }

    // device to host - no event
    {
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, nullptr, 0, nullptr, false, false);

        EXPECT_TRUE(verify(false));
    }

    // device to device - host visible event
    {
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(deviceBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, false, false);

        EXPECT_TRUE(verify(false));
    }

    // host to device - host visible event
    {
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(deviceBuffer, &dstRegion, 1, 1, hostBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, false, false);

        EXPECT_TRUE(verify(false));
    }

    // host to host - host visible event
    {
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, hostBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, false, false);

        EXPECT_TRUE(verify(false));
    }

    context->freeMem(hostBuffer);
    context->freeMem(deviceBuffer);
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListImmediateWithDummyBlitWaWhenCopyMemoryThenDummyBlitIsNotProgrammedButIsRequiredForNextFlushProgramming, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.ForceDummyBlitWa.set(1);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.isFlushTaskSubmissionEnabled = true;
    cmdList.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = ptrOffset(buffer, offset);
    void *dstPtr = buffer;
    constexpr auto size = 1;
    cmdList.appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, false, false);

    auto &cmdContainer = cmdList.getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed()));
    auto itors = findAll<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());

    EXPECT_EQ(1u, itors.size());
    auto itor = itors[0];
    ASSERT_NE(genCmdList.end(), itor);

    auto bltCmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(bltCmd->getSourceBaseAddress(), reinterpret_cast<uintptr_t>(srcPtr));
    EXPECT_EQ(bltCmd->getDestinationBaseAddress(), reinterpret_cast<uintptr_t>(dstPtr));

    if constexpr (IsPVC::isMatched<productFamily>()) {
        EXPECT_EQ(genCmdList.end(), find<typename GfxFamily::MEM_SET *>(genCmdList.begin(), genCmdList.end()));
    }
    EXPECT_EQ(genCmdList.end(), find<XY_COLOR_BLT *>(genCmdList.begin(), genCmdList.end()));

    EXPECT_TRUE(cmdList.dummyBlitWa.isWaRequired);

    context->freeMem(buffer);
}
} // namespace ult
} // namespace L0
