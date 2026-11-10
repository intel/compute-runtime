/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/helpers/test_traits.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/ze_api.h"

#include "igfxfmid.h"

#include <cstdint>
#include <memory>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace NEO {
namespace SysCalls {
extern bool getNumThreadsCalled;
}
} // namespace NEO

using namespace NEO;
#include "shared/test/common/test_macros/heapless_matchers.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

HWTEST_F(CommandListCreate, givenIndirectAccessFlagsAreChangedWhenResettingCommandListThenExpectAllFlagsSetToDefault) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(commandList->indirectAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectSharedAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectDeviceAllocationsAllowed);

    commandList->indirectAllocationsAllowed = true;
    commandList->unifiedMemoryControls.indirectHostAllocationsAllowed = true;
    commandList->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
    commandList->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;

    returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(commandList->indirectAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectSharedAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
}

HWTEST_F(CommandListCreate, whenContainsCooperativeKernelsIsCalledThenCorrectValueIsReturned) {
    for (auto testValue : ::testing::Bool()) {
        MockCommandListForAppendLaunchKernel<FamilyType::gfxCoreFamily> commandList;
        commandList.initialize(device, NEO::EngineGroupType::compute, 0u);
        commandList.containsCooperativeKernelsFlag = testValue;
        EXPECT_EQ(testValue, commandList.containsCooperativeKernels());
        commandList.reset();
        EXPECT_FALSE(commandList.containsCooperativeKernels());
    }
}

HWTEST_F(CommandListCreate, GivenSingleTileDeviceWhenCommandListIsResetThenPartitionCountIsReversedToOne) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(device,
                                                                     NEO::EngineGroupType::compute,
                                                                     0u,
                                                                     returnValue, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->getPartitionCount());

    returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->getPartitionCount());
}

HWTEST_F(CommandListCreate, givenExternalSemaphoreHostFunctionDataWhenResettingCommandListThenItIsCleared) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    commandList->externalSemaphoreHostFunctionData.emplace_back(*commandList, ExternalSemaphoreOperationData{});

    returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(0u, commandList->externalSemaphoreHostFunctionData.size());
}

HWTEST_F(CommandListCreate, WhenReservingSpaceThenCommandsAddedToBatchBuffer) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    ASSERT_NE(nullptr, commandList->getCmdContainer().getCommandStream());
    auto commandStream = commandList->getCmdContainer().getCommandStream();

    auto usedSpaceBefore = commandStream->getUsed();

    using MI_NOOP = typename FamilyType::MI_NOOP;
    MI_NOOP cmd = FamilyType::cmdInitNoop;
    uint32_t uniqueIDforTest = 0x12345u;
    cmd.setIdentificationNumber(uniqueIDforTest);

    size_t sizeToReserveForCommand = sizeof(cmd);
    void *ptrToReservedMemory = nullptr;
    returnValue = commandList->reserveSpace(sizeToReserveForCommand, &ptrToReservedMemory);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    if (ptrToReservedMemory != nullptr) {
        *reinterpret_cast<MI_NOOP *>(ptrToReservedMemory) = cmd;
    }

    auto usedSpaceAfter = commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandStream->getCpuBase(), usedSpaceAfter));

    auto itor = cmdList.begin();
    while (itor != cmdList.end()) {
        using MI_NOOP = typename FamilyType::MI_NOOP;
        itor = find<MI_NOOP *>(itor, cmdList.end());
        if (itor == cmdList.end()) {
            break;
        }

        auto cmd = genCmdCast<MI_NOOP *>(*itor);
        if (uniqueIDforTest == cmd->getIdentificationNumber()) {
            break;
        }

        itor++;
    }
    ASSERT_NE(itor, cmdList.end());
}

TEST_F(CommandListCreate, givenOrdinalBiggerThanAvailableEnginesWhenCreatingCommandListThenInvalidArgumentErrorIsReturned) {
    auto numAvailableEngineGroups = static_cast<uint32_t>(neoDevice->getRegularEngineGroups().size());
    ze_command_list_handle_t commandList = nullptr;
    ze_command_list_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    desc.commandQueueGroupOrdinal = numAvailableEngineGroups;
    auto returnValue = device->createCommandList(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);

    ze_command_queue_desc_t desc2 = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc2.ordinal = numAvailableEngineGroups;
    desc2.index = 0;
    returnValue = device->createCommandListImmediate(&desc2, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);

    desc2.ordinal = 0;
    desc2.index = 0x1000;
    returnValue = device->createCommandListImmediate(&desc2, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreate, givenCommandListWithValidOrdinalWhenCallingGetOrdinalThenSuccessIsReturned) {
    auto availableEngineGroups = neoDevice->getRegularEngineGroups();
    ze_command_list_handle_t commandList = nullptr;
    for (uint32_t engineGroup = 0; engineGroup < availableEngineGroups.size(); engineGroup++) {
        ze_command_list_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
        desc.commandQueueGroupOrdinal = engineGroup;
        auto returnValue = device->createCommandList(&desc, &commandList);
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        EXPECT_NE(nullptr, commandList);

        auto whiteboxCommandList = CommandList::whiteboxCast(CommandList::fromHandle(commandList));
        EXPECT_EQ(engineGroup, whiteboxCommandList->getOrdinal());

        whiteboxCommandList->destroy();
    }
}

TEST_F(CommandListCreate, givenCommandListWhenCallingImmediateCommandListIndexThenInvalidArgumentIsReturned) {
    ze_command_list_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    ze_command_list_handle_t commandList = nullptr;

    auto returnValue = device->createCommandList(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    auto whiteboxCommandList = CommandList::whiteboxCast(CommandList::fromHandle(commandList));

    uint32_t index = 4;
    ze_bool_t isImmediate = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->isImmediate(&isImmediate));
    EXPECT_FALSE(static_cast<bool>(isImmediate));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, whiteboxCommandList->getImmediateIndex(&index));
    ze_command_queue_flags_t flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
    ze_command_queue_mode_t mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_command_queue_priority_t priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, whiteboxCommandList->getImmediateFlags(&flags));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, whiteboxCommandList->getImmediateMode(&mode));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, whiteboxCommandList->getImmediatePriority(&priority));
    EXPECT_EQ(4u, index);

    whiteboxCommandList->destroy();
}

TEST_F(CommandListCreate, givenCommandListWithFlagsWhenCallingGetFlagsThenCorrectFlagsAreReturned) {
    ze_command_list_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    desc.flags = ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING;
    ze_command_list_handle_t commandList = nullptr;

    auto returnValue = device->createCommandList(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    auto whiteboxCommandList = CommandList::whiteboxCast(CommandList::fromHandle(commandList));

    ze_command_list_flags_t flags = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->getFlags(&flags));
    EXPECT_EQ(desc.flags, flags);

    ze_bool_t isMutable = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->isMutableExp(&isMutable));
    EXPECT_FALSE(static_cast<bool>(isMutable));

    whiteboxCommandList->destroy();
}

TEST_F(CommandListCreate, givenImmediateCommandListWhenCallingImmediateCommandListIndexThenValidIndexIsReturned) {
    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
    ze_command_list_handle_t commandList = nullptr;

    auto returnValue = device->createCommandListImmediate(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    auto whiteboxCommandList = CommandList::whiteboxCast(CommandList::fromHandle(commandList));

    uint32_t index = 4, cmdQIndex = 2;
    ze_command_queue_flags_t flags = 0u;
    ze_command_queue_mode_t mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    ze_command_queue_priority_t priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    ze_bool_t isImmediate = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->isImmediate(&isImmediate));
    EXPECT_TRUE(static_cast<bool>(isImmediate));
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->getImmediateIndex(&index));
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->getImmediateFlags(&flags));
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->getImmediateMode(&mode));
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->getImmediatePriority(&priority));
    EXPECT_NE(nullptr, whiteboxCommandList->cmdQImmediate);
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->cmdQImmediate->getIndex(&cmdQIndex));
    EXPECT_EQ(cmdQIndex, index);
    EXPECT_EQ(desc.flags, flags);
    EXPECT_EQ(desc.mode, mode);
    EXPECT_EQ(desc.priority, priority);

    ze_bool_t isMutable = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->isMutableExp(&isMutable));
    EXPECT_FALSE(static_cast<bool>(isMutable));

    whiteboxCommandList->destroy();
}

TEST_F(CommandListCreate, givenQueueFlagsWhenCreatingImmediateCommandListThenDontCopyFlags) {
    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
    ze_command_list_handle_t commandList = nullptr;

    auto returnValue = device->createCommandListImmediate(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    auto whiteboxCommandList = CommandList::whiteboxCast(CommandList::fromHandle(commandList));
    EXPECT_EQ(0u, whiteboxCommandList->flags);

    whiteboxCommandList->destroy();
}

TEST_F(CommandListCreate, givenRootDeviceAndImplicitScalingDisabledWhenCreatingCommandListThenValidateQueueOrdinalUsingSubDeviceEngines) {
    NEO::UltDeviceFactory deviceFactory{1, 2};
    auto &rootDevice = *deviceFactory.rootDevices[0];
    auto &subDevice0 = *deviceFactory.subDevices[0];
    rootDevice.regularEngineGroups.resize(1);
    subDevice0.getRegularEngineGroups().push_back(NEO::EngineGroupT{});
    subDevice0.getRegularEngineGroups().back().engineGroupType = EngineGroupType::compute;
    subDevice0.getRegularEngineGroups().back().engines.resize(1);
    subDevice0.getRegularEngineGroups().back().engines[0].commandStreamReceiver = &rootDevice.getGpgpuCommandStreamReceiver();
    auto ordinal = static_cast<uint32_t>(subDevice0.getRegularEngineGroups().size() - 1);
    MockDeviceImp l0RootDevice(&rootDevice);

    ze_command_list_handle_t commandList = nullptr;
    ze_command_list_desc_t cmdDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdDesc.commandQueueGroupOrdinal = ordinal;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.ordinal = ordinal;
    queueDesc.index = 0;

    l0RootDevice.driverHandle = driverHandle.get();

    l0RootDevice.implicitScalingCapable = true;
    auto returnValue = l0RootDevice.createCommandList(&cmdDesc, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);

    returnValue = l0RootDevice.createCommandListImmediate(&queueDesc, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);

    l0RootDevice.implicitScalingCapable = false;
    returnValue = l0RootDevice.createCommandList(&cmdDesc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_NE(nullptr, commandList);
    L0::CommandList::fromHandle(commandList)->destroy();
    commandList = nullptr;

    returnValue = l0RootDevice.createCommandListImmediate(&queueDesc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_NE(nullptr, commandList);
    L0::CommandList::fromHandle(commandList)->destroy();
}

HWTEST2_F(CommandListCreate, givenSingleTileOnlyPlatformsWhenProgrammingMultiTileBarrierThenNoProgrammingIsExpected, IsGen12LP) {
    auto neoDevice = device->getNEODevice();

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(0u, commandList->estimateBufferSizeMultiTileBarrier(neoDevice->getRootDeviceEnvironment()));

    auto cmdListStream = commandList->getCmdContainer().getCommandStream();
    size_t usedBefore = cmdListStream->getUsed();
    commandList->appendMultiTileBarrier(*neoDevice);
    size_t usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
HWTEST_F(CommandListAppendLaunchKernel, givenSignalEventWhenAppendLaunchCooperativeKernelIsCalledThenSuccessIsReturned) {
    createKernel();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    ze_group_count_t groupCount{1, 1, 1};

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, cooperativeParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(event.get(), commandList->appendKernelEventValue);
}

HWTEST_F(CommandListAppendLaunchKernel, givenSignalEventWhenAppendLaunchMultipleIndirectKernelIsCalledThenSuccessIsReturned) {
    createKernel();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    const ze_kernel_handle_t launchKernels = kernel->toHandle();
    const ze_group_count_t launchKernelArgs = {1, 1, 1};
    uint32_t *numLaunchArgs;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    returnValue = context->allocDeviceMem(
        device->toHandle(), &deviceDesc, 16 * MemoryConstants::kiloByte, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = commandList->appendLaunchMultipleKernelsIndirect(1, &launchKernels, numLaunchArgs, &launchKernelArgs, event->toHandle(), 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(event->toHandle(), commandList->appendEventMultipleKernelIndirectEventHandleValue);

    context->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

HWTEST_F(CommandListAppendLaunchKernel, givenSignalEventWhenAppendLaunchIndirectKernelIsCalledThenSuccessIsReturned) {
    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);

    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    kernel.privateState.groupSize[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.workDim = 4;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16 * MemoryConstants::kiloByte, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), *static_cast<ze_group_count_t *>(alloc), event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->toHandle(), commandList->appendEventKernelIndirectEventHandleValue);

    context->freeMem(alloc);
}

HWTEST2_F(CommandListAppendLaunchKernel, GivenComputeModePropertiesWhenUpdateStreamPropertiesIsCalledTwiceThenChangedFieldsAreDirty, IsHeapfulRequired) {
    DebugManagerStateRestore restorer;
    auto &productHelper = device->getProductHelper();

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x100;
    const ze_group_count_t launchKernelArgs = {};
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    if (commandList->stateComputeModeTracking) {
        EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    } else {
        EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x80;
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
        EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }
    EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
}

struct ProgramAllFieldsInComputeMode {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return !TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::programOnlyChangedFieldsInComputeStateMode;
    }
};

HWTEST2_F(CommandListAppendLaunchKernel,
          GivenComputeModeTraitsSetToFalsePropertiesWhenUpdateStreamPropertiesIsCalledTwiceThenFieldsAreDirtyWithTrackingAndCleanWithoutTracking,
          ProgramAllFieldsInComputeMode) {
    DebugManagerStateRestore restorer;
    auto &productHelper = device->getProductHelper();

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x100;
    const ze_group_count_t launchKernelArgs = {};
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    if (commandList->stateComputeModeTracking) {
        if (productHelper.isGrfNumReportedWithScm()) {
            EXPECT_NE(-1, commandList->finalStreamState.stateComputeMode.largeGrfMode.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.largeGrfMode.value);
        }
    } else {
        EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x80;
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
}

HWTEST2_F(CommandListAppendLaunchKernel, GivenComputeModePropertiesWhenPropertesNotChangedThenAllFieldsAreNotDirty, IsHeapfulRequired) {
    DebugManagerStateRestore restorer;
    auto &productHelper = device->getProductHelper();

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x100;
    const ze_group_count_t launchKernelArgs = {};
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    if (commandList->stateComputeModeTracking) {
        EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    } else {
        EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }

    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
}

HWTEST_F(CommandListCreate, givenFlushErrorWhenPerformingCpuMemoryCopyThenErrorIsReturned) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = false;

    ze_result_t returnValue;

    using cmdListImmediateHwType = typename L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>;

    std::unique_ptr<cmdListImmediateHwType> commandList0(static_cast<cmdListImmediateHwType *>(CommandList::createImmediate(device,
                                                                                                                            &desc,
                                                                                                                            internalEngine,
                                                                                                                            NEO::EngineGroupType::renderCompute,
                                                                                                                            returnValue)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList0);

    auto &commandStreamReceiver = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    CpuMemCopyInfo cpuMemCopyInfo(nullptr, nullptr, 8);
    returnValue = commandList0->performCpuMemcpy(cpuMemCopyInfo, nullptr, 6, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, returnValue);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;

    returnValue = commandList0->performCpuMemcpy(cpuMemCopyInfo, nullptr, 6, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, returnValue);
}

HWTEST_F(CommandListCreate, givenImmediateCommandListWhenAppendingMemoryCopyThenSuccessIsReturned) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList0.get());

    EXPECT_EQ(whiteBoxCmdList->cmdQImmediate->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    size_t src = 0;
    size_t dst = 0;
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList0->appendMemoryCopy(&dst, &src, sizeof(size_t), nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListCreate, givenImmediateCommandListWhenAppendingMemoryCopyWithInvalidEventThenInvalidArgumentErrorIsReturned) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList0.get());

    EXPECT_EQ(whiteBoxCmdList->cmdQImmediate->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 1, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

using CmdlistAppendLaunchKernelTests = Test<ModuleImmutableDataFixture>;

HWTEST2_F(CmdlistAppendLaunchKernelTests,
          givenImmediateCommandListUsesFlushTaskWhenDispatchingKernelWithSpillScratchSpaceThenExpectCsrHasCorrectValuesSet, IsGen12LP) {
    constexpr uint32_t scratchPerThreadSize = 0x200;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = scratchPerThreadSize;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    kernel->setGroupSize(4, 5, 6);
    kernel->setGroupCount(3, 2, 1);
    kernel->setGlobalOffsetExp(1, 2, 3);
    kernel->patchGlobalOffset();

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    commandList->cmdQImmediate = CommandQueue::create(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, ret);

    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(scratchPerThreadSize, commandList->getCommandListPerThreadScratchSize(0u));

    auto ultCsr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    EXPECT_EQ(scratchPerThreadSize, ultCsr->requiredScratchSlot0Size);
}

HWTEST2_F(CmdlistAppendLaunchKernelTests,
          givenScratchAllocationFailureWhenAppendingKernelWithScratchToImmediateCommandListThenOutOfDeviceMemoryIsReturnedAndNothingIsRecorded, IsAtLeastXeCore) {
    constexpr uint32_t scratchPerThreadSize = 0x200;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = scratchPerThreadSize;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());
    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);
    kernel->setGroupSize(4, 5, 6);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    commandList->device = device;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t ret = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    commandList->cmdQImmediate = CommandQueue::create(device, csr, &desc, false, false, false, ret);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u));
    commandList->getCmdContainer().setImmediateCmdListCsr(csr);

    auto memoryManager = static_cast<NEO::MockMemoryManager *>(device->getNEODevice()->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->validateAllocateProperties = [memoryManager](const NEO::AllocationProperties &properties) {
        memoryManager->forceFailureInPrimaryAllocation = (properties.allocationType == NEO::AllocationType::scratchSurface);
    };

    auto commandStream = commandList->getCmdContainer().getCommandStream();
    const auto commandStreamUsedBefore = commandStream->getUsed();
    const auto taskCountBefore = csr->peekTaskCount();

    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    CmdListNonUniformLaunchArgs nonUniformLaunchArgs = {};
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->appendLaunchKernelNonUniform(kernel->toHandle(), nonUniformLaunchArgs, nullptr, 0, nullptr, false));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->appendLaunchKernelIndirect(kernel->toHandle(), groupCount, nullptr, 0, nullptr, false));

    EXPECT_EQ(commandStreamUsedBefore, commandStream->getUsed());
    EXPECT_EQ(taskCountBefore, csr->peekTaskCount());
    EXPECT_EQ(0u, commandList->getCommandListPerThreadScratchSize(0u));

    memoryManager->validateAllocateProperties = [](const NEO::AllocationProperties &) -> void {};
    memoryManager->forceFailureInPrimaryAllocation = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    EXPECT_EQ(scratchPerThreadSize, commandList->getCommandListPerThreadScratchSize(0u));
}

HWTEST2_F(CmdlistAppendLaunchKernelTests,
          givenCommandListScratchSizeAndScratchAllocationFailureWhenReservingScratchSpaceForKernelThenOnlyKernelAndExternalScratchSizesAreReserved, IsAtLeastXeCore) {
    constexpr uint32_t scratchPerThreadSize = 0x200;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0u;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());
    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    commandList->device = device;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t ret = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    commandList->cmdQImmediate = CommandQueue::create(device, csr, &desc, false, false, false, ret);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u));
    commandList->getCmdContainer().setImmediateCmdListCsr(csr);
    commandList->setCommandListPerThreadScratchSize(0u, scratchPerThreadSize);
    commandList->setCommandListPerThreadScratchSize(1u, scratchPerThreadSize);

    auto memoryManager = static_cast<NEO::MockMemoryManager *>(device->getNEODevice()->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->validateAllocateProperties = [memoryManager](const NEO::AllocationProperties &properties) {
        memoryManager->forceFailureInPrimaryAllocation = (properties.allocationType == NEO::AllocationType::scratchSurface);
    };

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->reserveScratchSpaceForKernel(kernel->toHandle(), 0u, 0u));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->reserveScratchSpaceForKernel(kernel->toHandle(), scratchPerThreadSize, 0u));

    kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = scratchPerThreadSize;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->reserveScratchSpaceForKernel(kernel->toHandle(), 0u, 0u));

    memoryManager->validateAllocateProperties = [](const NEO::AllocationProperties &) -> void {};
    memoryManager->forceFailureInPrimaryAllocation = false;
}

HWTEST2_F(CmdlistAppendLaunchKernelTests,
          givenImmediateCommandListUsesFlushTaskWhenDispatchingKernelWithSpillAndPrivateScratchSpaceThenExpectCsrHasCorrectValuesSet, IsAtLeastXeCore) {
    constexpr uint32_t scratch0PerThreadSize = 0x200;
    constexpr uint32_t scratch1PerThreadSize = 0x100;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = scratch0PerThreadSize;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[1] = scratch1PerThreadSize;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    kernel->setGroupSize(4, 5, 6);
    kernel->setGroupCount(3, 2, 1);
    kernel->setGlobalOffsetExp(1, 2, 3);
    kernel->patchGlobalOffset();

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    commandList->device = device;
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;

    ze_result_t ret = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    commandList->cmdQImmediate = CommandQueue::create(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, ret);

    ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    commandList->getCmdContainer().setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(scratch0PerThreadSize, commandList->getCommandListPerThreadScratchSize(0u));
    EXPECT_EQ(scratch1PerThreadSize, commandList->getCommandListPerThreadScratchSize(1u));

    auto ultCsr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    EXPECT_EQ(scratch0PerThreadSize, ultCsr->requiredScratchSlot0Size);
    EXPECT_EQ(scratch1PerThreadSize, ultCsr->requiredScratchSlot1Size);
}

struct FrontEndMultiReturnMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return !(TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::heaplessRequired);
    }
};

using FrontEndMultiReturnCommandListTest = Test<FrontEndCommandListFixture<0>>;

HWTEST2_F(FrontEndMultiReturnCommandListTest, givenFrontEndTrackingIsUsedWhenPropertyDisableEuFusionSupportedThenExpectReturnPointsAndBbEndProgramming, FrontEndMultiReturnMatcher) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    NEO::FrontEndPropertiesSupport fePropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, device->getHwInfo());

    EXPECT_TRUE(commandList->frontEndStateTracking);

    auto &cmdStream = *commandList->getCmdContainer().getCommandStream();
    auto &cmdBuffers = commandList->getCmdContainer().getCmdBufferAllocations();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledEUFusion = 1;

    size_t usedBefore = cmdStream.getUsed();
    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t usedAfter = cmdStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    ASSERT_NE(0u, cmdList.size());

    if (fePropertiesSupport.disableEuFusion) {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_NE(nullptr, bbEndCmd);

        ASSERT_EQ(1u, commandList->getReturnPointsSize());
        auto &returnPoint = commandList->getReturnPoints()[0];

        uint64_t expectedGpuAddress = cmdStream.getGpuBase() + usedBefore + sizeof(MI_BATCH_BUFFER_END);
        EXPECT_EQ(expectedGpuAddress, returnPoint.gpuAddress);
        EXPECT_EQ(cmdStream.getGraphicsAllocation(), returnPoint.currentCmdBuffer);
        EXPECT_TRUE(returnPoint.configSnapshot.frontEndState.disableEUFusion.isDirty);
        EXPECT_EQ(1, returnPoint.configSnapshot.frontEndState.disableEUFusion.value);

        EXPECT_EQ(1u, cmdBuffers.size());
        EXPECT_EQ(cmdBuffers[0], returnPoint.currentCmdBuffer);
    } else {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_EQ(nullptr, bbEndCmd);

        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }

    usedBefore = cmdStream.getUsed();
    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    usedAfter = cmdStream.getUsed();

    cmdList.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    ASSERT_NE(0u, cmdList.size());

    if (fePropertiesSupport.disableEuFusion) {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_EQ(nullptr, bbEndCmd);

        EXPECT_EQ(1u, commandList->getReturnPointsSize());
    } else {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_EQ(nullptr, bbEndCmd);

        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledEUFusion = 0;

    cmdStream.getSpace(cmdStream.getAvailableSpace() - sizeof(MI_BATCH_BUFFER_END));
    auto oldCmdBuffer = cmdStream.getGraphicsAllocation();

    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    usedBefore = 0;
    usedAfter = cmdStream.getUsed();

    auto newCmdBuffer = cmdStream.getGraphicsAllocation();
    ASSERT_NE(oldCmdBuffer, newCmdBuffer);

    cmdList.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    ASSERT_NE(0u, cmdList.size());

    if (fePropertiesSupport.disableEuFusion) {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_NE(nullptr, bbEndCmd);

        ASSERT_EQ(2u, commandList->getReturnPointsSize());
        auto &returnPoint = commandList->getReturnPoints()[1];

        uint64_t expectedGpuAddress = cmdStream.getGpuBase() + usedBefore + sizeof(MI_BATCH_BUFFER_END);
        EXPECT_EQ(expectedGpuAddress, returnPoint.gpuAddress);
        EXPECT_EQ(cmdStream.getGraphicsAllocation(), returnPoint.currentCmdBuffer);
        EXPECT_TRUE(returnPoint.configSnapshot.frontEndState.disableEUFusion.isDirty);
        EXPECT_EQ(0, returnPoint.configSnapshot.frontEndState.disableEUFusion.value);

        EXPECT_EQ(2u, cmdBuffers.size());
        EXPECT_EQ(cmdBuffers[1], returnPoint.currentCmdBuffer);
    }

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledEUFusion = 1;

    cmdStream.getSpace(cmdStream.getAvailableSpace() - 2 * sizeof(MI_BATCH_BUFFER_END));

    usedBefore = cmdStream.getUsed();
    void *oldBase = cmdStream.getCpuBase();
    oldCmdBuffer = cmdStream.getGraphicsAllocation();

    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    newCmdBuffer = cmdStream.getGraphicsAllocation();
    ASSERT_NE(oldCmdBuffer, newCmdBuffer);

    cmdList.clear();

    size_t parseSpace = sizeof(MI_BATCH_BUFFER_END);
    if (fePropertiesSupport.disableEuFusion) {
        parseSpace *= 2;
    }

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(oldBase, usedBefore),
        parseSpace));

    if (fePropertiesSupport.disableEuFusion) {
        ASSERT_EQ(2u, cmdList.size());
        for (auto &cmd : cmdList) {
            auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(cmd);
            EXPECT_NE(nullptr, bbEndCmd);
        }
        ASSERT_EQ(3u, commandList->getReturnPointsSize());
        auto &returnPoint = commandList->getReturnPoints()[2];

        uint64_t expectedGpuAddress = oldCmdBuffer->getGpuAddress() + usedBefore + sizeof(MI_BATCH_BUFFER_END);
        EXPECT_EQ(expectedGpuAddress, returnPoint.gpuAddress);
        EXPECT_EQ(oldCmdBuffer, returnPoint.currentCmdBuffer);
        EXPECT_TRUE(returnPoint.configSnapshot.frontEndState.disableEUFusion.isDirty);
        EXPECT_EQ(1, returnPoint.configSnapshot.frontEndState.disableEUFusion.value);

        EXPECT_EQ(3u, cmdBuffers.size());
        EXPECT_EQ(cmdBuffers[1], returnPoint.currentCmdBuffer);
    } else {
        ASSERT_EQ(1u, cmdList.size());
        for (auto &cmd : cmdList) {
            auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(cmd);
            EXPECT_NE(nullptr, bbEndCmd);
        }
    }

    if (fePropertiesSupport.disableEuFusion) {
        commandList->reset();
        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }
}

HWTEST2_F(FrontEndMultiReturnCommandListTest, givenFrontEndTrackingIsUsedWhenPropertyComputeDispatchAllWalkerSupportedThenExpectReturnPointsAndBbEndProgramming, FrontEndMultiReturnMatcher) {
    debugManager.flags.EnableMemoryPrefetch.set(0);
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    NEO::FrontEndPropertiesSupport fePropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, device->getHwInfo());

    EXPECT_TRUE(commandList->frontEndStateTracking);

    auto &cmdStream = *commandList->getCmdContainer().getCommandStream();
    auto &cmdBuffers = commandList->getCmdContainer().getCmdBufferAllocations();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    size_t usedBefore = cmdStream.getUsed();
    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    size_t usedAfter = cmdStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    ASSERT_NE(0u, cmdList.size());

    if (fePropertiesSupport.computeDispatchAllWalker) {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_NE(nullptr, bbEndCmd);

        EXPECT_EQ(1u, commandList->getReturnPointsSize());
        auto &returnPoint = commandList->getReturnPoints()[0];

        uint64_t expectedGpuAddress = cmdStream.getGpuBase() + usedBefore + sizeof(MI_BATCH_BUFFER_END);
        EXPECT_EQ(expectedGpuAddress, returnPoint.gpuAddress);
        EXPECT_EQ(cmdStream.getGraphicsAllocation(), returnPoint.currentCmdBuffer);
        EXPECT_TRUE(returnPoint.configSnapshot.frontEndState.computeDispatchAllWalkerEnable.isDirty);
        EXPECT_EQ(1, returnPoint.configSnapshot.frontEndState.computeDispatchAllWalkerEnable.value);
    } else {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_EQ(nullptr, bbEndCmd);

        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }

    usedBefore = cmdStream.getUsed();
    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    usedAfter = cmdStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    ASSERT_NE(0u, cmdList.size());

    if (fePropertiesSupport.computeDispatchAllWalker) {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_EQ(nullptr, bbEndCmd);

        EXPECT_EQ(1u, commandList->getReturnPointsSize());
    } else {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_EQ(nullptr, bbEndCmd);

        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }

    auto oldCmdBuffer = cmdStream.getGraphicsAllocation();
    void *oldBase = cmdStream.getCpuBase();
    cmdStream.getSpace(cmdStream.getAvailableSpace() - 2 * sizeof(MI_BATCH_BUFFER_END));
    usedBefore = cmdStream.getUsed();
    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto newCmdBuffer = cmdStream.getGraphicsAllocation();
    ASSERT_NE(oldCmdBuffer, newCmdBuffer);

    cmdList.clear();

    size_t parseSpace = sizeof(MI_BATCH_BUFFER_END);
    if (fePropertiesSupport.computeDispatchAllWalker) {
        parseSpace *= 2;
    }

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(oldBase, usedBefore),
        parseSpace));

    if (fePropertiesSupport.computeDispatchAllWalker) {
        ASSERT_EQ(2u, cmdList.size());
        for (auto &cmd : cmdList) {
            auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(cmd);
            EXPECT_NE(nullptr, bbEndCmd);
        }
        ASSERT_EQ(2u, commandList->getReturnPointsSize());
        auto &returnPoint = commandList->getReturnPoints()[1];

        uint64_t expectedGpuAddress = oldCmdBuffer->getGpuAddress() + usedBefore + sizeof(MI_BATCH_BUFFER_END);
        EXPECT_EQ(expectedGpuAddress, returnPoint.gpuAddress);
        EXPECT_EQ(oldCmdBuffer, returnPoint.currentCmdBuffer);
        EXPECT_TRUE(returnPoint.configSnapshot.frontEndState.computeDispatchAllWalkerEnable.isDirty);
        EXPECT_EQ(0, returnPoint.configSnapshot.frontEndState.computeDispatchAllWalkerEnable.value);

        EXPECT_EQ(2u, cmdBuffers.size());
        EXPECT_EQ(cmdBuffers[0], returnPoint.currentCmdBuffer);
    } else {
        ASSERT_EQ(1u, cmdList.size());
        for (auto &cmd : cmdList) {
            auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(cmd);
            EXPECT_NE(nullptr, bbEndCmd);
        }
    }

    cmdStream.getSpace(cmdStream.getAvailableSpace() - sizeof(MI_BATCH_BUFFER_END));
    oldCmdBuffer = cmdStream.getGraphicsAllocation();

    usedBefore = 0;
    commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    usedAfter = cmdStream.getUsed();

    newCmdBuffer = cmdStream.getGraphicsAllocation();
    ASSERT_NE(oldCmdBuffer, newCmdBuffer);

    cmdList.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    ASSERT_NE(0u, cmdList.size());

    if (fePropertiesSupport.computeDispatchAllWalker) {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_NE(nullptr, bbEndCmd);

        ASSERT_EQ(3u, commandList->getReturnPointsSize());
        auto &returnPoint = commandList->getReturnPoints()[2];

        uint64_t expectedGpuAddress = cmdStream.getGpuBase() + usedBefore + sizeof(MI_BATCH_BUFFER_END);
        EXPECT_EQ(expectedGpuAddress, returnPoint.gpuAddress);
        EXPECT_EQ(cmdStream.getGraphicsAllocation(), returnPoint.currentCmdBuffer);
        EXPECT_TRUE(returnPoint.configSnapshot.frontEndState.computeDispatchAllWalkerEnable.isDirty);
        EXPECT_EQ(1, returnPoint.configSnapshot.frontEndState.computeDispatchAllWalkerEnable.value);

        EXPECT_EQ(3u, cmdBuffers.size());
        EXPECT_EQ(cmdBuffers[2], returnPoint.currentCmdBuffer);
    } else {
        auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdList.begin());
        EXPECT_EQ(nullptr, bbEndCmd);

        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }

    if (fePropertiesSupport.computeDispatchAllWalker) {
        commandList->reset();
        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }
}

HWTEST2_F(FrontEndMultiReturnCommandListTest,
          givenFrontEndTrackingCmdListIsExecutedWhenPropertyDisableEuFusionSupportedThenExpectFrontEndProgrammingInCmdQueue, FrontEndMultiReturnMatcher) {
    using FrontEndStateCommand = typename FamilyType::FrontEndStateCommand;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    NEO::FrontEndPropertiesSupport fePropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, device->getHwInfo());

    EXPECT_TRUE(commandList->frontEndStateTracking);
    EXPECT_TRUE(commandQueue->frontEndStateTracking);

    auto &cmdListStream = *commandList->getCmdContainer().getCommandStream();
    auto &cmdListBuffers = commandList->getCmdContainer().getCmdBufferAllocations();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledEUFusion = 1;

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledEUFusion = 0;
    cmdListStream.getSpace(cmdListStream.getAvailableSpace() - sizeof(MI_BATCH_BUFFER_END));

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledEUFusion = 1;
    cmdListStream.getSpace(cmdListStream.getAvailableSpace() - 2 * sizeof(MI_BATCH_BUFFER_END));

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(3u, commandList->getReturnPointsSize());
    } else {
        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }

    auto &returnPoints = commandList->getReturnPoints();

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(3u, cmdListBuffers.size());

    auto &cmdQueueStream = commandQueue->commandStream;
    size_t usedBefore = cmdQueueStream.getUsed();

    auto cmdListHandle = commandList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedAfter = cmdQueueStream.getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    ASSERT_NE(0u, cmdList.size());
    auto nextIt = cmdList.begin();

    if (fePropertiesSupport.disableEuFusion) {
        auto feCmdList = findAll<FrontEndStateCommand *>(nextIt, cmdList.end());
        EXPECT_EQ(4u, feCmdList.size());
        auto bbStartCmdList = findAll<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
        EXPECT_EQ(6u, bbStartCmdList.size());

        // initial FE -> requiresDisabledEUFusion = 0
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_FALSE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // initial jump to 1st cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[0]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = bbStartIt;
        }
        // reconfiguration FE -> requiresDisabledEUFusion = 1
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_TRUE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // jump to 1st cmd buffer after reconfiguration
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = returnPoints[0].gpuAddress;
            EXPECT_EQ(cmdListBuffers[0], returnPoints[0].currentCmdBuffer);

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = ++bbStartIt;
        }

        // jump to 2nd cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[1]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = bbStartIt;
        }

        // reconfiguration FE -> requiresDisabledEUFusion = 0
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_FALSE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // jump to 2nd cmd buffer after 2nd reconfiguration
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = returnPoints[1].gpuAddress;
            EXPECT_EQ(cmdListBuffers[1], returnPoints[1].currentCmdBuffer);

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = bbStartIt;
        }

        // reconfiguration FE -> requiresDisabledEUFusion = 1
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_TRUE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // jump to 2nd cmd buffer after 3rd reconfiguration
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = returnPoints[2].gpuAddress;
            EXPECT_EQ(cmdListBuffers[1], returnPoints[2].currentCmdBuffer);

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = ++bbStartIt;
        }

        // jump to 3rd cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[2]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
        }
    } else {
        auto feCmdList = findAll<FrontEndStateCommand *>(nextIt, cmdList.end());
        EXPECT_EQ(1u, feCmdList.size());
        auto bbStartCmdList = findAll<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
        EXPECT_EQ(3u, bbStartCmdList.size());

        // initial FE
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_FALSE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // jump to 1st cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[0]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = ++bbStartIt;
        }
        // jump to 2nd cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[1]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = ++bbStartIt;
        }
        // jump to 3rd cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[2]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
        }
    }
}

HWTEST2_F(FrontEndMultiReturnCommandListTest,
          givenFrontEndTrackingCmdListIsExecutedWhenPropertyComputeDispatchAllWalkerSupportedThenExpectFrontEndProgrammingInCmdQueue, FrontEndMultiReturnMatcher) {
    debugManager.flags.EnableMemoryPrefetch.set(0);
    using FrontEndStateCommand = typename FamilyType::FrontEndStateCommand;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    NEO::FrontEndPropertiesSupport fePropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, device->getHwInfo());

    EXPECT_TRUE(commandList->frontEndStateTracking);
    EXPECT_TRUE(commandQueue->frontEndStateTracking);

    auto &cmdListStream = *commandList->getCmdContainer().getCommandStream();
    auto &cmdListBuffers = commandList->getCmdContainer().getCmdBufferAllocations();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdListStream.getSpace(cmdListStream.getAvailableSpace() - 2 * sizeof(MI_BATCH_BUFFER_END));

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdListStream.getSpace(cmdListStream.getAvailableSpace() - sizeof(MI_BATCH_BUFFER_END));

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (fePropertiesSupport.computeDispatchAllWalker) {
        EXPECT_EQ(3u, commandList->getReturnPointsSize());
    } else {
        EXPECT_EQ(0u, commandList->getReturnPointsSize());
    }

    auto &returnPoints = commandList->getReturnPoints();

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(3u, cmdListBuffers.size());

    auto &cmdQueueStream = commandQueue->commandStream;
    size_t usedBefore = cmdQueueStream.getUsed();

    auto cmdListHandle = commandList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedAfter = cmdQueueStream.getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    ASSERT_NE(0u, cmdList.size());
    auto nextIt = cmdList.begin();

    if (fePropertiesSupport.computeDispatchAllWalker) {
        auto feCmdList = findAll<FrontEndStateCommand *>(nextIt, cmdList.end());
        EXPECT_EQ(4u, feCmdList.size());
        auto bbStartCmdList = findAll<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
        EXPECT_EQ(6u, bbStartCmdList.size());

        // initial FE -> computeDispatchAllWalker = 0
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_FALSE(NEO::UnitTestHelperWithHeap<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // initial jump to 1st cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[0]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = bbStartIt;
        }

        // reconfiguration FE -> computeDispatchAllWalker = 1
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_TRUE(NEO::UnitTestHelperWithHeap<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // jump to 1st cmd buffer after reconfiguration
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = returnPoints[0].gpuAddress;
            EXPECT_EQ(cmdListBuffers[0], returnPoints[0].currentCmdBuffer);

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = bbStartIt;
        }

        // reconfiguration FE -> requiresDisabledEUFusion = 0
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_FALSE(NEO::UnitTestHelperWithHeap<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // jump to 2nd cmd buffer after 2nd reconfiguration
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = returnPoints[1].gpuAddress;
            EXPECT_EQ(cmdListBuffers[0], returnPoints[1].currentCmdBuffer);

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = ++bbStartIt;
        }

        // jump to 2nd cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[1]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = ++bbStartIt;
        }

        // jump to 3rd cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[2]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = bbStartIt;
        }

        // reconfiguration FE -> requiresDisabledEUFusion = 1
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_TRUE(NEO::UnitTestHelperWithHeap<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // jump to 3nd cmd buffer after 3rd reconfiguration
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = returnPoints[2].gpuAddress;
            EXPECT_EQ(cmdListBuffers[2], returnPoints[2].currentCmdBuffer);

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
        }

    } else {
        auto feCmdList = findAll<FrontEndStateCommand *>(nextIt, cmdList.end());
        EXPECT_EQ(1u, feCmdList.size());
        auto bbStartCmdList = findAll<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
        EXPECT_EQ(3u, bbStartCmdList.size());

        // initial FE
        {
            auto feStateIt = find<FrontEndStateCommand *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), feStateIt);
            auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateIt);
            EXPECT_FALSE(NEO::UnitTestHelperWithHeap<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

            nextIt = feStateIt;
        }
        // jump to 1st cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[0]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = ++bbStartIt;
        }
        // jump to 2nd cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[1]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());

            nextIt = ++bbStartIt;
        }
        // jump to 3rd cmd buffer
        {
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(nextIt, cmdList.end());
            ASSERT_NE(cmdList.end(), bbStartIt);
            auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*bbStartIt);

            uint64_t bbStartGpuAddress = cmdListBuffers[2]->getGpuAddress();

            EXPECT_EQ(bbStartGpuAddress, bbStart->getBatchBufferStartAddress());
            EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStart->getSecondLevelBatchBuffer());
        }
    }
}

HWTEST2_F(FrontEndMultiReturnCommandListTest, givenCmdQueueAndImmediateCmdListUseSameCsrWhenAppendingKernelOnBothRegularFirstThenFrontEndStateIsNotChanged, FrontEndMultiReturnMatcher) {
    using FrontEndStateCommand = typename FamilyType::FrontEndStateCommand;
    NEO::FrontEndPropertiesSupport fePropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, device->getHwInfo());

    EXPECT_TRUE(commandList->frontEndStateTracking);
    EXPECT_TRUE(commandListImmediate->frontEndStateTracking);

    auto &regularCmdListStream = *commandList->getCmdContainer().getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledEUFusion = 1;

    size_t usedBefore = regularCmdListStream.getUsed();
    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = regularCmdListStream.getUsed();

    auto &regularCmdListRequiredState = commandList->getRequiredStreamState();
    auto &regularCmdListFinalState = commandList->getFinalStreamState();

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(1, regularCmdListRequiredState.frontEndState.disableEUFusion.value);
        EXPECT_EQ(1, regularCmdListFinalState.frontEndState.disableEUFusion.value);
    } else {
        EXPECT_EQ(-1, regularCmdListRequiredState.frontEndState.disableEUFusion.value);
        EXPECT_EQ(-1, regularCmdListFinalState.frontEndState.disableEUFusion.value);
    }

    commandList->close();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(regularCmdListStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    auto feStateCmds = findAll<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, feStateCmds.size());

    auto &cmdQueueStream = commandQueue->commandStream;
    auto cmdListHandle = commandList->toHandle();

    usedBefore = cmdQueueStream.getUsed();
    CommandListExecutionInternalOptions internalOptions = {};
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    usedAfter = cmdQueueStream.getUsed();

    auto cmdQueueCsr = commandQueue->getCsr();
    auto &csrProperties = cmdQueueCsr->getStreamProperties();

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(1, csrProperties.frontEndState.disableEUFusion.value);
    } else {
        EXPECT_EQ(-1, csrProperties.frontEndState.disableEUFusion.value);
    }

    cmdList.clear();
    feStateCmds.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    feStateCmds = findAll<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, feStateCmds.size());
    auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateCmds[0]);
    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_TRUE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));
    } else {
        EXPECT_FALSE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));
    }

    auto &immediateCmdListStream = *commandListImmediate->getCmdContainer().getCommandStream();
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = ultCsr.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    usedBefore = immediateCmdListStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    usedAfter = immediateCmdListStream.getUsed();
    size_t csrUsedAfter = csrStream.getUsed();

    auto &immediateCmdListRequiredState = commandListImmediate->getRequiredStreamState();

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(1, immediateCmdListRequiredState.frontEndState.disableEUFusion.value);
    } else {
        EXPECT_EQ(-1, immediateCmdListRequiredState.frontEndState.disableEUFusion.value);
    }

    cmdList.clear();
    feStateCmds.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(immediateCmdListStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    feStateCmds = findAll<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, feStateCmds.size());

    auto immediateCsr = commandListImmediate->getCsr(false);
    EXPECT_EQ(cmdQueueCsr, immediateCsr);

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(1, csrProperties.frontEndState.disableEUFusion.value);
    } else {
        EXPECT_EQ(-1, csrProperties.frontEndState.disableEUFusion.value);
    }

    if (csrUsedAfter > csrUsedBefore) {
        cmdList.clear();
        feStateCmds.clear();

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
            (csrUsedAfter - csrUsedBefore)));
        feStateCmds = findAll<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, feStateCmds.size());
    }
}

HWTEST2_F(FrontEndMultiReturnCommandListTest, givenCmdQueueAndImmediateCmdListUseSameCsrWhenAppendingKernelOnBothImmediateFirstThenFrontEndStateIsNotChanged, FrontEndMultiReturnMatcher) {
    using FrontEndStateCommand = typename FamilyType::FrontEndStateCommand;
    NEO::FrontEndPropertiesSupport fePropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, device->getHwInfo());

    EXPECT_TRUE(commandList->frontEndStateTracking);
    EXPECT_TRUE(commandListImmediate->frontEndStateTracking);

    auto cmdQueueCsr = commandQueue->getCsr();
    auto &csrProperties = cmdQueueCsr->getStreamProperties();

    auto immediateCsr = commandListImmediate->getCsr(false);
    EXPECT_EQ(cmdQueueCsr, immediateCsr);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledEUFusion = 1;

    auto &immediateCmdListStream = *commandListImmediate->getCmdContainer().getCommandStream();
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = ultCsr.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    size_t usedBefore = immediateCmdListStream.getUsed();
    ze_result_t result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = immediateCmdListStream.getUsed();
    size_t csrUsedAfter = csrStream.getUsed();

    auto &immediateCmdListRequiredState = commandListImmediate->getRequiredStreamState();

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(1, immediateCmdListRequiredState.frontEndState.disableEUFusion.value);
    } else {
        EXPECT_EQ(-1, immediateCmdListRequiredState.frontEndState.disableEUFusion.value);
    }

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(immediateCmdListStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    auto feStateCmds = findAll<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, feStateCmds.size());

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(1, csrProperties.frontEndState.disableEUFusion.value);
    } else {
        EXPECT_EQ(-1, csrProperties.frontEndState.disableEUFusion.value);
    }

    cmdList.clear();
    feStateCmds.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        (csrUsedAfter - csrUsedBefore)));
    feStateCmds = findAll<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, feStateCmds.size());
    auto &feState = *genCmdCast<FrontEndStateCommand *>(*feStateCmds[0]);
    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_TRUE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));
    } else {
        EXPECT_FALSE(NEO::UnitTestHelperWithHeap<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));
    }

    auto &regularCmdListStream = *commandList->getCmdContainer().getCommandStream();

    usedBefore = regularCmdListStream.getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    usedAfter = regularCmdListStream.getUsed();

    auto &regularCmdListRequiredState = commandList->getRequiredStreamState();
    auto &regularCmdListFinalState = commandList->getFinalStreamState();

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(1, regularCmdListRequiredState.frontEndState.disableEUFusion.value);
        EXPECT_EQ(1, regularCmdListFinalState.frontEndState.disableEUFusion.value);
    } else {
        EXPECT_EQ(-1, regularCmdListRequiredState.frontEndState.disableEUFusion.value);
        EXPECT_EQ(-1, regularCmdListFinalState.frontEndState.disableEUFusion.value);
    }

    cmdList.clear();
    feStateCmds.clear();
    commandList->close();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(regularCmdListStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    feStateCmds = findAll<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, feStateCmds.size());

    auto &cmdQueueStream = commandQueue->commandStream;
    auto cmdListHandle = commandList->toHandle();

    usedBefore = cmdQueueStream.getUsed();
    CommandListExecutionInternalOptions internalOptions = {};
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    usedAfter = cmdQueueStream.getUsed();

    if (fePropertiesSupport.disableEuFusion) {
        EXPECT_EQ(1, csrProperties.frontEndState.disableEUFusion.value);
    } else {
        EXPECT_EQ(-1, csrProperties.frontEndState.disableEUFusion.value);
    }

    cmdList.clear();
    feStateCmds.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), usedBefore),
        (usedAfter - usedBefore)));
    feStateCmds = findAll<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, feStateCmds.size());
}

HWTEST_F(CommandListCreate, givenImmediateCommandListWhenThereIsNoEnoughSpaceForImmediateCommandAndAllocationListNotEmptyThenReuseCommandBuffer) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    size_t useSize = commandList->getCmdContainer().getCommandStream()->getMaxAvailableSpace() - commonImmediateCommandSize + 1;
    EXPECT_EQ(1U, commandList->getCmdContainer().getCmdBufferAllocations().size());

    commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->updateTaskCount(0u, 0u);
    commandList->getCmdContainer().getCommandStream()->getSpace(useSize);
    reinterpret_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->checkAvailableSpace(0, false, commonImmediateCommandSize, false);
    EXPECT_EQ(1U, commandList->getCmdContainer().getCmdBufferAllocations().size());

    commandList->getCmdContainer().getCommandStream()->getSpace(useSize);
    auto latestFlushedTaskCount = whiteBoxCmdList->getCsr(false)->peekLatestFlushedTaskCount();
    reinterpret_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->checkAvailableSpace(0, false, commonImmediateCommandSize, false);
    EXPECT_EQ(1U, commandList->getCmdContainer().getCmdBufferAllocations().size());
    EXPECT_EQ(latestFlushedTaskCount + 1, whiteBoxCmdList->getCsr(false)->peekLatestFlushedTaskCount());
}

HWTEST_F(CommandListCreate, givenImmediateCommandListWhenRequiredSpaceExceedsDefaultCommandBufferThenCommandBufferFittingRequiredSpaceIsSet) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto commandStream = commandList->getCmdContainer().getCommandStream();

    constexpr uint32_t numEvents = 100;
    const size_t eventWaitSize = numEvents * NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();
    const size_t requiredCommandSize = commandStream->getMaxAvailableSpace() + 1;
    const auto defaultCmdBuffer = commandStream->getGraphicsAllocation();

    EXPECT_EQ(ZE_RESULT_SUCCESS, reinterpret_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->checkAvailableSpace(numEvents, false, requiredCommandSize, false));

    EXPECT_NE(defaultCmdBuffer, commandStream->getGraphicsAllocation());
    EXPECT_GE(commandStream->getAvailableSpace(), requiredCommandSize + eventWaitSize);
    EXPECT_NE(nullptr, commandStream->getSpace(requiredCommandSize + eventWaitSize));
}

HWTEST_F(CommandListCreate, givenImmediateCommandListWhenThereIsNoEnoughSpaceForWaitOnEventsAndImmediateCommandAndAllocationListNotEmptyThenReuseCommandBuffer) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    constexpr uint32_t numEvents = 100;
    size_t eventWaitSize = numEvents * NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    size_t useSize = commandList->getCmdContainer().getCommandStream()->getMaxAvailableSpace() - (commonImmediateCommandSize + eventWaitSize) + 1;

    EXPECT_EQ(1U, commandList->getCmdContainer().getCmdBufferAllocations().size());

    commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->updateTaskCount(0u, 0u);
    commandList->getCmdContainer().getCommandStream()->getSpace(useSize);
    reinterpret_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->checkAvailableSpace(numEvents, false, commonImmediateCommandSize, false);
    EXPECT_EQ(1U, commandList->getCmdContainer().getCmdBufferAllocations().size());

    commandList->getCmdContainer().getCommandStream()->getSpace(useSize);
    auto latestFlushedTaskCount = whiteBoxCmdList->getCsr(false)->peekLatestFlushedTaskCount();
    reinterpret_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->checkAvailableSpace(numEvents, false, commonImmediateCommandSize, false);
    EXPECT_EQ(1U, commandList->getCmdContainer().getCmdBufferAllocations().size());
    EXPECT_EQ(latestFlushedTaskCount + 1, whiteBoxCmdList->getCsr(false)->peekLatestFlushedTaskCount());
}

HWTEST_F(CommandListCreate, givenCommandListWhenRemoveDeallocationContainerDataThenHeapNotErased) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(device,
                                                                     NEO::EngineGroupType::compute,
                                                                     0u,
                                                                     returnValue, false));
    auto &cmdContainer = commandList->getCmdContainer();
    auto heapAlloc = cmdContainer.getIndirectHeapAllocation(HeapType::indirectObject);
    cmdContainer.getDeallocationContainer().push_back(heapAlloc);
    EXPECT_EQ(cmdContainer.getDeallocationContainer().size(), 1u);
    commandList->removeDeallocationContainerData();
    EXPECT_EQ(cmdContainer.getDeallocationContainer().size(), 1u);

    cmdContainer.getDeallocationContainer().clear();
}

TEST(CommandList, givenContextGroupEnabledWhenCreatingImmediateCommandListThenEachCmdListHasDifferentCsr) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0;
    desc.index = 0;
    ze_command_list_handle_t commandListHandle1, commandListHandle2;

    auto result = device->createCommandListImmediate(&desc, &commandListHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandListImmediate(&desc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList1 = L0::CommandList::fromHandle(commandListHandle1);
    auto commandList2 = L0::CommandList::fromHandle(commandListHandle2);

    EXPECT_NE(commandList1->getCsr(false), commandList2->getCsr(false));

    commandList1->destroy();
    commandList2->destroy();
}

struct CommandListCsrReuseTests : public ::testing::Test {
    void SetUp() override {
        HardwareInfo hwInfo = *defaultHwInfo;
        if (hwInfo.capabilityTable.defaultEngineType != aub_stream::ENGINE_CCS) {
            GTEST_SKIP();
        }
        debugManager.flags.ContextGroupSize.set(5);
        debugManager.flags.OverrideNumHighPriorityContexts.set(1);
        debugManager.flags.ExperimentalUseSecondaryContextForInternalEngine.set(0);
        hwInfo.featureTable.flags.ftrCCSNode = true;
        hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        ASSERT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));
        device = driverHandle->devices[0];
    }

    DebugManagerStateRestore dbgRestorer;
    NEO::MockDevice *neoDevice = nullptr;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    L0::Device *device = nullptr;
};

TEST_F(CommandListCsrReuseTests, givenSequentialImmediateCommandListsWhenDestroyedThenReuseCsrWithoutInitializingUnusedContexts) {
    auto &contexts = neoDevice->secondaryEngines[aub_stream::ENGINE_CCS];
    for (auto priority : {ZE_COMMAND_QUEUE_PRIORITY_NORMAL, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH}) {
        ze_command_queue_desc_t desc = {};
        desc.priority = priority;
        NEO::CommandStreamReceiver *firstCsr = nullptr;
        for (auto i = 0; i < 10; i++) {
            ze_command_list_handle_t handle = nullptr;
            ASSERT_EQ(ZE_RESULT_SUCCESS, device->createCommandListImmediate(&desc, &handle));
            auto commandList = L0::CommandList::fromHandle(handle);
            auto csr = commandList->getCsr(false);
            if (i == 0) {
                firstCsr = csr;
            }
            EXPECT_EQ(firstCsr, csr);
            EXPECT_EQ(1u, csr->getOwningQueueCount());
            EXPECT_TRUE(csr->isInitialized());
            commandList->destroy();
            EXPECT_EQ(0u, csr->getOwningQueueCount());
        }
    }
    EXPECT_EQ(1u, contexts.npIndices.size());
    EXPECT_EQ(1u, contexts.hpIndices.size());
    for (uint32_t i = 1; i < contexts.regularEnginesTotal; i++) {
        EXPECT_FALSE(contexts.engines[i].commandStreamReceiver->isInitialized());
    }
}

TEST_F(CommandListCsrReuseTests, givenAllRegularCsrsInUseWhenQueuesAreReleasedThenReuseOnlyAfterLastOwnerIsDestroyed) {
    ze_command_queue_desc_t desc = {};
    std::vector<L0::CommandQueue *> queues;
    auto &contexts = neoDevice->secondaryEngines[aub_stream::ENGINE_CCS];
    for (uint32_t i = 0; i <= contexts.regularEnginesTotal; i++) {
        ze_command_queue_handle_t handle = nullptr;
        ASSERT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&desc, &handle));
        queues.push_back(L0::CommandQueue::fromHandle(handle));
        if (i < contexts.regularEnginesTotal) {
            for (uint32_t j = 0; j < i; j++) {
                EXPECT_NE(queues[j]->getCsr(), queues[i]->getCsr());
            }
        }
    }
    auto sharedCsr = queues.back()->getCsr();
    uint32_t queuesSharingCsr = 0;
    for (auto queue : queues) {
        if (queue->getCsr() == sharedCsr) {
            queuesSharingCsr++;
        }
    }
    EXPECT_EQ(2u, queuesSharingCsr);
    EXPECT_EQ(2u, sharedCsr->getOwningQueueCount());
    queues.back()->destroy();
    queues.pop_back();
    EXPECT_EQ(1u, sharedCsr->getOwningQueueCount());

    auto releasedCsr = queues[2]->getCsr();
    queues[2]->destroy();
    queues[2] = nullptr;
    ze_command_list_handle_t handle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, device->createCommandListImmediate(&desc, &handle));
    auto commandList = L0::CommandList::fromHandle(handle);
    EXPECT_EQ(releasedCsr, commandList->getCsr(false));
    EXPECT_EQ(1u, releasedCsr->getOwningQueueCount());
    commandList->destroy();
    for (auto queue : queues) {
        if (queue) {
            queue->destroy();
        }
    }
    EXPECT_EQ(0u, sharedCsr->getOwningQueueCount());
}

TEST_F(CommandListCsrReuseTests, givenCommandQueueWhenCreatedAndDestroyedThenSecondaryContextOwnershipIsTakenAndReleased) {
    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t handle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&desc, &handle));

    auto commandQueue = L0::CommandQueue::fromHandle(handle);
    auto csr = commandQueue->getCsr();
    ASSERT_TRUE(csr->getOsContext().isPartOfContextGroup());
    EXPECT_EQ(1u, csr->getOwningQueueCount());

    commandQueue->destroy();
    EXPECT_EQ(0u, csr->getOwningQueueCount());
}

TEST_F(CommandListCsrReuseTests, givenInternalImmediateCommandListWhenDestroyedThenOwnershipIsNotReleased) {
    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    auto commandList = CommandList::createImmediate(device, &desc,
                                                    true, NEO::EngineGroupType::renderCompute, returnValue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    auto internalCsr = neoDevice->getInternalEngine().commandStreamReceiver;
    ASSERT_EQ(internalCsr, commandList->getCsr(false));
    EXPECT_EQ(0u, internalCsr->getOwningQueueCount());

    commandList->destroy();
    EXPECT_EQ(0u, internalCsr->getOwningQueueCount());
}

TEST_F(CommandListCsrReuseTests, givenDynamicQueueReassignmentEnabledWhenCreatingImmediateCommandListsThenCsrReassignmentIsAllowedOnlyInHeaplessModeForListsOwningQueue) {
    const bool heaplessModeEnabled = neoDevice->getCompilerProductHelper().isHeaplessModeEnabled(neoDevice->getHardwareInfo());
    auto createImmediateCommandList = [&](ze_command_queue_priority_t priority) {
        ze_command_queue_desc_t desc = {};
        desc.priority = priority;
        ze_command_list_handle_t handle = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, device->createCommandListImmediate(&desc, &handle));
        auto commandList = CommandList::whiteboxCast(L0::CommandList::fromHandle(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->ensureImmediateResourcesInitialized());
        return commandList;
    };

    auto commandListWithReassignmentDisabled = createImmediateCommandList(ZE_COMMAND_QUEUE_PRIORITY_NORMAL);
    EXPECT_FALSE(commandListWithReassignmentDisabled->csrReassignmentAllowed);
    commandListWithReassignmentDisabled->destroy();

    debugManager.flags.EnableDynamicQueueReassignment.set(1);
    auto commandListOwningQueue = createImmediateCommandList(ZE_COMMAND_QUEUE_PRIORITY_NORMAL);
    EXPECT_EQ(heaplessModeEnabled, commandListOwningQueue->csrReassignmentAllowed);

    auto lowPriorityCommandList = createImmediateCommandList(ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW);
    EXPECT_FALSE(lowPriorityCommandList->csrReassignmentAllowed);

    lowPriorityCommandList->destroy();
    commandListOwningQueue->destroy();
}

struct CommandListCsrReassignmentTests : public CommandListCsrReuseTests {
    void SetUp() override {
        debugManager.flags.EnableDynamicQueueReassignment.set(1);
        CommandListCsrReuseTests::SetUp();
        if (IsSkipped()) {
            return;
        }
        if (!neoDevice->getCompilerProductHelper().isHeaplessModeEnabled(neoDevice->getHardwareInfo())) {
            GTEST_SKIP();
        }
    }

    CommandList *createInitializedImmediateCommandList(const ze_command_queue_desc_t &desc) {
        ze_command_list_handle_t handle = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, device->createCommandListImmediate(&desc, &handle));
        auto commandList = CommandList::whiteboxCast(L0::CommandList::fromHandle(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->ensureImmediateResourcesInitialized());
        return commandList;
    }

    static WhiteBox<::L0::CommandQueue> *getImmediateQueue(CommandList *commandList) {
        return static_cast<WhiteBox<::L0::CommandQueue> *>(commandList->cmdQImmediate);
    }

    template <typename FamilyType>
    static void setCsrProgress(NEO::CommandStreamReceiver *csr, TaskCountType submittedTaskCount, TagAddressType completedTaskCount) {
        static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->taskCount = submittedTaskCount;
        static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->latestFlushedTaskCount = submittedTaskCount;
        *csr->getTagAddress() = completedTaskCount;
    }

    static void setInOrderCounterMemory(CommandList *commandList, uint64_t counterValue) {
        auto &inOrderExecInfo = commandList->inOrderExecInfo;
        *ptrOffset(inOrderExecInfo->getBaseHostAddress(), inOrderExecInfo->getAllocationOffset()) = counterValue;
    }

    static void markCsrIdle(NEO::CommandStreamReceiver *csr) {
        *csr->getTagAddress() = csr->peekTaskCount();
    }

    static ze_result_t appendBarrier(CommandList *commandList) {
        CmdListWaitEventParameters waitEventParameters{};
        waitEventParameters.apiRequest = true;
        CmdListSignalEventParameters signalEventParameters{};
        return commandList->appendBarrier(nullptr, 0, nullptr, waitEventParameters, signalEventParameters);
    }

    static constexpr TaskCountType loadedCsrSubmittedTaskCount = 4u;
};

HWTEST_F(CommandListCsrReassignmentTests, givenReassignmentNotEnabledWhenAppendingToSharedAndLoadedCsrThenCsrIsKept) {
    debugManager.flags.EnableDynamicQueueReassignment.set(-1);
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));
    EXPECT_EQ(2u, initialCsr->getOwningQueueCount());

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCsrWithMultipleActivePartitionsWhenAppendingToSharedAndLoadedCsrThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto initialCsr = commandList->getCsr(false);
    const auto initialActivePartitions = initialCsr->getActivePartitions();

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    initialCsr->setActivePartitions(2u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));
    EXPECT_EQ(2u, initialCsr->getOwningQueueCount());

    initialCsr->setActivePartitions(initialActivePartitions);
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCsrReassignmentNotAllowedForCommandListWhenTryingReassignmentOnSharedAndLoadedCsrThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);
    ASSERT_TRUE(immediateCommandList->csrReassignmentAllowed);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    immediateCommandList->csrReassignmentAllowed = false;

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    immediateCommandList->csrReassignmentAllowed = true;
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCsrReassignmentSuspendedWhenTryingReassignmentOnSharedAndLoadedCsrThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    immediateCommandList->csrReassignmentSuspended = true;

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    immediateCommandList->csrReassignmentSuspended = false;
    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_NE(initialCsr, commandList->getCsr(false));

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenSynchronizationTaskCountsStoredWhenCsrIsReassignedThenHostSynchronizeAndBarrierCachesAreInvalidated) {
    using SynchronizationTaskCounts = typename WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>::SynchronizationTaskCounts;
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);
    const auto queueTaskCount = commandList->cmdQImmediate->getTaskCount();

    immediateCommandList->lastHostSynchronizeTaskCounts.store(queueTaskCount, 0u);
    immediateCommandList->lastBarrierTaskCounts.store(queueTaskCount, 0u);
    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));

    constexpr auto invalidTaskCount = SynchronizationTaskCounts::invalidTaskCount;
    EXPECT_TRUE(immediateCommandList->lastHostSynchronizeTaskCounts.matches(invalidTaskCount, invalidTaskCount));
    EXPECT_TRUE(immediateCommandList->lastBarrierTaskCounts.matches(invalidTaskCount, invalidTaskCount));

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenStagingDependencyTaskCountsStoredWhenCsrIsReassignedThenStagingDependencyTaskCountsAreCleared) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);

    immediateCommandList->stagingDependencyTaskCounts.store(loadedCsrSubmittedTaskCount, loadedCsrSubmittedTaskCount);
    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));

    EXPECT_TRUE(immediateCommandList->stagingDependencyTaskCounts.matches(0u, 0u));

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedBeforeDependencyIsDispatchedWhenHostSynchronizingWithTaskCountWaitThenPreviousCsrIsWaited) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto queue = getImmediateQueue(commandList);
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);
    auto initialUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount + 1, 0u);
    queue->taskCount = loadedCsrSubmittedTaskCount;
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());
    initialUltCsr->callBaseWaitForCompletionWithTimeout = false;

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));
    ASSERT_TRUE(immediateCommandList->implicitDependencyOnPreviousCsrPending);
    immediateCommandList->latestFlushIsHostVisible = false;

    immediateCommandList->releasePreviousCsrCompletionTarget();
    EXPECT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);

    EXPECT_TRUE(immediateCommandList->handleInOrderImplicitDependencies(false, false));
    ASSERT_FALSE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

    const auto previousCsrWaitsBefore = initialUltCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(previousCsrWaitsBefore + 1, initialUltCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(loadedCsrSubmittedTaskCount, initialUltCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(nullptr, immediateCommandList->csrBeforeReassignment);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(previousCsrWaitsBefore + 1, initialUltCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    initialUltCsr->callBaseWaitForCompletionWithTimeout = true;
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedWithHangOnPreviousCsrWhenHostSynchronizingWithTaskCountWaitThenDeviceLostIsReturnedWithoutWaitingOnNewCsr) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto queue = getImmediateQueue(commandList);
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);
    auto initialUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount + 1, 0u);
    queue->taskCount = loadedCsrSubmittedTaskCount;
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());
    initialUltCsr->callBaseWaitForCompletionWithTimeout = false;
    initialUltCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    auto reassignedUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    ASSERT_NE(initialCsr, reassignedUltCsr);
    immediateCommandList->latestFlushIsHostVisible = false;

    const auto newCsrWaitsBefore = reassignedUltCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(newCsrWaitsBefore, reassignedUltCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    initialUltCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    initialUltCsr->callBaseWaitForCompletionWithTimeout = true;
    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    immediateCommandList->releasePreviousCsrCompletionTarget();
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListWithNonWalkerInOrderCmdsChainingWhenCsrIsReassignedThenChainingIsClearedAndImplicitDependencyIsResolvedWithSemaphore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());
    immediateCommandList->latestOperationRequiredNonWalkerInOrderCmdsChaining = true;

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));
    EXPECT_FALSE(immediateCommandList->latestOperationRequiredNonWalkerInOrderCmdsChaining);

    auto cmdStream = commandList->commandContainer.getCommandStream();
    const auto offset = cmdStream->getUsed();
    CmdListWaitEventParameters waitEventParameters{};
    waitEventParameters.waitForImplicitInOrderDependency = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immediateCommandList->addEventsToCmdList(0, nullptr, waitEventParameters));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
    EXPECT_EQ(1u, findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end()).size());
    EXPECT_FALSE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderSignalingConfigurationsFlaggedForReassignmentWhenCsrIsReassignedThenSwitchIsStillPerformedWithPendingDependency) {
    struct SignalingConfiguration {
        int32_t duplicatedCounterStorage;
        int32_t compactL3FlushEventPacket;
        bool dcFlushSupport;
    };
    for (const auto &configuration : {SignalingConfiguration{0, -1, false}, SignalingConfiguration{-1, 1, true}, SignalingConfiguration{-1, 1, false}}) {
        debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(configuration.duplicatedCounterStorage);
        debugManager.flags.CompactL3FlushEventPacket.set(configuration.compactL3FlushEventPacket);

        ze_command_queue_desc_t desc = {};
        desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
        auto commandList = createInitializedImmediateCommandList(desc);
        auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
        ASSERT_TRUE(!immediateCommandList->duplicatedInOrderCounterStorageEnabled || immediateCommandList->compactL3FlushEventPacket);
        immediateCommandList->dcFlushSupport = configuration.dcFlushSupport;
        auto initialCsr = commandList->getCsr(false);

        initialCsr->retainQueueOwnership();
        setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);

        immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
        EXPECT_NE(initialCsr, commandList->getCsr(false));
        EXPECT_TRUE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

        initialCsr->releaseQueueOwnership();
        markCsrIdle(initialCsr);
        commandList->destroy();
    }
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListWithPreviousCsrCompletionPendingWhenTryingReassignmentAgainThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    auto reassignedCsr = commandList->getCsr(false);
    ASSERT_NE(initialCsr, reassignedCsr);
    ASSERT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);

    reassignedCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(reassignedCsr, reassignedCsr->peekTaskCount() + loadedCsrSubmittedTaskCount, static_cast<TagAddressType>(reassignedCsr->peekTaskCount()));
    ASSERT_TRUE(getImmediateQueue(commandList)->isCsrOversubscribed());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_EQ(reassignedCsr, commandList->getCsr(false));
    EXPECT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);

    reassignedCsr->releaseQueueOwnership();
    markCsrIdle(reassignedCsr);
    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    immediateCommandList->releasePreviousCsrCompletionTarget();
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenFiniteTimeoutWhenHostSynchronizingAfterReassignmentThenTimeSpentOnPreviousCsrIsSubtractedFromRemainingWait) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto queue = getImmediateQueue(commandList);
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);
    auto initialUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount + 1, 0u);
    queue->taskCount = loadedCsrSubmittedTaskCount;
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());
    initialUltCsr->callBaseWaitForCompletionWithTimeout = false;

    constexpr uint64_t timeoutNs = 1000u;
    initialUltCsr->onWaitForCompletionWithTimeout = []() {
        std::this_thread::sleep_for(std::chrono::microseconds(2));
    };

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    auto reassignedUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    ASSERT_NE(initialUltCsr, reassignedUltCsr);
    immediateCommandList->latestFlushIsHostVisible = false;
    reassignedUltCsr->callBaseWaitForCompletionWithTimeout = false;

    const auto newCsrWaitsBefore = reassignedUltCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    commandList->hostSynchronize(timeoutNs);
    EXPECT_EQ(timeoutNs / 1000u, static_cast<uint64_t>(initialUltCsr->latestWaitForCompletionWithTimeoutWaitParams.waitTimeout));
    ASSERT_EQ(newCsrWaitsBefore + 1, reassignedUltCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_TRUE(reassignedUltCsr->latestWaitForCompletionWithTimeoutWaitParams.enableTimeout);
    EXPECT_EQ(0, reassignedUltCsr->latestWaitForCompletionWithTimeoutWaitParams.waitTimeout);

    initialUltCsr->onWaitForCompletionWithTimeout = nullptr;
    initialUltCsr->callBaseWaitForCompletionWithTimeout = true;
    reassignedUltCsr->callBaseWaitForCompletionWithTimeout = true;
    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    immediateCommandList->releasePreviousCsrCompletionTarget();
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedWhenAppendingCommandListsFailsToFlushThenPreviousCsrCompletionTargetIsKept) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    auto reassignedUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    ASSERT_NE(initialCsr, reassignedUltCsr);
    ASSERT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto regularCommandList = L0::CommandList::create(device, NEO::EngineGroupType::compute, 0u, returnValue, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    regularCommandList->close();
    ze_command_list_handle_t regularCommandListHandle = regularCommandList->toHandle();

    reassignedUltCsr->flushReturnValue = NEO::SubmissionStatus::failed;
    CommandListExecutionInternalOptions internalOptions = {};
    EXPECT_NE(ZE_RESULT_SUCCESS, commandList->appendCommandLists(1, &regularCommandListHandle, nullptr, 0, nullptr, internalOptions));
    EXPECT_FALSE(immediateCommandList->implicitDependencyOnPreviousCsrPending);
    EXPECT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);

    reassignedUltCsr->flushReturnValue.reset();
    regularCommandList->destroy();
    immediateCommandList->releasePreviousCsrCompletionTarget();
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCommandListReassignedWhenSwitchingCsrThenAllocationsOfCompletedWorkAreDownloadedFromPreviousCsrOnlyForNonInOrderCommandList) {
    for (const bool inOrder : {false, true}) {
        ze_command_queue_desc_t desc = {};
        desc.flags = inOrder ? static_cast<ze_command_queue_flags_t>(ZE_COMMAND_QUEUE_FLAG_IN_ORDER) : 0u;
        auto commandList = createInitializedImmediateCommandList(desc);
        auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
        auto queue = getImmediateQueue(commandList);
        auto initialCsr = commandList->getCsr(false);
        auto initialUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

        initialCsr->retainQueueOwnership();
        setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
        constexpr TaskCountType previousQueueTaskCount = 0u;
        queue->taskCount = previousQueueTaskCount;
        const auto downloadsBefore = initialUltCsr->downloadAllocationsCalledCount.load();

        immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
        ASSERT_NE(initialCsr, commandList->getCsr(false));

        if (inOrder) {
            EXPECT_EQ(downloadsBefore, initialUltCsr->downloadAllocationsCalledCount.load());
            immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
            immediateCommandList->releasePreviousCsrCompletionTarget();
        } else {
            EXPECT_EQ(downloadsBefore + 1, initialUltCsr->downloadAllocationsCalledCount.load());
            EXPECT_TRUE(initialUltCsr->latestDownloadAllocationsBlocking);
            EXPECT_EQ(previousQueueTaskCount, initialUltCsr->latestDownloadAllocationsTaskCount);
        }

        initialCsr->releaseQueueOwnership();
        markCsrIdle(initialCsr);
        commandList->destroy();
    }
}

HWTEST_F(CommandListCsrReassignmentTests, givenBarrierWithCopyOffloadSynchronizationWhenCsrBecomesSharedAndLoadedAfterFirstFlushThenCsrIsNotReassignedWithinOperation) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);
    auto initialUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    ze_command_queue_handle_t copyOffloadQueueHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&desc, &copyOffloadQueueHandle));
    auto copyOffloadQueue = static_cast<WhiteBox<::L0::CommandQueue> *>(L0::CommandQueue::fromHandle(copyOffloadQueueHandle));
    commandList->cmdQImmediateCopyOffload = copyOffloadQueue;
    immediateCommandList->copyOffloadMode = CopyOffloadModes::dualStream;

    initialCsr->retainQueueOwnership();
    initialUltCsr->isTagReachedReturnValue = false;
    initialUltCsr->onFlushImmediateTask = [&]() {
        initialUltCsr->isTagReachedReturnValue = true;
        setCsrProgress<FamilyType>(initialCsr, initialCsr->peekTaskCount() + loadedCsrSubmittedTaskCount, 0u);
    };

    CmdListWaitEventParameters waitEventParameters{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, immediateCommandList->appendBarrierWithCopyOffloadSynchronization(nullptr, 0, nullptr, waitEventParameters, false));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));
    EXPECT_FALSE(immediateCommandList->csrReassignmentSuspended);

    initialUltCsr->onFlushImmediateTask = nullptr;
    initialUltCsr->isTagReachedReturnValue.reset();
    immediateCommandList->copyOffloadMode = CopyOffloadModes::disabled;
    commandList->cmdQImmediateCopyOffload = nullptr;
    copyOffloadQueue->destroy();
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCsrReassignedWithoutSubmissionWhenHostSynchronizingThenWorkOfOtherQueuesOnNewCsrIsNotWaitedFor) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    auto reassignedUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
    ASSERT_NE(initialCsr, reassignedUltCsr);
    EXPECT_EQ(0u, commandList->cmdQImmediate->getTaskCount());

    reassignedUltCsr->callBaseWaitForCompletionWithTimeout = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(0u, reassignedUltCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    reassignedUltCsr->callBaseWaitForCompletionWithTimeout = true;
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedWhenHostSynchronizingWithCounterWaitThenPreviousCsrCompletionTargetIsReleasedOnlyOnSuccess) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));
    ASSERT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);
    immediateCommandList->latestFlushIsHostVisible = true;

    EXPECT_EQ(ZE_RESULT_NOT_READY, commandList->hostSynchronize(0u));
    EXPECT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);

    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(nullptr, immediateCommandList->csrBeforeReassignment);

    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenResolveDependenciesViaPipeControlsWhenImplicitDependencyOnPreviousCsrIsPendingThenDebugKeyIsNotAppliedAndSemaphoreIsKept) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    ASSERT_FALSE(immediateCommandList->isInOrderCounterSignalPending());
    ASSERT_FALSE(immediateCommandList->latestOperationHasCbEventWithProfiling);

    immediateCommandList->implicitDependencyOnPreviousCsrPending = true;
    EXPECT_FALSE(immediateCommandList->isResolveIoqDependencyWithBarrier(true, false, false));

    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    EXPECT_TRUE(immediateCommandList->isResolveIoqDependencyWithBarrier(true, false, false));

    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenSharedAndLoadedCsrWithoutWorkSubmittedByOtherQueuesAfterOwnWorkWhenTryingReassignmentThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto queue = getImmediateQueue(commandList);
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    queue->taskCount = loadedCsrSubmittedTaskCount;
    EXPECT_FALSE(queue->isCsrOversubscribed());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    queue->taskCount = loadedCsrSubmittedTaskCount - 1;
    EXPECT_TRUE(queue->isCsrOversubscribed());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_NE(initialCsr, commandList->getCsr(false));

    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    immediateCommandList->releasePreviousCsrCompletionTarget();
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCsrWithMultipleActivePartitionsWhenTryingReassignmentOnSharedAndLoadedCsrThenCsrIsKeptUntilSinglePartitionIsActive) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);
    const auto initialActivePartitions = initialCsr->getActivePartitions();

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    initialCsr->setActivePartitions(2u);

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    initialCsr->setActivePartitions(1u);
    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_NE(initialCsr, commandList->getCsr(false));

    initialCsr->setActivePartitions(initialActivePartitions);
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedWhenFlushingToCopyOffloadQueueThenPreviousCsrCompletionTargetIsKept) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);

    ze_command_queue_desc_t copyOffloadQueueDesc = {};
    ze_command_queue_handle_t copyOffloadQueueHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&copyOffloadQueueDesc, &copyOffloadQueueHandle));
    auto copyOffloadQueue = L0::CommandQueue::fromHandle(copyOffloadQueueHandle);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));
    ASSERT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);
    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;

    commandList->cmdQImmediateCopyOffload = copyOffloadQueue;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immediateCommandList->executeCommandListImmediateWithFlushTaskImpl(false, false, false, NEO::AppendOperations::nonKernel, false, copyOffloadQueue, nullptr, nullptr));
    EXPECT_EQ(initialCsr, immediateCommandList->csrBeforeReassignment);

    EXPECT_EQ(ZE_RESULT_SUCCESS, immediateCommandList->executeCommandListImmediateWithFlushTaskImpl(false, false, false, NEO::AppendOperations::nonKernel, false, commandList->cmdQImmediate, nullptr, nullptr));
    EXPECT_EQ(nullptr, immediateCommandList->csrBeforeReassignment);

    commandList->cmdQImmediateCopyOffload = nullptr;
    copyOffloadQueue->destroy();
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

struct CsrReassignmentMetricQuery : public L0::MetricQuery {
    ze_result_t appendBegin(L0::CommandList &commandList) override { return appendBeginResult; }
    ze_result_t appendEnd(L0::CommandList &commandList, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override { return appendEndResult; }
    ze_result_t getData(size_t *pRawDataSize, uint8_t *pRawData) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t reset() override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t destroy() override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }

    ze_result_t appendBeginResult = ZE_RESULT_SUCCESS;
    ze_result_t appendEndResult = ZE_RESULT_SUCCESS;
};

HWTEST_F(CommandListCsrReassignmentTests, givenMetricQueryOpenWhenTryingReassignmentOnSharedAndLoadedCsrThenCsrIsKeptUntilQueryIsEnded) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);
    CsrReassignmentMetricQuery metricQuery;
    auto loadInitialCsrWithOtherQueueWork = [initialCsr]() {
        setCsrProgress<FamilyType>(initialCsr, initialCsr->peekTaskCount() + loadedCsrSubmittedTaskCount, static_cast<TagAddressType>(initialCsr->peekTaskCount()));
    };

    initialCsr->retainQueueOwnership();

    metricQuery.appendBeginResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, commandList->appendMetricQueryBegin(metricQuery.toHandle()));
    EXPECT_EQ(0u, immediateCommandList->openMetricQueryCount);

    metricQuery.appendBeginResult = ZE_RESULT_SUCCESS;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMetricQueryBegin(metricQuery.toHandle()));
    EXPECT_EQ(1u, immediateCommandList->openMetricQueryCount);
    ASSERT_EQ(initialCsr, commandList->getCsr(false));

    loadInitialCsrWithOtherQueueWork();

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    metricQuery.appendEndResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, commandList->appendMetricQueryEnd(metricQuery.toHandle(), nullptr, 0, nullptr));
    EXPECT_EQ(1u, immediateCommandList->openMetricQueryCount);
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    loadInitialCsrWithOtherQueueWork();
    metricQuery.appendEndResult = ZE_RESULT_SUCCESS;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMetricQueryEnd(metricQuery.toHandle(), nullptr, 0, nullptr));
    EXPECT_EQ(0u, immediateCommandList->openMetricQueryCount);
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    loadInitialCsrWithOtherQueueWork();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMetricQueryEnd(metricQuery.toHandle(), nullptr, 0, nullptr));
    EXPECT_EQ(0u, immediateCommandList->openMetricQueryCount);
    EXPECT_NE(initialCsr, commandList->getCsr(false));

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedTwiceWhenHangOccursOnContextSwitchedAwayFromThenCounterWaitReturnsDeviceLostUntilSuccessfulSynchronization) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);
    auto initialUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    auto firstReassignedCsr = commandList->getCsr(false);
    ASSERT_NE(initialCsr, firstReassignedCsr);
    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    immediateCommandList->releasePreviousCsrCompletionTarget();
    ASSERT_EQ(nullptr, immediateCommandList->csrBeforeReassignment);

    firstReassignedCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(firstReassignedCsr, firstReassignedCsr->peekTaskCount() + loadedCsrSubmittedTaskCount, static_cast<TagAddressType>(firstReassignedCsr->peekTaskCount()));
    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    auto secondReassignedCsr = commandList->getCsr(false);
    ASSERT_NE(firstReassignedCsr, secondReassignedCsr);
    ASSERT_NE(initialCsr, secondReassignedCsr);
    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    immediateCommandList->releasePreviousCsrCompletionTarget();

    ASSERT_EQ(2u, immediateCommandList->previousCsrsAwaitingCompletion.size());
    EXPECT_EQ(initialCsr, immediateCommandList->previousCsrsAwaitingCompletion[0].first);
    EXPECT_EQ(firstReassignedCsr, immediateCommandList->previousCsrsAwaitingCompletion[1].first);

    immediateCommandList->latestFlushIsHostVisible = true;
    initialUltCsr->isGpuHangDetectedReturnValue = true;
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(2u, immediateCommandList->previousCsrsAwaitingCompletion.size());

    initialUltCsr->isGpuHangDetectedReturnValue = false;
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_TRUE(immediateCommandList->previousCsrsAwaitingCompletion.empty());

    initialUltCsr->isGpuHangDetectedReturnValue.reset();
    firstReassignedCsr->releaseQueueOwnership();
    markCsrIdle(firstReassignedCsr);
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenContextAlreadyAwaitingCompletionWhenInOrderCommandListSwitchesAwayFromItAgainThenContextIsTrackedOnce) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto queue = getImmediateQueue(commandList);
    auto initialCsr = commandList->getCsr(false);

    constexpr TaskCountType staleTaskCount = 1u;
    constexpr TaskCountType taskCountAtSwitch = 3u;
    immediateCommandList->previousCsrsAwaitingCompletion.push_back({initialCsr, staleTaskCount});
    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    queue->taskCount = taskCountAtSwitch;

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));
    ASSERT_EQ(1u, immediateCommandList->previousCsrsAwaitingCompletion.size());
    EXPECT_EQ(initialCsr, immediateCommandList->previousCsrsAwaitingCompletion[0].first);
    EXPECT_EQ(taskCountAtSwitch, immediateCommandList->previousCsrsAwaitingCompletion[0].second);

    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    immediateCommandList->releasePreviousCsrCompletionTarget();
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedInTbxModeWhenHostSynchronizeSucceedsThenCompletedAllocationsAreDownloadedFromPreviousContext) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto queue = getImmediateQueue(commandList);
    auto initialCsr = commandList->getCsr(false);
    auto initialUltCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    constexpr TaskCountType taskCountAtSwitch = 2u;
    queue->taskCount = taskCountAtSwitch;
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());

    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));
    immediateCommandList->implicitDependencyOnPreviousCsrPending = false;
    immediateCommandList->releasePreviousCsrCompletionTarget();
    ASSERT_EQ(nullptr, immediateCommandList->csrBeforeReassignment);
    ASSERT_EQ(1u, immediateCommandList->previousCsrsAwaitingCompletion.size());

    immediateCommandList->isTbxMode = true;
    immediateCommandList->latestFlushIsHostVisible = true;
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    const auto downloadsBefore = initialUltCsr->downloadAllocationsCalledCount.load();

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(downloadsBefore + 1, initialUltCsr->downloadAllocationsCalledCount.load());
    EXPECT_TRUE(initialUltCsr->latestDownloadAllocationsBlocking);
    EXPECT_EQ(taskCountAtSwitch, initialUltCsr->latestDownloadAllocationsTaskCount);
    EXPECT_TRUE(immediateCommandList->previousCsrsAwaitingCompletion.empty());

    immediateCommandList->isTbxMode = false;
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListWithPreviousContextsWhenFlushingWithSignalEventThenPreviousContextsArePassedToEventAndClearedOnFlushWithoutPreviousContexts) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto previousContextCsr = commandList->getCsr(false);
    constexpr TaskCountType previousContextTaskCount = 5u;

    for (const bool tbxMode : {false, true}) {
        MockEvent signalEvent;
        immediateCommandList->isTbxMode = tbxMode;
        immediateCommandList->previousCsrsAwaitingCompletion.push_back({previousContextCsr, previousContextTaskCount});

        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immediateCommandList->flushImmediate(ZE_RESULT_ERROR_INVALID_ARGUMENT, false, false, false, NEO::AppendOperations::nonKernel, false,
                                                                                         signalEvent.toHandle(), false, nullptr, nullptr));

        ASSERT_EQ(1u, signalEvent.signalerPreviousContexts.size());
        EXPECT_EQ(previousContextCsr, signalEvent.signalerPreviousContexts[0].first);
        EXPECT_EQ(previousContextTaskCount, signalEvent.signalerPreviousContexts[0].second);

        immediateCommandList->previousCsrsAwaitingCompletion.clear();
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immediateCommandList->flushImmediate(ZE_RESULT_ERROR_INVALID_ARGUMENT, false, false, false, NEO::AppendOperations::nonKernel, false,
                                                                                         signalEvent.toHandle(), false, nullptr, nullptr));

        EXPECT_TRUE(signalEvent.signalerPreviousContexts.empty());
    }

    immediateCommandList->isTbxMode = false;
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenOutOfOrderCommandListReassignedWhenSwitchingCsrThenPreviousContextIsNotTrackedForHangDetection) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    immediateCommandList->tryReassignCsrAtSafeTransitionPoint();
    ASSERT_NE(initialCsr, commandList->getCsr(false));
    EXPECT_TRUE(immediateCommandList->previousCsrsAwaitingCompletion.empty());

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCsrOwnedByOneQueueWhenAppendingWithLoadedCsrThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto initialCsr = commandList->getCsr(false);
    ASSERT_EQ(1u, initialCsr->getOwningQueueCount());

    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenSharedCsrWithLoadNotAboveThresholdWhenAppendingThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    const auto completedTaskCount = static_cast<TagAddressType>(initialCsr->peekTaskCount());
    setCsrProgress<FamilyType>(initialCsr, completedTaskCount + L0::CommandQueue::csrReassignmentLoadThreshold, completedTaskCount);
    ASSERT_EQ(L0::CommandQueue::csrReassignmentLoadThreshold, initialCsr->getOutstandingWorkCount());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenSharedCsrWithSinglePendingSubmissionWhenAppendingThenCsrIsReassigned) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, L0::CommandQueue::csrReassignmentLoadThreshold + 1, 0u);
    ASSERT_EQ(1u, initialCsr->getOutstandingWorkCount());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_NE(initialCsr, commandList->getCsr(false));

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenSharedAndLoadedCsrWithIncompleteQueueWorkWhenAppendingThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto initialCsr = commandList->getCsr(false);
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    ultCsr->isTagReachedReturnValue = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    ultCsr->isTagReachedReturnValue.reset();
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenOutOfOrderCommandListWithSharedAndLoadedCsrWhenAppendingThenCsrIsReassignedAndOwnershipIsMoved) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    ASSERT_FALSE(commandList->isInOrderExecutionEnabled());
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));

    auto reassignedCsr = commandList->getCsr(false);
    EXPECT_NE(initialCsr, reassignedCsr);
    EXPECT_TRUE(reassignedCsr->isInitialized());
    EXPECT_EQ(reassignedCsr, commandList->commandContainer.getImmediateCmdListCsr());
    EXPECT_EQ(1u, initialCsr->getOwningQueueCount());
    EXPECT_EQ(1u, reassignedCsr->getOwningQueueCount());
    EXPECT_EQ(reassignedCsr->peekTaskCount(), getImmediateQueue(commandList)->getTaskCount());
    EXPECT_EQ(loadedCsrSubmittedTaskCount, initialCsr->peekTaskCount());

    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
    EXPECT_EQ(0u, reassignedCsr->getOwningQueueCount());
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListWithPendingWorkWhenAppendingToSharedAndLoadedCsrThenCsrIsReassignedAndDependencyOnPreviousCsrIsResolvedWithSemaphore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    ASSERT_TRUE(commandList->isInOrderExecutionEnabled());
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue() - 1);
    ASSERT_FALSE(inOrderExecInfo->isCounterSignalPending());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    auto reassignedCsr = commandList->getCsr(false);
    EXPECT_NE(initialCsr, reassignedCsr);
    EXPECT_EQ(reassignedCsr, commandList->commandContainer.getImmediateCmdListCsr());
    EXPECT_EQ(1u, initialCsr->getOwningQueueCount());
    ASSERT_EQ(commandList->cmdQImmediate->getTaskCount(), reassignedCsr->peekTaskCount());
    EXPECT_TRUE(immediateCommandList->implicitDependencyOnPreviousCsrPending);
    EXPECT_FALSE(immediateCommandList->isResolveIoqDependencyWithBarrier(true, false, false));

    auto cmdStream = commandList->commandContainer.getCommandStream();
    const auto offset = cmdStream->getUsed();
    EXPECT_TRUE(immediateCommandList->handleInOrderImplicitDependencies(false, false));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
    EXPECT_EQ(1u, findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end()).size());
    EXPECT_EQ(cmdList.end(), find<typename FamilyType::StallingBarrierType *>(cmdList.begin(), cmdList.end()));
    EXPECT_FALSE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedWithAlreadyWaitedCounterWhenHandlingImplicitDependencyThenNoWaitIsProgrammedAndPendingDependencyIsCleared) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    inOrderExecInfo->setProgrammedCounterValue(inOrderExecInfo->getCounterValue());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    ASSERT_NE(initialCsr, commandList->getCsr(false));
    ASSERT_TRUE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

    inOrderExecInfo->setLastWaitedCounterValue(inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset());
    auto cmdStream = commandList->commandContainer.getCommandStream();
    const auto usedBefore = cmdStream->getUsed();
    EXPECT_FALSE(immediateCommandList->handleInOrderImplicitDependencies(false, false));
    EXPECT_EQ(usedBefore, cmdStream->getUsed());
    EXPECT_FALSE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListReassignedWithoutPreviousWorkWhenHandlingImplicitDependencyThenPendingDependencyIsCleared) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    ultCsr->isTagReachedReturnValue = false;
    ASSERT_EQ(0u, commandList->inOrderExecInfo->getCounterValue());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_NE(initialCsr, commandList->getCsr(false));
    ASSERT_TRUE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

    EXPECT_FALSE(immediateCommandList->handleInOrderImplicitDependencies(false, false));
    EXPECT_FALSE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

    ultCsr->isTagReachedReturnValue.reset();
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListWithPendingCounterSignalWhenAppendingToSharedAndLoadedCsrThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto &inOrderExecInfo = commandList->inOrderExecInfo;
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    inOrderExecInfo->addCounterValue(1u);
    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    ASSERT_TRUE(inOrderExecInfo->isCounterSignalPending());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    setInOrderCounterMemory(commandList, inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListWithLatestOperationUsingProfilingCounterBasedEventWhenAppendingToSharedAndLoadedCsrThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto immediateCommandList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(static_cast<L0::CommandList *>(commandList));
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    immediateCommandList->latestOperationHasCbEventWithProfiling = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));
    EXPECT_FALSE(immediateCommandList->implicitDependencyOnPreviousCsrPending);

    immediateCommandList->latestOperationHasCbEventWithProfiling = false;
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenInOrderCommandListWithIncompleteTaskCountWhenAppendingToSharedAndLoadedCsrThenCsrIsReassigned) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createInitializedImmediateCommandList(desc);
    auto initialCsr = commandList->getCsr(false);
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(initialCsr);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    ultCsr->isTagReachedReturnValue = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_NE(initialCsr, commandList->getCsr(false));

    ultCsr->isTagReachedReturnValue.reset();
    setInOrderCounterMemory(commandList, commandList->inOrderExecInfo->getCounterValue());
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCopyOffloadQueueWithIncompleteWorkWhenAppendingToSharedAndLoadedCsrThenCsrIsReassignedOnlyAfterCopyOffloadCompletes) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto initialCsr = commandList->getCsr(false);

    ze_command_queue_handle_t copyOffloadQueueHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&desc, &copyOffloadQueueHandle));
    auto copyOffloadQueue = static_cast<WhiteBox<::L0::CommandQueue> *>(L0::CommandQueue::fromHandle(copyOffloadQueueHandle));
    copyOffloadQueue->taskCount = std::numeric_limits<TagAddressType>::max();
    commandList->cmdQImmediateCopyOffload = copyOffloadQueue;

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));

    copyOffloadQueue->taskCount = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_NE(initialCsr, commandList->getCsr(false));

    commandList->cmdQImmediateCopyOffload = nullptr;
    copyOffloadQueue->destroy();
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCsrSelectionFailingWhenAppendingToSharedAndLoadedCsrThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto queue = getImmediateQueue(commandList);
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    const auto validOrdinal = queue->desc.ordinal;
    queue->desc.ordinal = std::numeric_limits<uint32_t>::max();

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));
    EXPECT_EQ(2u, initialCsr->getOwningQueueCount());

    queue->desc.ordinal = validOrdinal;
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenCsrSelectionNotTakingOwnershipWhenAppendingToSharedAndLoadedCsrThenCsrIsKept) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto queue = getImmediateQueue(commandList);
    auto initialCsr = commandList->getCsr(false);

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, loadedCsrSubmittedTaskCount, 0u);
    const auto requestedPriority = queue->desc.priority;
    queue->desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));
    EXPECT_EQ(2u, initialCsr->getOwningQueueCount());

    queue->desc.priority = requestedPriority;
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

HWTEST_F(CommandListCsrReassignmentTests, givenAllCsrsOwnedAndCurrentCsrLeastLoadedWhenAppendingThenCsrAndOwnershipAreKept) {
    ze_command_queue_desc_t desc = {};
    auto &contexts = neoDevice->secondaryEngines[aub_stream::ENGINE_CCS];
    std::vector<CommandList *> commandLists;
    for (uint32_t i = 0; i < contexts.regularEnginesTotal; i++) {
        commandLists.push_back(createInitializedImmediateCommandList(desc));
    }
    ASSERT_EQ(contexts.regularEnginesTotal, contexts.npIndices.size());

    constexpr TaskCountType highlyLoadedSubmittedTaskCount = 10u;
    for (auto index : contexts.npIndices) {
        setCsrProgress<FamilyType>(contexts.engines[index].commandStreamReceiver, highlyLoadedSubmittedTaskCount, 0u);
    }
    auto commandList = commandLists.front();
    auto initialCsr = commandList->getCsr(false);
    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, L0::CommandQueue::csrReassignmentLoadThreshold + 1, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList));
    EXPECT_EQ(initialCsr, commandList->getCsr(false));
    EXPECT_EQ(2u, initialCsr->getOwningQueueCount());

    initialCsr->releaseQueueOwnership();
    for (auto index : contexts.npIndices) {
        markCsrIdle(contexts.engines[index].commandStreamReceiver);
    }
    for (auto list : commandLists) {
        list->destroy();
    }
}

HWTEST_F(CommandListCsrReassignmentTests, givenRegularCommandListAppendedToReassignedImmediateCommandListThenSavedTagAndTaskCountReferToNewCsr) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createInitializedImmediateCommandList(desc);
    auto queue = getImmediateQueue(commandList);
    auto initialCsr = commandList->getCsr(false);
    ASSERT_TRUE(queue->getSaveWaitForPreamble());

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto regularCommandList = L0::CommandList::create(device, NEO::EngineGroupType::compute, 0u, returnValue, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    regularCommandList->close();
    ze_command_list_handle_t regularCommandListHandle = regularCommandList->toHandle();
    commandList->setPatchingPreamble(true);

    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendCommandLists(1, &regularCommandListHandle, nullptr, 0, nullptr, internalOptions));
    EXPECT_EQ(initialCsr->getTagAllocation()->getGpuAddress(), regularCommandList->getLatestTagGpuAddress());
    EXPECT_EQ(initialCsr->peekTaskCount(), regularCommandList->getLatestTaskCount());

    initialCsr->retainQueueOwnership();
    setCsrProgress<FamilyType>(initialCsr, initialCsr->peekTaskCount() + loadedCsrSubmittedTaskCount, 0u);
    queue->taskCount = 0u;

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendCommandLists(1, &regularCommandListHandle, nullptr, 0, nullptr, internalOptions));
    auto reassignedCsr = commandList->getCsr(false);
    ASSERT_NE(initialCsr, reassignedCsr);
    EXPECT_EQ(reassignedCsr->peekTaskCount(), queue->getTaskCount());
    EXPECT_EQ(reassignedCsr->getTagAllocation()->getGpuAddress(), regularCommandList->getLatestTagGpuAddress());
    EXPECT_EQ(reassignedCsr->peekTaskCount(), regularCommandList->getLatestTaskCount());

    regularCommandList->destroy();
    initialCsr->releaseQueueOwnership();
    markCsrIdle(initialCsr);
    commandList->destroy();
}

struct DeferredFirstSubmissionCmdListTests : public Test<ModuleFixture> {
    void SetUp() override {
        debugManager.flags.ContextGroupSize.set(5);
        debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.set(1);
        Test<ModuleFixture>::SetUp();
    }

    DebugManagerStateRestore dbgRestorer;
};

HWTEST2_F(DeferredFirstSubmissionCmdListTests, givenDebugFlagSetWhenSubmittingToSecondaryThenDeferFirstSubmission, IsAtLeastXeCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    createKernel();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0;
    desc.index = 0;
    ze_command_list_handle_t commandListHandle1, commandListHandle2;

    auto result = device->createCommandListImmediate(&desc, &commandListHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandListImmediate(&desc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList1 = L0::CommandList::fromHandle(commandListHandle1);
    auto commandList2 = L0::CommandList::fromHandle(commandListHandle2);

    EXPECT_NE(commandList1->getCsr(false), commandList2->getCsr(false));

    auto primaryCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(static_cast<UltCommandStreamReceiver<FamilyType> *>(commandList2->getCsr(false))->primaryCsr);
    EXPECT_NE(nullptr, primaryCsr);
    EXPECT_NE(commandList2->getCsr(false), primaryCsr);

    EXPECT_EQ(0u, primaryCsr->peekTaskCount());
    EXPECT_EQ(0u, primaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_EQ(0u, commandList1->getCsr(false)->peekTaskCount());
    EXPECT_EQ(0u, commandList2->getCsr(false)->peekTaskCount());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    commandList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_NE(0u, primaryCsr->peekTaskCount());
    EXPECT_EQ(1u, primaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_NE(0u, commandList2->getCsr(false)->peekTaskCount());

    commandList1->destroy();
    commandList2->destroy();
}

HWTEST2_F(DeferredFirstSubmissionCmdListTests, givenDebugFlagSetWhenSubmittingToPrimaryThenDeferFirstSubmission, IsAtLeastXeCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    createKernel();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0;
    desc.index = 0;
    ze_command_list_handle_t commandListHandle1, commandListHandle2;

    auto result = device->createCommandListImmediate(&desc, &commandListHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandListImmediate(&desc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList1 = L0::CommandList::fromHandle(commandListHandle1);
    auto commandList2 = L0::CommandList::fromHandle(commandListHandle2);

    EXPECT_NE(commandList1->getCsr(false), commandList2->getCsr(false));

    auto primaryCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(static_cast<UltCommandStreamReceiver<FamilyType> *>(commandList2->getCsr(false))->primaryCsr);
    EXPECT_EQ(commandList1->getCsr(false), primaryCsr);

    EXPECT_EQ(0u, primaryCsr->peekTaskCount());
    EXPECT_EQ(0u, primaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_EQ(0u, commandList1->getCsr(false)->peekTaskCount());
    EXPECT_EQ(0u, commandList2->getCsr(false)->peekTaskCount());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    commandList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    EXPECT_NE(0u, primaryCsr->peekTaskCount());
    EXPECT_EQ(1u, primaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_EQ(0u, commandList2->getCsr(false)->peekTaskCount());

    commandList1->destroy();
    commandList2->destroy();
}

TEST(CommandList, givenCopyContextGroupEnabledWhenCreatingImmediateCommandListThenEachCmdListHasDifferentCsr) {

    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);
    debugManager.flags.ExperimentalUseSecondaryContextForInternalEngine.set(0);

    hwInfo.featureTable.ftrBcsInfo = 0b111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
    driverHandle->initialize(std::move(devices));
    auto device = static_cast<Device *>(driverHandle->devices[0]);

    auto &regularEngines = device->getNEODevice()->getRegularEngineGroups();

    auto iter = std::find_if(regularEngines.begin(), regularEngines.end(), [](const auto &engine) {
        return (engine.engineGroupType == EngineGroupType::copy || engine.engineGroupType == EngineGroupType::linkedCopy);
    });

    if (iter == regularEngines.end()) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t desc = {};
    desc.ordinal = device->getCopyEngineOrdinal();
    desc.index = 0;
    ze_command_list_handle_t commandListHandle1, commandListHandle2;

    auto result = device->createCommandListImmediate(&desc, &commandListHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandListImmediate(&desc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList1 = L0::CommandList::fromHandle(commandListHandle1);
    auto commandList2 = L0::CommandList::fromHandle(commandListHandle2);

    EXPECT_NE(commandList1->getCsr(false), commandList2->getCsr(false));

    commandList1->destroy();
    commandList2->destroy();

    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;

    result = device->createCommandListImmediate(&desc, &commandListHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandListImmediate(&desc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList1 = L0::CommandList::fromHandle(commandListHandle1);
    commandList2 = L0::CommandList::fromHandle(commandListHandle2);

    EXPECT_NE(commandList1->getCsr(false), commandList2->getCsr(false));

    commandList1->destroy();
    commandList2->destroy();
}

TEST(CommandList, givenLowPriorityCopyEngineWhenCreatingCmdListThenAssignCorrectEngine) {
    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);
    debugManager.flags.ExperimentalUseSecondaryContextForInternalEngine.set(0);

    hwInfo.featureTable.ftrBcsInfo = 0b111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
    driverHandle->initialize(std::move(devices));
    auto device = static_cast<Device *>(driverHandle->devices[0]);

    auto &engines = device->getNEODevice()->getAllEngines();

    auto lpBcsEngine = std::find_if(engines.begin(), engines.end(), [](const auto &engine) {
        return ((engine.getEngineUsage() == EngineUsage::lowPriority) && EngineHelpers::isBcs(engine.getEngineType()));
    });

    if (lpBcsEngine == engines.end()) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t desc = {};
    desc.ordinal = device->getCopyEngineOrdinal();
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
    ze_command_list_handle_t commandListHandle;

    auto result = device->createCommandListImmediate(&desc, &commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList = L0::CommandList::fromHandle(commandListHandle);

    EXPECT_EQ(commandList->getCsr(false), lpBcsEngine->commandStreamReceiver);

    commandList->destroy();
}

} // namespace ult
} // namespace L0
