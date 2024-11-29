/*
 * Copyright (C) 2020-2024 Intel Corporation
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
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

namespace L0 {
namespace ult {
using CommandListCreateTests = Test<CommandListCreateFixture>;
HWTEST2_F(CommandListCreateTests, givenCopyOnlyCommandListWhenAppendWriteGlobalTimestampCalledThenMiFlushDWWithTimestampEncoded, MatchAny) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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

HWTEST2_F(CommandListCreateTests, givenCommandListWhenAppendWriteGlobalTimestampCalledThenPipeControlWithTimestampWriteEncoded, MatchAny) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    uint64_t timestampAddress = 0x123456785500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

    const auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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

HWTEST2_F(CommandListCreateTests, givenMovedDstptrWhenAppendWriteGlobalTimestampCalledThenPipeControlWithProperTimestampEncoded, MatchAny) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    uint64_t timestampAddress = 0x123456785500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress) + 1;
    uint64_t expectedTimestampAddress = timestampAddress + sizeof(uint64_t);
    const auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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

HWTEST2_F(CommandListCreateTests, givenCommandListWhenAppendWriteGlobalTimestampCalledThenTimestampAllocationIsInsideResidencyContainer, MatchAny) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
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

HWTEST2_F(CommandListCreateTests, givenImmediateCommandListWhenAppendWriteGlobalTimestampThenReturnsSuccess, MatchAny) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
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

HWTEST2_F(CommandListCreateTests, givenUseCsrImmediateSubmissionEnabledForCopyImmediateCommandListThenAppendImageCopyRegionReturnsSuccess, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(true);
    const ze_command_queue_desc_t queueDesc = {};
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
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

HWTEST2_F(CommandListCreateTests, givenUseCsrImmediateSubmissionDisabledForCopyImmediateCommandListThenAppendImageCopyRegionReturnsSuccess, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(false);

    const ze_command_queue_desc_t queueDesc = {};

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
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

HWTEST_F(CommandListCreateTests, givenUseCsrImmediateSubmissionEnabledForCopyImmediateCommandListThenAppendMemoryCopyRegionReturnsSuccess) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::copy, returnValue);
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dr, 0, 0, srcPtr, &sr, 0, 0, nullptr, 0, nullptr, copyParams);
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    EXPECT_EQ(true, commandList->flushTaskSubmissionEnabled());
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenFlushTaskSubmissionDisabledWhenCommandListIsInititalizedThenFlushTaskIsSetToFalse) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    EXPECT_EQ(false, commandList->flushTaskSubmissionEnabled());
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendLaunchKernelThenSuccessIsReturned) {
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

using CommandListAppendLaunchKernelResetKernelCount = Test<DeviceFixture>;

HWTEST2_F(CommandListAppendLaunchKernelResetKernelCount, givenIsKernelSplitOperationFalseWhenAppendLaunchKernelThenResetKernelCount, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.CompactL3FlushEventPacket.set(0);
    NEO::debugManager.flags.UsePipeControlMultiKernelEventSync.set(0);

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

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
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

    size_t size = 0x100000001;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x100003456), size, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    auto result = commandList->appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenBindlessModeAndUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendPageFaultThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);
    NEO::debugManager.flags.UseBindlessMode.set(1);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);

    size_t size = 0x100000001;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x100003456), size, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    auto result = commandList->appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendEventResetThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendEventResetWithTimestampThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendEventResetWithTimestampThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendSignalEventThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(true);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendSignalEvent(event->toHandle(), false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendSignalEventWithTimestampThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendSignalEvent(event->toHandle(), false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendSignalEventWithTimestampThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendSignalEvent(event->toHandle(), false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendWaitOnEventWithTimestampThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendBarrierWithEventThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendBarrierWithTimestampEventThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendBarrierWithoutEventThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    auto result = commandList->appendBarrier(nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendBarrierWithEventThenSuccessIsReturned) {
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST2_F(CommandListCreateTests, givenImmediateCopyOnlyCmdListWhenAppendBarrierThenIncrementBarrierCountAndDispatchBarrierTagUpdate, MatchAny) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::copy, returnValue));
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(whiteBoxCmdList->getCsr(false)->getNextBarrierCount(), 0u);

    auto result = commandList->appendBarrier(nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(whiteBoxCmdList->getCsr(false)->getNextBarrierCount(), 2u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs)) {
        itor++;
    }
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
    EXPECT_EQ(cmd->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);
    EXPECT_EQ(cmd->getDestinationAddress(), whiteBoxCmdList->getCsr(false)->getBarrierCountGpuAddress());
    EXPECT_EQ(cmd->getImmediateData(), 2u);
}

HWTEST2_F(CommandListCreateTests, givenImmediateCopyOnlyCmdListWhenAppendWaitOnEventsThenIncrementBarrierCountAndDispatchBarrierTagUpdate, MatchAny) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::copy, returnValue));
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(whiteBoxCmdList->getCsr(false)->getNextBarrierCount(), 0u);

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

    result = commandList->appendWaitOnEvents(1u, &eventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(whiteBoxCmdList->getCsr(false)->getNextBarrierCount(), 2u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs)) {
        itor++;
    }
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
    EXPECT_EQ(cmd->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);
    EXPECT_EQ(cmd->getDestinationAddress(), whiteBoxCmdList->getCsr(false)->getBarrierCountGpuAddress());
    EXPECT_EQ(cmd->getImmediateData(), 2u);
}

HWTEST2_F(CommandListCreateTests, givenImmediateCopyOnlyCmdListWhenAppendWaitOnEventsWithTrackDependenciesSetToFalseThenDoNotIncrementBarrierCountAndDispatchBarrierTagUpdate, MatchAny) {
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::copy, returnValue));
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(whiteBoxCmdList->getCsr(false)->getNextBarrierCount(), 0u);

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

    result = commandList->appendWaitOnEvents(1u, &eventHandle, nullptr, false, false, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(whiteBoxCmdList->getCsr(false)->getNextBarrierCount(), 1u);
}

HWTEST_F(CommandListCreateTests, GivenCommandListWhenUnalignedPtrThenSingleCopyAdded) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->getDevice());

    void *srcPtr = reinterpret_cast<void *>(0x4321);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 2 * MemoryConstants::cacheLineSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<XY_COPY_BLT *>(++itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreateTests, whenCommandListIsCreatedThenFlagsAreCorrectlySet, MatchAny) {
    ze_command_list_flags_t flags[] = {0b0, 0b1, 0b10, 0b11};

    ze_result_t returnValue;
    for (auto flag : flags) {
        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, flag, returnValue, false));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        auto pCommandListCoreFamily = static_cast<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> *>(commandList.get());
        EXPECT_EQ(flag, pCommandListCoreFamily->flags);
    }
}

using HostPointerManagerCommandListTest = Test<HostPointerManagerFixure>;
HWTEST2_F(HostPointerManagerCommandListTest,
          givenImportedHostPointerWhenAppendMemoryFillUsingHostPointerThenAppendFillUsingHostPointerAllocation,
          MatchAny) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

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
          MatchAny) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    char pattern = 'a';
    ret = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), 64u, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest,
          givenHostPointerImportedWhenGettingAlignedAllocationThenRetrieveProperOffsetAndAddress,
          MatchAny) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

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

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device, importPointer, importSize, false, false);
    auto gpuBaseAddress = static_cast<size_t>(hostAllocation->getGpuAddress());
    auto expectedAlignedAddress = alignDown(gpuBaseAddress, NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    size_t expectedOffset = gpuBaseAddress - expectedAlignedAddress;
    EXPECT_EQ(importPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(expectedOffset, outData.offset);

    outData = commandList->getAlignedAllocationData(device, offsetPointer, offsetSize, false, false);
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
          MatchAny) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    size_t pointerSize = MemoryConstants::pageSize;
    size_t offset = 100u + 2 * MemoryConstants::pageSize;
    void *offsetPointer = ptrOffset(heapPointer, offset);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, heapSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto hostAllocation = hostDriverHandle->findHostPointerAllocation(offsetPointer, pointerSize, device->getRootDeviceIndex());
    ASSERT_NE(nullptr, hostAllocation);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device, offsetPointer, pointerSize, false, false);
    auto expectedAlignedAddress = static_cast<uintptr_t>(hostAllocation->getGpuAddress());
    EXPECT_EQ(heapPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(offset, outData.offset);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, MatchAny) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto pc = genCmdCast<PIPE_CONTROL *>(*cmdList.rbegin());

    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment())) {
        EXPECT_NE(nullptr, pc);
        EXPECT_TRUE(pc->getDcFlushEnable());
    } else {
        EXPECT_EQ(nullptr, pc);
    }
}

HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, MatchAny) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    char pattern = 'a';
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

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), size,
                                           events[0], 1, &events[1], false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndWaitEventsUsingRenderEngineThenSuccessIsReturned, MatchAny) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t ret = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
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

HWTEST2_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, MatchAny) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t ret = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::copy,
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

HWTEST2_F(HostPointerManagerCommandListTest, givenDebugModeToRegisterAllHostPointerWhenFindIsCalledThenRegisterHappens, MatchAny) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceHostPointerImport.set(1);
    void *testPtr = heapPointer;

    auto gfxAllocation = hostDriverHandle->findHostPointerAllocation(testPtr, 0x10u, device->getRootDeviceIndex());
    EXPECT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(testPtr, gfxAllocation->getUnderlyingBuffer());

    auto result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListCreateTests, givenStateBaseAddressTrackingStateWhenCommandListCreatedThenPlatformSurfaceHeapSizeUsed, MatchAny) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    ze_result_t returnValue;
    {
        debugManager.flags.EnableStateBaseAddressTracking.set(0);

        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

        auto &commandContainer = commandList->getCmdContainer();
        auto sshSize = commandContainer.getIndirectHeap(NEO::IndirectHeapType::surfaceState)->getMaxAvailableSpace();
        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto &productHelper = device->getProductHelper();
        auto expectedSshSize = gfxCoreHelper.getDefaultSshSize(productHelper);
        EXPECT_EQ(expectedSshSize, sshSize);
    }
    {
        debugManager.flags.EnableStateBaseAddressTracking.set(1);

        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

        auto &commandContainer = commandList->getCmdContainer();
        auto sshSize = commandContainer.getIndirectHeap(NEO::IndirectHeapType::surfaceState)->getMaxAvailableSpace();
        EXPECT_EQ(NEO::EncodeStates<FamilyType>::getSshHeapSize(), sshSize);
    }
}

using ImmediateCmdListSharedHeapsRegularFlushTaskTest = Test<ImmediateCmdListSharedHeapsFlushTaskFixture<0>>;

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendBarrierProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::Barrier);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendSignalEventProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::SignalEvent);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendResetEventProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::ResetEvent);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendWaitOnEventsProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::WaitOnEvents);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendWriteGlobalTimestampProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::WriteGlobalTimestamp);
}

HWTEST2_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest,
          givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendMemoryRangesBarrierProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::MemoryRangesBarrier);
}

using ImmediateCmdListSharedHeapsImmediateFlushTaskTest = Test<ImmediateCmdListSharedHeapsFlushTaskFixture<1>>;

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendBarrierProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::Barrier);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendSignalEventProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::SignalEvent);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendResetEventProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::ResetEvent);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendWaitOnEventsProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::WaitOnEvents);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendWriteGlobalTimestampProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::WriteGlobalTimestamp);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendMemoryRangesBarrierProvidesNoHeapInfo,
          MatchAny) {
    testBody<FamilyType>(NonKernelOperation::MemoryRangesBarrier);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenImmediateCommandListWhenFirstAppendIsNonKernelAppendAndSecondAppendIsKernelAppendThenExpectAllBaseAddressSbaCommandBeforeSecondAppend,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    if (ultCsr.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto &csrStream = ultCsr.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    appendNonKernelOperation(commandListImmediate.get(), NonKernelOperation::Barrier);
    size_t csrUsedAfter = csrStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        (csrUsedAfter - csrUsedBefore)));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto csrSshHeap = &ultCsr.getIndirectHeap(HeapType::surfaceState, MemoryConstants::pageSize64k);
    auto &commandContainer = commandList->getCmdContainer();
    auto ioh = commandContainer.getIndirectHeap(NEO::HeapType::indirectObject);
    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioh->getHeapGpuBase());

    sbaCmd = genCmdCast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ(csrSshHeap->getHeapGpuBase(), sbaCmd->getSurfaceStateBaseAddress());
    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
}

using ImmediateCommandListTest = Test<ModuleMutableCommandListFixture>;

HWTEST2_F(ImmediateCommandListTest, givenImmediateCommandListWhenClosingCommandListThenExpectNoEndingCmdDispatched, MatchAny) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::compute, returnValue));
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
    auto commandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;

    void *srcPtr = reinterpret_cast<void *>(0x12000);
    void *dstPtr = reinterpret_cast<void *>(0x23000);
    CmdListMemoryCopyParams copyParams = {};
    returnValue = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.dispatchMonitorFence);
}

HWTEST2_F(ImmediateCommandListTest, givenCopyEngineSyncCmdListWhenAppendingCopyOperationThenRequireMonitorFence, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    auto commandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;

    void *srcPtr = reinterpret_cast<void *>(0x12000);
    void *dstPtr = reinterpret_cast<void *>(0x23000);
    CmdListMemoryCopyParams copyParams = {};
    returnValue = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.dispatchMonitorFence);
}

HWTEST_F(CommandListCreateTests, givenDeviceWhenCreatingCommandListForInternalUsageThenInternalEngineGroupIsUsed) {
    ze_command_list_handle_t commandList = nullptr;
    ze_command_list_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    reinterpret_cast<L0::DeviceImp *>(device)->createInternalCommandList(&desc, &commandList);
    auto whiteboxCommandList = reinterpret_cast<WhiteBox<::L0::CommandListImp> *>(L0::CommandList::fromHandle(commandList));
    EXPECT_TRUE(whiteboxCommandList->internalUsage);
    whiteboxCommandList->destroy();
}

HWTEST_F(CommandListCreateTests, givenDeviceWhenCreatingCommandListForNotInternalUsageThenInternalEngineGroupIsNotUsed) {
    ze_command_list_handle_t commandList = nullptr;
    ze_command_list_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    device->createCommandList(&desc, &commandList);
    auto whiteboxCommandList = reinterpret_cast<WhiteBox<::L0::CommandListImp> *>(L0::CommandList::fromHandle(commandList));
    EXPECT_FALSE(whiteboxCommandList->internalUsage);
    whiteboxCommandList->destroy();
}

using CommandListScratchPatchPrivateHeapsTest = Test<CommandListScratchPatchFixture<0, 0, true>>;
using CommandListScratchPatchGlobalStatelessHeapsTest = Test<CommandListScratchPatchFixture<1, 0, true>>;

using CommandListScratchPatchPrivateHeapsStateInitTest = Test<CommandListScratchPatchFixture<0, 1, true>>;
using CommandListScratchPatchGlobalStatelessHeapsStateInitTest = Test<CommandListScratchPatchFixture<1, 1, true>>;

HWTEST2_F(CommandListScratchPatchPrivateHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchInline<FamilyType>(false);
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchTwiceThenCorrectAddressPatchedOnce, IsAtLeastXeHpcCore) {
    testScratchSameNotPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithBiggerScratchSlot1ThenNewCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchGrowingPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithChangedScratchControllerThenUpdatedCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchChangedControllerPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithCommandViewFlagThenScratchIsNotStoredToPatch, IsAtLeastXeHpcCore) {
    testScratchCommandViewNoPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithoutScratchAndExternalScratchFlagThenScratchIsPatched, IsAtLeastXeHpcCore) {
    testExternalScratchPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchInline<FamilyType>(false);
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchTwiceThenCorrectAddressPatchedOnce, IsAtLeastXeHpcCore) {
    testScratchSameNotPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithBiggerScratchSlot1ThenNewCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchGrowingPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithCommandViewFlagThenScratchIsNotStoredToPatch, IsAtLeastXeHpcCore) {
    testScratchCommandViewNoPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithoutScratchAndExternalScratchFlagThenScratchIsPatched, IsAtLeastXeHpcCore) {
    testExternalScratchPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchInline<FamilyType>(false);
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchTwiceThenCorrectAddressPatchedOnce, IsAtLeastXeHpcCore) {
    testScratchSameNotPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithBiggerScratchSlot1ThenNewCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchGrowingPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithCommandViewFlagThenScratchIsNotStoredToPatch, IsAtLeastXeHpcCore) {
    testScratchCommandViewNoPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithoutScratchAndExternalScratchFlagThenScratchIsPatched, IsAtLeastXeHpcCore) {
    testExternalScratchPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchInline<FamilyType>(false);
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchTwiceThenCorrectAddressPatchedOnce, IsAtLeastXeHpcCore) {
    testScratchSameNotPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithBiggerScratchSlot1ThenNewCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchGrowingPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithCommandViewFlagThenScratchIsNotStoredToPatch, IsAtLeastXeHpcCore) {
    testScratchCommandViewNoPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithoutScratchAndExternalScratchFlagThenScratchIsPatched, IsAtLeastXeHpcCore) {
    testExternalScratchPatching<FamilyType>();
}

} // namespace ult
} // namespace L0
