/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_command_encoder.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppendWriteGlobalTimestampCalledThenMiFlushDWWithTimestampEncoded, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto &commandContainer = commandList->getCmdContainer();
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    uint64_t dstAddress = 0xfffffffffff0L;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(dstAddress);
    commandContainer.getResidencyContainer().clear();

    const auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);

    auto residencyContainer = commandContainer.getResidencyContainer();
    auto timestampAlloc = residencyContainer[0];
    EXPECT_EQ(dstAddress, reinterpret_cast<uint64_t>(timestampAlloc->getUnderlyingBuffer()));
    auto timestampAddress = timestampAlloc->getGpuAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto iterator = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    bool postSyncFound = false;
    ASSERT_NE(0u, iterator.size());
    for (auto it : iterator) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*it);
        auto postSyncDestAddress = cmd->getDestinationAddress();
        if ((cmd->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER) &&
            (postSyncDestAddress == timestampAddress)) {
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendWriteGlobalTimestampCalledThenPipeControlWithTimestampWriteEncoded, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->getCmdContainer();

    uint64_t timestampAddress = 0x123456785500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

    const auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto pcList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pcList.size());
    bool foundTimestampPipeControl = false;
    for (auto it : pcList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_FALSE(cmd->getDcFlushEnable());
            EXPECT_EQ(timestampAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            foundTimestampPipeControl = true;
        }
    }
    EXPECT_TRUE(foundTimestampPipeControl);
}

HWTEST2_F(CommandListCreate, givenMovedDstptrWhenAppendWriteGlobalTimestampCalledThenPipeControlWithProperTimestampEncoded, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->getCmdContainer();

    uint64_t timestampAddress = 0x123456785500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress) + 1;
    uint64_t expectedTimestampAddress = timestampAddress + sizeof(uint64_t);
    const auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto pcList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pcList.size());
    bool foundTimestampPipeControl = false;
    for (auto it : pcList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_FALSE(cmd->getDcFlushEnable());
            EXPECT_EQ(expectedTimestampAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            foundTimestampPipeControl = true;
        }
    }
    EXPECT_TRUE(foundTimestampPipeControl);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendWriteGlobalTimestampCalledThenTimestampAllocationIsInsideResidencyContainer, IsAtLeastSkl) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    uint64_t timestampAddress = 0x123456785500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);
    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);

    auto &commandContainer = commandList->getCmdContainer();
    auto &residencyContainer = commandContainer.getResidencyContainer();
    const bool addressIsInContainer = std::any_of(residencyContainer.begin(), residencyContainer.end(), [timestampAddress](NEO::GraphicsAllocation *alloc) {
        return alloc->getGpuAddress() == timestampAddress;
    });
    EXPECT_TRUE(addressIsInContainer);
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenAppendWriteGlobalTimestampThenReturnsSuccess, IsAtLeastSkl) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    uint64_t timestampAddress = 0x123456785500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

    auto result = commandList0->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListCreate, givenUseCsrImmediateSubmissionEnabledForCopyImmediateCommandListThenAppendImageCopyRegionReturnsSuccess, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);
    const ze_command_queue_desc_t queueDesc = {};
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &desc);
    imageHWDst->initialize(device, &desc);

    returnValue = commandList0->appendImageCopy(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyFromMemory(imageHWDst->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyToMemory(dstPtr, imageHWSrc->toHandle(), nullptr, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST2_F(CommandListCreate, givenUseCsrImmediateSubmissionDisabledForCopyImmediateCommandListThenAppendImageCopyRegionReturnsSuccess, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    const ze_command_queue_desc_t queueDesc = {};

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &desc);
    imageHWDst->initialize(device, &desc);

    returnValue = commandList0->appendImageCopy(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyFromMemory(imageHWDst->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyToMemory(dstPtr, imageHWSrc->toHandle(), nullptr, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST_F(CommandListCreate, givenUseCsrImmediateSubmissionEnabledForCopyImmediateCommandListThenAppendMemoryCopyRegionReturnsSuccess) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Copy, returnValue);

    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dr, 0, 0, srcPtr, &sr, 0, 0, nullptr, 0, nullptr, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

class CommandListImmediateFlushTaskTests : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

using CommandListImmediateFlushTaskComputeTests = Test<CommandListImmediateFlushTaskTests>;
HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenImmediateCommandListIsInititalizedThenByDefaultFlushTaskSubmissionEnabled) {
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    EXPECT_EQ(true, commandList->flushTaskSubmissionEnabled());
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenFlushTaskSubmissionDisabledWhenCommandListIsInititalizedThenFlushTaskIsSetToFalse) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    EXPECT_EQ(false, commandList->flushTaskSubmissionEnabled());
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendLaunchKernelThenSuccessIsReturned) {
    Mock<::L0::KernelImp> kernel;

    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

using CommandListAppendLaunchKernelResetKernelCount = Test<DeviceFixture>;

HWTEST2_F(CommandListAppendLaunchKernelResetKernelCount, givenIsKernelSplitOperationFalseWhenAppendLaunchKernelThenResetKernelCount, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;

    NEO::DebugManager.flags.CompactL3FlushEventPacket.set(0);
    NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.set(0);

    Mock<::L0::KernelImp> kernel;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    CmdListKernelLaunchParams launchParams = {};
    {
        event->zeroKernelCount();
        event->increaseKernelCount();
        event->increaseKernelCount();
        launchParams.isKernelSplitOperation = true;

        result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(2u, event->getKernelCount());
    }
    {
        launchParams.isKernelSplitOperation = false;
        result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(1u, event->getKernelCount());
    }
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendPageFaultThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    size_t size = 0x100000001;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x100003456), size, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    auto result = commandList->appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenBindlessModeAndUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendPageFaultThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);
    NEO::DebugManager.flags.UseBindlessMode.set(1);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());

    size_t size = 0x100000001;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x100003456), size, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    auto result = commandList->appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendEventResetThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendEventResetWithTimestampThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendEventResetWithTimestampThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendSignalEventThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendSignalEvent(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendSignalEventWithTimestampThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendSignalEvent(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendSignalEventWithTimestampThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendSignalEvent(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendWaitOnEventWithTimestampThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle, false, true, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendBarrierWithEventThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendBarrierWithTimestampEventThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendBarrierWithoutEventThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    auto result = commandList->appendBarrier(nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendBarrierWithEventThenSuccessIsReturned) {
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST2_F(CommandListCreate, givenImmediateCopyOnlyCmdListWhenAppendBarrierThenIncrementBarrierCountAndDispatchBarrierTagUpdate, IsAtLeastSkl) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(whiteBoxCmdList->csr->getNextBarrierCount(), 0u);

    auto result = commandList->appendBarrier(nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(whiteBoxCmdList->csr->getNextBarrierCount(), 2u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    NEO::EncodeDummyBlitWaArgs waArgs{true, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs)) {
        itor++;
    }
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
    EXPECT_EQ(cmd->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);
    EXPECT_EQ(cmd->getDestinationAddress(), whiteBoxCmdList->csr->getBarrierCountGpuAddress());
    EXPECT_EQ(cmd->getImmediateData(), 2u);
}

HWTEST2_F(CommandListCreate, givenImmediateCopyOnlyCmdListWhenAppendWaitOnEventsThenIncrementBarrierCountAndDispatchBarrierTagUpdate, IsAtLeastSkl) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(whiteBoxCmdList->csr->getNextBarrierCount(), 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    auto eventHandle = event->toHandle();

    result = commandList->appendWaitOnEvents(1u, &eventHandle, false, true, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(whiteBoxCmdList->csr->getNextBarrierCount(), 2u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    NEO::EncodeDummyBlitWaArgs waArgs{true, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs)) {
        itor++;
    }
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
    EXPECT_EQ(cmd->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);
    EXPECT_EQ(cmd->getDestinationAddress(), whiteBoxCmdList->csr->getBarrierCountGpuAddress());
    EXPECT_EQ(cmd->getImmediateData(), 2u);
}

HWTEST2_F(CommandListCreate, givenImmediateCopyOnlyCmdListWhenAppendWaitOnEventsWithTrackDependenciesSetToFalseThenDoNotIncrementBarrierCountAndDispatchBarrierTagUpdate, IsAtLeastSkl) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(whiteBoxCmdList->csr->getNextBarrierCount(), 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    auto eventHandle = event->toHandle();

    result = commandList->appendWaitOnEvents(1u, &eventHandle, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(whiteBoxCmdList->csr->getNextBarrierCount(), 1u);
}

HWTEST_F(CommandListCreate, GivenCommandListWhenUnalignedPtrThenSingleCopyAdded) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->getDevice());

    void *srcPtr = reinterpret_cast<void *>(0x4321);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 2 * MemoryConstants::cacheLineSize, nullptr, 0, nullptr, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<XY_COPY_BLT *>(++itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, whenCommandListIsCreatedThenFlagsAreCorrectlySet, IsAtLeastSkl) {
    ze_command_list_flags_t flags[] = {0b0, 0b1, 0b10, 0b11};

    ze_result_t returnValue;
    for (auto flag : flags) {
        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, flag, returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        auto pCommandListCoreFamily = static_cast<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> *>(commandList.get());
        EXPECT_EQ(flag, pCommandListCoreFamily->flags);
    }
}

using HostPointerManagerCommandListTest = Test<HostPointerManagerFixure>;
HWTEST2_F(HostPointerManagerCommandListTest,
          givenImportedHostPointerWhenAppendMemoryFillUsingHostPointerThenAppendFillUsingHostPointerAllocation,
          IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    int pattern = 1;
    ret = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), 64u, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest,
          givenImportedHostPointerAndCopyEngineWhenAppendMemoryFillUsingHostPointerThenAppendFillUsingHostPointerAllocation,
          IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    int pattern = 1;
    ret = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), 64u, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest,
          givenHostPointerImportedWhenGettingAlignedAllocationThenRetrieveProperOffsetAndAddress,
          IsAtLeastSkl) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    size_t mainOffset = 100;
    size_t importSize = 100;
    void *importPointer = ptrOffset(heapPointer, mainOffset);

    auto ret = hostDriverHandle->importExternalPointer(importPointer, importSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto hostAllocation = hostDriverHandle->findHostPointerAllocation(importPointer, importSize, device->getRootDeviceIndex());
    ASSERT_NE(nullptr, hostAllocation);

    size_t allocOffset = 10;
    size_t offsetSize = 20;
    void *offsetPointer = ptrOffset(importPointer, allocOffset);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device, importPointer, importSize, false);
    auto gpuBaseAddress = static_cast<size_t>(hostAllocation->getGpuAddress());
    auto expectedAlignedAddress = alignDown(gpuBaseAddress, NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    size_t expectedOffset = gpuBaseAddress - expectedAlignedAddress;
    EXPECT_EQ(importPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(expectedOffset, outData.offset);

    outData = commandList->getAlignedAllocationData(device, offsetPointer, offsetSize, false);
    expectedOffset += allocOffset;
    EXPECT_EQ(importPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(expectedOffset, outData.offset);

    ret = hostDriverHandle->releaseImportedPointer(importPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest,
          givenHostPointerImportedWhenGettingPointerFromAnotherPageThenRetrieveBaseAddressAndProperOffset,
          IsAtLeastSkl) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    size_t pointerSize = MemoryConstants::pageSize;
    size_t offset = 100u + 2 * MemoryConstants::pageSize;
    void *offsetPointer = ptrOffset(heapPointer, offset);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, heapSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto hostAllocation = hostDriverHandle->findHostPointerAllocation(offsetPointer, pointerSize, device->getRootDeviceIndex());
    ASSERT_NE(nullptr, hostAllocation);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device, offsetPointer, pointerSize, false);
    auto expectedAlignedAddress = static_cast<uintptr_t>(hostAllocation->getGpuAddress());
    EXPECT_EQ(heapPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(offset, outData.offset);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    auto &commandContainer = commandList->getCmdContainer();

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1, &events[1], false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto pc = genCmdCast<PIPE_CONTROL *>(*cmdList.rbegin());

    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment())) {
        EXPECT_NE(nullptr, pc);
        EXPECT_TRUE(pc->getDcFlushEnable());
    } else {
        EXPECT_EQ(nullptr, pc);
    }
}

using SupportedPlatformsSklIcllp = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE>;
HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndInvalidWaitHandleUsingRenderEngineThenErrorIsReturnedAndPipeControlIsNotAdded, SupportedPlatformsSklIcllp) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    auto &commandContainer = commandList->getCmdContainer();

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    auto offset = commandContainer.getCommandStream()->getUsed();

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), offset), commandContainer.getCommandStream()->getUsed() - offset));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, IsAtLeastSkl) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1, &events[1], false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndiInvalidWaitHandleUsingCopyEngineThenErrorIsReturned, SupportedPlatformsSklIcllp) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    auto &commandContainer = commandList->getCmdContainer();

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndWaitEventsUsingRenderEngineThenSuccessIsReturned, IsAtLeastSkl) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t ret = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               ret));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), context, 0, nullptr, &eventPoolDesc, ret));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ret = commandList0->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                         events[0], 1, &events[1], false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, IsAtLeastSkl) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t ret = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               ret));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    if (neoDevice->getInternalCopyEngine()) {
        EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalCopyEngine()->commandStreamReceiver);
    } else {
        EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);
    }

    ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), context, 0, nullptr, &eventPoolDesc, ret));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ret = commandList0->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                         events[0], 1, &events[1], false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndInvalidWaitHandleUsingCopyEngineThenErrorIsReturned, SupportedPlatformsSklIcllp) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t ret = ZE_RESULT_SUCCESS;
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               ret));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    if (neoDevice->getInternalCopyEngine()) {
        EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalCopyEngine()->commandStreamReceiver);
    } else {
        EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);
    }

    ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), context, 0, nullptr, &eventPoolDesc, ret));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ret = commandList0->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                         events[0], 1u, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenDebugModeToRegisterAllHostPointerWhenFindIsCalledThenRegisterHappens, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceHostPointerImport.set(1);
    void *testPtr = heapPointer;

    auto gfxAllocation = hostDriverHandle->findHostPointerAllocation(testPtr, 0x10u, device->getRootDeviceIndex());
    EXPECT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(testPtr, gfxAllocation->getUnderlyingBuffer());

    auto result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListCreate, givenStateBaseAddressTrackingStateWhenCommandListCreatedThenPlatformSurfaceHeapSizeUsed, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;

    ze_result_t returnValue;
    {
        DebugManager.flags.EnableStateBaseAddressTracking.set(0);

        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

        auto &commandContainer = commandList->getCmdContainer();
        auto sshSize = commandContainer.getIndirectHeap(NEO::IndirectHeapType::SURFACE_STATE)->getMaxAvailableSpace();
        EXPECT_EQ(NEO::HeapSize::defaultHeapSize, sshSize);
    }
    {
        DebugManager.flags.EnableStateBaseAddressTracking.set(1);

        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

        auto &commandContainer = commandList->getCmdContainer();
        auto sshSize = commandContainer.getIndirectHeap(NEO::IndirectHeapType::SURFACE_STATE)->getMaxAvailableSpace();
        EXPECT_EQ(NEO::EncodeStates<FamilyType>::getSshHeapSize(), sshSize);
    }
}

using ImmediateCmdListSharedHeapsRegularFlushTaskTest = Test<ImmediateCmdListSharedHeapsFlushTaskFixture<0>>;

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendBarrierProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::Barrier);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendSignalEventProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::SignalEvent);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendResetEventProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::ResetEvent);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendWaitOnEventsProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::WaitOnEvents);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendWriteGlobalTimestampProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::WriteGlobalTimestamp);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendMemoryRangesBarrierProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::MemoryRangesBarrier);
}

using ImmediateCmdListSharedHeapsImmediateFlushTaskTest = Test<ImmediateCmdListSharedHeapsFlushTaskFixture<1>>;

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendBarrierProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::Barrier);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendSignalEventProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::SignalEvent);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendResetEventProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::ResetEvent);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendWaitOnEventsProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::WaitOnEvents);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendWriteGlobalTimestampProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::WriteGlobalTimestamp);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendMemoryRangesBarrierProvidesNoHeapInfo,
          IsAtLeastSkl) {
    testBody<FamilyType>(NonKernelOperation::MemoryRangesBarrier);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenImmediateCommandListWhenFirstAppendIsNonKernelAppendAndSecondAppendIsKernelAppendThenExpectAllBaseAddressSbaCommandBeforeSecondAppend,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = ultCsr.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    appendNonKernelOperation(commandListImmediate.get(), NonKernelOperation::Barrier);
    size_t csrUsedAfter = csrStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        (csrUsedAfter - csrUsedBefore)));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = genCmdCast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ(0u, sbaCmd->getSurfaceStateBaseAddress());
    EXPECT_FALSE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_FALSE(sbaCmd->getGeneralStateBaseAddressModifyEnable());

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = ZE_RESULT_SUCCESS;

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    csrUsedAfter = csrStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        (csrUsedAfter - csrUsedBefore)));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto csrSshHeap = &ultCsr.getIndirectHeap(HeapType::SURFACE_STATE, MemoryConstants::pageSize64k);
    auto &commandContainer = commandList->getCmdContainer();
    auto ioh = commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioh->getHeapGpuBase());

    sbaCmd = genCmdCast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ(csrSshHeap->getHeapGpuBase(), sbaCmd->getSurfaceStateBaseAddress());
    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
}

using ImmediateCommandListTest = Test<ModuleMutableCommandListFixture>;

HWTEST2_F(ImmediateCommandListTest, givenImmediateCommandListWhenClosingCommandListThenExpectNoEndingCmdDispatched, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Compute, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    returnValue = commandListImmediate.close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto &commandContainer = commandListImmediate.commandContainer;
    EXPECT_EQ(0u, commandContainer.getCommandStream()->getUsed());
}

HWTEST2_F(ImmediateCommandListTest, givenCopyEngineAsyncCmdListWhenAppendingCopyOperationThenDoNotRequireMonitorFence, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;
    auto commandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    void *srcPtr = reinterpret_cast<void *>(0x12000);
    void *dstPtr = reinterpret_cast<void *>(0x23000);

    returnValue = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.dispatchMonitorFence);
}

HWTEST2_F(ImmediateCommandListTest, givenCopyEngineSyncCmdListWhenAppendingCopyOperationThenRequireMonitorFence, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    auto commandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    void *srcPtr = reinterpret_cast<void *>(0x12000);
    void *dstPtr = reinterpret_cast<void *>(0x23000);

    returnValue = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.dispatchMonitorFence);
}

} // namespace ult
} // namespace L0
