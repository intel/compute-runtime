/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

TEST(zeCommandListCreateImmediate, redirectsToObject) {
    Mock<Device> device;
    ze_command_queue_desc_t desc = {};
    ze_command_list_handle_t commandList = {};

    EXPECT_CALL(device, createCommandListImmediate(&desc, &commandList))
        .Times(1)
        .WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    auto result = zeCommandListCreateImmediate(device.toHandle(), &desc, &commandList);
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
                                            ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
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
                                            ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
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
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, false));
    ASSERT_NE(nullptr, commandList);

    const uint32_t cmdListImmediateIdds = CommandList::commandListimmediateIddsPerBlock;
    EXPECT_EQ(cmdListImmediateIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListThenItHasImmediateCommandQueueCreated) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};
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

HWTEST_F(CommandListCreate, whenCommandListIsCreatedThenStateBaseAddressCmdIsAddedAndCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

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
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
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
template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandList : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandList() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>(1) {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyKernelWithGA(void *dstPtr,
                                             NEO::GraphicsAllocation *dstPtrAlloc,
                                             uint64_t dstOffset,
                                             void *srcPtr,
                                             NEO::GraphicsAllocation *srcPtrAlloc,
                                             uint64_t srcOffset,
                                             uint32_t size,
                                             uint32_t elementSize,
                                             Builtin builtin) override {
        appendMemoryCopyKernelWithGACalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendMemoryCopyBlit(NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset, uint32_t size) override {
        appendMemoryCopyBlitCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcptr,
                                           NEO::GraphicsAllocation *dstptr,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, Vec3<size_t> copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           size_t srcSize, size_t dstSize) override {
        appendMemoryCopyBlitRegionCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel2d(NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         size_t srcOffset, ze_event_handle_t hSignalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        appendMemoryCopyKernel2dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel3d(NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         uint32_t srcSlicePitch, size_t srcOffset,
                                         ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents) override {
        appendMemoryCopyKernel3dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendBlitFill(void *ptr, const void *pattern,
                               size_t patternSize, size_t size,
                               ze_event_handle_t hEvent) override {
        appendBlitFillCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyKernelWithGACalledTimes = 0;
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
    uint32_t appendMemoryCopyBlitRegionCalledTimes = 0;
    uint32_t appendMemoryCopyKernel2dCalledTimes = 0;
    uint32_t appendMemoryCopyKernel3dCalledTimes = 0;
    uint32_t appendBlitFillCalledTimes = 0;
};

using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyKernelWithGACalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

class MockDriverHandle : public L0::DriverHandleImp {
  public:
    bool findAllocationDataForRange(const void *buffer,
                                    size_t size,
                                    NEO::SvmAllocationData **allocData) override {
        mockAllocation.reset(new NEO::MockGraphicsAllocation(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                             MemoryPool::System4KBPages));
        data.gpuAllocation = mockAllocation.get();
        if (allocData) {
            *allocData = &data;
        }
        return true;
    }
    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
    NEO::SvmAllocationData data = {};
};

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    MockDriverHandle driverHandle;
    device->setDriverHandle(&driverHandle);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListAnd3DWhbufferenMemoryCopyRegionCalledThenCopyKernel3DCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    MockDriverHandle driverHandle;
    device->setDriverHandle(&driverHandle);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel3dCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenTwoNewAllocationAreAddedToHostMapPtr, Platforms) {
    class MockAppendMemoryCopyRegion : public MockCommandList<gfxCoreFamily> {
      public:
        using CommandList::hostPtrMap;
        AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
            return L0::CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocation(device, buffer, bufferSize);
        }
    };

    MockAppendMemoryCopyRegion cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.hostPtrMap.size(), 2u);
}

HWTEST2_F(CommandListCreate, givenCommandListAnd2DWhbufferenMemoryCopyRegionCalledThenCopyKernel2DCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    MockDriverHandle driverHandle;
    device->setDriverHandle(&driverHandle);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel2dCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppendMemoryFillCalledThenAppendBlitFillCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr);
    EXPECT_GT(cmdList.appendBlitFillCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendMemoryFillCalledThenAppendBlitFillNotCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr);
    EXPECT_EQ(cmdList.appendBlitFillCalledTimes, 0u);
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
    event.waitScope = ZE_EVENT_SCOPE_FLAG_NONE;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_NONE;
    commandList->appendSignalEvent(event.toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListyWhenAppendSignalEventThePipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_NONE;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_NONE;
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
    event.signalScope = ZE_EVENT_SCOPE_FLAG_NONE;
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
    event.signalScope = ZE_EVENT_SCOPE_FLAG_NONE;
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
    MockCommandListForMemFill() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>(1) {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlit(NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset, uint32_t size) override {
        appendMemoryCopyBlitCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
};

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
    MockDriverHandle driverHandle;
    device->setDriverHandle(&driverHandle);
    commandList.initialize(device, true);
    uint16_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    commandList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}
} // namespace ult
} // namespace L0
