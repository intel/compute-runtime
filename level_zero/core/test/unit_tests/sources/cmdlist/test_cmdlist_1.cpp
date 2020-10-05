/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

using ContextCommandListCreate = Test<ContextFixture>;

TEST_F(ContextCommandListCreate, whenCreatingCommandListFromContextThenSuccessIsReturned) {
    ze_command_list_desc_t desc = {};
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandList(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::CommandList *commandList = L0::CommandList::fromHandle(hCommandList);
    commandList->destroy();
}

TEST_F(ContextCommandListCreate, whenCreatingCommandListImmediateFromContextThenSuccessIsReturned) {
    ze_command_queue_desc_t desc = {};
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandListImmediate(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::CommandList *commandList = L0::CommandList::fromHandle(hCommandList);
    commandList->destroy();
}

using CommandListCreate = Test<DeviceFixture>;

TEST(zeCommandListCreateImmediate, DISABLED_redirectsToObject) {
    Mock<Device> device;
    Mock<Context> context;
    ze_command_queue_desc_t desc = {};
    ze_command_list_handle_t commandList = {};

    EXPECT_CALL(device, createCommandListImmediate(&desc, &commandList))
        .Times(1)
        .WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    auto result = zeCommandListCreateImmediate(context.toHandle(), device.toHandle(), &desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, whenCommandListIsCreatediWithInvalidProductFamilyThenFailureIsReturned) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(PRODUCT_FAMILY::IGFX_MAX_PRODUCT, device, NEO::EngineGroupType::RenderCompute, returnValue));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, returnValue);
    ASSERT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreate, whenCommandListImmediateIsCreatediWithInvalidProductFamilyThenFailureIsReturned) {
    ze_result_t returnValue;
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(PRODUCT_FAMILY::IGFX_MAX_PRODUCT,
                                                                              device,
                                                                              &desc,
                                                                              internalEngine,
                                                                              NEO::EngineGroupType::RenderCompute,
                                                                              returnValue));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, returnValue);
    ASSERT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreate, whenCommandListIsCreatedThenItIsInitialized) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    ASSERT_GT(commandList->commandContainer.getCmdBufferAllocations().size(), 0u);

    auto numAllocations = 0u;
    auto allocation = whitebox_cast(commandList->commandContainer.getCmdBufferAllocations()[0]);
    ASSERT_NE(allocation, nullptr);

    ++numAllocations;

    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    for (uint32_t i = 0; i < NEO::HeapType::NUM_TYPES; i++) {
        ASSERT_NE(commandList->commandContainer.getIndirectHeap(static_cast<NEO::HeapType>(i)), nullptr);
        ++numAllocations;
        ASSERT_NE(commandList->commandContainer.getIndirectHeapAllocation(static_cast<NEO::HeapType>(i)), nullptr);
    }

    EXPECT_LT(0u, commandList->commandContainer.getCommandStream()->getAvailableSpace());
    ASSERT_EQ(commandList->commandContainer.getResidencyContainer().size(), numAllocations);
    EXPECT_EQ(commandList->commandContainer.getResidencyContainer().front(), allocation);
}

TEST_F(CommandListCreate, givenRegularCommandListThenDefaultNumIddPerBlockIsUsed) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    const uint32_t defaultNumIdds = CommandList::defaultNumIddsPerBlock;
    EXPECT_EQ(defaultNumIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemAdviseReturnsError) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemAdvise(device, nullptr, 0, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemoryPrefetchReturnsError) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemoryPrefetch(nullptr, 0);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseReturnsSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    auto res = driverHandle->allocDeviceMem(device->toHandle(),
                                            0u,
                                            size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = driverHandle->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemoryPrefetchReturnsSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    auto res = driverHandle->allocDeviceMem(device->toHandle(),
                                            0u,
                                            size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = driverHandle->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenImmediateCommandListThenInternalEngineIsUsedIfRequested) {
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

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    internalEngine = false;

    std::unique_ptr<L0::CommandList> commandList1(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList1);

    cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList1->cmdQImmediate);
    EXPECT_NE(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);
}

TEST_F(CommandListCreate, givenImmediateCommandListThenCustomNumIddPerBlockUsed) {
    const ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    const uint32_t cmdListImmediateIdds = CommandList::commandListimmediateIddsPerBlock;
    EXPECT_EQ(cmdListImmediateIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListThenItHasImmediateCommandQueueCreated) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
}

TEST_F(CommandListCreate, whenInvokingAppendMemoryCopyFromContextForImmediateCommandListThenSuccessIsReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, givenQueueDescriptionwhenCreatingImmediateCommandListForEveryEnigneThenItHasImmediateCommandQueueCreated) {
    auto engines = neoDevice->getEngineGroups();
    uint32_t numaAvailableEngineGroups = 0;
    for (uint32_t ordinal = 0; ordinal < static_cast<uint32_t>(NEO::EngineGroupType::MaxEngineGroups); ordinal++) {
        if (engines[ordinal].size()) {
            numaAvailableEngineGroups++;
        }
    }
    for (uint32_t ordinal = 0; ordinal < numaAvailableEngineGroups; ordinal++) {
        uint32_t engineGroupIndex = ordinal;
        device->mapOrdinalForAvailableEngineGroup(&engineGroupIndex);
        for (uint32_t index = 0; index < engines[engineGroupIndex].size(); index++) {
            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal;
            desc.index = index;
            ze_result_t returnValue;
            std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
            ASSERT_NE(nullptr, commandList);

            EXPECT_EQ(device, commandList->device);
            EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
            EXPECT_NE(nullptr, commandList->cmdQImmediate);
        }
    }
}

TEST_F(CommandListCreate, givenInvalidProductFamilyThenReturnsNullPointer) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(IGFX_UNKNOWN, device, NEO::EngineGroupType::RenderCompute, returnValue));
    EXPECT_EQ(nullptr, commandList);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListCreate, whenCommandListIsCreatedThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandContainer = commandList->commandContainer;
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPc = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPc);
    auto cmdPc = genCmdCast<PIPE_CONTROL *>(*itorPc);
    EXPECT_TRUE(cmdPc->getDcFlushEnable());
    EXPECT_TRUE(cmdPc->getCommandStreamerStallEnable());
    EXPECT_TRUE(cmdPc->getTextureCacheInvalidationEnable());

    auto itor = find<STATE_BASE_ADDRESS *>(itorPc, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    auto ioh = commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);

    EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
    EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());

    EXPECT_TRUE(cmdSba->getIndirectObjectBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(ioh->getHeapGpuBase(), cmdSba->getIndirectObjectBaseAddress());
    EXPECT_EQ(ioh->getHeapSizeInPages(), cmdSba->getIndirectObjectBufferSize());

    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenCreatedThenStateBaseAddressCmdIsNotProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, returnValue));
    auto &commandContainer = commandList->commandContainer;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenSetBarrierThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, returnValue));
    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenResetThenStateBaseAddressNotProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, returnValue));
    auto &commandContainer = commandList->commandContainer;
    commandList->reset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenSetBarrierThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

class MockEvent : public Mock<Event> {
  public:
    MockEvent() {
        mockAllocation.reset(new NEO::MockGraphicsAllocation(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                             MemoryPool::System4KBPages));
        gpuAddress = mockAllocation->getGpuAddress();
    }
    NEO::GraphicsAllocation &getAllocation() override {
        return *mockAllocation.get();
    }
    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
};

HWTEST_F(CommandListCreate, givenCommandListWithInvalidWaitEventArgWhenAppendQueryKernelTimestampsThenProperErrorRetruned) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    device->getBuiltinFunctionsLib()->initFunctions();
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    auto result = driverHandle->allocDeviceMem(device, ZE_DEVICE_MEM_ALLOC_FLAG_FORCE_UINT32, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto eventHandle = event.toHandle();

    result = commandList->appendQueryKernelTimestamps(1u, &eventHandle, alloc, nullptr, nullptr, 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    driverHandle->freeMem(alloc);
}

struct CmdListHelper {
    NEO::GraphicsAllocation *isaAllocation = nullptr;
    NEO::ResidencyContainer residencyContainer;
    ze_group_count_t threadGroupDimensions;
    const uint32_t *groupSize = nullptr;
    uint32_t useOnlyGlobalTimestamp = std::numeric_limits<uint32_t>::max();
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForAppendLaunchKernel : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {

  public:
    CmdListHelper cmdListHelper;
    ze_result_t appendLaunchKernel(ze_kernel_handle_t hKernel,
                                   const ze_group_count_t *pThreadGroupDimensions,
                                   ze_event_handle_t hEvent,
                                   uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents) override {

        const auto kernel = Kernel::fromHandle(hKernel);
        cmdListHelper.isaAllocation = kernel->getIsaAllocation();
        cmdListHelper.residencyContainer = kernel->getResidencyContainer();
        cmdListHelper.groupSize = kernel->getGroupSize();
        cmdListHelper.threadGroupDimensions = *pThreadGroupDimensions;

        auto kernelName = kernel->getImmutableData()->getDescriptor().kernelMetadata.kernelName;
        NEO::ArgDescriptor arg;
        if (kernelName == "QueryKernelTimestamps") {
            arg = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[2u];
        } else if (kernelName == "QueryKernelTimestampsWithOffsets") {
            arg = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[3u];
        } else {
            return ZE_RESULT_SUCCESS;
        }
        auto crossThreadData = kernel->getCrossThreadData();
        auto element = arg.as<NEO::ArgDescValue>().elements[0];
        auto pDst = ptrOffset(crossThreadData, element.offset);
        cmdListHelper.useOnlyGlobalTimestamp = *(uint32_t *)(pDst);

        return ZE_RESULT_SUCCESS;
    }
};

using AppendQueryKernelTimestamps = CommandListCreate;
using TestPlatforms = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsWithoutOffsetsThenProperBuiltinWasAdded, TestPlatforms) {
    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::RenderCompute);

    device->getBuiltinFunctionsLib()->initFunctions();
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    auto result = driverHandle->allocDeviceMem(device, ZE_DEVICE_MEM_ALLOC_FLAG_FORCE_UINT32, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    bool containsDstPtr = false;

    for (auto &a : commandList.cmdListHelper.residencyContainer) {
        if (a != nullptr && a->getGpuAddress() == reinterpret_cast<uint64_t>(alloc)) {
            containsDstPtr = true;
        }
    }

    EXPECT_TRUE(containsDstPtr);

    EXPECT_EQ(device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestamps)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());
    EXPECT_EQ(2u, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[2]);

    EXPECT_EQ(NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    driverHandle->freeMem(alloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsWithOffsetsThenProperBuiltinWasAdded, TestPlatforms) {
    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::RenderCompute);

    device->getBuiltinFunctionsLib()->initFunctions();
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    auto result = driverHandle->allocDeviceMem(device, ZE_DEVICE_MEM_ALLOC_FLAG_FORCE_UINT32, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    void *offsetAlloc;
    result = driverHandle->allocDeviceMem(device, ZE_DEVICE_MEM_ALLOC_FLAG_FORCE_UINT32, 128, 1, &offsetAlloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};

    auto offsetSizes = reinterpret_cast<size_t *>(offsetAlloc);
    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, offsetSizes, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    bool containsDstPtr = false;

    for (auto &a : commandList.cmdListHelper.residencyContainer) {
        if (a != nullptr && a->getGpuAddress() == reinterpret_cast<uint64_t>(alloc)) {
            containsDstPtr = true;
        }
    }

    EXPECT_TRUE(containsDstPtr);

    bool containOffsetPtr = false;

    for (auto &a : commandList.cmdListHelper.residencyContainer) {
        if (a != nullptr && a->getGpuAddress() == reinterpret_cast<uint64_t>(offsetAlloc)) {
            containOffsetPtr = true;
        }
    }

    EXPECT_TRUE(containOffsetPtr);

    EXPECT_EQ(device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestampsWithOffsets)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());
    EXPECT_EQ(2u, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[2]);

    EXPECT_EQ(NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    driverHandle->freeMem(alloc);
    driverHandle->freeMem(offsetAlloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsWithEventsNumberBiggerThanMaxWorkItemSizeThenProperGroupSizeAndGroupCountIsSet, TestPlatforms) {
    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::RenderCompute);

    device->getBuiltinFunctionsLib()->initFunctions();
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    auto result = driverHandle->allocDeviceMem(device, ZE_DEVICE_MEM_ALLOC_FLAG_FORCE_UINT32, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    size_t eventCount = device->getNEODevice()->getDeviceInfo().maxWorkItemSizes[0] * 2u;
    std::unique_ptr<ze_event_handle_t[]> events = std::make_unique<ze_event_handle_t[]>(eventCount);

    for (size_t i = 0u; i < eventCount; ++i) {
        events[i] = event.toHandle();
    }

    result = commandList.appendQueryKernelTimestamps(static_cast<uint32_t>(eventCount), events.get(), alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestamps)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());

    uint32_t groupSizeX = static_cast<uint32_t>(eventCount);
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestamps)->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);

    EXPECT_EQ(groupSizeX, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(groupSizeY, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(groupSizeZ, commandList.cmdListHelper.groupSize[2]);

    EXPECT_EQ(NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(static_cast<uint32_t>(eventCount) / groupSizeX, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    driverHandle->freeMem(alloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsAndInvalidResultSuggestGroupSizeThanUnknownResultReturned, TestPlatforms) {
    class MockQueryKernelTimestampsKernel : public L0::KernelImp {
      public:
        ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY,
                                     uint32_t globalSizeZ, uint32_t *groupSizeX,
                                     uint32_t *groupSizeY, uint32_t *groupSizeZ) override {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
            return;
        }
        void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
            return;
        }
        std::unique_ptr<Kernel> clone() const override { return nullptr; }
    };
    struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {

        using BuiltinFunctionsLibImpl::builtins;
        using BuiltinFunctionsLibImpl::getFunction;
        using BuiltinFunctionsLibImpl::imageBuiltins;
        MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {}
    };
    struct MockBuiltinFunctionsForQueryKernelTimestamps : BuiltinFunctionsLibImpl {
        MockBuiltinFunctionsForQueryKernelTimestamps(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
            tmpMockKernel = new MockQueryKernelTimestampsKernel;
        }
        MockQueryKernelTimestampsKernel *getFunction(Builtin func) override {
            return tmpMockKernel;
        }
        ~MockBuiltinFunctionsForQueryKernelTimestamps() override {
            delete tmpMockKernel;
        }
        MockQueryKernelTimestampsKernel *tmpMockKernel = nullptr;
    };
    class MockDeviceHandle : public L0::DeviceImp {
      public:
        MockDeviceHandle() {
        }
        void initialize(L0::Device *device) {
            neoDevice = device->getNEODevice();
            neoDevice->incRefInternal();
            execEnvironment = device->getExecEnvironment();
            driverHandle = device->getDriverHandle();
            tmpMockBultinLib = new MockBuiltinFunctionsForQueryKernelTimestamps{nullptr, nullptr};
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *getBuiltinFunctionsLib() override {
            return tmpMockBultinLib;
        }
        ~MockDeviceHandle() override {
            delete tmpMockBultinLib;
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *tmpMockBultinLib = nullptr;
    };

    MockDeviceHandle mockDevice;
    mockDevice.initialize(device);

    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;

    commandList.initialize(&mockDevice, NEO::EngineGroupType::RenderCompute);

    MockEvent event;
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    auto result = driverHandle->allocDeviceMem(&mockDevice, ZE_DEVICE_MEM_ALLOC_FLAG_FORCE_UINT32, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    driverHandle->freeMem(alloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsAndInvalidResultSetGroupSizeThanUnknownResultReturned, TestPlatforms) {
    class MockQueryKernelTimestampsKernel : public L0::KernelImp {
      public:
        ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY,
                                     uint32_t globalSizeZ, uint32_t *groupSizeX,
                                     uint32_t *groupSizeY, uint32_t *groupSizeZ) override {
            *groupSizeX = static_cast<uint32_t>(1u);
            *groupSizeY = static_cast<uint32_t>(1u);
            *groupSizeZ = static_cast<uint32_t>(1u);
            return ZE_RESULT_SUCCESS;
        }
        ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY,
                                 uint32_t groupSizeZ) override {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
            return;
        }
        void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
            return;
        }
        std::unique_ptr<Kernel> clone() const override { return nullptr; }
    };
    struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {

        using BuiltinFunctionsLibImpl::builtins;
        using BuiltinFunctionsLibImpl::getFunction;
        using BuiltinFunctionsLibImpl::imageBuiltins;
        MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {}
    };
    struct MockBuiltinFunctionsForQueryKernelTimestamps : BuiltinFunctionsLibImpl {
        MockBuiltinFunctionsForQueryKernelTimestamps(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
            tmpMockKernel = new MockQueryKernelTimestampsKernel;
        }
        MockQueryKernelTimestampsKernel *getFunction(Builtin func) override {
            return tmpMockKernel;
        }
        ~MockBuiltinFunctionsForQueryKernelTimestamps() override {
            delete tmpMockKernel;
        }
        MockQueryKernelTimestampsKernel *tmpMockKernel = nullptr;
    };
    class MockDeviceHandle : public L0::DeviceImp {
      public:
        MockDeviceHandle() {
        }
        void initialize(L0::Device *device) {
            neoDevice = device->getNEODevice();
            neoDevice->incRefInternal();
            execEnvironment = device->getExecEnvironment();
            driverHandle = device->getDriverHandle();
            tmpMockBultinLib = new MockBuiltinFunctionsForQueryKernelTimestamps{nullptr, nullptr};
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *getBuiltinFunctionsLib() override {
            return tmpMockBultinLib;
        }
        ~MockDeviceHandle() override {
            delete tmpMockBultinLib;
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *tmpMockBultinLib = nullptr;
    };

    MockDeviceHandle mockDevice;
    mockDevice.initialize(device);

    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;

    commandList.initialize(&mockDevice, NEO::EngineGroupType::RenderCompute);

    MockEvent event;
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    auto result = driverHandle->allocDeviceMem(&mockDevice, ZE_DEVICE_MEM_ALLOC_FLAG_FORCE_UINT32, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    driverHandle->freeMem(alloc);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenAppendSignalEventThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    commandList->appendSignalEvent(event.toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenAppendSignalEventThePipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    commandList->appendSignalEvent(event.toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenAppendWaitEventsWithDcFlushThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventHandle = event.toHandle();
    commandList->appendWaitOnEvents(1, &eventHandle);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListyWhenAppendWaitEventsWithDcFlushThePipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventHandle = event.toHandle();
    commandList->appendWaitOnEvents(1, &eventHandle);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListyWhenAppendWaitEventsWithDcFlushThePipeControlIsProgrammedOnlyOnce) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event2.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    ze_event_handle_t events[] = {&event, &event2};

    commandList->appendWaitOnEvents(2, events);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    auto itor2 = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor2);
    itor2++;
    auto itor3 = find<PIPE_CONTROL *>(itor2, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor3);
}

using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingBeforeCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    commandList->appendEventForProfilingCopyCommand(event->toHandle(), true);

    auto contextOffset = offsetof(KernelTimestampEvent, contextStart);
    auto globalOffset = offsetof(KernelTimestampEvent, globalStart);
    auto baseAddr = event->getGpuAddress();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalOffset));
    EXPECT_NE(cmdList.end(), ++itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextOffset));
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingAfterCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    commandList->appendEventForProfilingCopyCommand(event->toHandle(), false);

    auto contextOffset = offsetof(KernelTimestampEvent, contextEnd);
    auto globalOffset = offsetof(KernelTimestampEvent, globalEnd);
    auto baseAddr = event->getGpuAddress();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalOffset));
    EXPECT_NE(cmdList.end(), ++itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextOffset));
}

HWTEST2_F(CommandListCreate, givenNullEventWhenAppendEventAfterWalkerThenNothingAddedToStream, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);

    auto usedBefore = commandList->commandContainer.getCommandStream()->getUsed();

    commandList->appendSignalEventPostWalker(nullptr);

    EXPECT_EQ(commandList->commandContainer.getCommandStream()->getUsed(), usedBefore);
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenGettingAllocInRangeThenAllocFromMapReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = ptrOffset(cpuPtr, 0x10);
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_NE(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenSizeIsOutOfRangeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = ptrOffset(cpuPtr, 0x10);
    auto newBufferSize = allocSize + 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsOutOfRangeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = reinterpret_cast<const void *>(gpuAddress - 0x100);
    auto newBufferSize = allocSize - 0x200;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenGetHostPtrAllocCalledThenCorrectOffsetIsSet, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));
    size_t expectedOffset = 0x10;
    auto newBufferPtr = ptrOffset(cpuPtr, expectedOffset);
    auto newBufferSize = allocSize - 0x20;
    size_t offset = 0;
    auto newAlloc = commandList->getHostPtrAlloc(newBufferPtr, newBufferSize, &offset);
    EXPECT_NE(newAlloc, nullptr);
    EXPECT_EQ(offset, expectedOffset);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsInMapThenAllocationReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = cpuPtr;
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, &alloc);
    commandList->hostPtrMap.clear();
}
HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsInMapButWithBiggerSizeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = cpuPtr;
    auto newBufferSize = allocSize + 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}
HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrLowerThanAnyInMapThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = reinterpret_cast<const void *>(gpuAddress - 0x10);
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

} // namespace ult
} // namespace L0
