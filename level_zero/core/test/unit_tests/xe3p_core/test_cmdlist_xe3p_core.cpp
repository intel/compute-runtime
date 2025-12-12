/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, xe3pCoreEnumValue);
HWTEST_EXCLUDE_PRODUCT(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToMemoryCopyRegionBlitThenTimeStampRegistersAreAdded, xe3pCoreEnumValue);
HWTEST_EXCLUDE_PRODUCT(InOrderCmdListTests, givenCmdListWhenAskingForQwordDataSizeThenReturnFalse, xe3pCoreEnumValue);
HWTEST_EXCLUDE_PRODUCT(InOrderCmdListTests, givenDebugFlagSetWhenAskingIfSkipInOrderNonWalkerSignallingAllowedThenReturnTrue_IsAtLeastXeHpcCore, xe3pCoreEnumValue);

using InOrderCmdListTestsXe3p = Test<ModuleFixture>;

XE3P_CORETEST_F(InOrderCmdListTestsXe3p, givenDebugFlagSetWhenAskingIfSkipInOrderNonWalkerSignallingAllowedThenReturnFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.set(1);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    eventDesc.index = 0;
    auto event = DestroyableZeUniquePtr<MockEvent>(static_cast<MockEvent *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));

    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    auto cmdQ = std::make_unique<Mock<CommandQueue>>(device, csr, &desc);

    auto cmdList = makeZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>>();

    cmdList->cmdQImmediate = cmdQ.get();
    cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList->commandContainer.setImmediateCmdListCsr(csr);
    cmdList->enableInOrderExecution();

    EXPECT_FALSE(cmdList->skipInOrderNonWalkerSignalingAllowed(event.get()));
}

using CommandListAppendLaunchKernelXe3p = Test<ModuleFixture>;
XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3p, givenVariousKernelsWhenUpdateStreamPropertiesIsCalledThenRequiredStateFinalStateAndCommandsToPatchAreCorrectlySet) {
    DebugManagerStateRestore restorer;

    debugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::KernelImp> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());

    const ze_group_count_t launchKernelArgs = {};
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);

    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
}

XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3p, givenCmdListWhenAskingForQwordDataSizeThenReturnTrue) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();

    EXPECT_TRUE(commandList->isQwordInOrderCounter());
}

XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3p, givenVariousKernelsAndPatchingDisallowedWhenUpdateStreamPropertiesIsCalledThenCommandsToPatchAreEmpty) {
    DebugManagerStateRestore restorer;

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::KernelImp> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const ze_group_count_t launchKernelArgs = {};
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    debugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();
}

struct LocalMemoryModuleFixture : public ModuleFixture {
    void setUp() {
        debugManager.flags.EnableLocalMemory.set(1);
        ModuleFixture::setUp();
    }
    DebugManagerStateRestore restore;
};

using CommandListAppendLaunchKernelXe3pCore = Test<LocalMemoryModuleFixture>;

XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3pCore, givenAppendKernelWhenKernelNotUsingSystemMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed) {

    using WalkerType = typename FamilyType::DefaultWalkerType;
    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.privateState.argumentsResidencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto it = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(commands.begin(), commands.end());
    ASSERT_NE(it, commands.end());

    auto walker = genCmdCast<WalkerType *>(*it);
    EXPECT_FALSE(walker->getPostSync().getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3pCore,
                givenAppendKernelWhenKernelUsingUsmHostMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.privateState.argumentsResidencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto it = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(commands.begin(), commands.end());
    ASSERT_NE(it, commands.end());

    auto walker = genCmdCast<WalkerType *>(*it);
    auto &postSync = walker->getPostSync();
    EXPECT_FALSE(postSync.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3pCore,
                givenAppendKernelWhenMigrationOnComputeUsingUsmSharedCpuMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto dstAllocation = allocData->cpuAllocation;
    ASSERT_NE(nullptr, dstAllocation);
    auto srcAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, srcAllocation);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendPageFaultCopy(dstAllocation, srcAllocation, size, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto it = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(commands.begin(), commands.end());
    ASSERT_NE(it, commands.end());

    auto walker = genCmdCast<WalkerType *>(*it);
    auto &postSync = walker->getPostSync();
    EXPECT_FALSE(postSync.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3pCore,
                givenAppendKernelWhenKernelUsingIndirectSystemMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.privateState.argumentsResidencyContainer.push_back(kernelAllocation);

    kernel.privateState.unifiedMemoryControls.indirectHostAllocationsAllowed = true;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto it = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(commands.begin(), commands.end());
    ASSERT_NE(it, commands.end());

    auto walker = genCmdCast<WalkerType *>(*it);
    auto &postSync = walker->getPostSync();
    EXPECT_FALSE(postSync.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3pCore,
                givenAppendKernelWhenKernelUsingDeviceMemoryAllocationsAndEventHostSignalScopeThenExpectsNoSystemFenceUsed) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.privateState.argumentsResidencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto it = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(commands.begin(), commands.end());
    ASSERT_NE(it, commands.end());

    auto walker = genCmdCast<WalkerType *>(*it);
    auto &postSync = walker->getPostSync();
    EXPECT_FALSE(postSync.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

XE3P_CORETEST_F(CommandListAppendLaunchKernelXe3pCore,
                givenAppendKernelWhenKernelUsingUsmHostMemoryAllocationsAndEventHostSignalScopeThenExpectsSystemFenceUsed) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.privateState.argumentsResidencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto it = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(commands.begin(), commands.end());
    ASSERT_NE(it, commands.end());

    auto walker = genCmdCast<WalkerType *>(*it);
    auto &postSync = walker->getPostSync();
    EXPECT_EQ(postSync.getSystemMemoryFenceRequest(), !device->getHwInfo().capabilityTable.isIntegratedDevice);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

using CommandListCreateXe3pTest = Test<DeviceFixture>;

XE3P_CORETEST_F(CommandListCreateXe3pTest,
                givenHeaplessEnabledWhenCreatingRegularCommandListThenScratchAddressPatchingEnabled) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.Enable64BitAddressing.set(1);

    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList.get());

    EXPECT_TRUE(commandList->scratchAddressPatchingEnabled);
}

XE3P_CORETEST_F(CommandListCreateXe3pTest,
                givenHeaplessEnabledWhenCreatingImmediateCommandListThenScratchAddressPatchingDisabled) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.Enable64BitAddressing.set(1);

    ze_result_t returnValue;
    const ze_command_queue_desc_t desc = {};
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::compute, returnValue)));
    ASSERT_NE(nullptr, commandList.get());

    EXPECT_FALSE(commandList->scratchAddressPatchingEnabled);
}

using CommandListTestsReserveSize = Test<DeviceFixture>;
XE3P_CORETEST_F(CommandListTestsReserveSize, givenCommandListWhenGetReserveSshSizeThen16slotSpaceReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    L0::CommandListCoreFamily<FamilyType::gfxCoreFamily> commandList(1u);
    commandList.initialize(device, NEO::EngineGroupType::compute, 0u);

    EXPECT_EQ(commandList.getReserveSshSize(), (16 * 2 + 1) * 2 * sizeof(typename FamilyType::RENDER_SURFACE_STATE));
}

using MultiTileSynchronizedDispatchTestsXe3p = MultiTileSynchronizedDispatchFixture;

XE3P_CORETEST_F(MultiTileSynchronizedDispatchTestsXe3p, givenLimitedSyncDispatchWhenAppendingThenProgramTokenCheck) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>>;
    class MyCmdList : public BaseClass {
      public:
        void appendSynchronizedDispatchInitializationSection() override {
            initCalled++;
            BaseClass::appendSynchronizedDispatchInitializationSection();
        }

        void appendSynchronizedDispatchCleanupSection() override {
            cleanupCalled++;
            BaseClass::appendSynchronizedDispatchCleanupSection();
        }

        uint32_t initCalled = 0;
        uint32_t cleanupCalled = 0;
    };

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto immCmdList = createImmCmdListImpl<FamilyType::gfxCoreFamily, MyCmdList>(false);
    immCmdList->partitionCount = partitionCount;
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint32_t expectedInitCalls = 1;
    uint32_t expectedCleanupCalls = 1;

    auto verifyTokenCheck = [&](uint32_t numDependencies, uint32_t semaphoreOffsetFromEnd) {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto allSemaphores = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        auto nSemaphores = allSemaphores.size();
        auto i = nSemaphores - 1u;
        i -= semaphoreOffsetFromEnd;
        auto semaphore = allSemaphores[i];

        EXPECT_NE(cmdList.end(), semaphore);

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        EXPECT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(0u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(device->getSyncDispatchTokenAllocation()->getGpuAddress() + sizeof(uint32_t), semaphoreCmd->getSemaphoreGraphicsAddress());
        EXPECT_EQ(COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());

        EXPECT_EQ(expectedInitCalls++, immCmdList->initCalled);
        EXPECT_EQ(expectedCleanupCalls++, immCmdList->cleanupCalled);

        return !::testing::Test::HasFailure();
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_TRUE(verifyTokenCheck(0, 0));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), nullptr, 0, nullptr, false);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    offset = cmdStream->getUsed();
    const ze_kernel_handle_t launchKernels = kernel->toHandle();
    immCmdList->appendLaunchMultipleKernelsIndirect(1, &launchKernels, reinterpret_cast<const uint32_t *>(alloc), &groupCount, nullptr, 0, nullptr, false);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    offset = cmdStream->getUsed();
    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_TRUE(verifyTokenCheck(1, 1));

    offset = cmdStream->getUsed();
    size_t rangeSizes = 1;
    const void **ranges = const_cast<const void **>(&alloc);
    immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    offset = cmdStream->getUsed();
    CmdListMemoryCopyParams copyParams = {};
    immCmdList->appendMemoryCopy(alloc, alloc, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(alloc, &region, 1, 1, alloc, &region, 1, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(alloc, alloc, 8, 8, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    offset = cmdStream->getUsed();
    immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(alloc), nullptr, 0, nullptr);
    EXPECT_TRUE(verifyTokenCheck(1, 0));

    offset = cmdStream->getUsed();
    auto handle = events[0]->toHandle();
    events[0]->unsetCmdQueue();
    immCmdList->appendBarrier(nullptr, 1, &handle, false);
    EXPECT_TRUE(verifyTokenCheck(2, 0));

    context->freeMem(alloc);
}

using CommandListTestsScratchPtrPatchXe3p = Test<DeviceFixture>;

XE3P_CORETEST_F(CommandListTestsScratchPtrPatchXe3p, whenAddPatchScratchAddressInImplicitArgsIsCalledThenCommandsToPatchAreCorrect) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto dispatchInterface = std::make_unique<NEO::MockDispatchKernelEncoder>();

    NEO::ImplicitArgs implicitArgs{};
    implicitArgs.initializeHeader(1);
    dispatchInterface->implicitArgsPtr = &implicitArgs;

    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = 8u;
    DebugManagerStateRestore dbgRestorer;

    ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_result_t returnValue;

    WhiteBox<L0::CommandQueue> *commandQueue = nullptr;
    commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                     device,
                                                     neoDevice->getDefaultEngine().commandStreamReceiver,
                                                     &queueDesc,
                                                     false,
                                                     false,
                                                     false,
                                                     returnValue));
    ASSERT_NE(nullptr, commandQueue);
    commandQueue->setPatchingPreamble(true, false);

    NEO::EncodeDispatchKernelArgs args{};
    args.isHeaplessModeEnabled = true;
    args.device = device->getNEODevice();
    args.dispatchInterface = dispatchInterface.get();
    args.outImplicitArgsPtr = reinterpret_cast<void *>(0xABCD);
    args.outImplicitArgsGpuVa = 0xAAFF0000;

    void *queueCpuBase = commandQueue->commandStream.getCpuBase();

    {
        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        CommandList::CommandsToPatch &commandsToPatch = commandList->commandsToPatch;
        bool kernelNeedsImplicitArgs = false;
        commandList->addPatchScratchAddressInImplicitArgs(commandsToPatch, args, dispatchInterface->kernelDescriptor, kernelNeedsImplicitArgs);
        ASSERT_EQ(commandsToPatch.size(), 0u);
    }
    {
        NEO::ImplicitArgs implicitArgs0{};
        implicitArgs0.initializeHeader(0);
        VariableBackup backupImplicitArgs{&dispatchInterface->implicitArgsPtr, &implicitArgs0};
        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        CommandList::CommandsToPatch &commandsToPatch = commandList->commandsToPatch;
        bool kernelNeedsImplicitArgs = true;
        commandList->addPatchScratchAddressInImplicitArgs(commandsToPatch, args, dispatchInterface->kernelDescriptor, kernelNeedsImplicitArgs);
        ASSERT_EQ(commandsToPatch.size(), 0u);
    }
    {
        NEO::ImplicitArgs implicitArgs2{};
        implicitArgs2.initializeHeader(2);
        VariableBackup backupImplicitArgs{&dispatchInterface->implicitArgsPtr, &implicitArgs2};
        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        CommandList::CommandsToPatch &commandsToPatch = commandList->commandsToPatch;
        bool kernelNeedsImplicitArgs = true;
        commandList->addPatchScratchAddressInImplicitArgs(commandsToPatch, args, dispatchInterface->kernelDescriptor, kernelNeedsImplicitArgs);
        ASSERT_EQ(commandsToPatch.size(), 0u);
    }
    {
        debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::privateHeaps));
        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        CommandList::CommandsToPatch &commandsToPatch = commandList->commandsToPatch;
        bool kernelNeedsImplicitArgs = true;
        commandList->addPatchScratchAddressInImplicitArgs(commandsToPatch, args, dispatchInterface->kernelDescriptor, kernelNeedsImplicitArgs);
        ASSERT_EQ(1u, commandsToPatch.size());
        EXPECT_EQ(1u, commandList->getActiveScratchPatchElements());

        auto expectedImplicitArgsPtr = args.outImplicitArgsPtr;
        auto expectedScratchOffset = dispatchInterface->getImplicitArgs()->getScratchPtrOffset();
        EXPECT_TRUE(expectedScratchOffset.has_value());
        auto expectedPatchCommandType = CommandToPatch::CommandType::ComputeWalkerImplicitArgsScratch;
        auto expectedPatchSize = dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize;
        auto ssh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        uint64_t expectedScratchAddress = ssh != nullptr ? ssh->getGpuBase() : 0u;
        uint64_t expectedScratchAddressAfterPatch = 0u;

        EXPECT_EQ(expectedPatchCommandType, commandsToPatch[0].type);
        EXPECT_EQ(expectedPatchSize, commandsToPatch[0].patchSize);
        EXPECT_EQ(expectedImplicitArgsPtr, commandsToPatch[0].pDestination);
        EXPECT_EQ(expectedScratchOffset, commandsToPatch[0].offset);
        EXPECT_EQ(expectedScratchAddress, commandsToPatch[0].baseAddress);
        EXPECT_EQ(expectedScratchAddressAfterPatch, commandsToPatch[0].scratchAddressAfterPatch);

        commandList->close();
        auto cmdListHandle = commandList->toHandle();

        auto usedSpaceBefore = commandQueue->commandStream.getUsed();
        returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        auto usedSpaceAfter = commandQueue->commandStream.getUsed();
        ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(queueCpuBase, usedSpaceBefore),
            usedSpaceAfter - usedSpaceBefore));
        auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        ASSERT_LT(2u, sdiCmds.size()); // last two SDI encodes returning BB_START

        auto expectedGpuVa = args.outImplicitArgsGpuVa + expectedScratchOffset.value();

        size_t sdiMax = sdiCmds.size() - 2;
        for (size_t i = 0; i < sdiMax; i++) {
            auto sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmds[i]);
            EXPECT_EQ(expectedGpuVa, sdiCmd->getAddress());
            if (sdiCmd->getStoreQword() == false) {
                expectedGpuVa += sizeof(uint32_t);
            }
        }
    }
    {
        debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::globalStateless));
        debugManager.flags.Enable64BitAddressing.set(1);
        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        CommandList::CommandsToPatch &commandsToPatch = commandList->commandsToPatch;

        bool kernelNeedsImplicitArgs = true;
        commandList->addPatchScratchAddressInImplicitArgs(commandsToPatch, args, dispatchInterface->kernelDescriptor, kernelNeedsImplicitArgs);
        ASSERT_EQ(1u, commandsToPatch.size());
        EXPECT_EQ(1u, commandList->getActiveScratchPatchElements());

        auto expectedImplicitArgsPtr = args.outImplicitArgsPtr;
        auto expectedScratchOffset = dispatchInterface->getImplicitArgs()->getScratchPtrOffset();
        EXPECT_TRUE(expectedScratchOffset.has_value());
        auto expectedPatchCommandType = CommandToPatch::CommandType::ComputeWalkerImplicitArgsScratch;
        auto expectedPatchSize = dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize;
        uint64_t expectedBaseAddress = 0;
        uint64_t expectedScratchAddressAfterPatch = 0u;

        EXPECT_EQ(expectedPatchCommandType, commandsToPatch[0].type);
        EXPECT_EQ(expectedPatchSize, commandsToPatch[0].patchSize);
        EXPECT_EQ(expectedImplicitArgsPtr, commandsToPatch[0].pDestination);
        EXPECT_EQ(expectedScratchOffset.value(), commandsToPatch[0].offset);
        EXPECT_EQ(expectedBaseAddress, commandsToPatch[0].baseAddress);
        EXPECT_EQ(expectedScratchAddressAfterPatch, commandsToPatch[0].scratchAddressAfterPatch);
    }

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
