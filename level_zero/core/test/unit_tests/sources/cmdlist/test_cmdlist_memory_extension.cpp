/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

class CommandListWaitOnMemFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();
        ze_result_t returnValue;
        commandList.reset(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
        commandListBcs.reset(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue)));

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = 2;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.signal = 0;

        eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        size_t size = sizeof(uint32_t);
        size_t alignment = 1u;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        auto result = context->allocDeviceMem(device->toHandle(),
                                              &deviceDesc,
                                              size, alignment, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);

        signalAllPackets = L0GfxCoreHelper::useSignalAllEventPackets(device->getHwInfo());
    }

    void tearDown() {
        context->freeMem(ptr);
        event.reset(nullptr);
        eventPool.reset(nullptr);
        commandListBcs.reset(nullptr);
        commandList.reset(nullptr);
        DeviceFixture::tearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<L0::ult::CommandList> commandListBcs;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
    void *ptr = nullptr;
    uint32_t waitMemData = 1u;
    bool signalAllPackets = false;
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListExtensionHw : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListExtensionHw() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}
    MockCommandListExtensionHw(bool failOnFirst) : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>(), failOnFirstCopy(failOnFirst) {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize, bool allowHostCopy) override {
        getAlignedAllocationCalledTimes++;
        if (buffer) {
            return {0, 0, &alignedAlloc, true};
        }
        return {0, 0, nullptr, false};
    }

    ze_result_t appendMemoryCopyKernelWithGA(void *dstPtr,
                                             NEO::GraphicsAllocation *dstPtrAlloc,
                                             uint64_t dstOffset,
                                             void *srcPtr,
                                             NEO::GraphicsAllocation *srcPtrAlloc,
                                             uint64_t srcOffset,
                                             uint64_t size,
                                             uint64_t elementSize,
                                             Builtin builtin,
                                             Event *signalEvent,
                                             bool isStateless,
                                             CmdListKernelLaunchParams &launchParams) override {
        appendMemoryCopyKernelWithGACalledTimes++;
        if (isStateless) {
            appendMemoryCopyKernelWithGAStatelessCalledTimes++;
        }
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        if (failOnFirstCopy &&
            (appendMemoryCopyKernelWithGACalledTimes == 1 || appendMemoryCopyKernelWithGAStatelessCalledTimes == 1)) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                     NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset, uintptr_t srcPtr,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset,
                                     uint64_t size) override {
        appendMemoryCopyBlitCalledTimes++;
        if (failOnFirstCopy && appendMemoryCopyBlitCalledTimes == 1) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcAllocation,
                                           NEO::GraphicsAllocation *dstAllocation,
                                           size_t srcOffset,
                                           size_t dstOffset,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                           Event *signalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        appendMemoryCopyBlitRegionCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         size_t srcOffset, Event *signalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        appendMemoryCopyKernel2dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         uint32_t srcSlicePitch, size_t srcOffset,
                                         Event *signalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents) override {
        appendMemoryCopyKernel3dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendBlitFill(void *ptr, const void *pattern,
                               size_t patternSize, size_t size,
                               Event *signalEvent, uint32_t numWaitEvents,
                               ze_event_handle_t *phWaitEvents) override {
        appendBlitFillCalledTimes++;
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendCopyImageBlit(NEO::GraphicsAllocation *src,
                                    NEO::GraphicsAllocation *dst,
                                    const Vec3<size_t> &srcOffsets, const Vec3<size_t> &dstOffsets,
                                    size_t srcRowPitch, size_t srcSlicePitch,
                                    size_t dstRowPitch, size_t dstSlicePitch,
                                    size_t bytesPerPixel, const Vec3<size_t> &copySize,
                                    const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                    Event *signalEvent) override {
        appendCopyImageBlitCalledTimes++;
        appendImageRegionCopySize = copySize;
        appendImageRegionSrcOrigin = srcOffsets;
        appendImageRegionDstOrigin = dstOffsets;
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        return ZE_RESULT_SUCCESS;
    }
    uint8_t mockAlignedAllocData[2 * MemoryConstants::pageSize]{};

    Vec3<size_t> appendImageRegionCopySize = {0, 0, 0};
    Vec3<size_t> appendImageRegionSrcOrigin = {9, 9, 9};
    Vec3<size_t> appendImageRegionDstOrigin = {9, 9, 9};

    void *alignedDataPtr = alignUp(mockAlignedAllocData, MemoryConstants::pageSize);

    NEO::MockGraphicsAllocation alignedAlloc{alignedDataPtr, reinterpret_cast<uint64_t>(alignedDataPtr), MemoryConstants::pageSize};

    uint32_t appendMemoryCopyKernelWithGACalledTimes = 0;
    uint32_t appendMemoryCopyKernelWithGAStatelessCalledTimes = 0;
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
    uint32_t appendMemoryCopyBlitRegionCalledTimes = 0;
    uint32_t appendMemoryCopyKernel2dCalledTimes = 0;
    uint32_t appendMemoryCopyKernel3dCalledTimes = 0;
    uint32_t appendBlitFillCalledTimes = 0;
    uint32_t appendCopyImageBlitCalledTimes = 0;
    uint32_t getAlignedAllocationCalledTimes = 0;
    bool failOnFirstCopy = false;
    bool useEvents = false;
};

using CommandListAppendWaitOnMem = Test<CommandListWaitOnMemFixture>;

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataAndNotEqualOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST2_F(CommandListAppendWaitOnMem, givenCommandListWaitOnMemoryCalledWithNullPtrThenAppendWaitOnMemoryReturnsError, IsAtLeastSkl) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MockCommandListExtensionHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    uint32_t waitMemData = 1u;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
    void *badPtr = nullptr;
    result = cmdList.appendWaitOnMemory(reinterpret_cast<void *>(&desc), badPtr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataAndEqualOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataGreaterOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataGreaterThanEqualOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataLessThanOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_LESS_THAN_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataLessThanEqualOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_LESS_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndInvalidOpThenReturnsInvalid) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_BIT(6);
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithSignalEventAndHostScopeThenSemaphoreWaitAndPipeControlProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;
    std::unique_ptr<EventPool> signalEventPool;
    std::unique_ptr<Event> signalEvent;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    signalEventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    signalEvent = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, signalEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto gpuAddress = event->getCompletionFieldGpuAddress(this->device);

    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuAddress;
    if (signalAllPackets) {
        expectedPostSyncStoreDataImm = event->getMaxPacketsCount() - 1;
    }

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedPostSyncStoreDataImm, itorStoreDataImm.size());

    for (size_t i = 0; i < expectedPostSyncStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        storeDataImmAddress += event->getSinglePacketSize();
    }

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor++;
    auto itorPC = findAll<PIPE_CONTROL *>(itor, cmdList.end());
    ASSERT_NE(0u, itorPC.size());

    auto pipeControlAddress = storeDataImmAddress;
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_EQ(pipeControlAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithSignalEventAndNoScopeThenSemaphoreWaitAndPipeControlProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;
    std::unique_ptr<EventPool> signalEventPool;
    std::unique_ptr<Event> signalEvent;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    signalEventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    signalEvent = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, signalEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto gpuAddress = event->getCompletionFieldGpuAddress(this->device);

    size_t expectedPostSyncStoreDataImm = 0;
    uint64_t storeDataImmAddress = gpuAddress;
    if (signalAllPackets) {
        expectedPostSyncStoreDataImm = event->getMaxPacketsCount() - 1;
    }

    auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedPostSyncStoreDataImm, itorStoreDataImm.size());

    for (size_t i = 0; i < expectedPostSyncStoreDataImm; i++) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
        EXPECT_EQ(storeDataImmAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        storeDataImmAddress += event->getSinglePacketSize();
    }

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor++;
    auto itorPC = findAll<PIPE_CONTROL *>(itor, cmdList.end());
    ASSERT_NE(0u, itorPC.size());

    auto pipeControlAddress = storeDataImmAddress;
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_EQ(pipeControlAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_FALSE(cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemOnBcsWithSignalEventAndNoScopeThenSemaphoreWaitAndFlushDwProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandListBcs->commandContainer;
    std::unique_ptr<EventPool> signalEventPool;
    std::unique_ptr<Event> signalEvent;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    signalEventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    signalEvent = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandListBcs->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, signalEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    auto itorFDW = findAll<MI_FLUSH_DW *>(itor, cmdList.end());
    ASSERT_NE(0u, itorFDW.size());
    bool postSyncFound = false;
    for (auto it : itorFDW) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*it);
        if (cmd->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            auto gpuAddress = event->getCompletionFieldGpuAddress(device);
            EXPECT_EQ(cmd->getDestinationAddress(), gpuAddress);
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithNoScopeAndSystemMemoryPtrThenAlignedPtrUsed, IsAtLeastSkl) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    size_t cmdListHostPtrSize = MemoryConstants::pageSize;
    void *cmdListHostBuffer = device->getNEODevice()->getMemoryManager()->allocateSystemMemory(cmdListHostPtrSize, cmdListHostPtrSize);
    void *startMemory = cmdListHostBuffer;
    void *baseAddress = alignDown(startMemory, MemoryConstants::pageSize);
    size_t expectedOffset = ptrDiff(startMemory, baseAddress);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device, startMemory, cmdListHostPtrSize, false);
    ASSERT_NE(nullptr, outData.alloc);
    auto expectedGpuAddress = static_cast<uintptr_t>(alignDown(outData.alloc->getGpuAddress(), MemoryConstants::pageSize));
    EXPECT_EQ(startMemory, outData.alloc->getUnderlyingBuffer());
    EXPECT_EQ(expectedGpuAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(expectedOffset, outData.offset);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), cmdListHostBuffer, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());
    EXPECT_EQ(expectedGpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);

    commandList->removeHostPtrAllocations();
    device->getNEODevice()->getMemoryManager()->freeSystemMemory(cmdListHostBuffer);
}

HWTEST2_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithHostMemAndNoScopeThenMiMemFenceEncoded, IsXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    auto hwInfo = commandList->commandContainer.getDevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0x03;
    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;

    constexpr size_t allocSize = sizeof(uint32_t);
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), dstBuffer, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto itor = find<MI_MEM_FENCE *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_MEM_FENCE *>(*itor);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_ACQUIRE, cmd->getFenceType());

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithDeviceMemAndNoScopeThenMiMemFenceNotEncoded, IsXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    auto hwInfo = commandList->commandContainer.getDevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0x03;
    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto itor = find<MI_MEM_FENCE *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

using CommandListAppendWriteToMem = Test<CommandListWaitOnMemFixture>;

HWTEST_F(CommandListAppendWriteToMem, givenAppendWriteToMemWithNoScopeThenPipeControlEncodedCorrectly) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_write_to_mem_desc_t desc = {};
    uint64_t data = 0xabc;
    result = commandList->appendWriteToMemory(reinterpret_cast<void *>(&desc), ptr, data);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), data);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_FALSE(cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendWriteToMem, givenCommandListWriteToMemCalledWithNullPtrThenAppendWriteToMemoryReturnsError, IsAtLeastSkl) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MockCommandListExtensionHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    zex_write_to_mem_desc_t desc = {};
    uint64_t data = 0xabc;
    void *badPtr = nullptr;
    result = cmdList.appendWriteToMemory(reinterpret_cast<void *>(&desc), badPtr, data);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
}

HWTEST_F(CommandListAppendWriteToMem, givenAppendWriteToMemOnBcsWithNoScopeThenFlushDwEncodedCorrectly) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandListBcs->commandContainer;

    zex_write_to_mem_desc_t desc = {};
    uint64_t data = 0xabc;
    result = commandListBcs->appendWriteToMemory(reinterpret_cast<void *>(&desc), ptr, data);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorFDW = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorFDW.size());
    bool postSyncFound = false;
    for (auto it : itorFDW) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*it);
        if (cmd->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) {
            EXPECT_EQ(cmd->getImmediateData(), data);
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendWriteToMem, givenAppendWriteToMemWithScopeThenPipeControlEncodedCorrectly) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_write_to_mem_desc_t desc = {};
    desc.writeScope = ZEX_MEM_ACTION_SCOPE_FLAG_HOST;
    uint64_t data = 0xabc;
    result = commandList->appendWriteToMemory(reinterpret_cast<void *>(&desc), ptr, data);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), data);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendWriteToMem, givenAppendWriteToMemWithScopeThenPipeControlEncodedCorrectlyAlignedPtrUsed, IsAtLeastSkl) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    size_t cmdListHostPtrSize = MemoryConstants::pageSize;
    void *cmdListHostBuffer = device->getNEODevice()->getMemoryManager()->allocateSystemMemory(cmdListHostPtrSize, cmdListHostPtrSize);
    void *startMemory = cmdListHostBuffer;
    void *baseAddress = alignDown(startMemory, MemoryConstants::pageSize);
    size_t expectedOffset = ptrDiff(startMemory, baseAddress);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device, startMemory, cmdListHostPtrSize, false);
    ASSERT_NE(nullptr, outData.alloc);
    auto expectedGpuAddress = static_cast<uintptr_t>(alignDown(outData.alloc->getGpuAddress(), MemoryConstants::pageSize));
    EXPECT_EQ(startMemory, outData.alloc->getUnderlyingBuffer());
    EXPECT_EQ(expectedGpuAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(expectedOffset, outData.offset);

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_write_to_mem_desc_t desc = {};
    desc.writeScope = ZEX_MEM_ACTION_SCOPE_FLAG_HOST;
    uint64_t data = 0xabc;
    result = commandList->appendWriteToMemory(reinterpret_cast<void *>(&desc), cmdListHostBuffer, data);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), data);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            uint64_t pcAddress = cmd->getAddress() | (static_cast<uint64_t>(cmd->getAddressHigh()) << 32);
            EXPECT_EQ(expectedGpuAddress, pcAddress);
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
    commandList->removeHostPtrAllocations();
    device->getNEODevice()->getMemoryManager()->freeSystemMemory(cmdListHostBuffer);
}

} // namespace ult
} // namespace L0
