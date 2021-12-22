/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using AppendMemoryCopy = Test<DeviceFixture>;

template <GFXCORE_FAMILY gfxCoreFamily>
class MockAppendMemoryCopy : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    ADDMETHOD_NOBASE(appendMemoryCopyKernelWithGA, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstPtr, NEO::GraphicsAllocation *dstPtrAlloc,
                      uint64_t dstOffset, void *srcPtr,
                      NEO::GraphicsAllocation *srcPtrAlloc,
                      uint64_t srcOffset, uint64_t size,
                      uint64_t elementSize, Builtin builtin,
                      ze_event_handle_t hSignalEvent,
                      bool isStateless));
    ADDMETHOD_NOBASE(appendMemoryCopyBlit, ze_result_t, ZE_RESULT_SUCCESS,
                     (uintptr_t dstPtr,
                      NEO::GraphicsAllocation *dstPtrAlloc,
                      uint64_t dstOffset, uintptr_t srcPtr,
                      NEO::GraphicsAllocation *srcPtrAlloc,
                      uint64_t srcOffset,
                      uint64_t size));
    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize, bool allowHostCopy) override {
        return L0::CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocation(device, buffer, bufferSize, allowHostCopy);
    }
    ze_result_t appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         size_t srcOffset, ze_event_handle_t hSignalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        srcAlignedPtr = srcAlignedAllocation->alignedAllocationPtr;
        dstAlignedPtr = dstAlignedAllocation->alignedAllocationPtr;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel2d(dstAlignedAllocation, srcAlignedAllocation, builtin, dstRegion, dstPitch, dstOffset, srcRegion, srcPitch, srcOffset, hSignalEvent, numWaitEvents, phWaitEvents);
    }

    ze_result_t appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         uint32_t srcSlicePitch, size_t srcOffset,
                                         ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents) override {
        srcAlignedPtr = srcAlignedAllocation->alignedAllocationPtr;
        dstAlignedPtr = dstAlignedAllocation->alignedAllocationPtr;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel3d(dstAlignedAllocation, srcAlignedAllocation, builtin, dstRegion, dstPitch, dstSlicePitch, dstOffset, srcRegion, srcPitch, srcSlicePitch, srcOffset, hSignalEvent, numWaitEvents, phWaitEvents);
    }

    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcAllocation,
                                           NEO::GraphicsAllocation *dstAllocation,
                                           size_t srcOffset,
                                           size_t dstOffset,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize, ze_event_handle_t hSignalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        srcBlitCopyRegionOffset = srcOffset;
        dstBlitCopyRegionOffset = dstOffset;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyBlitRegion(srcAllocation, dstAllocation, srcOffset, dstOffset, srcRegion, dstRegion, copySize, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, srcSize, dstSize, hSignalEvent, numWaitEvents, phWaitEvents);
    }
    uintptr_t srcAlignedPtr;
    uintptr_t dstAlignedPtr;
    size_t srcBlitCopyRegionOffset = 0;
    size_t dstBlitCopyRegionOffset = 0;
};

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenTwoNewAllocationAreAddedToHostMapPtr, IsAtLeastSkl) {
    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.hostPtrMap.size(), 2u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndUnalignedHostPointersWhenMemoryCopyRegion2DCalledThenSrcDstPointersArePageAligned, IsAtLeastSkl) {
    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 0};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    auto sshAlignmentMask = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignmentMask();
    EXPECT_TRUE(cmdList.srcAlignedPtr == (cmdList.srcAlignedPtr & sshAlignmentMask));
    EXPECT_TRUE(cmdList.dstAlignedPtr == (cmdList.dstAlignedPtr & sshAlignmentMask));
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndUnalignedHostPointersWhenMemoryCopyRegion3DCalledThenSrcDstPointersArePageAligned, IsAtLeastSkl) {
    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    auto sshAlignmentMask = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignmentMask();
    EXPECT_TRUE(cmdList.srcAlignedPtr == (cmdList.srcAlignedPtr & sshAlignmentMask));
    EXPECT_TRUE(cmdList.dstAlignedPtr == (cmdList.dstAlignedPtr & sshAlignmentMask));
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndUnalignedHostPointersWhenBlitMemoryCopyRegion2DCalledThenSrcDstNotZeroOffsetsArePassed, IsAtLeastSkl) {
    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1233);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 0};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 0};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_GT(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndUnalignedHostPointersWhenBlitMemoryCopyRegion3DCalledThenSrcDstNotZeroOffsetsArePassed, IsAtLeastSkl) {
    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1233);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_GT(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndAlignedHostPointersWhenBlitMemoryCopyRegion3DCalledThenSrcDstZeroOffsetsArePassed, IsAtLeastSkl) {
    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = alignDown(reinterpret_cast<void *>(0x1233), NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    void *dstPtr = alignDown(reinterpret_cast<void *>(0x2345), NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.srcBlitCopyRegionOffset, 0u);
    EXPECT_EQ(cmdList.dstBlitCopyRegionOffset, 0u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenPipeControlWithDcFlushAdded, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);

    auto &commandContainer = cmdList.commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    PIPE_CONTROL *cmd = nullptr;
    while (itor != genCmdList.end()) {
        cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());
    }
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
}

HWTEST2_F(AppendMemoryCopy, givenImmediateCommandListWhenAppendingMemoryCopyThenSuccessIsReturned, IsAtLeastSkl) {
    Mock<CommandQueue> cmdQueue;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr);
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
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 1, nullptr);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    commandList->cmdQImmediate = nullptr;
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAdded, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr);

    auto &commandContainer = cmdList.commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    PIPE_CONTROL *cmd = nullptr;
    while (itor != genCmdList.end()) {
        cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());
    }
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
}

HWTEST2_F(AppendMemoryCopy, givenCopyCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::Copy, 0u);
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

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr);
    EXPECT_GT(commandList.appendMemoryCopyBlitCalled, 1u);
    EXPECT_EQ(1u, event->getPacketsInUse());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);

    itor = find<MI_FLUSH_DW *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

using SupportedPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_DG1>;
HWTEST2_F(AppendMemoryCopy, givenCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, SupportedPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr);
    EXPECT_GT(commandList.appendMemoryCopyKernelWithGACalled, 0u);
    EXPECT_EQ(commandList.appendMemoryCopyBlitCalled, 0u);
    EXPECT_EQ(1u, event->getPacketsInUse());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(cmd->getSourceRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    }

    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(cmd->getSourceRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    }

    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        EXPECT_FALSE(cmd->getDcFlushEnable());
    }

    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(cmd->getSourceRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    }

    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(cmd->getSourceRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    }

    auto temp = itor;
    auto numPCs = findAll<PIPE_CONTROL *>(temp, cmdList.end());
    //we should have only one PC with dcFlush added
    ASSERT_EQ(1u, numPCs.size());

    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
    }
}
} // namespace ult
} // namespace L0
