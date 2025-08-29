/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_command_encoder.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

namespace L0 {
namespace ult {
using CommandListCreateTests = Test<CommandListCreateFixture>;
HWTEST_F(CommandListCreateTests, givenCopyOnlyCommandListWhenAppendWriteGlobalTimestampCalledThenMiFlushDWWithTimestampEncoded) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
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

HWTEST_F(CommandListCreateTests, givenCommandListWhenAppendWriteGlobalTimestampCalledThenPipeControlWithTimestampWriteEncoded) {
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

HWTEST_F(CommandListCreateTests, givenMovedDstptrWhenAppendWriteGlobalTimestampCalledThenPipeControlWithProperTimestampEncoded) {
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

HWTEST_F(CommandListCreateTests, givenCommandListWhenAppendWriteGlobalTimestampCalledThenTimestampAllocationIsInsideResidencyContainer) {
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

HWTEST_F(CommandListCreateTests, givenImmediateCommandListWhenAppendWriteGlobalTimestampThenReturnsSuccess) {
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

HWTEST2_F(CommandListCreateTests, givenUseCsrImmediateSubmissionEnabledForCopyImmediateCommandListThenAppendImageCopyRegionReturnsSuccess, IsAtLeastXeCore) {
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
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &desc);
    imageHWDst->initialize(device, &desc);
    CmdListMemoryCopyParams copyParams = {};
    returnValue = commandList0->appendImageCopy(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyFromMemory(imageHWDst->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyToMemory(dstPtr, imageHWSrc->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST_F(CommandListCreateTests, givenUseCsrImmediateSubmissionEnabledForCopyImmediateCommandListThenAppendMemoryCopyRegionReturnsSuccess) {
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

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendLaunchKernelThenSuccessIsReturned) {
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

using CommandListAppendLaunchKernelResetKernelCount = Test<DeviceFixture>;

HWTEST2_F(CommandListAppendLaunchKernelResetKernelCount, givenIsKernelSplitOperationFalseWhenAppendLaunchKernelThenResetKernelCount, IsAtLeastXeCore) {
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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    CmdListKernelLaunchParams launchParams = {};
    {
        event->zeroKernelCount();
        event->increaseKernelCount();
        event->increaseKernelCount();
        launchParams.isKernelSplitOperation = true;

        result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(2u, event->getKernelCount());
    }
    {
        launchParams.isKernelSplitOperation = false;
        result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(1u, event->getKernelCount());
    }
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendPageFaultThenSuccessIsReturned) {
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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendEventResetWithTimestampThenSuccessIsReturned) {
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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendSignalEventThenSuccessIsReturned) {
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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendSignalEvent(event->toHandle(), false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendSignalEventWithTimestampThenSuccessIsReturned) {
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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendSignalEvent(event->toHandle(), false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendWaitOnEventWithTimestampThenSuccessIsReturned) {

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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendBarrierWithEventThenSuccessIsReturned) {
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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendBarrierWithTimestampEventThenSuccessIsReturned) {

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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendBarrierWithoutEventThenSuccessIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    auto result = commandList->appendBarrier(nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListImmediateFlushTaskComputeTests, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendBarrierWithEventThenSuccessIsReturned) {

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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->destroy();
}

HWTEST_F(CommandListCreateTests, givenImmediateCopyOnlyCmdListWhenAppendBarrierThenIncrementBarrierCountAndDispatchBarrierTagUpdate) {
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

HWTEST_F(CommandListCreateTests, givenImmediateCopyOnlyCmdListWhenAppendWaitOnEventsThenIncrementBarrierCountAndDispatchBarrierTagUpdate) {
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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
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

HWTEST_F(CommandListCreateTests, givenImmediateCopyOnlyCmdListWhenAppendWaitOnEventsWithTrackDependenciesSetToFalseThenDoNotIncrementBarrierCountAndDispatchBarrierTagUpdate) {
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
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
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

HWTEST_F(CommandListCreateTests, whenCommandListIsCreatedThenFlagsAreCorrectlySet) {
    ze_command_list_flags_t flags[] = {0b0, 0b1, 0b10, 0b11};

    ze_result_t returnValue;
    for (auto flag : flags) {
        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, flag, returnValue, false));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        auto pCommandListCoreFamily = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(commandList.get());
        EXPECT_EQ(flag, pCommandListCoreFamily->flags);
    }
}

using HostPointerManagerCommandListTest = Test<HostPointerManagerFixure>;
HWTEST_F(HostPointerManagerCommandListTest, givenImportedHostPointerWhenAppendMemoryFillUsingHostPointerThenAppendFillUsingHostPointerAllocation) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    CmdListMemoryCopyParams copyParams = {};
    int pattern = 1;
    ret = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), 64u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(HostPointerManagerCommandListTest, givenImportedHostPointerAndCopyEngineWhenAppendMemoryFillUsingHostPointerThenAppendFillUsingHostPointerAllocation) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    CmdListMemoryCopyParams copyParams = {};
    char pattern = 'a';
    ret = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), 64u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(HostPointerManagerCommandListTest, givenHostPointerImportedWhenGettingAlignedAllocationThenRetrieveProperOffsetAndAddress) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
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

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device, false, importPointer, importSize, false, false);
    auto gpuBaseAddress = static_cast<size_t>(hostAllocation->getGpuAddress());
    auto expectedAlignedAddress = alignDown(gpuBaseAddress, NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment());
    size_t expectedOffset = gpuBaseAddress - expectedAlignedAddress;
    EXPECT_EQ(importPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(expectedOffset, outData.offset);

    outData = commandList->getAlignedAllocationData(device, false, offsetPointer, offsetSize, false, false);
    expectedOffset += allocOffset;
    EXPECT_EQ(importPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(expectedOffset, outData.offset);

    ret = hostDriverHandle->releaseImportedPointer(importPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(HostPointerManagerCommandListTest, givenHostPointerImportedWhenGettingPointerFromAnotherPageThenRetrieveBaseAddressAndProperOffset) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    size_t pointerSize = MemoryConstants::pageSize;
    size_t offset = 100u + 2 * MemoryConstants::pageSize;
    void *offsetPointer = ptrOffset(heapPointer, offset);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, heapSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto hostAllocation = hostDriverHandle->findHostPointerAllocation(offsetPointer, pointerSize, device->getRootDeviceIndex());
    ASSERT_NE(nullptr, hostAllocation);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device, false, offsetPointer, pointerSize, false, false);
    auto expectedAlignedAddress = static_cast<uintptr_t>(hostAllocation->getGpuAddress());
    EXPECT_EQ(heapPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(offset, outData.offset);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
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
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    events.push_back(event1.get());

    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1, &events[1], copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto pc = genCmdCast<PIPE_CONTROL *>(*cmdList.rbegin());

    auto l3FlushAfterPostSupported = device->getProductHelper().isL3FlushAfterPostSyncSupported(device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo));

    if (!l3FlushAfterPostSupported && NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment())) {
        EXPECT_NE(nullptr, pc);
        EXPECT_TRUE(pc->getDcFlushEnable());
    } else {
        EXPECT_EQ(nullptr, pc);
    }
}

HWTEST_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    events.push_back(event1.get());

    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), size,
                                           events[0], 1, &events[1], copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndWaitEventsUsingRenderEngineThenSuccessIsReturned) {
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, ret));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, ret));
    events.push_back(event1.get());

    CmdListMemoryCopyParams copyParams = {};
    ret = commandList0->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                         events[0], 1, &events[1], copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned) {
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, ret));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, ret));
    events.push_back(event1.get());

    CmdListMemoryCopyParams copyParams = {};
    ret = commandList0->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                         events[0], 1, &events[1], copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(HostPointerManagerCommandListTest, givenDebugModeToRegisterAllHostPointerWhenFindIsCalledThenRegisterHappens) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceHostPointerImport.set(1);
    void *testPtr = heapPointer;

    auto gfxAllocation = hostDriverHandle->findHostPointerAllocation(testPtr, 0x10u, device->getRootDeviceIndex());
    EXPECT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(testPtr, gfxAllocation->getUnderlyingBuffer());

    auto result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListCreateTests, givenStateBaseAddressTrackingStateWhenCommandListCreatedThenPlatformSurfaceHeapSizeUsed) {
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

HWTEST_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest, givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendBarrierProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::Barrier);
}

HWTEST_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest, givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendSignalEventProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::SignalEvent);
}

HWTEST_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest, givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendResetEventProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::ResetEvent);
}

HWTEST_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest, givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendWaitOnEventsProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::WaitOnEvents);
}

HWTEST_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest, givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendWriteGlobalTimestampProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::WriteGlobalTimestamp);
}

HWTEST_F(ImmediateCmdListSharedHeapsRegularFlushTaskTest, givenMultipleCommandListsUsingRegularWhenOldSharedHeapIsDepletedThenNonKernelAppendMemoryRangesBarrierProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::MemoryRangesBarrier);
}

using ImmediateCmdListSharedHeapsImmediateFlushTaskTest = Test<ImmediateCmdListSharedHeapsFlushTaskFixture<1>>;

HWTEST_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest, givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendBarrierProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::Barrier);
}

HWTEST_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest, givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendSignalEventProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::SignalEvent);
}

HWTEST_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest, givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendResetEventProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::ResetEvent);
}

HWTEST_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest, givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendWaitOnEventsProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::WaitOnEvents);
}

HWTEST_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest, givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendWriteGlobalTimestampProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::WriteGlobalTimestamp);
}

HWTEST_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest, givenMultipleCommandListsUsingImmediateWhenOldSharedHeapIsDepletedThenNonKernelAppendMemoryRangesBarrierProvidesNoHeapInfo) {
    testBody<FamilyType>(NonKernelOperation::MemoryRangesBarrier);
}

HWTEST2_F(ImmediateCmdListSharedHeapsImmediateFlushTaskTest,
          givenImmediateCommandListWhenFirstAppendIsNonKernelAppendAndSecondAppendIsKernelAppendThenExpectAllBaseAddressSbaCommandBeforeSecondAppend,
          IsAtLeastXeCore) {
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
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
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

HWTEST_F(ImmediateCommandListTest, givenImmediateCommandListWhenClosingCommandListThenExpectNoEndingCmdDispatched) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::compute, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

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

    constexpr size_t transferSize = sizeof(size_t);
    void *src, *dst;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, transferSize, 0u, &src));
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, transferSize, 0u, &dst));

    CmdListMemoryCopyParams copyParams = {};
    returnValue = commandList->appendMemoryCopy(dst, src, transferSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.dispatchMonitorFence);

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(src));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(dst));
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

    size_t src = 0;
    size_t dst = 0;
    CmdListMemoryCopyParams copyParams = {};
    returnValue = commandList->appendMemoryCopy(&dst, &src, sizeof(size_t), nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.dispatchMonitorFence);
}

HWTEST2_F(ImmediateCommandListTest, givenCopyEngineAsyncCmdListWhenAppendingRegularCmdListThenFlushTaskRequired, IsAtLeastXeHpcCore) {
    ze_result_t returnValue;

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    auto cmdListHandle = commandList->toHandle();
    commandList->close();

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    auto commandListImmediate = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandListImmediate);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandListImmediate.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;

    returnValue = commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);
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

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchInline<FamilyType>(false, false);
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithPatchPreambleWithScratchThenExpectCorrectEncoding, IsAtLeastXeHpcCore) {
    testScratchInline<FamilyType>(false, true);
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

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithUndefinedScratchAddressThenScratchIsNotStoredToPatch, IsAtLeastXeHpcCore) {
    testScratchUndefinedPatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXeHpcCore) {
    testScratchInline<FamilyType>(false, false);
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithPatchPreambleWithScratchThenExpectCorrectEncoding, IsAtLeastXeHpcCore) {
    testScratchInline<FamilyType>(false, true);
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

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTest,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingKernelWithUndefinedScratchAddressThenScratchIsNotStoredToPatch, IsAtLeastXeHpcCore) {
    testScratchUndefinedPatching<FamilyType>();
}

HWTEST_F(ImmediateCommandListTest, givenImmediateCmdListWhenAppendingRegularThenImmediateStreamIsSelected) {
    commandList->close();
    auto cmdListHandle = commandList->toHandle();

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.recursiveLockCounter = 0;

    // first append can carry preamble
    commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);

    EXPECT_EQ(1u, ultCsr.recursiveLockCounter);

    // regular append can dispatch bb_start to secondary regular or primary directly regular
    commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);

    EXPECT_EQ(2u, ultCsr.recursiveLockCounter);

    auto startStream = static_cast<L0::CommandQueueImp *>(commandListImmediate->cmdQImmediate)->getStartingCmdBuffer();

    if (commandListImmediate->getCmdListBatchBufferFlag()) {
        auto expectedStreamAllocation = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();
        EXPECT_EQ(expectedStreamAllocation, startStream->getGraphicsAllocation());
    } else {
        auto expectedStream = commandListImmediate->getCmdContainer().getCommandStream();
        EXPECT_EQ(expectedStream, startStream);
    }
}

HWTEST_F(ImmediateCommandListTest, givenImmediateCmdListWhenAppendingRegularCmdListThenDoNotConsumeQueueInternalLinearStream) {
    auto immediateQueue = whiteboxCast(commandListImmediate->cmdQImmediate);

    commandList->close();
    auto cmdListHandle = commandList->toHandle();

    auto usedBefore = immediateQueue->commandStream.getUsed();
    commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);
    auto usedAfter = immediateQueue->commandStream.getUsed();

    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST_F(ImmediateCommandListTest, givenImmediateCmdListWithPrimaryBatchBufferWhenAppendingRegularCmdListThenCorrectEpilogueCmdBufferIsUsed) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    commandList->close();
    auto cmdListHandle = commandList->toHandle();

    auto regularCmdBufferStream = commandList->getCmdContainer().getCommandStream();
    auto regularCmdBufferAllocation = regularCmdBufferStream->getGraphicsAllocation();

    auto cmdQImmediate = static_cast<WhiteBox<::L0::CommandQueue> *>(commandListImmediate->cmdQImmediate);

    commandListImmediate->dispatchCmdListBatchBufferAsPrimary = true;
    cmdQImmediate->dispatchCmdListBatchBufferAsPrimary = true;
    auto dispatchRegularBufferLinearStream = &cmdQImmediate->firstCmdListStream;

    // first append can carry preamble
    commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);

    ultCsr.recordFlushedBatchBuffer = true;

    auto immediateCmdBufferStream = commandListImmediate->getCmdContainer().getCommandStream();
    auto immediateCmdBufferOffset = immediateCmdBufferStream->getUsed();

    // no preamble - regular cmdlist buffer will be first and immediate cmd buffer will be epilogue
    commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);

    if (L0GfxCoreHelper::useImmediateComputeFlushTask(device->getNEODevice()->getRootDeviceEnvironment())) {
        EXPECT_EQ(NEO::AppendOperations::cmdList, ultCsr.recordedImmediateDispatchFlags.dispatchOperation);
        EXPECT_EQ(dispatchRegularBufferLinearStream, ultCsr.lastFlushedImmediateCommandStream);
        EXPECT_EQ(immediateCmdBufferStream, ultCsr.recordedImmediateDispatchFlags.optionalEpilogueCmdStream);
    } else {
        EXPECT_EQ(dispatchRegularBufferLinearStream, ultCsr.lastFlushedCommandStream);
        EXPECT_EQ(immediateCmdBufferStream, ultCsr.recordedDispatchFlags.optionalEpilogueCmdStream);
    }
    EXPECT_EQ(regularCmdBufferAllocation, ultCsr.latestFlushedBatchBuffer.commandBufferAllocation);

    auto startStream = static_cast<L0::CommandQueueImp *>(commandListImmediate->cmdQImmediate)->getStartingCmdBuffer();
    EXPECT_EQ(dispatchRegularBufferLinearStream, startStream);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(immediateCmdBufferStream->getCpuBase(), immediateCmdBufferOffset),
        immediateCmdBufferStream->getUsed() - immediateCmdBufferOffset));

    auto iterator = find<MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), iterator);
}

HWTEST_F(ImmediateCommandListTest, givenCopyEngineImmediateCmdListWithPrimaryBatchBufferWhenAppendingRegularCmdListThenCorrectEpilogueCmdBufferIsUsed) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    ze_result_t returnValue;

    commandList.reset(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false)));
    commandList->close();
    auto cmdListHandle = commandList->toHandle();

    ze_command_queue_desc_t desc = {};
    commandListImmediate.reset(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue)));

    auto regularCmdBufferStream = commandList->getCmdContainer().getCommandStream();
    auto regularCmdBufferAllocation = regularCmdBufferStream->getGraphicsAllocation();

    auto cmdQImmediate = static_cast<WhiteBox<::L0::CommandQueue> *>(commandListImmediate->cmdQImmediate);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(cmdQImmediate->csr);

    commandListImmediate->dispatchCmdListBatchBufferAsPrimary = true;
    cmdQImmediate->dispatchCmdListBatchBufferAsPrimary = true;
    auto dispatchRegularBufferLinearStream = &cmdQImmediate->firstCmdListStream;

    // first append can carry preamble
    commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);

    ultCsr->recordFlushedBatchBuffer = true;

    auto immediateCmdBufferStream = commandListImmediate->getCmdContainer().getCommandStream();
    auto immediateCmdBufferOffset = immediateCmdBufferStream->getUsed();

    // no preamble - regular cmdlist buffer will be first and immediate cmd buffer will be epilogue
    commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);

    EXPECT_EQ(NEO::AppendOperations::cmdList, ultCsr->recordedBcsDispatchFlags.dispatchOperation);
    EXPECT_EQ(dispatchRegularBufferLinearStream, ultCsr->lastFlushedBcsCommandStream);
    EXPECT_EQ(immediateCmdBufferStream, ultCsr->recordedBcsDispatchFlags.optionalEpilogueCmdStream);
    EXPECT_EQ(regularCmdBufferAllocation, ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);

    auto startStream = static_cast<L0::CommandQueueImp *>(commandListImmediate->cmdQImmediate)->getStartingCmdBuffer();
    EXPECT_EQ(dispatchRegularBufferLinearStream, startStream);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(immediateCmdBufferStream->getCpuBase(), immediateCmdBufferOffset),
        immediateCmdBufferStream->getUsed() - immediateCmdBufferOffset));

    auto iterator = find<MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), iterator);
}

HWTEST_F(ImmediateCommandListTest, givenImmediateCmdListWithPrimaryBatchBufferWhenAppendingRegularCmdListWithWaitEventThenDispatchSemaphoreAndJumpFromImmediateToRegular) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    std::unique_ptr<EventPool> eventPool = std::unique_ptr<EventPool>(static_cast<EventPool *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, returnValue));
    auto eventHandle = event->toHandle();

    commandList->close();
    auto cmdListHandle = commandList->toHandle();

    auto regularCmdBufferStream = commandList->getCmdContainer().getCommandStream();
    auto regularCmdBufferAllocation = regularCmdBufferStream->getGraphicsAllocation();

    auto cmdQImmediate = static_cast<WhiteBox<::L0::CommandQueue> *>(commandListImmediate->cmdQImmediate);

    commandListImmediate->dispatchCmdListBatchBufferAsPrimary = true;
    cmdQImmediate->dispatchCmdListBatchBufferAsPrimary = true;

    // first append can carry preamble
    returnValue = commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto immediateCmdBufferStream = commandListImmediate->getCmdContainer().getCommandStream();
    auto offsetBefore = immediateCmdBufferStream->getUsed();

    // no preamble but wait event as first, then bb_start jumping to regular cmdlist
    returnValue = commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto offsetAfter = immediateCmdBufferStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(immediateCmdBufferStream->getCpuBase(), offsetBefore),
        offsetAfter - offsetBefore));

    auto iteratorWait = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), iteratorWait);

    auto iteratorBbStart = find<MI_BATCH_BUFFER_START *>(iteratorWait, cmdList.end());
    ASSERT_NE(cmdList.end(), iteratorBbStart);

    auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*iteratorBbStart);

    EXPECT_EQ(regularCmdBufferAllocation->getGpuAddress(), bbStart->getBatchBufferStartAddress());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
}

HWTEST_F(ImmediateCommandListTest, givenAsyncCmdlistWhenCmdlistIsDestroyedThenHostSynchronizeCalled) {
    ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.ordinal = 0u;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;
    auto immediateCmdList = CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, engineGroupType, returnValue));

    immediateCmdList->cmdQImmediate->registerCsrClient();
    auto csr = immediateCmdList->getCsr(false);

    auto clientCount = csr->getNumClients();
    EXPECT_EQ(1u, clientCount);

    immediateCmdList->destroy();

    clientCount = csr->getNumClients();
    EXPECT_EQ(0u, clientCount);
}

HWTEST_F(ImmediateCommandListTest,
         givenImmediateCmdListWhenAppendingRegularCmdListWithPrintfKernelThenPrintfIsCalledAfterSynchronization) {
    ze_result_t returnValue;

    auto kernel = new Mock<KernelImp>{};
    kernel->setModule(module.get());
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    module->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto commandListHandle = commandList->toHandle();
    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    returnValue = commandListImmediate->appendCommandLists(1, &commandListHandle, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = commandListImmediate->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
}

HWTEST_F(ImmediateCommandListTest,
         givenImmediateCmdListWhenAppendingRegularCmdListThenInternalResidencyContainerProcessed) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
    ASSERT_NE(nullptr, event.get());

    auto eventAllocation = event->getAllocation(device);
    auto cmdBufferAllocation = commandListImmediate->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.storeMakeResidentAllocations = true;

    returnValue = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto commandListHandle = commandList->toHandle();
    auto eventHandle = event->toHandle();
    returnValue = commandListImmediate->appendCommandLists(1, &commandListHandle, eventHandle, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(ultCsr.isMadeResident(eventAllocation));
    EXPECT_TRUE(ultCsr.isMadeResident(cmdBufferAllocation));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            ImmediateCommandListTest,
            givenPatchPreambleInImmediateCommandListWhenExecutingRegularCommandListThenPatchBbStartInStoreDataCommand) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint32_t bbStartDwordBuffer[alignUp(sizeof(MI_BATCH_BUFFER_START) / sizeof(uint32_t), 2)] = {0};

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    returnValue = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    uint64_t startGpuAddress = commandList->getCmdContainer().getCmdBufferAllocations()[0]->getGpuAddress();
    uint64_t endGpuAddress = commandList->getCmdContainer().getEndCmdGpuAddress();

    auto cmdStream = commandListImmediate->getCmdContainer().getCommandStream();
    void *queueCpuBase = cmdStream->getCpuBase();
    uint64_t queueGpuBase = cmdStream->getGpuBase();

    auto commandListHandle = commandList->toHandle();
    commandListImmediate->setPatchingPreamble(true, false);
    auto usedSpaceBefore = cmdStream->getUsed();
    returnValue = commandListImmediate->appendCommandLists(1, &commandListHandle, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(queueCpuBase, usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    GenCmdList::iterator patchCmdIterator = cmdList.end();
    size_t bbStartIdx = 0;
    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(0u, sdiCmds.size());
    for (auto &sdiCmd : sdiCmds) {
        auto storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmd);
        EXPECT_EQ(endGpuAddress + bbStartIdx * sizeof(uint64_t), storeDataImm->getAddress());

        bbStartDwordBuffer[2 * bbStartIdx] = storeDataImm->getDataDword0();
        if (storeDataImm->getStoreQword()) {
            bbStartDwordBuffer[2 * bbStartIdx + 1] = storeDataImm->getDataDword1();
        }
        bbStartIdx++;
        patchCmdIterator = sdiCmd;
    }

    auto bbStarts = findAll<MI_BATCH_BUFFER_START *>(patchCmdIterator, cmdList.end());
    ASSERT_NE(0u, bbStarts.size());
    auto startingBbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStarts[0]);
    EXPECT_EQ(startGpuAddress, startingBbStart->getBatchBufferStartAddress());

    size_t offsetToReturn = ptrDiff(startingBbStart, queueCpuBase);
    offsetToReturn += sizeof(MI_BATCH_BUFFER_START);

    uint64_t expectedReturnAddress = queueGpuBase + offsetToReturn;

    MI_BATCH_BUFFER_START *chainBackBbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(bbStartDwordBuffer);
    ASSERT_NE(nullptr, chainBackBbStartCmd);
    EXPECT_EQ(expectedReturnAddress, chainBackBbStartCmd->getBatchBufferStartAddress());
}

HWTEST_F(CommandListCreateTests, givenRegularOutOfOrderCommandListWhenGettingInOrderPropertiesThenReturnZeros) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto commandListImp = static_cast<L0::CommandListImp *>(commandList.get());
    EXPECT_EQ(0u, commandListImp->getInOrderExecDeviceRequiredSize());
    EXPECT_EQ(0u, commandListImp->getInOrderExecDeviceGpuAddress());
    EXPECT_EQ(0u, commandListImp->getInOrderExecHostRequiredSize());
    EXPECT_EQ(0u, commandListImp->getInOrderExecHostGpuAddress());
}

HWTEST_F(CommandListCreateTests, givenCooperativeDescriptorWhenCloneIsCalledThenClonedValueDesciriptorIsAvailable) {
    ze_command_list_append_launch_kernel_param_cooperative_desc_t cooperativeDesc{ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC, nullptr, true};

    void *outExtPtr = nullptr;
    auto result = CommandList::cloneAppendKernelExtensions(reinterpret_cast<ze_base_desc_t *>(&cooperativeDesc), outExtPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, outExtPtr);

    auto cloneExt = reinterpret_cast<ze_command_list_append_launch_kernel_param_cooperative_desc_t *>(outExtPtr);
    EXPECT_EQ(cooperativeDesc.stype, cloneExt->stype);
    EXPECT_EQ(cooperativeDesc.isCooperative, cloneExt->isCooperative);

    CommandList::freeClonedAppendKernelExtensions(outExtPtr);
}

HWTEST_F(CommandListCreateTests, givenUnsupportedDescriptorWhenCloneIsCalledThenErrorIsReturned) {
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    void *outExtPtr = nullptr;
    auto result = CommandList::cloneAppendKernelExtensions(&ext, outExtPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, outExtPtr);
}

} // namespace ult
} // namespace L0
