/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/helpers/compiler_product_helper.h"
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

using AppendMemoryCopyTests = Test<AppendMemoryCopyFixture>;

HWTEST_F(AppendMemoryCopyTests, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenTwoNewAllocationAreAddedToHostMapPtr) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.hostPtrMap.size(), 2u);
}

HWTEST_F(AppendMemoryCopyTests, givenCommandListAndUnalignedHostPointersWhenMemoryCopyRegion2DCalledThenSrcDstPointersArePageAligned) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 0};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    auto sshAlignmentMask = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignmentMask();
    EXPECT_TRUE(cmdList.srcAlignedPtr == (cmdList.srcAlignedPtr & sshAlignmentMask));
    EXPECT_TRUE(cmdList.dstAlignedPtr == (cmdList.dstAlignedPtr & sshAlignmentMask));
}

HWTEST_F(AppendMemoryCopyTests, givenCommandListAndUnalignedHostPointersWhenMemoryCopyRegion3DCalledThenSrcDstPointersArePageAligned) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    auto sshAlignmentMask = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignmentMask();
    EXPECT_TRUE(cmdList.srcAlignedPtr == (cmdList.srcAlignedPtr & sshAlignmentMask));
    EXPECT_TRUE(cmdList.dstAlignedPtr == (cmdList.dstAlignedPtr & sshAlignmentMask));
}

HWTEST_F(AppendMemoryCopyTests, givenCommandListAndUnalignedHostPointersWhenBlitMemoryCopyRegion2DCalledThenSrcDstNotZeroOffsetsArePassed) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1233);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 0};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_GT(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST_F(AppendMemoryCopyTests, givenCommandListAndUnalignedHostPointersWhenBlitMemoryCopyRegion3DCalledThenSrcDstNotZeroOffsetsArePassed) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1233);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_GT(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST_F(AppendMemoryCopyTests, givenCommandListAndAlignedHostPointersWhenBlitMemoryCopyRegion3DCalledThenSrcDstZeroOffsetsArePassed) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = alignDown(reinterpret_cast<void *>(0x1233), NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    void *dstPtr = alignDown(reinterpret_cast<void *>(0x2345), NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_EQ(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListAndDestinationPtrOffsetWhenMemoryCopyRegionToSameUsmHostAllocationThenDestinationBlitCopyRegionHasOffset) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
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
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);

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

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListAndSourcePtrOffsetWhenMemoryCopyRegionToSameUsmHostAllocationThenSourceBlitCopyRegionHasOffset) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
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
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);

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

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListAndDestinationPtrOffsetWhenMemoryCopyRegionToSameUsmSharedAllocationThenDestinationBlitCopyRegionHasOffset) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
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
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);

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

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListAndSourcePtrOffsetWhenMemoryCopyRegionToSameUsmSharedAllocationThenSourceBlitCopyRegionHasOffset) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
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
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);

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

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListAndDestinationPtrOffsetWhenMemoryCopyRegionToSameUsmDeviceAllocationThenDestinationBlitCopyRegionHasOffset) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
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
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);

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

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListAndSourcePtrOffsetWhenMemoryCopyRegionToSameUsmDeviceAllocationThenSourceBlitCopyRegionHasOffset) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
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
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);

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

HWTEST_F(AppendMemoryCopyTests, givenImmediateCommandListWhenAppendingMemoryCopyThenSuccessIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = queue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto taskCountBefore = queue->getTaskCount();
    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, queue->executeCommandListsCalled);
    EXPECT_GT(queue->getTaskCount(), taskCountBefore);

    commandList->cmdQImmediate = nullptr;
}

HWTEST_F(AppendMemoryCopyTests, givenImmediateCommandListWhenAppendingMemoryCopySharedSystemUsmThenSuccessIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = queue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};

    sharedSystemMemCapabilities = 0xf;
    auto taskCountBefore = queue->getTaskCount();
    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, queue->executeCommandListsCalled);
    EXPECT_GT(queue->getTaskCount(), taskCountBefore);

    commandList->cmdQImmediate = nullptr;
}

HWTEST_F(AppendMemoryCopyTests, givenImmediateCommandListWhenAppendingMemoryCopyWithInvalidEventThenInvalidArgumentErrorIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = queue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 1, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST_F(AppendMemoryCopyTests, givenAsyncImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineThenSuccessIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = queue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto taskCountBefore = queue->getTaskCount();
    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, queue->executeCommandListsCalled);
    EXPECT_GT(queue->getTaskCount(), taskCountBefore);
    EXPECT_EQ(0u, commandList->commandContainer.getResidencyContainer().size());
    commandList->getCsr(false)->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST2_F(AppendMemoryCopyTests, givenImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineAndSharedSystemUsmThenSuccessIsReturned, IsNotXeHpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(-1);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = queue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};

    sharedSystemMemCapabilities = 0xf;
    auto taskCountBefore = queue->getTaskCount();
    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, queue->executeCommandListsCalled);
    EXPECT_GT(queue->getTaskCount(), taskCountBefore);
}

HWTEST_F(AppendMemoryCopyTests, givenAsyncImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineThenProgramCmdStreamWithFlushTask) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;
    bool heaplessStateInit = ultCsr->heaplessStateInitialized;
    auto cmdQueue = std::make_unique<Mock<CommandQueue>>();
    cmdQueue->csr = ultCsr;
    cmdQueue->isCopyOnlyCommandQueue = true;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->isSyncModeQueue = false;
    commandList->cmdQImmediate = cmdQueue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;

    // Program CSR state on first submit

    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    if (heaplessStateInit) {
        EXPECT_NE(0u, ultCsr->getCS(0).getUsed());
    } else {
        EXPECT_EQ(0u, ultCsr->getCS(0).getUsed());
    }
    auto sizeUsedBefore = ultCsr->getCS(0).getUsed();

    bool hwContextProgrammingRequired = (ultCsr->getCmdsSizeForHardwareContext() > 0);

    size_t expectedSize = 0;
    if (hwContextProgrammingRequired) {
        expectedSize = alignUp(ultCsr->getCmdsSizeForHardwareContext() + sizeof(typename FamilyType::MI_BATCH_BUFFER_START), MemoryConstants::cacheLineSize);
    }

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams));

    EXPECT_EQ(expectedSize, ultCsr->getCS(0).getUsed() - sizeUsedBefore);

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

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams));

    EXPECT_EQ(csrOfffset, ultCsr->getCS(0).getUsed());

    EXPECT_FALSE(findTagUpdate(ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), cmdListOffset),
                               commandList->commandContainer.getCommandStream()->getUsed() - cmdListOffset,
                               ultCsr->getTagAllocation()->getGpuAddress()));
}

HWTEST_F(AppendMemoryCopyTests, givenSyncImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineThenProgramCmdStreamWithFlushTask) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;
    bool heaplessStateInit = ultCsr->heaplessStateInitialized;

    auto cmdQueue = std::make_unique<Mock<CommandQueue>>();
    cmdQueue->csr = ultCsr;
    cmdQueue->isCopyOnlyCommandQueue = true;

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->isSyncModeQueue = true;
    commandList->cmdQImmediate = cmdQueue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;

    // Program CSR state on first submit

    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    if (heaplessStateInit) {
        EXPECT_NE(0u, ultCsr->getCS(0).getUsed());

    } else {
        EXPECT_EQ(0u, ultCsr->getCS(0).getUsed());
    }

    auto sizeUsedBefore = ultCsr->getCS(0).getUsed();

    bool hwContextProgrammingRequired = (ultCsr->getCmdsSizeForHardwareContext() > 0);

    size_t expectedSize = 0;
    if (hwContextProgrammingRequired) {
        expectedSize = alignUp(ultCsr->getCmdsSizeForHardwareContext() + sizeof(typename FamilyType::MI_BATCH_BUFFER_START), MemoryConstants::cacheLineSize);
    }

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams));

    EXPECT_EQ(expectedSize, ultCsr->getCS(0).getUsed() - sizeUsedBefore);
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

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams));

    EXPECT_EQ(csrOfffset, ultCsr->getCS(0).getUsed());

    EXPECT_TRUE(findTagUpdate(ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), cmdListOffset),
                              commandList->commandContainer.getCommandStream()->getUsed() - cmdListOffset,
                              ultCsr->getTagAllocation()->getGpuAddress()));
}

HWTEST_F(AppendMemoryCopyTests, givenSyncModeImmediateCommandListWhenAppendingMemoryCopyWithCopyEngineThenSuccessIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = queue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->isSyncModeQueue = true;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    auto taskCountBefore = queue->getTaskCount();
    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, queue->executeCommandListsCalled);
    EXPECT_GT(queue->getTaskCount(), taskCountBefore);

    commandList->getCsr(false)->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST_F(AppendMemoryCopyTests, givenImmediateCommandListWhenAppendingMemoryCopyThenNBlitsIsSuccessfullyCalculated) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdQImmediate = queue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->isCopyOnly(false));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->getCsr(false)->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand) {

    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
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

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(commandList.appendMemoryCopyBlitCalled, 1u);
    EXPECT_EQ(1u, event->getPacketsInUse());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::globalTimestampLdw);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::gpThreadTimeRegAddressOffsetLow);

    itor = find<MI_FLUSH_DW *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::globalTimestampLdw);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListWhenForcingTlbFlushBeforeCopyThenMiFlushProgrammedAndTlbFlushDetected) {
    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.FlushTlbBeforeCopy.set(1);

    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr, copyParams);
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

HWTEST2_F(AppendMemoryCopyTests,
          givenCommandListUsesTimestampPassedToMemoryCopyWhenTwoKernelsAreUsedThenAppendProfilingCalledForSinglePacket, IsGen12LP) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
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

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr, copyParams);
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
                                           false,
                                           true);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           secondWalker,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           false,
                                           true);
}

HWTEST2_F(AppendMemoryCopyTests,
          givenCommandListUsesTimestampPassedToMemoryCopyWhenThreeKernelsAreUsedThenAppendProfilingCalledForSinglePacket, IsGen12LP) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> commandList;
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

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100002345, event->toHandle(), 0, nullptr, copyParams);
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
                                           false,
                                           true);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           thirdWalker,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           false,
                                           true);
}

HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListImmediateWithDummyBlitWaWhenCopyMemoryRegionThenDummyBlitIsNotProgrammedButIsRequiredForNextFlushProgramming, IsAtLeastXeCore) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.ForceDummyBlitWa.set(1);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

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
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, dstPitch, dstSlicePitch, srcPtr, &srcRegion, srcPitch, srcSlicePitch, nullptr, 0, nullptr, copyParams);

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
        EXPECT_EQ(itors.back(), find<typename FamilyType::MEM_SET *>(genCmdList.begin(), itors.back()));
    }
    EXPECT_EQ(itors.back(), find<XY_COLOR_BLT *>(genCmdList.begin(), itors.back()));

    EXPECT_TRUE(cmdList.dummyBlitWa.isWaRequired);

    context->freeMem(buffer);
}

HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListWithDummyBlitWaWhenCopyMemoryRegionThenDummyBlitIsNotProgrammedButIsRequiredForNextFlushProgramming, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.ForceDummyBlitWa.set(1);

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

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
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, dstPitch, dstSlicePitch, srcPtr, &srcRegion, srcPitch, srcSlicePitch, nullptr, 0, nullptr, copyParams);

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
        EXPECT_EQ(itors.back(), find<typename FamilyType::MEM_SET *>(genCmdList.begin(), itors.back()));
    }
    EXPECT_EQ(itors.back(), find<XY_COLOR_BLT *>(genCmdList.begin(), itors.back()));

    EXPECT_TRUE(cmdList.dummyBlitWa.isWaRequired);

    context->freeMem(buffer);
}

struct AppendMemoryCopyFenceTest : public AppendMemoryCopyTests {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        AppendMemoryCopyTests::SetUp();
    }
    DebugManagerStateRestore restore;
};

HWTEST2_F(AppendMemoryCopyFenceTest, givenDeviceToHostCopyWhenProgrammingThenAddFence, IsAtLeastXeHpcCore) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device));
    auto regularEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    ze_command_queue_desc_t desc = {};

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdListRegular;
    cmdListRegular.initialize(device, NEO::EngineGroupType::copy, 0u);

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdListRegularInOrder;
    cmdListRegularInOrder.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdListRegularInOrder.enableInOrderExecution();

    auto queue1 = std::make_unique<Mock<CommandQueue>>(device, csr, &desc);
    auto queue2 = std::make_unique<Mock<CommandQueue>>(device, csr, &desc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdListImmediate;
    cmdListImmediate.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdListImmediate.cmdQImmediate = queue1.get();
    cmdListImmediate.initialize(device, NEO::EngineGroupType::copy, 0u);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdListImmediateInOrder;
    cmdListImmediateInOrder.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdListImmediateInOrder.cmdQImmediate = queue2.get();
    cmdListImmediateInOrder.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdListImmediateInOrder.enableInOrderExecution();

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

    LinearStream *cmdStream = nullptr;
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

    for (bool inOrderCmdList : {true, false}) {
        for (bool immediateCmdList : {true, false}) {
            L0::CommandListCoreFamily<FamilyType::gfxCoreFamily> *cmdList = nullptr;

            if (immediateCmdList) {
                cmdList = inOrderCmdList ? &cmdListImmediateInOrder : &cmdListImmediate;
            } else {
                cmdList = inOrderCmdList ? &cmdListRegularInOrder : &cmdListRegular;
            }
            cmdStream = cmdList->getCmdContainer().getCommandStream();

            bool isImmediateInOrder = immediateCmdList && inOrderCmdList;

            // device to host - host visible event
            {
                offset = cmdStream->getUsed();
                cmdList->appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, copyParams);

                EXPECT_TRUE(verify(true));
            }

            // device to host - regular event
            {
                offset = cmdStream->getUsed();
                cmdList->appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, regularEvent->toHandle(), 0, nullptr, copyParams);

                EXPECT_TRUE(verify(isImmediateInOrder));
            }

            // device to host - no event
            {
                offset = cmdStream->getUsed();
                cmdList->appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, nullptr, 0, nullptr, copyParams);

                EXPECT_TRUE(verify(isImmediateInOrder));
            }

            // device to device - host visible event
            {
                offset = cmdStream->getUsed();
                cmdList->appendMemoryCopyRegion(deviceBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, copyParams);

                EXPECT_TRUE(verify(false));
            }

            // host to device - host visible event
            {
                offset = cmdStream->getUsed();
                cmdList->appendMemoryCopyRegion(deviceBuffer, &dstRegion, 1, 1, hostBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, copyParams);

                EXPECT_TRUE(verify(false));
            }

            // host to host - host visible event
            {
                offset = cmdStream->getUsed();
                cmdList->appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, hostBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, copyParams);

                EXPECT_TRUE(verify(false));
            }
        }
    }

    context->freeMem(hostBuffer);
    context->freeMem(deviceBuffer);
}

HWTEST2_F(AppendMemoryCopyFenceTest, givenAppendMemAdviseWithRegularAndHeapLessCmdListThenMemAdvicesAreDispatched, IsAtLeastXeHpcCore) {

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_command_queue_desc_t desc = {};
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::compute, 0u);

    constexpr size_t allocSize = 1;
    void *deviceBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocSize, allocSize, &deviceBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdListHandle = cmdList.toHandle();

    {
        cmdList.appendMemAdvise(device->toHandle(), deviceBuffer, allocSize, ZE_MEMORY_ADVICE_CLEAR_SYSTEM_MEMORY_PREFERRED_LOCATION);
        cmdList.close();
        mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(1u, cmdList.executeMemAdviseCallCount);
        EXPECT_EQ(0u, cmdList.getMemAdviseOperations().size());
    }

    {
        mockCmdQHw->heaplessStateInitEnabled = true;
        cmdList.reset();
        cmdList.appendMemAdvise(device->toHandle(), deviceBuffer, allocSize, ZE_MEMORY_ADVICE_CLEAR_SYSTEM_MEMORY_PREFERRED_LOCATION);
        cmdList.close();
        mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(2u, cmdList.executeMemAdviseCallCount);
        EXPECT_EQ(0u, cmdList.getMemAdviseOperations().size());
    }

    context->freeMem(deviceBuffer);
}

HWTEST2_F(AppendMemoryCopyFenceTest, givenRegularCmdListWhenDeviceToHostCopyProgrammedWithoutEventThenAddFenceDuringTagUpdate, IsAtLeastXeHpcCore) {

    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device));
    auto regularEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t desc = {};
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

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

    size_t offset = 0;

    auto cmdStream = &mockCmdQHw->commandStream;
    auto cmdListHandle = cmdList.toHandle();

    const bool fenceSupported = device->getProductHelper().isDeviceToHostCopySignalingFenceRequired();

    auto verify = [&](bool expected) {
        expected &= fenceSupported;

        EXPECT_EQ(expected, cmdList.taskCountUpdateFenceRequired);

        GenCmdList genCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto itorBegin = genCmdList.begin();
        auto miFence = find<MI_MEM_FENCE *>(itorBegin, genCmdList.end());
        EXPECT_EQ(expected, genCmdList.end() != miFence);

        if (expected) {
            itorBegin = miFence;
        }

        auto miFlush = find<MI_FLUSH_DW *>(itorBegin, genCmdList.end());
        EXPECT_NE(genCmdList.end(), miFlush);

        return !::testing::Test::HasFailure();
    };

    // device to host - host visible event
    {
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, copyParams);
        cmdList.close();
        mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

        EXPECT_TRUE(verify(false));
    }

    // device to host - regular event
    {
        cmdList.reset();
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, regularEvent->toHandle(), 0, nullptr, copyParams);
        cmdList.close();
        mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

        EXPECT_TRUE(verify(true));
    }

    // device to host - without event
    {
        cmdList.reset();
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, nullptr, 0, nullptr, copyParams);
        cmdList.close();
        mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

        EXPECT_TRUE(verify(true));
    }

    // reset requirement on cmdlist fence
    {
        cmdList.reset();
        offset = cmdStream->getUsed();
        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, nullptr, 0, nullptr, copyParams);
        EXPECT_EQ(fenceSupported, cmdList.taskCountUpdateFenceRequired);

        cmdList.appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, copyParams);
        EXPECT_FALSE(cmdList.taskCountUpdateFenceRequired);

        cmdList.close();
        mockCmdQHw->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

        EXPECT_TRUE(verify(false));
    }

    context->freeMem(hostBuffer);
    context->freeMem(deviceBuffer);
}

HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListImmediateWithDummyBlitWaWhenCopyMemoryThenDummyBlitIsNotProgrammedButIsRequiredForNextFlushProgramming, IsAtLeastXeCore) {

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.ForceDummyBlitWa.set(1);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);

    constexpr size_t allocSize = 4096;
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t offset = 64u;
    void *srcPtr = ptrOffset(buffer, offset);
    void *dstPtr = buffer;
    constexpr auto size = 1;
    cmdList.appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);

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
        EXPECT_EQ(genCmdList.end(), find<typename FamilyType::MEM_SET *>(genCmdList.begin(), genCmdList.end()));
    }
    EXPECT_EQ(genCmdList.end(), find<XY_COLOR_BLT *>(genCmdList.begin(), genCmdList.end()));

    EXPECT_TRUE(cmdList.dummyBlitWa.isWaRequired);

    context->freeMem(buffer);
}
} // namespace ult
} // namespace L0
