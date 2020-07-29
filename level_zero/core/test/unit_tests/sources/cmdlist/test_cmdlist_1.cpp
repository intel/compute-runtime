/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

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

TEST_F(CommandListCreate, whenCommandListIsCreatedThenItIsInitialized) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    const uint32_t defaultNumIdds = CommandList::defaultNumIddsPerBlock;
    EXPECT_EQ(defaultNumIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemAdviseReturnsError) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemAdvise(device, nullptr, 0, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemoryPrefetchReturnsError) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
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

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
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

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = driverHandle->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenImmediateCommandListThenCustomNumIddPerBlockUsed) {
    const ze_command_queue_desc_t desc = {};

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, false));
    ASSERT_NE(nullptr, commandList);

    const uint32_t cmdListImmediateIdds = CommandList::commandListimmediateIddsPerBlock;
    EXPECT_EQ(cmdListImmediateIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListThenItHasImmediateCommandQueueCreated) {
    const ze_command_queue_desc_t desc = {};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, false));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
}

TEST_F(CommandListCreate, givenInvalidProductFamilyThenReturnsNullPointer) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(IGFX_UNKNOWN, device, false));
    EXPECT_EQ(nullptr, commandList);
}

HWTEST_F(CommandListCreate, whenCommandListIsCreatedThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
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

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto &commandContainer = commandList->commandContainer;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenSetBarrierThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenSetBarrierThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}
using AppendMemoryCopy = CommandListCreate;
using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(AppendMemoryCopy, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr);

    auto &commandContainer = commandList->commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());

    EXPECT_EQ(genCmdList.end(), itor);
}

HWTEST2_F(AppendMemoryCopy, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyRegionCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);

    auto &commandContainer = commandList->commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());

    EXPECT_EQ(genCmdList.end(), itor);
}

using AppendMemoryCopyFromContext = CommandListCreate;
using Platform = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(AppendMemoryCopyFromContext, givenCommandListThenUpOnPerformingAppendMemoryCopyFromContextSuccessIsReturned, Platform) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
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

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenAppendSignalEventThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
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

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForMemFill : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListForMemFill() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                     NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset, uintptr_t srcPtr,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset,
                                     uint32_t size,
                                     ze_event_handle_t hSignalEvent) override {
        appendMemoryCopyBlitCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
};
class MockDriverHandle : public L0::DriverHandleImp {
  public:
    bool findAllocationDataForRange(const void *buffer,
                                    size_t size,
                                    NEO::SvmAllocationData **allocData) override {
        mockAllocation.reset(new NEO::MockGraphicsAllocation(rootDeviceIndex, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                             MemoryPool::System4KBPages));
        data.gpuAllocations.addAllocation(mockAllocation.get());
        if (allocData) {
            *allocData = &data;
        }
        return true;
    }
    const uint32_t rootDeviceIndex = 0u;
    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
    NEO::SvmAllocationData data{rootDeviceIndex};
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForMemFillHostPtr : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListForMemFillHostPtr() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return L0::CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocation(device, buffer, bufferSize);
    }
};

struct AppendMemoryFillFixture {
    class MockDriverHandleHostPtr : public L0::DriverHandleImp {
      public:
        bool findAllocationDataForRange(const void *buffer,
                                        size_t size,
                                        NEO::SvmAllocationData **allocData) override {
            if (buffer == reinterpret_cast<void *>(registeredGraphicsAllocationAddress)) {
                mockAllocation.reset(new NEO::MockGraphicsAllocation(rootDeviceIndex, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                                     reinterpret_cast<void *>(registeredGraphicsAllocationAddress), 0x1000, 0, sizeof(uint32_t),
                                                                     MemoryPool::System4KBPages));
                data.gpuAllocations.addAllocation(mockAllocation.get());
                if (allocData) {
                    *allocData = &data;
                }
                return true;
            }
            return false;
        }
        const uint32_t rootDeviceIndex = 0u;
        std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
        NEO::SvmAllocationData data{rootDeviceIndex};
    };
    class MockKernelForMemFill : public L0::KernelImp {
      public:
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

    struct MockBuiltinFunctionsForMemFill : BuiltinFunctionsLibImpl {
        MockBuiltinFunctionsForMemFill(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
            tmpMockKernel = new MockKernelForMemFill;
        }
        MockKernelForMemFill *getFunction(Builtin func) override {
            return tmpMockKernel;
        }
        ~MockBuiltinFunctionsForMemFill() override {
            delete tmpMockKernel;
        }
        MockKernelForMemFill *tmpMockKernel = nullptr;
    };
    class MockDeviceHandle : public L0::DeviceImp {
      public:
        MockDeviceHandle() {
            tmpMockBultinLib = new MockBuiltinFunctionsForMemFill{nullptr, nullptr};
        }
        MockBuiltinFunctionsForMemFill *getBuiltinFunctionsLib() override {
            return tmpMockBultinLib;
        }
        ~MockDeviceHandle() override {
            delete tmpMockBultinLib;
        }
        MockBuiltinFunctionsForMemFill *tmpMockBultinLib = nullptr;
    };
    virtual void SetUp() { // NOLINT(readability-identifier-naming)
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        deviceMock = std::make_unique<MockDeviceHandle>();
        driverHandle->initialize(std::move(devices));
        neoMockDevice->incRefInternal();
        deviceMock.get()->neoDevice = neoMockDevice;
    }

    virtual void TearDown() { // NOLINT(readability-identifier-naming)
    }
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::unique_ptr<MockDeviceHandle> deviceMock;
    NEO::MockDevice *neoDevice = nullptr;
    NEO::MockDevice *neoMockDevice = nullptr;
    static constexpr uint64_t registeredGraphicsAllocationAddress = 0x1234;
};

using AppendMemoryfillHostPtr = Test<AppendMemoryFillFixture>;

HWTEST2_F(AppendMemoryfillHostPtr, givenTwoCommandListsAndHostPointerUsedInBothWhenMemoryfillCalledThenNewUniqueAllocationIsAddedtoHostPtrMap, Platforms) {
    MockCommandListForMemFillHostPtr<gfxCoreFamily> cmdListFirst;
    MockCommandListForMemFillHostPtr<gfxCoreFamily> cmdListSecond;
    MockDriverHandleHostPtr driverHandleMock;
    deviceMock.get()->setDriverHandle(&driverHandleMock);
    cmdListFirst.initialize(deviceMock.get(), false);
    cmdListSecond.initialize(deviceMock.get(), false);
    uint64_t pattern[4] = {1, 2, 3, 4};
    void *ptr = reinterpret_cast<void *>(registeredGraphicsAllocationAddress);
    cmdListFirst.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    cmdListSecond.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    EXPECT_EQ(cmdListFirst.hostPtrMap.size(), 1u);
    EXPECT_EQ(cmdListSecond.hostPtrMap.size(), 1u);
    auto allocationFirstList = cmdListFirst.hostPtrMap.begin()->second;
    auto allocationSecondList = cmdListSecond.hostPtrMap.begin()->second;
    EXPECT_NE(allocationFirstList, allocationSecondList);
    deviceMock.get()->setDriverHandle(driverHandle.get());
}

HWTEST2_F(AppendMemoryfillHostPtr, givenCommandListAndHostPointerWhenMemoryfillCalledThenNewAllocationisAddedToHostPtrMap, Platforms) {
    MockCommandListForMemFillHostPtr<gfxCoreFamily> cmdList;
    MockDriverHandleHostPtr driverHandleMock;
    deviceMock.get()->setDriverHandle(&driverHandleMock);
    cmdList.initialize(deviceMock.get(), false);
    uint64_t pattern[4] = {1, 2, 3, 4};
    void *ptr = reinterpret_cast<void *>(registeredGraphicsAllocationAddress);
    cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    EXPECT_EQ(cmdList.hostPtrMap.size(), 1u);
    deviceMock.get()->setDriverHandle(driverHandle.get());
}

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppenBlitFillCalledWithLargePatternSizeThenMemCopyWasCalled, Platforms) {
    MockCommandListForMemFill<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    uint64_t pattern[4] = {1, 2, 3, 4};
    void *ptr = reinterpret_cast<void *>(0x1234);
    cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppenBlitFillToNotDeviceMemThenInvalidArgumentReturned, Platforms) {
    MockCommandListForMemFill<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    uint8_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    auto ret = cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppenBlitFillThenCopyBltIsProgrammed, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    MockCommandListForMemFill<gfxCoreFamily> commandList;
    MockDriverHandle driverHandleMock;
    device->setDriverHandle(&driverHandleMock);
    commandList.initialize(device, true);
    uint16_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    commandList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    device->setDriverHandle(driverHandle.get());
}

using ImageSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyFromImagBlitThenCommandAddedToStream, ImageSupport) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    ze_image_desc_t zeDesc = {};
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    commandList->appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, nullptr, nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenTimestampPassedToMemoryCopyRegionBlitThenTimeStampRegistersAreAdded, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);

    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, srcRegion, dstRegion, {0, 0, 0}, 0, 0, 0, 0, 0, 0, event->toHandle());
    GenCmdList cmdList;

    auto baseAddr = event->getGpuAddress();
    auto contextStartOffset = offsetof(KernelTimestampEvent, contextStart);
    auto globalStartOffset = offsetof(KernelTimestampEvent, globalStart);
    auto contextEndOffset = offsetof(KernelTimestampEvent, contextEnd);
    auto globalEndOffset = offsetof(KernelTimestampEvent, globalEnd);

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalStartOffset));
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextStartOffset));
    itor++;
    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalEndOffset));
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextEndOffset));
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenTimestampPassedToImageCopyBlitThenTimeStampRegistersAreAdded, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);

    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 0, 0, 0, 0, 1, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, event->toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingBeforeCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
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
    commandList->initialize(device, true);
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
    commandList->initialize(device, true);

    auto usedBefore = commandList->commandContainer.getCommandStream()->getUsed();

    commandList->appendSignalEventPostWalker(nullptr);

    EXPECT_EQ(commandList->commandContainer.getCommandStream()->getUsed(), usedBefore);
}
HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppendBlitFillCalledWithLargePatternSizeThenInternalAllocHasPattern, Platforms) {
    MockCommandListForMemFill<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    uint64_t pattern[4] = {1, 2, 3, 4};
    void *ptr = reinterpret_cast<void *>(0x1234);
    uint32_t fillElements = 0x101;
    auto size = fillElements * sizeof(uint64_t);
    cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), size, nullptr);
    auto internalAlloc = cmdList.commandContainer.getDeallocationContainer()[0];
    for (uint32_t i = 0; i < fillElements; i++) {
        auto allocValue = reinterpret_cast<uint64_t *>(internalAlloc->getUnderlyingBuffer())[i];
        EXPECT_EQ(allocValue, pattern[i % 4]);
    }
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenGettingAllocInRangeThenAllocFromMapReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
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
    commandList->initialize(device, true);
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
    commandList->initialize(device, true);
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
    commandList->initialize(device, true);
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
    commandList->initialize(device, true);
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
    commandList->initialize(device, true);
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
    commandList->initialize(device, true);
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

HWTEST2_F(AppendMemoryCopy, givenCopyOnlyCommandListWhenWithDcFlushAddedIsNotAddedAfterBlitCopy, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uintptr_t srcPtr = 0x5001;
    uintptr_t dstPtr = 0x7001;
    uint64_t srcOffset = 0x101;
    uint64_t dstOffset = 0x201;
    uint32_t copySize = 0x301;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(srcPtr), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(dstPtr), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    commandList->appendMemoryCopyBlit(ptrOffset(dstPtr, dstOffset), &mockAllocationDst, 0, ptrOffset(srcPtr, srcOffset), &mockAllocationSrc, 0, copySize, nullptr);

    auto &commandContainer = commandList->commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    auto cmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getDestinationBaseAddress(), ptrOffset(dstPtr, dstOffset));
    EXPECT_EQ(cmd->getSourceBaseAddress(), ptrOffset(srcPtr, srcOffset));
}
} // namespace ult
} // namespace L0
