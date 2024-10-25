/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_interrupt_helper.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/event/event_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/multi_tile_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

#include <type_traits>

namespace L0 {
namespace ult {

using CommandListAppendLaunchKernel = Test<ModuleFixture>;

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenFunctionWhenBindingTablePrefetchAllowedThenProgramBindingTableEntryCount) {
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    for (auto debugKey : {-1, 0, 1}) {
        DebugManagerStateRestore restore;
        DebugManager.flags.ForceBtpPrefetchMode.set(debugKey);

        createKernel();

        ze_group_count_t groupCount{1, 1, 1};
        ze_result_t returnValue;
        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
        CmdListKernelLaunchParams launchParams = {};
        commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

        auto commandStream = commandList->getCmdContainer().getCommandStream();

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, commandStream->getCpuBase(), commandStream->getUsed()));

        auto itorMIDL = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(itorMIDL, cmdList.end());

        auto cmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(*itorMIDL);
        ASSERT_NE(cmd, nullptr);

        auto dsh = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
        auto idd = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(dsh->getCpuBase(), cmd->getInterfaceDescriptorDataStartAddress()));

        if (NEO::EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
            uint32_t numArgs = kernel->kernelImmData->getDescriptor().payloadMappings.bindingTable.numEntries;
            EXPECT_EQ(numArgs, idd->getBindingTableEntryCount());
        } else {
            EXPECT_EQ(0u, idd->getBindingTableEntryCount());
        }
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenEventsWhenAppendingKernelThenPostSyncToEventIsGenerated) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    Mock<::L0::KernelImp> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(
        kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_FALSE(cmd->getDcFlushEnable());
            auto gpuAddress = event->getGpuAddress(device);
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            postSyncFound = true;
        }
    }
    EXPECT_TRUE(postSyncFound);

    {
        auto itorEvent = std::find(std::begin(commandList->getCmdContainer().getResidencyContainer()),
                                   std::end(commandList->getCmdContainer().getResidencyContainer()),
                                   &event->getAllocation(device));
        EXPECT_NE(itorEvent, std::end(commandList->getCmdContainer().getResidencyContainer()));
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenAppendLaunchMultipleKernelsIndirectThenEnablesPredicate) {
    createKernel();

    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    const ze_kernel_handle_t launchKernels = kernel->toHandle();
    uint32_t *numLaunchArgs;
    const ze_group_count_t launchKernelArgs = {1, 1, 1};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(
        device->toHandle(), &deviceDesc, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
    result = commandList->appendLaunchMultipleKernelsIndirect(1, &launchKernels, numLaunchArgs, &launchKernelArgs, nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    *numLaunchArgs = 0;
    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));
    auto itorWalker = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);

    auto cmd = genCmdCast<GPGPU_WALKER *>(*itorWalker);
    EXPECT_TRUE(cmd->getPredicateEnable());
    context->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenAppendLaunchMultipleKernelsThenUsesMathAndWalker) {
    createKernel();

    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_MATH = typename FamilyType::MI_MATH;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    const ze_kernel_handle_t launchKernels[3] = {kernel->toHandle(), kernel->toHandle(), kernel->toHandle()};
    uint32_t *numLaunchArgs;
    const uint32_t numKernels = 3;
    const ze_group_count_t launchKernelArgs[numKernels] = {{1, 1, 1}, {2, 2, 2}, {1, 1, 1}};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(
        device->toHandle(), &deviceDesc, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
    result = commandList->appendLaunchMultipleKernelsIndirect(numKernels, launchKernels, numLaunchArgs, launchKernelArgs, nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    *numLaunchArgs = 2;
    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = cmdList.begin();

    for (uint32_t i = 0; i < numKernels; i++) {
        itor = find<MI_MATH *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<GPGPU_WALKER *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
    }

    itor = find<MI_MATH *>(itor, cmdList.end());
    ASSERT_EQ(cmdList.end(), itor);
    context->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelThenKernelIsExecutedOnImmediateCmdQ, IsAtLeastSkl) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    CmdListKernelLaunchParams launchParams = {};
    result = commandList0->appendLaunchKernel(
        kernel->toHandle(),
        groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelWithInvalidEventThenInvalidArgumentErrorIsReturned, IsAtLeastSkl) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    CmdListKernelLaunchParams launchParams = {};
    result = commandList0->appendLaunchKernel(
        kernel->toHandle(),
        groupCount, nullptr, 1, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenNonemptyAllocPrintfBufferKernelWhenAppendingLaunchKernelIndirectThenKernelIsStoredOnEvent, IsAtLeastSkl) {
    Mock<Module> module(this->device, nullptr);
    Mock<::L0::KernelImp> kernel;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    kernel.module = &module;
    kernel.descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel.createPrintfBuffer();

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    auto event = std::unique_ptr<Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(nullptr, event->getKernelForPrintf());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenEmptyAllocPrintfBufferKernelWhenAppendingLaunchKernelIndirectThenKernelIsNotStoredOnEvent, IsAtLeastSkl) {
    Mock<Module> module(this->device, nullptr);
    Mock<::L0::KernelImp> kernel;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    kernel.module = &module;
    kernel.descriptor.kernelAttributes.flags.usesPrintf = false;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    auto event = std::unique_ptr<Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(nullptr, event->getKernelForPrintf());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenNonemptyAllocPrintfBufferKernelWhenAppendingLaunchKernelWithParamThenKernelIsStoredOnEvent, IsAtLeastSkl) {
    Mock<Module> module(this->device, nullptr);
    Mock<::L0::KernelImp> kernel;

    ze_result_t returnValue;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    kernel.module = &module;
    kernel.descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel.createPrintfBuffer();

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto event = std::unique_ptr<Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);

    auto result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(nullptr, event->getKernelForPrintf());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenEmptyAllocPrintfBufferKernelWhenAppendingLaunchKernelWithParamThenKernelIsNotStoredOnEvent, IsAtLeastSkl) {
    Mock<Module> module(this->device, nullptr);
    Mock<::L0::KernelImp> kernel;

    ze_result_t returnValue;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    kernel.module = &module;
    kernel.descriptor.kernelAttributes.flags.usesPrintf = false;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto event = std::unique_ptr<Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);

    auto result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(nullptr, event->getKernelForPrintf());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelIndirectThenKernelIsExecutedOnImmediateCmdQ, IsAtLeastSkl) {
    createKernel();
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    result = commandList0->appendLaunchKernelIndirect(
        kernel->toHandle(),
        groupCount, nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelIndirectWithInvalidEventThenInvalidArgumentErrorIsReturned, IsAtLeastSkl) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    result = commandList0->appendLaunchKernelIndirect(
        kernel->toHandle(),
        groupCount, nullptr, 1, nullptr, false);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelUsingSyncBufferWhenAppendLaunchCooperativeKernelIsCalledThenCorrectValueIsReturned, IsAtLeastSkl) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.usesSyncBuffer = true;
    kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto &productHelper = device->getProductHelper();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto engineGroupType = NEO::EngineGroupType::Compute;
    if (productHelper.isCooperativeEngineSupported(*defaultHwInfo)) {
        engineGroupType = gfxCoreHelper.getEngineGroupType(aub_stream::EngineType::ENGINE_CCS, EngineUsage::Cooperative, *defaultHwInfo);
    }
    pCommandList->initialize(device, engineGroupType, 0u);
    auto result = pCommandList->appendLaunchCooperativeKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, engineGroupType, 0u);
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    {
        VariableBackup<std::array<bool, 4>> usesSyncBuffer{&kernelAttributes.flags.packed};
        usesSyncBuffer = {};
        pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
        result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    {
        VariableBackup<uint32_t> groupCountX{&groupCount.groupCountX};
        uint32_t maximalNumberOfWorkgroupsAllowed;
        kernel.suggestMaxCooperativeGroupCount(&maximalNumberOfWorkgroupsAllowed, engineGroupType, false);
        groupCountX = maximalNumberOfWorkgroupsAllowed + 1;
        pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        pCommandList->initialize(device, engineGroupType, 0u);
        result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
    {
        VariableBackup<bool> cooperative{&launchParams.isCooperative};
        cooperative = false;
        result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
}

HWTEST2_F(CommandListAppendLaunchKernel, givenDisableOverdispatchPropertyWhenUpdateStreamPropertiesIsCalledThenRequiredStateAndFinalStateAreCorrectlySet, IsAtLeastSkl) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const auto &productHelper = device->getProductHelper();
    int32_t expectedDisableOverdispatch = productHelper.isDisableOverdispatchAvailable(*defaultHwInfo) ? 1 : -1;

    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);

    const ze_group_count_t launchKernelArgs = {1, 1, 1};
    pCommandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);

    pCommandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenCooperativeKernelWhenAppendLaunchCooperativeKernelIsCalledThenCommandListTypeIsProperlySet, IsAtLeastSkl) {
    createKernel();
    kernel->setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_FALSE(pCommandList->containsCooperativeKernelsFlag);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenAnyCooperativeKernelAndMixingAllowedWhenAppendLaunchCooperativeKernelIsCalledThenCommandListTypeIsProperlySet, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    createKernel();
    kernel->setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_FALSE(pCommandList->containsCooperativeKernelsFlag);

    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);

    launchParams.isCooperative = false;
    result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenCooperativeAndNonCooperativeKernelsAndAllowMixingWhenAppendLaunchCooperativeKernelIsCalledThenReturnSuccess, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    launchParams.isCooperative = false;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenNotEnoughSpaceInCommandStreamWhenAppendingKernelWithImmediateListWithoutFlushTaskUnrecoverableIsCalled, IsWithinXeGfxFamily) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    createKernel();

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    std::unique_ptr<L0::ult::CommandList> commandList(whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue)));

    auto &commandContainer = commandList->getCmdContainer();
    const auto stream = commandContainer.getCommandStream();

    Vec3<size_t> groupCount{1, 1, 1};
    auto sizeLeftInStream = sizeof(MI_BATCH_BUFFER_END);
    auto available = stream->getAvailableSpace();
    stream->getSpace(available - sizeLeftInStream);

    const uint32_t threadGroupDimensions[3] = {1, 1, 1};

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs{
        0,                                    // eventAddress
        0,                                    // postSyncImmValue
        device->getNEODevice(),               // device
        kernel.get(),                         // dispatchInterface
        nullptr,                              // surfaceStateHeap
        nullptr,                              // dynamicStateHeap
        threadGroupDimensions,                // threadGroupDimensions
        nullptr,                              // outWalkerPtr
        nullptr,                              // additionalCommands
        PreemptionMode::MidBatch,             // preemptionMode
        0,                                    // partitionCount
        false,                                // isIndirect
        false,                                // isPredicate
        false,                                // isTimestampEvent
        false,                                // requiresUncachedMocs
        false,                                // useGlobalAtomics
        false,                                // isInternal
        false,                                // isCooperative
        false,                                // isHostScopeSignalEvent
        false,                                // isKernelUsingSystemAllocation
        false,                                // isKernelDispatchedFromImmediateCmdList
        false,                                // isRcs
        commandList->getDcFlushRequired(true) // dcFlushEnable
    };
    EXPECT_THROW(NEO::EncodeDispatchKernel<FamilyType>::encode(commandContainer, dispatchKernelArgs), std::exception);
}

HWTEST_F(CommandListAppendLaunchKernel, givenInvalidKernelWhenAppendingThenReturnErrorInvalidArgument) {
    createKernel();
    const_cast<NEO::KernelDescriptor &>(kernel->getKernelDescriptor()).kernelAttributes.flags.isInvalid = true;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_group_count_t groupCount{8, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
}

struct InOrderCmdListTests : public CommandListAppendLaunchKernel {
    struct MockEvent : public EventImp<uint32_t> {
        using EventImp<uint32_t>::inOrderExecEvent;
        using EventImp<uint32_t>::maxPacketCount;
        using EventImp<uint32_t>::inOrderExecInfo;
        using EventImp<uint32_t>::inOrderExecSignalValue;
        using EventImp<uint32_t>::inOrderAllocationOffset;
        using EventImp<uint32_t>::csrs;
        using EventImp<uint32_t>::signalScope;
    };

    void SetUp() override {
        NEO::DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(NEO::PreemptionMode::Disabled));

        CommandListAppendLaunchKernel::SetUp();
        createKernel();

        const_cast<KernelDescriptor &>(kernel->getKernelDescriptor()).kernelAttributes.flags.usesPrintf = false;
    }

    void TearDown() override {
        events.clear();

        CommandListAppendLaunchKernel::TearDown();
    }

    template <typename GfxFamily>
    std::unique_ptr<L0::EventPool> createEvents(uint32_t numEvents, bool timestampEvent) {
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = numEvents;

        if (timestampEvent) {
            eventPoolDesc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        }

        ze_event_desc_t eventDesc = {};
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

        auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

        for (uint32_t i = 0; i < numEvents; i++) {
            eventDesc.index = i;
            events.emplace_back(DestroyableZeUniquePtr<MockEvent>(static_cast<MockEvent *>(Event::create<typename GfxFamily::TimestampPacketType>(eventPool.get(), &eventDesc, device))));
            EXPECT_FALSE(events.back()->inOrderExecEvent);
            events.back()->inOrderExecEvent = true;
        }

        return eventPool;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createImmCmdList() {
        return createImmCmdListImpl<gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    }

    template <GFXCORE_FAMILY gfxCoreFamily, typename CmdListT>
    DestroyableZeUniquePtr<CmdListT> createImmCmdListImpl() {
        auto cmdList = makeZeUniquePtr<CmdListT>();

        auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

        ze_command_queue_desc_t desc = {};
        desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

        mockCmdQs.emplace_back(std::make_unique<Mock<CommandQueue>>(device, csr, &desc));

        cmdList->cmdQImmediate = mockCmdQs[createdCmdLists].get();
        cmdList->isFlushTaskSubmissionEnabled = true;
        cmdList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
        cmdList->csr = csr;
        cmdList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
        cmdList->commandContainer.setImmediateCmdListCsr(csr);
        cmdList->enableInOrderExecution();

        createdCmdLists++;

        return cmdList;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<gfxCoreFamily>>> createRegularCmdList(bool copyOnly) {
        auto cmdList = makeZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<gfxCoreFamily>>>();

        auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

        ze_command_queue_desc_t desc = {};

        mockCmdQs.emplace_back(std::make_unique<Mock<CommandQueue>>(device, csr, &desc));

        auto engineType = copyOnly ? EngineGroupType::Copy : EngineGroupType::RenderCompute;

        cmdList->initialize(device, engineType, ZE_COMMAND_LIST_FLAG_IN_ORDER);

        createdCmdLists++;

        return cmdList;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createCopyOnlyImmCmdList() {
        auto cmdList = createImmCmdList<gfxCoreFamily>();

        cmdList->engineGroupType = EngineGroupType::Copy;

        mockCopyOsContext = std::make_unique<NEO::MockOsContext>(0, NEO::EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, DeviceBitfield(1)));
        cmdList->csr->setupContext(*mockCopyOsContext);
        return cmdList;
    }

    template <typename FamilyType>
    GenCmdList::iterator findBltFillCmd(GenCmdList::iterator begin, GenCmdList::iterator end) {
        using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

        if constexpr (!std::is_same<XY_COPY_BLT, typename FamilyType::XY_BLOCK_COPY_BLT>::value) {
            auto fillItor = find<typename FamilyType::MEM_SET *>(begin, end);
            if (fillItor != end) {
                return fillItor;
            }
        }

        return find<typename FamilyType::XY_COLOR_BLT *>(begin, end);
    }

    void *allocHostMem(size_t size) {
        ze_host_mem_alloc_desc_t desc = {};
        void *ptr = nullptr;
        context->allocHostMem(&desc, size, 1, &ptr);

        return ptr;
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<NEO::MockOsContext> mockCopyOsContext;

    uint32_t createdCmdLists = 0;
    std::vector<DestroyableZeUniquePtr<MockEvent>> events;
    std::vector<std::unique_ptr<Mock<CommandQueue>>> mockCmdQs;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
};

HWTEST2_F(InOrderCmdListTests, givenCmdListWhenAskingForQwordDataSizeThenReturnFalse, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_FALSE(immCmdList->isQwordInOrderCounter());
}

HWTEST2_F(InOrderCmdListTests, givenQueueFlagWhenCreatingCmdListThenEnableRelaxedOrdering, IsAtLeastXeHpCore) {
    NEO::DebugManager.flags.ForceInOrderImmediateCmdListExecution.set(-1);

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

    ze_command_list_handle_t cmdList;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));

    EXPECT_TRUE(static_cast<CommandListCoreFamilyImmediate<gfxCoreFamily> *>(cmdList)->isInOrderExecutionEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(cmdList));
}

HWTEST2_F(InOrderCmdListTests, givenNotSignaledInOrderEventWhenAddedToWaitListThenReturnError, IsAtLeastSkl) {
    DebugManager.flags.ForceInOrderEvents.set(1);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ze_event_desc_t eventDesc = {};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    eventDesc.index = 0;
    auto event = std::unique_ptr<MockEvent>(static_cast<MockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));
    EXPECT_TRUE(event->inOrderExecEvent);

    auto handle = event->toHandle();

    returnValue = immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &handle, launchParams, false);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
}

HWTEST2_F(InOrderCmdListTests, givenNotSignaledInOrderWhenWhenCallingQueryStatusThenReturnNotReady, IsAtLeastSkl) {
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = true;

    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->queryStatus());
}

HWTEST2_F(InOrderCmdListTests, givenCmdListsWhenDispatchingThenUseInternalTaskCountForWaits, IsAtLeastSkl) {
    auto immCmdList0 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    immCmdList0->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, immCmdList0->cmdQImmediate->getTaskCount());
    EXPECT_EQ(2u, immCmdList1->cmdQImmediate->getTaskCount());

    // explicit wait
    {
        immCmdList0->hostSynchronize(0);
        EXPECT_EQ(1u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

        immCmdList1->hostSynchronize(0);
        EXPECT_EQ(2u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    }

    // implicit wait
    {
        immCmdList0->copyThroughLockedPtrEnabled = true;
        immCmdList1->copyThroughLockedPtrEnabled = true;

        void *deviceAlloc = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);

        uint32_t hostCopyData = 0;

        immCmdList0->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, false, false);
        EXPECT_EQ(1u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

        immCmdList1->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, false, false);
        EXPECT_EQ(2u, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

        context->freeMem(deviceAlloc);
    }
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenEventHostSyncCalledThenCallWaitUserFence, IsAtLeastXeHpCore) {
    NEO::DebugManager.flags.WaitForUserFenceOnEventHostSynchronize.set(1);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(2, false);

    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(2));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    events[0]->inOrderAllocationOffset = 123;

    auto hostAddress = castToUint64(ptrOffset(events[0]->inOrderExecInfo->inOrderDependencyCounterAllocation.getUnderlyingBuffer(), events[0]->inOrderAllocationOffset));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr->waitUserFenecParams.forceRetStatusEnabled = true;
    ultCsr->waitUserFenecParams.forceRetStatusValue = false;
    EXPECT_EQ(0u, ultCsr->waitUserFenecParams.callCount);

    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(2));

    EXPECT_EQ(1u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(hostAddress, ultCsr->waitUserFenecParams.latestWaitedAddress);
    EXPECT_EQ(events[0]->inOrderExecSignalValue, ultCsr->waitUserFenecParams.latestWaitedValue);
    EXPECT_EQ(2, ultCsr->waitUserFenecParams.latestWaitedTimeout);

    ultCsr->waitUserFenecParams.forceRetStatusValue = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(3));

    EXPECT_EQ(2u, ultCsr->waitUserFenecParams.callCount);
    EXPECT_EQ(hostAddress, ultCsr->waitUserFenecParams.latestWaitedAddress);
    EXPECT_EQ(events[0]->inOrderExecSignalValue, ultCsr->waitUserFenecParams.latestWaitedValue);
    EXPECT_EQ(3, ultCsr->waitUserFenecParams.latestWaitedTimeout);

    // already completed
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(3));
    EXPECT_EQ(2u, ultCsr->waitUserFenecParams.callCount);

    // non in-order event
    events[1]->inOrderExecEvent = false;
    events[1]->hostSynchronize(2);
    EXPECT_EQ(2u, ultCsr->waitUserFenecParams.callCount);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenHostResetOrSignalEventCalledThenReturnError, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(3, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(MemoryConstants::pageSize64k, immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getUnderlyingBufferSize());

    EXPECT_TRUE(events[0]->inOrderExecEvent);
    EXPECT_EQ(events[0]->inOrderExecSignalValue, immCmdList->inOrderExecInfo->inOrderDependencyCounter);
    EXPECT_EQ(&events[0]->inOrderExecInfo->inOrderDependencyCounterAllocation, &immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation);
    EXPECT_EQ(events[0]->inOrderAllocationOffset, 0u);

    events[0]->inOrderAllocationOffset = 123;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, events[0]->reset());

    EXPECT_EQ(events[0]->inOrderExecSignalValue, immCmdList->inOrderExecInfo->inOrderDependencyCounter);
    EXPECT_EQ(events[0]->inOrderExecInfo.get(), immCmdList->inOrderExecInfo.get());
    EXPECT_EQ(events[0]->inOrderAllocationOffset, 123u);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, events[0]->hostSignal());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventWhenAppendEventResetCalledThenReturnError, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(3, false);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendEventReset(events[0]->toHandle()));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWheUsingRegularEventThenDontSetInOrderParams, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = false;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_FALSE(events[0]->inOrderExecEvent);
    EXPECT_EQ(events[0]->inOrderExecSignalValue, 0u);
    EXPECT_EQ(events[0]->inOrderExecInfo.get(), nullptr);
    EXPECT_EQ(events[0]->inOrderAllocationOffset, 0u);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenSubmittingThenProgramSemaphoreForPreviousDispatch, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderAllocationOffset = counterOffset;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);

    EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress() + counterOffset, semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, semaphoreCmd->getCompareOperation());
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenDispatchingSemaphoreThenProgramUserInterrupt, IsAtLeastSkl) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManager.flags.ProgramUserInterruptOnResolvedDependency.set(NEO::EncodeUserInterruptHelper::afterSemaphoreMask);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();
    events[0]->inOrderExecEvent = false;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendBarrier(nullptr, 1, &eventHandle, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor);

    auto userInterruptCmd = genCmdCast<MI_USER_INTERRUPT *>(*(++itor));
    ASSERT_NE(nullptr, userInterruptCmd);

    auto allCmds = findAll<MI_USER_INTERRUPT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, allCmds.size());
}

HWTEST2_F(InOrderCmdListTests, givenTimestmapEventWhenProgrammingBarrierThenDontAddPipeControl, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenDispatchingStoreDataImmThenProgramUserInterrupt, IsAtLeastSkl) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    DebugManager.flags.ProgramUserInterruptOnResolvedDependency.set(NEO::EncodeUserInterruptHelper::onSignalingFenceMask);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();
    events[0]->inOrderExecEvent = false;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendBarrier(nullptr, 1, &eventHandle, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());

    auto userInterruptCmd = genCmdCast<MI_USER_INTERRUPT *>(*(++itor));
    ASSERT_NE(nullptr, userInterruptCmd);

    auto allCmds = findAll<MI_USER_INTERRUPT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, allCmds.size());
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetAsMaskWhenDispatchingStoreDataImmAndSemaphoreThenProgramUserInterrupt, IsAtLeastSkl) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr int32_t invalidMask = 0b100;

    DebugManager.flags.ProgramUserInterruptOnResolvedDependency.set(invalidMask);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();
    events[0]->inOrderExecEvent = false;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendBarrier(nullptr, 1, &eventHandle, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), offset),
            cmdStream->getUsed() - offset));

        auto allCmds = findAll<MI_USER_INTERRUPT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, allCmds.size());
    }

    DebugManager.flags.ProgramUserInterruptOnResolvedDependency.set(NEO::EncodeUserInterruptHelper::onSignalingFenceMask | NEO::EncodeUserInterruptHelper::afterSemaphoreMask);

    offset = cmdStream->getUsed();

    immCmdList->appendBarrier(nullptr, 1, &eventHandle, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), offset),
            cmdStream->getUsed() - offset));

        auto allCmds = findAll<MI_USER_INTERRUPT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(2u, allCmds.size());
    }
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenWaitingForEventFromPreviousAppendThenSkip, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    if (immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get())) {
        EXPECT_EQ(cmdList.end(), itor); // already waited on previous call
    } else {
        ASSERT_NE(cmdList.end(), itor); // implicit dependency

        itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

        EXPECT_EQ(cmdList.end(), itor);
    }
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenWaitingForEventFromPreviousAppendOnRegularCmdListThenSkip, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    if (regularCmdList->isInOrderNonWalkerSignalingRequired(events[0].get())) {
        EXPECT_EQ(cmdList.end(), itor); // already waited on previous call
    } else {
        ASSERT_NE(cmdList.end(), itor); // implicit dependency

        itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

        EXPECT_EQ(cmdList.end(), itor);
    }
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenWaitingForRegularEventFromPreviousAppendThenSkip, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = false;
    auto eventHandle = events[0]->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    immCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, eventHandle, 0, nullptr, false, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(deviceAlloc, &copyData, 1, nullptr, 1, &eventHandle, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor); // implicit dependency

    itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventModeWhenSubmittingThenProgramSemaphoreOnlyForExternalEvent, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint32_t counterOffset = 64;
    uint32_t counterOffset2 = counterOffset + 32;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    immCmdList->inOrderAllocationOffset = counterOffset;
    immCmdList2->inOrderAllocationOffset = counterOffset2;

    auto eventPool = createEvents<FamilyType>(2, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();
    auto event1Handle = events[1]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, event1Handle, 0, nullptr, launchParams, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    ze_event_handle_t waitlist[] = {event0Handle, event1Handle};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 2, waitlist, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor);

    itor++; // skip implicit dependency

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(immCmdList2->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress() + counterOffset2, semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, semaphoreCmd->getCompareOperation());

    itor++;
    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(nullptr, semaphoreCmd);
}

HWTEST2_F(InOrderCmdListTests, givenCmdsChainingWhenDispatchingKernelThenProgramSemaphoreOnce, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = false;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    auto offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    uint32_t copyData = 0;

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto findSemaphores = [&](size_t expectedNumSemaphores) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        EXPECT_EQ(expectedNumSemaphores, cmds.size());
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(1); // chaining

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    findSemaphores(0); // no implicit dependency semaphore

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(0); // no implicit dependency

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(0); // no implicit dependency

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(alloc, &copyData, 1, 16, nullptr, 0, nullptr, false);
    findSemaphores(0); // no implicit dependency

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), nullptr, 0, nullptr, false);
    findSemaphores(0); // no implicit dependency

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findSemaphores(2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchCooperativeKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, false);
    findSemaphores(0); // no implicit dependency

    context->freeMem(alloc);
}

HWTEST2_F(InOrderCmdListTests, givenCmdsChainingFromAppendCopyWhenDispatchingKernelThenProgramSemaphoreOnce, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = false;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    auto offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    uint32_t copyData = 0;

    auto findSemaphores = [&](size_t expectedNumSemaphores) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        EXPECT_EQ(expectedNumSemaphores, cmds.size());
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    uint32_t numSemaphores = immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope())) ? 1 : 2;

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, false, false);
    findSemaphores(numSemaphores); // implicit dependency + optional chaining

    numSemaphores = immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope())) ? 1 : 0;

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(numSemaphores); // implicit dependency for Compact event or no semaphores for non-compact

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, eventHandle, 0, nullptr, false, false);
    findSemaphores(2); // implicit dependency + chaining

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);
    findSemaphores(0); // no implicit dependency
}

HWTEST2_F(InOrderCmdListTests, givenEventWithRequiredPipeControlWhenDispatchingCopyThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();

    uint32_t copyData = 0;

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

    if (immCmdList->eventSignalPipeControl(false, immCmdList->getDcFlushRequired(events[0]->isSignalScope()))) {
        EXPECT_NE(cmdList.end(), sdiItor);
    } else {
        EXPECT_EQ(cmdList.end(), sdiItor);

        auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
        auto &postSync = walkerCmd->getPostSync();

        EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(1u, postSync.getImmediateData());
        EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), postSync.getDestinationAddress());
    }
}

HWTEST2_F(InOrderCmdListTests, givenCmdsChainingWhenDispatchingKernelWithRelaxedOrderingThenProgramAllDependencies, IsAtLeastXeHpCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = false;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventHandle = events[0]->toHandle();
    size_t offset = 0;

    auto findConditionalBbStarts = [&](size_t expectedNumBbStarts) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto cmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());

        EXPECT_EQ(expectedNumBbStarts, cmds.size());
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    findConditionalBbStarts(1); // chaining

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    findConditionalBbStarts(1); // implicit dependency
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventModeWhenWaitingForEventFromPreviousAppendThenSkip, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), offset),
            cmdStream->getUsed() - offset));

        auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        if (immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get())) {
            EXPECT_EQ(cmdList.end(), itor); // already waited on previous call
        } else {
            ASSERT_NE(cmdList.end(), itor);

            itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());

            EXPECT_EQ(cmdList.end(), itor);
        }
    }
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventModeWhenSubmittingFromDifferentCmdListThenProgramSemaphoreForEvent, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto event0Handle = events[0]->toHandle();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, event0Handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[&immCmdList1->inOrderExecInfo->inOrderDependencyCounterAllocation]);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &event0Handle, launchParams, false);

    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[&immCmdList1->inOrderExecInfo->inOrderDependencyCounterAllocation]);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto itor = find<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), itor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);

    EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
    EXPECT_NE(immCmdList1->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), immCmdList2->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress());
    EXPECT_EQ(immCmdList1->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, semaphoreCmd->getCompareOperation());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderEventModeWhenSubmittingThenClearEventCsrList, IsAtLeastSkl) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    UltCommandStreamReceiver<FamilyType> tempCsr(*device->getNEODevice()->getExecutionEnvironment(), 0, 1);

    auto eventPool = createEvents<FamilyType>(1, false);

    events[0]->csrs.clear();
    events[0]->csrs.push_back(&tempCsr);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, events[0]->csrs[0]);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenDispatchingThenHandleDependencyCounter, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_NE(nullptr, immCmdList->inOrderExecInfo.get());
    EXPECT_EQ(AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getAllocationType());

    EXPECT_EQ(0u, immCmdList->inOrderExecInfo->inOrderDependencyCounter);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->inOrderDependencyCounter);
    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[&immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation]);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->inOrderDependencyCounter);
    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[&immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation]);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenAddingRelaxedOrderingEventsThenConfigureRegistersFirst, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->addEventsToCmdList(0, nullptr, true, true, true);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    auto lrrCmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_REG *>(*cmdList.begin());
    ASSERT_NE(nullptr, lrrCmd);

    EXPECT_EQ(CS_GPR_R4, lrrCmd->getSourceRegisterAddress());
    EXPECT_EQ(CS_GPR_R0, lrrCmd->getDestinationRegisterAddress());
    lrrCmd++;
    EXPECT_EQ(CS_GPR_R4 + 4, lrrCmd->getSourceRegisterAddress());
    EXPECT_EQ(CS_GPR_R0 + 4, lrrCmd->getDestinationRegisterAddress());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingWalkerThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderAllocationOffset = counterOffset;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    bool isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

        auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
        auto &postSync = walkerCmd->getPostSync();

        EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(1u, postSync.getImmediateData());
        EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress() + counterOffset, postSync.getDestinationAddress());
    }

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
        auto &postSync = walkerCmd->getPostSync();

        if (isCompactEvent) {
            EXPECT_EQ(POSTSYNC_DATA::OPERATION_NO_WRITE, postSync.getOperation());

            auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
            ASSERT_NE(cmdList.end(), pcItor);

            auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(pcItor, cmdList.end());
            ASSERT_NE(cmdList.end(), semaphoreItor);

            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());

            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
            ASSERT_NE(nullptr, sdiCmd);

            EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress() + counterOffset, sdiCmd->getAddress());
            EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
            EXPECT_EQ(2u, sdiCmd->getDataDword0());
        } else {
            EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(2u, postSync.getImmediateData());
            EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress() + counterOffset, postSync.getDestinationAddress());
        }
    }

    auto hostAddress = static_cast<uint64_t *>(ptrOffset(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getUnderlyingBuffer(), counterOffset));

    *hostAddress = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(1));

    *hostAddress = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));

    *hostAddress = 3;
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingTimestampEventThenClearAndChainWithSyncAllocSignaling, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(events[0]->getCompletionFieldGpuAddress(device), sdiCmd->getAddress());
    EXPECT_EQ(0u, sdiCmd->getStoreQword());
    EXPECT_EQ(Event::STATE_CLEARED, sdiCmd->getDataDword0());

    auto walkerItor = find<COMPUTE_WALKER *>(sdiItor, cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
    auto &postSync = walkerCmd->getPostSync();

    auto eventBaseGpuVa = events[0]->getPacketAddress(device);
    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
    EXPECT_EQ(eventBaseGpuVa, postSync.getDestinationAddress());

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++walkerCmd);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());

    sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
}

HWTEST2_F(InOrderCmdListTests, givenRelaxedOrderingWhenProgrammingTimestampEventThenClearAndChainWithSyncAllocSignalingAsTwoSeparateSubmissions, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool kernelOperation, ze_event_handle_t hSignalEvent) override {
            flushData.push_back(this->cmdListCurrentStartOffset);

            this->cmdListCurrentStartOffset = this->commandContainer.getCommandStream()->getUsed();

            return ZE_RESULT_SUCCESS;
        }

        std::vector<size_t> flushData; // start_offset
    };

    DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    DebugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdListImpl<gfxCoreFamily, MyMockCmdList>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    immCmdList->inOrderExecInfo->inOrderDependencyCounter = 1;

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0));

    EXPECT_EQ(0u, immCmdList->flushData.size());

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    ASSERT_EQ(2u, immCmdList->flushData.size());
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->inOrderDependencyCounter);

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), immCmdList->flushData[1]));

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), sdiItor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(events[0]->getCompletionFieldGpuAddress(device), sdiCmd->getAddress());
        EXPECT_EQ(0u, sdiCmd->getStoreQword());
        EXPECT_EQ(Event::STATE_CLEARED, sdiCmd->getDataDword0());

        auto sdiOffset = ptrDiff(sdiCmd, cmdStream->getCpuBase());
        EXPECT_TRUE(sdiOffset >= immCmdList->flushData[0]);
        EXPECT_TRUE(sdiOffset < immCmdList->flushData[1]);

        auto walkerItor = find<COMPUTE_WALKER *>(sdiItor, cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
        auto &postSync = walkerCmd->getPostSync();

        auto eventBaseGpuVa = events[0]->getPacketAddress(device);

        EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
        EXPECT_EQ(eventBaseGpuVa, postSync.getDestinationAddress());

        auto walkerOffset = ptrDiff(walkerCmd, cmdStream->getCpuBase());
        EXPECT_TRUE(walkerOffset >= immCmdList->flushData[0]);
        EXPECT_TRUE(walkerOffset < immCmdList->flushData[1]);
    }

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), immCmdList->flushData[1]), (cmdStream->getUsed() - immCmdList->flushData[1])));

        // Relaxed Ordering registers
        auto lrrCmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_REG *>(*cmdList.begin());
        ASSERT_NE(nullptr, lrrCmd);

        EXPECT_EQ(CS_GPR_R4, lrrCmd->getSourceRegisterAddress());
        EXPECT_EQ(CS_GPR_R0, lrrCmd->getDestinationRegisterAddress());
        lrrCmd++;
        EXPECT_EQ(CS_GPR_R4 + 4, lrrCmd->getSourceRegisterAddress());
        EXPECT_EQ(CS_GPR_R0 + 4, lrrCmd->getDestinationRegisterAddress());

        lrrCmd++;

        auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

        EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyConditionalDataMemBbStart<FamilyType>(lrrCmd, 0, eventEndGpuVa, static_cast<uint64_t>(Event::STATE_CLEARED),
                                                                                               NEO::CompareOperation::Equal, true, false));

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(ptrOffset(lrrCmd, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(false)));
        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(2u, sdiCmd->getDataDword0());
    }
}

HWTEST2_F(InOrderCmdListTests, givenDebugFlagSetWhenChainingWithRelaxedOrderingThenSignalAsSingleSubmission, IsAtLeastXeHpcCore) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
      public:
        using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>;
        using BaseClass::BaseClass;

        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool kernelOperation, ze_event_handle_t hSignalEvent) override {
            flushCount++;

            return ZE_RESULT_SUCCESS;
        }

        uint32_t flushCount = 0;
    };

    DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    DebugManager.flags.EnableInOrderRelaxedOrderingForEventsChaining.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto immCmdList = createImmCmdListImpl<gfxCoreFamily, MyMockCmdList>();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    immCmdList->inOrderExecInfo->inOrderDependencyCounter = 1;

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0));

    EXPECT_EQ(0u, immCmdList->flushCount);

    zeCommandListAppendLaunchKernel(immCmdList->toHandle(), kernel->toHandle(), &groupCount, events[0]->toHandle(), 0, nullptr);

    ASSERT_EQ(1u, immCmdList->flushCount);
    EXPECT_EQ(2u, immCmdList->inOrderExecInfo->inOrderDependencyCounter);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingRegularEventThenClearAndChainWithSyncAllocSignaling, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->signalScope = 0;
    events[0]->inOrderExecEvent = false;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(events[0]->getCompletionFieldGpuAddress(device), sdiCmd->getAddress());
    EXPECT_EQ(0u, sdiCmd->getStoreQword());
    EXPECT_EQ(Event::STATE_CLEARED, sdiCmd->getDataDword0());

    auto walkerItor = find<COMPUTE_WALKER *>(sdiItor, cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
    auto &postSync = walkerCmd->getPostSync();

    auto eventBaseGpuVa = events[0]->getPacketAddress(device);
    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
    EXPECT_EQ(eventBaseGpuVa, postSync.getDestinationAddress());

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++walkerCmd);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());

    sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
}

HWTEST2_F(InOrderCmdListTests, givenHostVisibleEventOnLatestFlushWhenCallingSynchronizeThenUseInOrderSync, IsAtLeastSkl) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->signalScope = 0;

    EXPECT_FALSE(immCmdList->latestFlushIsHostVisible);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_FALSE(immCmdList->latestFlushIsHostVisible);

    EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    immCmdList->hostSynchronize(0, 1, false);
    EXPECT_EQ(0u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    events[0]->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_TRUE(immCmdList->latestFlushIsHostVisible);

    immCmdList->hostSynchronize(0, 1, false);
    EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(1u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);

    // handle post sync operations
    immCmdList->hostSynchronize(0, 1, true);
    EXPECT_EQ(1u, immCmdList->synchronizeInOrderExecutionCalled);
    EXPECT_EQ(2u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
}

using NonPostSyncWalkerMatcher = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_GEN12LP_CORE>;

HWTEST2_F(InOrderCmdListTests, givenNonPostSyncWalkerWhenPatchingThenThrow, NonPostSyncWalkerMatcher) {
    InOrderPatchCommandHelpers::PatchCmd<FamilyType> incorrectCmd(nullptr, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::None);

    EXPECT_ANY_THROW(incorrectCmd.patch(1));

    InOrderPatchCommandHelpers::PatchCmd<FamilyType> walkerCmd(nullptr, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::Walker);

    EXPECT_ANY_THROW(walkerCmd.patch(1));
}

HWTEST2_F(InOrderCmdListTests, givenNonPostSyncWalkerWhenAskingForNonWalkerSignalingRequiredThenReturnFalse, NonPostSyncWalkerMatcher) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto eventPool1 = createEvents<FamilyType>(1, true);
    auto eventPool2 = createEvents<FamilyType>(1, false);
    auto eventPool3 = createEvents<FamilyType>(1, false);
    events[2]->inOrderExecEvent = false;

    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[1].get()));
    EXPECT_FALSE(immCmdList->isInOrderNonWalkerSignalingRequired(events[2].get()));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingWalkerThenProgramPipeControlWithSignalAllocation, NonPostSyncWalkerMatcher) {
    using WALKER = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderAllocationOffset = 64;
    immCmdList->inOrderExecInfo->inOrderDependencyCounter = 123;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pcCmd->getPostSyncOperation());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t expectedAddress = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress() + immCmdList->inOrderAllocationOffset;

    EXPECT_EQ(expectedAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounter, sdiCmd->getDataDword0());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitThenProgramPcAndSignalAlloc, NonPostSyncWalkerMatcher) {
    using WALKER = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderAllocationOffset = 64;
    immCmdList->inOrderExecInfo->inOrderDependencyCounter = 123;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 256;
    const size_t offset = 1;

    void *hostAlloc = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, ptrBaseSize, MemoryConstants::cacheLineSize, &hostAlloc);

    ASSERT_NE(nullptr, hostAlloc);

    auto unalignedPtr = ptrOffset(hostAlloc, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, false, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto lastWalkerItor = reverseFind<WALKER *>(cmdList.rbegin(), cmdList.rend());
    ASSERT_NE(cmdList.rend(), lastWalkerItor);

    auto pcItor = reverseFind<PIPE_CONTROL *>(cmdList.rbegin(), lastWalkerItor);
    ASSERT_NE(lastWalkerItor, pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pcCmd->getPostSyncOperation());

    auto sdiItor = reverseFind<MI_STORE_DATA_IMM *>(cmdList.rbegin(), pcItor);
    ASSERT_NE(pcItor, sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t expectedAddress = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress() + immCmdList->inOrderAllocationOffset;

    EXPECT_EQ(expectedAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounter, sdiCmd->getDataDword0());

    context->freeMem(hostAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendSignalEventThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList->appendSignalEvent(events[0]->toHandle(), false);

    uint64_t inOrderSyncVa = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*cmdList.begin());

        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(inOrderSyncVa, semaphoreCmd->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, semaphoreCmd->getCompareOperation());
    }

    {

        auto rbeginItor = cmdList.rbegin();

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*rbeginItor);
        while (sdiCmd == nullptr) {
            sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++rbeginItor));
            if (rbeginItor == cmdList.rend()) {
                break;
            }
        }

        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(inOrderSyncVa, sdiCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(2u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
    }
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingNonKernelAppendThenWaitForDependencyAndSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->inOrderExecEvent = false;

    uint64_t inOrderSyncVa = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    uint8_t ptr[64] = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    uint32_t inOrderCounter = 1;

    auto verifySemaphore = [&inOrderSyncVa](const GenCmdList::iterator &iterator, uint64_t waitValue) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*iterator);

        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(getLowPart(waitValue), semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(inOrderSyncVa, semaphoreCmd->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, semaphoreCmd->getCompareOperation());
    };

    auto verifySdi = [&inOrderSyncVa, &immCmdList](GenCmdList::reverse_iterator rIterator, GenCmdList::reverse_iterator rEnd, uint64_t signalValue) {
        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*rIterator);
        while (sdiCmd == nullptr) {
            sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++rIterator));
            if (rIterator == rEnd) {
                break;
            }
        }

        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(inOrderSyncVa, sdiCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(getLowPart(signalValue), sdiCmd->getDataDword0());
        EXPECT_EQ(getHighPart(signalValue), sdiCmd->getDataDword1());
    };

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendEventReset(events[0]->toHandle());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        verifySemaphore(cmdList.begin(), inOrderCounter);
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();

        size_t rangeSizes = 1;
        const void **ranges = reinterpret_cast<const void **>(&ptr[0]);
        immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        verifySemaphore(cmdList.begin(), inOrderCounter);
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(ptr), nullptr, 0, nullptr);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        verifySemaphore(cmdList.begin(), inOrderCounter);
        verifySdi(cmdList.rbegin(), cmdList.rend(), ++inOrderCounter);
    }
}

HWTEST2_F(InOrderCmdListTests, givenInOrderRegularCmdListWhenProgrammingAppendWithSignalEventThenAssignInOrderInfo, IsAtLeastSkl) {
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(2, false);

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(regularCmdList->inOrderExecInfo.get(), events[0]->inOrderExecInfo.get());

    uint32_t copyData = 0;
    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, events[1]->toHandle(), 0, nullptr, false, false);

    EXPECT_EQ(regularCmdList->inOrderExecInfo.get(), events[1]->inOrderExecInfo.get());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderRegularCmdListWhenProgrammingNonKernelAppendThenWaitForDependencyAndSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    events[0]->inOrderExecEvent = false;

    uint8_t ptr[64] = {};

    uint64_t inOrderSyncVa = regularCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto verifySemaphore = [&inOrderSyncVa](const GenCmdList::iterator &iterator, uint64_t waitValue) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*iterator);

        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(getLowPart(waitValue), semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(inOrderSyncVa, semaphoreCmd->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, semaphoreCmd->getCompareOperation());
    };

    auto verifySdi = [&inOrderSyncVa, &regularCmdList](GenCmdList::reverse_iterator rIterator, GenCmdList::reverse_iterator rEnd, uint64_t signalValue) {
        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*rIterator);
        while (sdiCmd == nullptr) {
            sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++rIterator));
            if (rIterator == rEnd) {
                break;
            }
        }

        ASSERT_NE(nullptr, sdiCmd);

        EXPECT_EQ(inOrderSyncVa, sdiCmd->getAddress());
        EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(getLowPart(signalValue), sdiCmd->getDataDword0());
        EXPECT_EQ(getHighPart(signalValue), sdiCmd->getDataDword1());
    };

    {
        auto offset = cmdStream->getUsed();

        regularCmdList->appendEventReset(events[0]->toHandle());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        verifySemaphore(cmdList.begin(), 1);
        verifySdi(cmdList.rbegin(), cmdList.rend(), 2);
    }

    {
        auto offset = cmdStream->getUsed();

        size_t rangeSizes = 1;
        const void **ranges = reinterpret_cast<const void **>(&ptr[0]);
        regularCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        verifySemaphore(cmdList.begin(), 2);
        verifySdi(cmdList.rbegin(), cmdList.rend(), 3);
    }

    {
        auto offset = cmdStream->getUsed();

        regularCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(ptr), nullptr, 0, nullptr);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        verifySemaphore(cmdList.begin(), 3);
        verifySdi(cmdList.rbegin(), cmdList.rend(), 4);
    }

    {
        auto offset = cmdStream->getUsed();

        zex_wait_on_mem_desc_t desc;
        desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
        regularCmdList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, 1, nullptr);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        verifySemaphore(cmdList.begin(), 4);
        verifySdi(cmdList.rbegin(), cmdList.rend(), 5);
    }

    {
        auto offset = cmdStream->getUsed();

        zex_write_to_mem_desc_t desc = {};
        uint64_t data = 0xabc;
        regularCmdList->appendWriteToMemory(reinterpret_cast<void *>(&desc), ptr, data);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        verifySemaphore(cmdList.begin(), 5);
        verifySdi(cmdList.rbegin(), cmdList.rend(), 6);
    }
}

HWTEST2_F(InOrderCmdListTests, givenImmediateEventWhenWaitingFromRegularCmdListThenDontPatch, IsAtLeastSkl) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size());

    if (NonPostSyncWalkerMatcher::isMatched<productFamily>()) {
        EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::Sdi, regularCmdList->inOrderPatchCmds[0].patchCmdType);
    } else {
        EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::Walker, regularCmdList->inOrderPatchCmds[0].patchCmdType);
    }

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);
    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), semaphoreCmd->getSemaphoreGraphicsAddress());

    auto walkerItor = find<WALKER_TYPE *>(semaphoreItor, cmdList.end());
    EXPECT_NE(cmdList.end(), walkerItor);
}

HWTEST2_F(InOrderCmdListTests, givenEventGeneratedByRegularCmdListWhenWaitingFromImmediateThenUseSubmissionCounter, IsAtLeastSkl) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto regularCmdListHandle = regularCmdList->toHandle();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    uint64_t expectedCounterValue = regularCmdList->inOrderExecInfo->inOrderDependencyCounter;

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->close();

    uint64_t expectedCounterAppendValue = regularCmdList->inOrderExecInfo->inOrderDependencyCounter;

    auto verifySemaphore = [&](uint64_t expectedValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, semaphoreCmd);

        if (semaphoreCmd->getSemaphoreGraphicsAddress() == immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress()) {
            // skip implicit dependency
            semaphoreItor++;
            semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
            ASSERT_NE(nullptr, semaphoreCmd);
        }

        EXPECT_EQ(expectedValue, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(regularCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), semaphoreCmd->getSemaphoreGraphicsAddress());
    };

    // 0 Execute calls
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    verifySemaphore(expectedCounterValue);

    // 1 Execute call
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    verifySemaphore(expectedCounterValue);

    // 2 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    verifySemaphore(expectedCounterValue + expectedCounterAppendValue);

    // 3 Execute calls
    offset = cmdStream->getUsed();
    mockCmdQHw->executeCommandLists(1, &regularCmdListHandle, nullptr, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    verifySemaphore(expectedCounterValue + (expectedCounterAppendValue * 2));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitThenDontSignalFromWalker, IsAtLeastXeHpCore) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());

    uint32_t walkersFound = 0;
    while (cmdList.end() != walkerItor) {
        walkersFound++;

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
        auto &postSync = walkerCmd->getPostSync();

        EXPECT_EQ(POSTSYNC_DATA::OPERATION_NO_WRITE, postSync.getOperation());

        walkerItor = find<COMPUTE_WALKER *>(++walkerItor, cmdList.end());
    }

    EXPECT_TRUE(walkersFound > 1);

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingCopyThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;

    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), copyItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(copyItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t syncVa = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingComputeCopyThenDontSingalFromSdi, IsAtLeastXeHpCore) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto alignedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    immCmdList->appendMemoryCopy(alignedPtr, alignedPtr, 1, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);
    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);

    auto &postSync = walkerCmd->getPostSync();

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), postSync.getDestinationAddress());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingFillThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto fillItor = findBltFillCmd<FamilyType>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), fillItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(fillItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t syncVa = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithSplitAndOutEventThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, (size / 2) + 1, events[0]->toHandle(), 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);

    while (PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE == pcCmd->getPostSyncOperation()) {
        pcItor = find<PIPE_CONTROL *>(++pcItor, cmdList.end());
        ASSERT_NE(cmdList.end(), pcItor);

        pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
        ASSERT_NE(nullptr, pcCmd);
    }

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    uint64_t syncVa = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithSplitAndWithoutOutEventThenAddPipeControlSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, (size / 2) + 1, nullptr, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<typename FamilyType::COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto pcItor = find<PIPE_CONTROL *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItor);
    ASSERT_NE(nullptr, pcCmd);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    uint64_t syncVa = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    context->freeMem(data);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingFillWithoutSplitThenSignalByWalker, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);

    auto &postSync = walkerCmd->getPostSync();

    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
    EXPECT_EQ(1u, postSync.getImmediateData());
    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), postSync.getDestinationAddress());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(walkerItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    context->freeMem(data);
}

HWTEST2_F(InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingCopyRegionThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);

    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), copyItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(copyItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t syncVa = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendWaitOnEventsThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    zeCommandListAppendWaitOnEvents(immCmdList->toHandle(), 1, &eventHandle);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(2u, semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), semaphoreCmd->getSemaphoreGraphicsAddress());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(3u, sdiCmd->getDataDword0());
}

HWTEST2_F(InOrderCmdListTests, givenRegularInOrderCmdListWhenProgrammingAppendWaitOnEventsThenDontSignalSyncAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = false;

    auto eventHandle = events[0]->toHandle();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    zeCommandListAppendWaitOnEvents(regularCmdList->toHandle(), 1, &eventHandle);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), semaphoreItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    EXPECT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    ASSERT_NE(nullptr, sdiCmd);

    uint64_t syncVa = regularCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(syncVa, sdiCmd->getAddress());
    EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(3u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingCounterWithOverflowThenHandleItCorrectly, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderExecInfo->inOrderDependencyCounter = std::numeric_limits<uint32_t>::max() - 1;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    bool isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    auto eventHandle = events[0]->toHandle();

    uint64_t baseGpuVa = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());

    uint64_t expectedCounter = 1;
    uint32_t offset = 0;

    if (immCmdList->isQwordInOrderCounter()) {
        expectedCounter = std::numeric_limits<uint32_t>::max();

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
        auto &postSync = walkerCmd->getPostSync();

        if (isCompactEvent) {
            EXPECT_NE(cmdList.end(), semaphoreItor);

            auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
            ASSERT_NE(cmdList.end(), sdiItor);

            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
            ASSERT_NE(nullptr, sdiCmd);

            EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
            EXPECT_EQ(getLowPart(expectedCounter), sdiCmd->getDataDword0());
            EXPECT_EQ(getHighPart(expectedCounter), sdiCmd->getDataDword1());

            EXPECT_EQ(POSTSYNC_DATA::OPERATION_NO_WRITE, postSync.getOperation());
        } else {
            EXPECT_EQ(cmdList.end(), semaphoreItor);

            EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(expectedCounter, postSync.getImmediateData());
            EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), postSync.getDestinationAddress());
        }
    } else {
        ASSERT_NE(cmdList.end(), semaphoreItor);

        if (isCompactEvent) {
            // commands chaining
            semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
            ASSERT_NE(cmdList.end(), semaphoreItor);
        }

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(baseGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(++semaphoreCmd);
        ASSERT_NE(nullptr, sdiCmd);

        offset = static_cast<uint32_t>(sizeof(uint64_t));

        EXPECT_EQ(baseGpuVa + offset, sdiCmd->getAddress());
        EXPECT_EQ(1u, sdiCmd->getDataDword0());
    }

    EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->inOrderDependencyCounter);
    EXPECT_EQ(offset, immCmdList->inOrderAllocationOffset);

    EXPECT_EQ(expectedCounter, events[0]->inOrderExecSignalValue);
    EXPECT_EQ(offset, events[0]->inOrderAllocationOffset);
}

HWTEST2_F(InOrderCmdListTests, givenCopyOnlyInOrderModeWhenProgrammingBarrierThenSignalInOrderAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList1 = createCopyOnlyImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createCopyOnlyImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    uint32_t copyData = 0;

    immCmdList1->appendMemoryCopy(&copyData, &copyData, 1, eventHandle, 0, nullptr, false, false);

    auto offset = cmdStream->getUsed();

    immCmdList2->appendBarrier(nullptr, 1, &eventHandle, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(immCmdList2->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList2->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithWaitlistThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    immCmdList2->appendBarrier(nullptr, 1, &eventHandle, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto pcItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), pcItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(immCmdList2->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList2->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistThenInheritSignalSyncAllocation, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->inOrderDependencyCounter);

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendBarrier(nullptr, 0, nullptr, false);
    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

    EXPECT_EQ(offset, cmdStream->getUsed());

    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithDifferentEventsThenDontInherit, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(3, false);

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto offset = cmdStream->getUsed();

    ze_event_handle_t waitlist[] = {events[0]->toHandle(), events[1]->toHandle()};

    immCmdList2->appendBarrier(events[2]->toHandle(), 2, waitlist, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor); // implicit dependency

    itor = find<MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor); // event0

    itor = find<MI_SEMAPHORE_WAIT *>(++itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    EXPECT_EQ(3u, events[2]->inOrderExecSignalValue);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAndTimestampEventThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->inOrderDependencyCounter);

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingAppendBarrierWithoutWaitlistAndRegularEventThenSignalSyncAllocation, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->inOrderDependencyCounter);

    auto offset = cmdStream->getUsed();

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = false;

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendBarrier(eventHandle, 0, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(2u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenCallingSyncThenHandleCompletion, IsAtLeastXeHpCore) {
    uint32_t counterOffset = 64;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->inOrderAllocationOffset = counterOffset;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    auto hostAddress = static_cast<uint64_t *>(ptrOffset(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getUnderlyingBuffer(), counterOffset));
    *hostAddress = 0;

    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;
    bool forceFail = false;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        callCounter++;
        if (callCounter >= failCounter && !forceFail) {
            (*hostAddress)++;
        }
    };

    // single check - not ready
    {
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, ultCsr->taskCount, false));
        EXPECT_EQ(1u, callCounter);
        EXPECT_EQ(1u, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(0u, *hostAddress);
    }

    // timeout - not ready
    {
        forceFail = true;
        EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(10, ultCsr->taskCount, false));
        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // gpu hang
    {
        ultCsr->forceReturnGpuHang = true;

        EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, immCmdList->hostSynchronize(10, ultCsr->taskCount, false));

        EXPECT_TRUE(callCounter > 1);
        EXPECT_TRUE(ultCsr->checkGpuHangDetectedCalled > 1);
        EXPECT_EQ(0u, *hostAddress);
    }

    // success
    {
        ultCsr->checkGpuHangDetectedCalled = 0;
        ultCsr->forceReturnGpuHang = false;
        forceFail = false;
        callCounter = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(std::numeric_limits<uint64_t>::max(), ultCsr->taskCount, false));

        EXPECT_EQ(failCounter, callCounter);
        EXPECT_EQ(failCounter - 1, ultCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(1u, *hostAddress);
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    *ultCsr->getTagAddress() = ultCsr->taskCount - 1;

    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, ultCsr->taskCount, true));

    *ultCsr->getTagAddress() = ultCsr->taskCount + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, ultCsr->taskCount, true));
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenDoingCpuCopyThenSynchronize, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto eventHandle = events[0]->toHandle();

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getUnderlyingBuffer());
    *hostAddress = 0;

    const uint32_t failCounter = 3;
    uint32_t callCounter = 0;

    ultCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        callCounter++;
        if (callCounter >= failCounter) {
            (*hostAddress)++;
        }
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    events[0]->setIsCompleted();

    ultCsr->waitForCompletionWithTimeoutTaskCountCalled = 0;
    ultCsr->flushTagUpdateCalled = false;

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;

    immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 1, &eventHandle, false, false);

    EXPECT_EQ(3u, callCounter);
    EXPECT_EQ(1u, *hostAddress);
    EXPECT_EQ(2u, ultCsr->checkGpuHangDetectedCalled);
    EXPECT_EQ(0u, ultCsr->waitForCompletionWithTimeoutTaskCountCalled);
    EXPECT_FALSE(ultCsr->flushTagUpdateCalled);

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, wWhenUsingImmediateCmdListThenDontAddCmdsToPatch, IsAtLeastXeHpCore) {
    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    uint32_t copyData = 0;

    immCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    EXPECT_EQ(0u, immCmdList->inOrderPatchCmds.size());
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenGpuHangDetectedInCpuCopyPathThenReportError, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();
    immCmdList->copyThroughLockedPtrEnabled = true;

    auto eventPool = createEvents<FamilyType>(1, false);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto hostAddress = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getUnderlyingBuffer());
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 128, 128, &deviceAlloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    uint32_t hostCopyData = 0;

    ultCsr->forceReturnGpuHang = true;

    auto status = immCmdList->appendMemoryCopy(deviceAlloc, &hostCopyData, 1, nullptr, 0, nullptr, false, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, status);

    ultCsr->forceReturnGpuHang = false;

    context->freeMem(deviceAlloc);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithoutEventThenAddBarrierAndSignalCounter, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto cmdItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);

    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pcCmd->getPostSyncOperation());

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));

    while (sdiCmd == nullptr && cmdItor != cmdList.end()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));
    }

    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderCmdListTests, givenInOrderModeWhenProgrammingKernelSplitWithEventThenSignalCounter, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eventHandle, 0, nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto cmdItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdItor);

    auto pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pcCmd);

    while (PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE == pcCmd->getPostSyncOperation()) {
        cmdItor = find<PIPE_CONTROL *>(++cmdItor, cmdList.end());
        ASSERT_NE(cmdList.end(), cmdItor);

        pcCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pcCmd);
    }

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));

    while (sdiCmd == nullptr && cmdItor != cmdList.end()) {
        sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++cmdItor));
    }

    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());

    alignedFree(alignedPtr);
}

struct MultiTileInOrderCmdListTests : public InOrderCmdListTests {
    void SetUp() override {
        NEO::DebugManager.flags.CreateMultipleSubDevices.set(2);
        NEO::DebugManager.flags.EnableImplicitScaling.set(1);

        InOrderCmdListTests::SetUp();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createMultiTileImmCmdList() {
        auto cmdList = createImmCmdList<gfxCoreFamily>();

        cmdList->partitionCount = 2;

        return cmdList;
    }
};

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingWaitOnEventsThenHandleAllEventPackets, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    size_t offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        if (isCompactEvent) {
            ASSERT_NE(cmdList.end(), semaphoreItor);
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);

            ASSERT_NE(nullptr, semaphoreCmd);

            auto gpuAddress = events[0]->getCompletionFieldGpuAddress(device);

            while (gpuAddress != semaphoreCmd->getSemaphoreGraphicsAddress()) {
                semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
                ASSERT_NE(cmdList.end(), semaphoreItor);

                semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
                ASSERT_NE(nullptr, semaphoreCmd);
            }

            EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress, semaphoreCmd->getSemaphoreGraphicsAddress());

            semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress + sizeof(uint64_t), semaphoreCmd->getSemaphoreGraphicsAddress());
        }
    }

    offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*cmdList.begin());

        if (isCompactEvent) {
            ASSERT_EQ(nullptr, semaphoreCmd); // already waited on previous call
        } else {
            ASSERT_NE(nullptr, semaphoreCmd);

            auto gpuAddress = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

            EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress, semaphoreCmd->getSemaphoreGraphicsAddress());

            semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress + sizeof(uint64_t), semaphoreCmd->getSemaphoreGraphicsAddress());
        }
    }
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenSignalingSyncAllocationThenEnablePartitionOffset, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendSignalInOrderDependencyCounter();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*cmdList.begin());
    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenCallingSyncThenHandleCompletion, IsAtLeastXeHpCore) {
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    auto hostAddress0 = static_cast<uint64_t *>(immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getUnderlyingBuffer());
    auto hostAddress1 = ptrOffset(hostAddress0, sizeof(uint64_t));

    *hostAddress0 = 0;
    *hostAddress1 = 0;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, ultCsr->taskCount, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    *hostAddress0 = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, ultCsr->taskCount, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    *hostAddress0 = 0;
    *hostAddress1 = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, ultCsr->taskCount, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    *hostAddress0 = 1;
    *hostAddress1 = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, ultCsr->taskCount, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(0));

    *hostAddress0 = 3;
    *hostAddress1 = 3;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, ultCsr->taskCount, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(0));
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingTimestampEventThenHandleChaining, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->signalScope = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      cmdStream->getCpuBase(),
                                                      cmdStream->getUsed()));

    auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto computeWalkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
    ASSERT_NE(nullptr, computeWalkerCmd);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
    ASSERT_NE(nullptr, semaphoreCmd);

    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    if (eventEndGpuVa != semaphoreCmd->getSemaphoreGraphicsAddress()) {
        semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);

        semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
        ASSERT_NE(nullptr, semaphoreCmd);
    }

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + events[0]->getSinglePacketSize(), semaphoreCmd->getSemaphoreGraphicsAddress());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingTimestampEventThenHandlePacketsChaining, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->signalScope = 0;

    immCmdList->signalAllEventPackets = true;
    events[0]->maxPacketCount = 4;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      cmdStream->getCpuBase(),
                                                      cmdStream->getUsed()));

    auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto computeWalkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
    ASSERT_NE(nullptr, computeWalkerCmd);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
    ASSERT_NE(nullptr, semaphoreCmd);

    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    if (eventEndGpuVa != semaphoreCmd->getSemaphoreGraphicsAddress()) {
        semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);

        semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
        ASSERT_NE(nullptr, semaphoreCmd);
    }

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    auto offset = events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    offset += events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    offset += events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());
}

HWTEST2_F(MultiTileInOrderCmdListTests, whenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXeHpCore) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    ASSERT_EQ(4u, regularCmdList->inOrderPatchCmds.size()); // Walker + 2x Semaphore + Walker

    auto walkerFromContainer1 = genCmdCast<COMPUTE_WALKER *>(regularCmdList->inOrderPatchCmds[0].cmd);
    ASSERT_NE(nullptr, walkerFromContainer1);
    auto walkerFromContainer2 = genCmdCast<COMPUTE_WALKER *>(regularCmdList->inOrderPatchCmds[3].cmd);
    ASSERT_NE(nullptr, walkerFromContainer2);
    COMPUTE_WALKER *walkerFromParser1 = nullptr;
    COMPUTE_WALKER *walkerFromParser2 = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        walkerFromParser1 = genCmdCast<COMPUTE_WALKER *>(*itor);

        itor = find<COMPUTE_WALKER *>(++itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        walkerFromParser2 = genCmdCast<COMPUTE_WALKER *>(*itor);
    }

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->inOrderDependencyCounter);

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = regularCmdList->inOrderExecInfo->inOrderDependencyCounter * executionCounter;

        EXPECT_EQ(1u + appendValue, walkerFromContainer1->getPostSync().getImmediateData());
        EXPECT_EQ(1u + appendValue, walkerFromParser1->getPostSync().getImmediateData());

        EXPECT_EQ(2u + appendValue, walkerFromContainer2->getPostSync().getImmediateData());
        EXPECT_EQ(2u + appendValue, walkerFromParser2->getPostSync().getImmediateData());
    };

    regularCmdList->close();

    auto handle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(2);
}

struct BcsSplitInOrderCmdListTests : public InOrderCmdListTests {
    void SetUp() override {
        NEO::DebugManager.flags.SplitBcsCopy.set(1);
        NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

        hwInfoBackup = std::make_unique<VariableBackup<HardwareInfo>>(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        defaultHwInfo->featureTable.ftrBcsInfo = 0b111111111;

        InOrderCmdListTests::SetUp();
    }

    bool verifySplit(uint64_t expectedTaskCount) {
        auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

        for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
            if (static_cast<CommandQueueImp *>(bcsSplit.cmdQs[0])->getTaskCount() != expectedTaskCount) {
                return false;
            }
        }

        return true;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createBcsSplitImmCmdList() {
        auto cmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

        auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

        ze_command_queue_desc_t desc = {};
        desc.ordinal = static_cast<uint32_t>(device->getNEODevice()->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

        cmdList->isBcsSplitNeeded = bcsSplit.setupDevice(device->getHwInfo().platform.eProductFamily, false, &desc, cmdList->csr);
        cmdList->isFlushTaskSubmissionEnabled = false;

        return cmdList;
    }

    template <typename FamilyType, GFXCORE_FAMILY gfxCoreFamily>
    void verifySplitCmds(LinearStream &cmdStream, size_t streamOffset, L0::Device *device, uint64_t submissionId, WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> &immCmdList,
                         uint64_t externalDependencyGpuVa);

    std::unique_ptr<VariableBackup<HardwareInfo>> hwInfoBackup;
    const uint32_t numLinkCopyEngines = 4;
};

template <typename FamilyType, GFXCORE_FAMILY gfxCoreFamily>
void BcsSplitInOrderCmdListTests::verifySplitCmds(LinearStream &cmdStream, size_t streamOffset, L0::Device *device, uint64_t submissionId,
                                                  WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> &immCmdList, uint64_t externalDependencyGpuVa) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;
    auto counterGpuAddress = immCmdList.inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), streamOffset), (cmdStream.getUsed() - streamOffset)));

    auto itor = cmdList.begin();

    for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
        auto beginItor = itor;

        auto signalSubCopyEventGpuVa = bcsSplit.events.subcopy[i + (submissionId * numLinkCopyEngines)]->getCompletionFieldGpuAddress(device);

        size_t numExpectedSemaphores = 0;

        if (submissionId > 0) {
            numExpectedSemaphores++;
            itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
            ASSERT_NE(cmdList.end(), itor);
            auto implicitSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            ASSERT_NE(nullptr, implicitSemaphore);

            EXPECT_EQ(counterGpuAddress, implicitSemaphore->getSemaphoreGraphicsAddress());
            EXPECT_EQ(submissionId, implicitSemaphore->getSemaphoreDataDword());
            itor++;
        }

        if (externalDependencyGpuVa > 0) {
            numExpectedSemaphores++;
            itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
            ASSERT_NE(cmdList.end(), itor);
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(externalDependencyGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
        }

        itor = find<XY_COPY_BLT *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
        ASSERT_NE(nullptr, genCmdCast<XY_COPY_BLT *>(*itor));

        auto flushDwItor = find<MI_FLUSH_DW *>(++itor, cmdList.end());
        ASSERT_NE(cmdList.end(), flushDwItor);

        auto signalSubCopyEvent = genCmdCast<MI_FLUSH_DW *>(*flushDwItor);
        ASSERT_NE(nullptr, signalSubCopyEvent);

        while (signalSubCopyEvent->getDestinationAddress() != signalSubCopyEventGpuVa) {
            flushDwItor = find<MI_FLUSH_DW *>(++flushDwItor, cmdList.end());
            ASSERT_NE(cmdList.end(), flushDwItor);

            signalSubCopyEvent = genCmdCast<MI_FLUSH_DW *>(*flushDwItor);
            ASSERT_NE(nullptr, signalSubCopyEvent);
        }

        itor = ++flushDwItor;

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(beginItor, itor);
        EXPECT_EQ(numExpectedSemaphores, semaphoreCmds.size());
    }

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());

    if (submissionId > 0) {
        auto implicitSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, implicitSemaphore);

        EXPECT_EQ(counterGpuAddress, implicitSemaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(submissionId, implicitSemaphore->getSemaphoreDataDword());

        ++semaphoreItor;
    }

    for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
        auto subCopyEventSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, subCopyEventSemaphore);

        EXPECT_EQ(bcsSplit.events.subcopy[i + (submissionId * numLinkCopyEngines)]->getCompletionFieldGpuAddress(device), subCopyEventSemaphore->getSemaphoreGraphicsAddress());

        itor = ++semaphoreItor;
    }

    ASSERT_NE(nullptr, genCmdCast<MI_FLUSH_DW *>(*itor)); // marker event

    auto implicitCounterSdi = genCmdCast<MI_STORE_DATA_IMM *>(*(++itor));
    ASSERT_NE(nullptr, implicitCounterSdi);

    EXPECT_EQ(counterGpuAddress, implicitCounterSdi->getAddress());
    EXPECT_EQ(submissionId + 1, implicitCounterSdi->getDataDword0());

    EXPECT_EQ(submissionId + 1, immCmdList.inOrderExecInfo->inOrderDependencyCounter);

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(++itor, cmdList.end());
    EXPECT_EQ(0u, sdiCmds.size());
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenDispatchingCopyThenHandleInOrderSignaling, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    EXPECT_TRUE(verifySplit(0));

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    EXPECT_TRUE(verifySplit(1));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyAfterBarrierWithoutImplicitDependenciesThenHandleCorrectInOrderSignaling, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    *immCmdList->csr->getBarrierCountTagAddress() = 0u;
    immCmdList->csr->getNextBarrierCount();

    size_t offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    // no implicit dependencies
    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 0, *immCmdList, 0);
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyAfterBarrierWithImplicitDependenciesThenHandleCorrectInOrderSignaling, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    *immCmdList->csr->getBarrierCountTagAddress() = 0u;
    immCmdList->csr->getNextBarrierCount();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    size_t offset = cmdStream->getUsed();

    *immCmdList->csr->getBarrierCountTagAddress() = 0u;
    immCmdList->csr->getNextBarrierCount();
    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    // implicit dependencies
    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 1, *immCmdList, 0);
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyWithEventDependencyThenRequiredSemaphores, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->inOrderExecEvent = false;
    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, false, false);

    size_t offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 1, &eventHandle, false, false);

    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 1, *immCmdList, events[0]->getCompletionFieldGpuAddress(device));
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenDispatchingCopyRegionThenHandleInOrderSignaling, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    EXPECT_TRUE(verifySplit(0));

    ze_copy_region_t region = {0, 0, 0, copySize, 1, 1};

    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, false, false);

    EXPECT_TRUE(verifySplit(1));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

using InOrderRegularCmdListTests = InOrderCmdListTests;

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderFlagWhenCreatingCmdListThenEnableInOrderMode, IsAtLeastSkl) {
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

    ze_command_list_handle_t cmdList;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    EXPECT_TRUE(static_cast<CommandListCoreFamily<gfxCoreFamily> *>(cmdList)->isInOrderExecutionEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(cmdList));
}

HWTEST2_F(InOrderRegularCmdListTests, whenUsingRegularCmdListThenAddCmdsToPatch, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    uint32_t copyData = 0;

    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    EXPECT_EQ(1u, regularCmdList->inOrderPatchCmds.size()); // SDI

    auto sdiFromContainer1 = genCmdCast<MI_STORE_DATA_IMM *>(regularCmdList->inOrderPatchCmds[0].cmd);
    ASSERT_NE(nullptr, sdiFromContainer1);
    MI_STORE_DATA_IMM *sdiFromParser1 = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        sdiFromParser1 = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
    }

    offset = cmdStream->getUsed();
    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);
    ASSERT_EQ(3u, regularCmdList->inOrderPatchCmds.size()); // SDI + Semaphore + SDI

    auto semaphoreFromContainer2 = genCmdCast<MI_SEMAPHORE_WAIT *>(regularCmdList->inOrderPatchCmds[1].cmd);
    ASSERT_NE(nullptr, semaphoreFromContainer2);
    MI_SEMAPHORE_WAIT *semaphoreFromParser2 = nullptr;

    auto sdiFromContainer2 = genCmdCast<MI_STORE_DATA_IMM *>(regularCmdList->inOrderPatchCmds[2].cmd);
    ASSERT_NE(nullptr, sdiFromContainer2);
    MI_STORE_DATA_IMM *sdiFromParser2 = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);

        semaphoreFromParser2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), sdiItor);

        sdiFromParser2 = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    }

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->inOrderDependencyCounter);

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = regularCmdList->inOrderExecInfo->inOrderDependencyCounter * executionCounter;

        EXPECT_EQ(1u + appendValue, sdiFromContainer1->getDataDword0());
        EXPECT_EQ(1u + appendValue, sdiFromParser1->getDataDword0());

        EXPECT_EQ(1u + appendValue, semaphoreFromContainer2->getSemaphoreDataDword());
        EXPECT_EQ(1u + appendValue, semaphoreFromParser2->getSemaphoreDataDword());

        EXPECT_EQ(2u + appendValue, sdiFromContainer2->getDataDword0());
        EXPECT_EQ(2u + appendValue, sdiFromParser2->getDataDword0());
    };

    regularCmdList->close();

    auto handle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(2);
}

HWTEST2_F(InOrderRegularCmdListTests, givenCrossRegularCmdListDependenciesWhenExecutingThenDontPatchWhenExecutedOnlyOnce, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList1 = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList2 = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    regularCmdList1->close();

    uint64_t baseEventWaitValue = 3;

    auto implicitCounterGpuVa = regularCmdList2->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();
    auto externalCounterGpuVa = regularCmdList1->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    auto cmdStream2 = regularCmdList2->getCmdContainer().getCommandStream();

    size_t offset2 = cmdStream2->getUsed();

    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    regularCmdList2->close();

    size_t sizeToParse2 = cmdStream2->getUsed();

    auto verifyPatching = [&](uint64_t expectedImplicitDependencyValue, uint64_t expectedExplicitDependencyValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream2->getCpuBase(), offset2), (sizeToParse2 - offset2)));

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, semaphoreCmds.size());

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreCmds[0]);
        EXPECT_EQ(expectedImplicitDependencyValue, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(implicitCounterGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

        semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreCmds[1]);
        EXPECT_EQ(expectedExplicitDependencyValue, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(externalCounterGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
    };

    auto cmdListHandle1 = regularCmdList1->toHandle();
    auto cmdListHandle2 = regularCmdList2->toHandle();

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false);

    verifyPatching(5, baseEventWaitValue);

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false);

    verifyPatching(7, baseEventWaitValue);
}

HWTEST2_F(InOrderRegularCmdListTests, givenCrossRegularCmdListDependenciesWhenExecutingThenPatchWhenExecutedMultipleTimes, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList1 = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList2 = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    regularCmdList1->close();

    uint64_t baseEventWaitValue = 3;

    auto implicitCounterGpuVa = regularCmdList2->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();
    auto externalCounterGpuVa = regularCmdList1->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

    auto cmdListHandle1 = regularCmdList1->toHandle();
    auto cmdListHandle2 = regularCmdList2->toHandle();

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false);

    auto cmdStream2 = regularCmdList2->getCmdContainer().getCommandStream();

    size_t offset2 = cmdStream2->getUsed();
    size_t sizeToParse2 = 0;

    auto verifyPatching = [&](uint64_t expectedImplicitDependencyValue, uint64_t expectedExplicitDependencyValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream2->getCpuBase(), offset2), (sizeToParse2 - offset2)));

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, semaphoreCmds.size());

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreCmds[0]);
        EXPECT_EQ(expectedImplicitDependencyValue, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(implicitCounterGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

        semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreCmds[1]);
        EXPECT_EQ(expectedExplicitDependencyValue, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(externalCounterGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
    };

    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    regularCmdList2->close();

    sizeToParse2 = cmdStream2->getUsed();

    verifyPatching(1, baseEventWaitValue);

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false);

    verifyPatching(1, baseEventWaitValue + (2 * regularCmdList1->inOrderExecInfo->inOrderDependencyCounter));

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false);

    verifyPatching(5, baseEventWaitValue + (2 * regularCmdList1->inOrderExecInfo->inOrderDependencyCounter));

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false);

    verifyPatching(7, baseEventWaitValue + (3 * regularCmdList1->inOrderExecInfo->inOrderDependencyCounter));
}

HWTEST2_F(InOrderRegularCmdListTests, givenDebugFlagSetWhenUsingRegularCmdListThenDontAddCmdsToPatch, IsAtLeastXeHpCore) {
    DebugManager.flags.EnableInOrderRegularCmdListPatching.set(0);

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    uint32_t copyData = 0;

    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, false, false);

    EXPECT_EQ(0u, regularCmdList->inOrderPatchCmds.size());
}

HWTEST2_F(InOrderRegularCmdListTests, whenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXeHpCore) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    ASSERT_EQ(3u, regularCmdList->inOrderPatchCmds.size()); // Walker + Semaphore + Walker

    auto walkerFromContainer1 = genCmdCast<COMPUTE_WALKER *>(regularCmdList->inOrderPatchCmds[0].cmd);
    ASSERT_NE(nullptr, walkerFromContainer1);
    auto walkerFromContainer2 = genCmdCast<COMPUTE_WALKER *>(regularCmdList->inOrderPatchCmds[2].cmd);
    ASSERT_NE(nullptr, walkerFromContainer2);
    COMPUTE_WALKER *walkerFromParser1 = nullptr;
    COMPUTE_WALKER *walkerFromParser2 = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        walkerFromParser1 = genCmdCast<COMPUTE_WALKER *>(*itor);

        itor = find<COMPUTE_WALKER *>(++itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        walkerFromParser2 = genCmdCast<COMPUTE_WALKER *>(*itor);
    }

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->inOrderDependencyCounter);

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = regularCmdList->inOrderExecInfo->inOrderDependencyCounter * executionCounter;

        EXPECT_EQ(1u + appendValue, walkerFromContainer1->getPostSync().getImmediateData());
        EXPECT_EQ(1u + appendValue, walkerFromParser1->getPostSync().getImmediateData());

        EXPECT_EQ(2u + appendValue, walkerFromContainer2->getPostSync().getImmediateData());
        EXPECT_EQ(2u + appendValue, walkerFromParser2->getPostSync().getImmediateData());
    };

    regularCmdList->close();

    auto handle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false);
    verifyPatching(2);
}

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenProgramPipeControlsToHandleDependencies, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->inOrderDependencyCounter);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(1u, regularCmdList->inOrderExecInfo->inOrderDependencyCounter);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
        auto &postSync = walkerCmd->getPostSync();

        EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(1u, postSync.getImmediateData());
        EXPECT_EQ(regularCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), postSync.getDestinationAddress());

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), sdiItor);
    }

    offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->inOrderDependencyCounter);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));
        EXPECT_NE(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*cmdList.begin()));

        auto walkerItor = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*walkerItor);
        auto &postSync = walkerCmd->getPostSync();

        EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(2u, postSync.getImmediateData());
        EXPECT_EQ(regularCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress(), postSync.getDestinationAddress());

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), sdiItor);
    }

    regularCmdList->inOrderAllocationOffset = 123;
    auto hostAddr = static_cast<uint64_t *>(regularCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getUnderlyingBuffer());
    *hostAddr = 0x1234;

    regularCmdList->reset();
    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->inOrderDependencyCounter);
    EXPECT_EQ(0u, regularCmdList->inOrderAllocationOffset);
    EXPECT_EQ(0u, *hostAddr);
}

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenUpdateCounterAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->inOrderExecEvent = false;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCopyOnlyCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto copyOnlyCmdStream = regularCopyOnlyCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->inOrderDependencyCounter);
    EXPECT_NE(nullptr, regularCmdList->inOrderExecInfo.get());

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

    regularCmdList->appendMemoryCopyRegion(data, &region, 1, 1, data, &region, 1, 1, nullptr, 0, nullptr, false, false);

    regularCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    regularCmdList->appendSignalEvent(eventHandle, false);

    regularCmdList->appendBarrier(nullptr, 1, &eventHandle, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(2u, sdiCmds.size());
    }

    offset = copyOnlyCmdStream->getUsed();
    regularCopyOnlyCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(copyOnlyCmdStream->getCpuBase(), offset),
                                                          (copyOnlyCmdStream->getUsed() - offset)));

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);
    }

    context->freeMem(data);
}

using InOrderRegularCopyOnlyCmdListTests = InOrderCmdListTests;

HWTEST2_F(InOrderRegularCopyOnlyCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenDontProgramBarriers, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    auto alignedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    regularCmdList->appendMemoryCopy(alignedPtr, alignedPtr, MemoryConstants::cacheLineSize, nullptr, 0, nullptr, false, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

        ASSERT_NE(nullptr, sdiCmd);

        auto gpuAddress = regularCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

        EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
        EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(1u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
    }

    offset = cmdStream->getUsed();

    regularCmdList->appendMemoryCopy(alignedPtr, alignedPtr, MemoryConstants::cacheLineSize, nullptr, 0, nullptr, false, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        EXPECT_NE(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*itor));

        itor++;
        auto copyCmd = genCmdCast<XY_COPY_BLT *>(*itor);

        EXPECT_NE(nullptr, copyCmd);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

        ASSERT_NE(nullptr, sdiCmd);

        auto gpuAddress = regularCmdList->inOrderExecInfo->inOrderDependencyCounterAllocation.getGpuAddress();

        EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
        EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(2u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
    }

    alignedFree(alignedPtr);
}

struct CommandListAppendLaunchKernelWithImplicitArgs : CommandListAppendLaunchKernel {
    template <typename FamilyType>
    uint64_t getIndirectHeapOffsetForImplicitArgsBuffer(const Mock<::L0::KernelImp> &kernel) {
        if (FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
            const auto &gfxCoreHelper = device->getGfxCoreHelper();
            auto implicitArgsProgrammingSize = ImplicitArgsHelper::getSizeForImplicitArgsPatching(kernel.pImplicitArgs.get(), kernel.getKernelDescriptor(), !kernel.kernelRequiresGenerationOfLocalIdsByRuntime, gfxCoreHelper);
            return implicitArgsProgrammingSize - sizeof(ImplicitArgs);
        } else {
            return 0u;
        }
    }
};

HWTEST_F(CommandListAppendLaunchKernelWithImplicitArgs, givenIndirectDispatchWithImplicitArgsWhenAppendingThenMiMathCommandsForWorkGroupCountAndGlobalWorkSizeAndWorkDimAreProgrammed) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    kernel.immutableData.crossThreadDataSize = sizeof(uint64_t);
    kernel.pImplicitArgs.reset(new ImplicitArgs());
    UnitTestHelper<FamilyType>::adjustKernelDescriptorForImplicitArgs(*kernel.immutableData.kernelDescriptor);

    kernel.setGroupSize(1, 1, 1);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     *static_cast<ze_group_count_t *>(alloc),
                                                     nullptr, 0, nullptr, false);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto heap = commandList->getCmdContainer().getIndirectHeap(HeapType::INDIRECT_OBJECT);
    uint64_t pImplicitArgsGPUVA = heap->getGraphicsAllocation()->getGpuAddress() + getIndirectHeapOffsetForImplicitArgsBuffer<FamilyType>(kernel);

    auto workDimStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    workDimStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R0);
    workDimStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA);

    auto groupCountXStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountXStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMX);
    groupCountXStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountX));

    auto groupCountYStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountYStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMY);
    groupCountYStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountY));

    auto groupCountZStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountZStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMZ);
    groupCountZStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountZ));

    auto globalSizeXStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeXStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R1);
    globalSizeXStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeX));

    auto globalSizeYStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeYStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R1);
    globalSizeYStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeY));

    auto globalSizeZStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeZStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R1);
    globalSizeZStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeZ));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountXStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountXStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountYStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountYStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountZStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountZStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeXStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeXStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeYStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeYStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeZStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeZStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    auto cmd2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    auto memoryMaskCmd = FamilyType::cmdInitLoadRegisterImm;
    memoryMaskCmd.setDataDword(0xFF00FFFF);

    EXPECT_EQ(cmd2->getDataDword(), memoryMaskCmd.getDataDword());

    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    cmd2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    auto offsetCmd = FamilyType::cmdInitLoadRegisterImm;
    offsetCmd.setDataDword(0x0000FFFF);

    EXPECT_EQ(cmd2->getDataDword(), offsetCmd.getDataDword());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), workDimStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), workDimStoreRegisterMemCmd.getMemoryAddress());

    context->freeMem(alloc);
}

using MultiTileImmediateCommandListAppendLaunchKernelXeHpCoreTest = Test<MultiTileImmediateCommandListAppendLaunchKernelFixture>;

HWTEST2_F(MultiTileImmediateCommandListAppendLaunchKernelXeHpCoreTest, givenImplicitScalingWhenUsingImmediateCommandListThenDoNotAddSelfCleanup, IsAtLeastXeHpCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.UsePipeControlAfterPartitionedWalker.set(1);

    ze_group_count_t groupCount{128, 1, 1};

    auto immediateCmdList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    immediateCmdList->cmdListType = ::L0::CommandList::CommandListType::TYPE_IMMEDIATE;
    immediateCmdList->isFlushTaskSubmissionEnabled = true;
    auto result = immediateCmdList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = immediateCmdList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdStream = immediateCmdList->getCmdContainer().getCommandStream();

    auto sizeBefore = cmdStream->getUsed();
    result = immediateCmdList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = cmdStream->getUsed();

    uint64_t bbStartGpuAddress = cmdStream->getGraphicsAllocation()->getGpuAddress() + sizeBefore;
    bbStartGpuAddress += sizeof(WALKER_TYPE) + sizeof(PIPE_CONTROL) + sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                         sizeof(MI_BATCH_BUFFER_START) + 3 * sizeof(uint32_t);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), sizeBefore),
        sizeAfter - sizeBefore));

    auto itorWalker = find<WALKER_TYPE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);
    auto cmdWalker = genCmdCast<WALKER_TYPE *>(*itorWalker);
    EXPECT_TRUE(cmdWalker->getWorkloadPartitionEnable());

    auto itorPipeControl = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    ASSERT_NE(cmdList.end(), itorPipeControl);

    auto itorStoreDataImm = find<MI_STORE_DATA_IMM *>(itorWalker, itorPipeControl);
    EXPECT_EQ(itorPipeControl, itorStoreDataImm);

    auto itorBbStart = find<MI_BATCH_BUFFER_START *>(itorPipeControl, cmdList.end());
    ASSERT_NE(cmdList.end(), itorBbStart);
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBbStart);
    EXPECT_EQ(bbStartGpuAddress, cmdBbStart->getBatchBufferStartAddress());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, cmdBbStart->getSecondLevelBatchBuffer());

    auto itorMiAtomic = find<MI_ATOMIC *>(itorBbStart, cmdList.end());
    EXPECT_EQ(cmdList.end(), itorMiAtomic);

    auto itorSemaphoreWait = find<MI_SEMAPHORE_WAIT *>(itorBbStart, cmdList.end());
    EXPECT_EQ(cmdList.end(), itorSemaphoreWait);
}

HWTEST2_F(MultiTileImmediateCommandListAppendLaunchKernelXeHpCoreTest, givenImplicitScalingWhenUsingImmediateCommandListWithoutFlushTaskThenUseSecondaryBuffer, IsAtLeastXeHpCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManager.flags.UsePipeControlAfterPartitionedWalker.set(1);

    ze_group_count_t groupCount{128, 1, 1};

    auto immediateCmdList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    immediateCmdList->cmdListType = ::L0::CommandList::CommandListType::TYPE_IMMEDIATE;
    immediateCmdList->isFlushTaskSubmissionEnabled = false;
    auto result = immediateCmdList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdStream = immediateCmdList->getCmdContainer().getCommandStream();

    auto sizeBefore = cmdStream->getUsed();
    CmdListKernelLaunchParams launchParams = {};
    result = immediateCmdList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), sizeBefore),
        sizeAfter - sizeBefore));

    auto itorWalker = find<WALKER_TYPE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);
    auto cmdWalker = genCmdCast<WALKER_TYPE *>(*itorWalker);
    EXPECT_TRUE(cmdWalker->getWorkloadPartitionEnable());

    auto itorBbStart = find<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBbStart);
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBbStart);
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, cmdBbStart->getSecondLevelBatchBuffer());
}
} // namespace ult
} // namespace L0
