/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

#include "test_traits_common.h"

namespace NEO {
namespace SysCalls {
extern bool getNumThreadsCalled;
}
} // namespace NEO

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::compute,
                                                                     0u,
                                                                     returnValue, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->getPartitionCount());

    returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->getPartitionCount());
}

HWTEST_F(CommandListCreate, WhenReservingSpaceThenCommandsAddedToBatchBuffer) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
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
        if (itor == cmdList.end())
            break;

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

        auto whiteboxCommandList = static_cast<CommandList *>(CommandList::fromHandle(commandList));
        uint32_t ordinal = 0;
        whiteboxCommandList->getOrdinal(&ordinal);
        EXPECT_EQ(engineGroup, ordinal);

        whiteboxCommandList->destroy();
    }
}

TEST_F(CommandListCreate, givenCommandListWhenCallingImmediateCommandListIndexThenInvalidArgumentIsReturned) {
    ze_command_list_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    ze_command_list_handle_t commandList = nullptr;

    auto returnValue = device->createCommandList(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    auto whiteboxCommandList = static_cast<CommandList *>(CommandList::fromHandle(commandList));

    uint32_t index = 4;
    ze_bool_t isImmediate = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->isImmediate(&isImmediate));
    EXPECT_FALSE(static_cast<bool>(isImmediate));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, whiteboxCommandList->getImmediateIndex(&index));
    EXPECT_EQ(4u, index);

    whiteboxCommandList->destroy();
}

TEST_F(CommandListCreate, givenImmediateCommandListWhenCallingImmediateCommandListIndexThenValidIndexIsReturned) {
    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_command_list_handle_t commandList = nullptr;

    auto returnValue = device->createCommandListImmediate(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    auto whiteboxCommandList = static_cast<CommandList *>(CommandList::fromHandle(commandList));

    uint32_t index = 4, cmdQIndex = 2;
    ze_bool_t isImmediate = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->isImmediate(&isImmediate));
    EXPECT_TRUE(static_cast<bool>(isImmediate));
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->getImmediateIndex(&index));
    EXPECT_NE(nullptr, whiteboxCommandList->cmdQImmediate);
    EXPECT_EQ(ZE_RESULT_SUCCESS, whiteboxCommandList->cmdQImmediate->getIndex(&cmdQIndex));
    EXPECT_EQ(cmdQIndex, index);

    whiteboxCommandList->destroy();
}

TEST_F(CommandListCreate, givenQueueFlagsWhenCreatingImmediateCommandListThenDontCopyFlags) {
    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
    ze_command_list_handle_t commandList = nullptr;

    auto returnValue = device->createCommandListImmediate(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    auto whiteboxCommandList = static_cast<CommandList *>(CommandList::fromHandle(commandList));
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
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

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
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    const ze_kernel_handle_t launchKernels = kernel->toHandle();
    const ze_group_count_t launchKernelArgs = {1, 1, 1};
    uint32_t *numLaunchArgs;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    returnValue = context->allocDeviceMem(
        device->toHandle(), &deviceDesc, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
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
    kernel.groupSize[0] = 2;
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
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), *static_cast<ze_group_count_t *>(alloc), event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->toHandle(), commandList->appendEventKernelIndirectEventHandleValue);

    context->freeMem(alloc);
}

HWTEST2_F(CommandListAppendLaunchKernel, GivenComputeModePropertiesWhenUpdateStreamPropertiesIsCalledTwiceThenChangedFieldsAreDirty, IsHeapfulSupported) {
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
    if (commandList->stateComputeModeTracking) {
        if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
            EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        }
        EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    } else {
        if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
            EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        }
        EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x80;
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
        EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }
    if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
        EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    } else {
        EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    }
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
    if (commandList->stateComputeModeTracking) {
        if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
            EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        }
        if (productHelper.isGrfNumReportedWithScm()) {
            EXPECT_NE(-1, commandList->finalStreamState.stateComputeMode.largeGrfMode.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.largeGrfMode.value);
        }
    } else {
        if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
            EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        }
        EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x80;
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
        EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    } else {
        EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    }
}

HWTEST2_F(CommandListAppendLaunchKernel, GivenComputeModePropertiesWhenPropertesNotChangedThenAllFieldsAreNotDirty, IsHeapfulSupported) {
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
    if (commandList->stateComputeModeTracking) {
        if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
            EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        }
        EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    } else {
        if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
            EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
        }
        EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }

    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    if (productHelper.getScmPropertyCoherencyRequiredSupport()) {
        EXPECT_EQ(0, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    } else {
        EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.isCoherencyRequired.value);
    }
    EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
}

HWTEST_F(CommandListCreate, givenFlushErrorWhenPerformingCpuMemoryCopyThenErrorIsReturned) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = false;

    ze_result_t returnValue;

    using cmdListImmediateHwType = typename L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>;

    std::unique_ptr<cmdListImmediateHwType> commandList0(static_cast<cmdListImmediateHwType *>(CommandList::createImmediate(productFamily,
                                                                                                                            device,
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

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListCreate, givenImmediateCommandListWhenAppendingMemoryCopyWithInvalidEventThenInvalidArgumentErrorIsReturned) {
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
    commandList->cmdQImmediate = CommandQueue::create(productFamily, device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, ret);

    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(scratchPerThreadSize, commandList->getCommandListPerThreadScratchSize(0u));

    auto ultCsr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    EXPECT_EQ(scratchPerThreadSize, ultCsr->requiredScratchSlot0Size);
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
    commandList->cmdQImmediate = CommandQueue::create(productFamily, device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, ret);

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
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
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
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

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
            EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

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
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

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
            EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

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
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));

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
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
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
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

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
            EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

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
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

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
            EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

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
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(feState));

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
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
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
        EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));
    } else {
        EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));
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
        EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));
    } else {
        EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(feState));
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
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

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

HWTEST_F(CommandListCreate, givenImmediateCommandListWhenThereIsNoEnoughSpaceForWaitOnEventsAndImmediateCommandAndAllocationListNotEmptyThenReuseCommandBuffer) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    constexpr uint32_t numEvents = 100;
    constexpr size_t eventWaitSize = numEvents * NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
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
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
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

    auto commandList1 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle1));
    auto commandList2 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle2));

    EXPECT_NE(commandList1->getCsr(false), commandList2->getCsr(false));

    commandList1->destroy();
    commandList2->destroy();
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
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
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

    auto commandList1 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle1));
    auto commandList2 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle2));

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
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
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

    auto commandList1 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle1));
    auto commandList2 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle2));

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

TEST(CommandList, givenContextGroupEnabledWhenCreatingImmediateCommandListWithInterruptFlagThenPassInterruptFlagToContext) {
    class MockOsContext : public OsContext {
      public:
        using OsContext::OsContext;

        bool initializeContext(bool allocateInterrupt) override {
            allocateInterruptPassed = allocateInterrupt;
            return OsContext::initializeContext(allocateInterrupt);
        }

        bool allocateInterruptPassed = false;
    };

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    std::vector<MockOsContext *> mockOsContexts;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    {
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        auto device = driverHandle->devices[0];

        for (auto &engine : neoDevice->secondaryEngines[aub_stream::ENGINE_CCS].engines) {
            EngineDescriptor descriptor({aub_stream::ENGINE_CCS, engine.osContext->getEngineUsage()}, engine.osContext->getDeviceBitfield(), PreemptionMode::Disabled, false);
            auto newOsContext = new MockOsContext(0, 0, descriptor);
            mockOsContexts.push_back(newOsContext);
            newOsContext->incRefInternal();

            newOsContext->setIsPrimaryEngine(engine.osContext->getIsPrimaryEngine());
            newOsContext->setContextGroup(engine.osContext->isPartOfContextGroup());

            engine.osContext = newOsContext;
            engine.commandStreamReceiver->setupContext(*newOsContext);
        }

        zex_intel_queue_allocate_msix_hint_exp_desc_t allocateMsix = {};
        allocateMsix.stype = ZEX_INTEL_STRUCTURE_TYPE_QUEUE_ALLOCATE_MSIX_HINT_EXP_PROPERTIES;
        allocateMsix.uniqueMsix = true;

        ze_command_queue_desc_t desc = {};
        desc.pNext = &allocateMsix;

        ze_command_list_handle_t commandListHandle1, commandListHandle2, commandListHandle3;

        auto result = device->createCommandListImmediate(&desc, &commandListHandle1);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = device->createCommandListImmediate(&desc, &commandListHandle2);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        allocateMsix.uniqueMsix = false;
        result = device->createCommandListImmediate(&desc, &commandListHandle3);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        auto commandList1 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle1));
        auto commandList2 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle2));
        auto commandList3 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle3));

        EXPECT_TRUE(static_cast<MockOsContext &>(commandList1->getCsr(false)->getOsContext()).allocateInterruptPassed);
        EXPECT_TRUE(static_cast<MockOsContext &>(commandList2->getCsr(false)->getOsContext()).allocateInterruptPassed);
        EXPECT_FALSE(static_cast<MockOsContext &>(commandList3->getCsr(false)->getOsContext()).allocateInterruptPassed);

        commandList1->destroy();
        commandList2->destroy();
        commandList3->destroy();
    }

    for (auto &context : mockOsContexts) {
        context->decRefInternal();
    }
}

TEST(CommandList, givenCopyContextGroupEnabledWhenCreatingImmediateCommandListThenEachCmdListHasDifferentCsr) {

    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.ftrBcsInfo = 0b111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = static_cast<DeviceImp *>(driverHandle->devices[0]);

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

    auto commandList1 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle1));
    auto commandList2 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle2));

    EXPECT_NE(commandList1->getCsr(false), commandList2->getCsr(false));

    commandList1->destroy();
    commandList2->destroy();

    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;

    result = device->createCommandListImmediate(&desc, &commandListHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandListImmediate(&desc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList1 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle1));
    commandList2 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle2));

    EXPECT_NE(commandList1->getCsr(false), commandList2->getCsr(false));

    commandList1->destroy();
    commandList2->destroy();
}

TEST(CommandList, givenLowPriorityCopyEngineWhenCreatingCmdListThenAssignCorrectEngine) {
    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.ftrBcsInfo = 0b111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = static_cast<DeviceImp *>(driverHandle->devices[0]);

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

    auto commandList = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle));

    EXPECT_EQ(commandList->getCsr(false), lpBcsEngine->commandStreamReceiver);

    commandList->destroy();
}

} // namespace ult
} // namespace L0
