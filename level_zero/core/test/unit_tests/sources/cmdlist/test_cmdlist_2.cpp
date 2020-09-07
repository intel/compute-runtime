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

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;
template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListHw : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListHw() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

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

    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcptr,
                                           NEO::GraphicsAllocation *dstptr,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, Vec3<size_t> copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           Vec3<uint32_t> srcSize, Vec3<uint32_t> dstSize, ze_event_handle_t hSignalEvent) override {
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
    ze_result_t appendCopyImageBlit(NEO::GraphicsAllocation *src,
                                    NEO::GraphicsAllocation *dst,
                                    Vec3<size_t> srcOffsets, Vec3<size_t> dstOffsets,
                                    size_t srcRowPitch, size_t srcSlicePitch,
                                    size_t dstRowPitch, size_t dstSlicePitch,
                                    size_t bytesPerPixel, Vec3<size_t> copySize,
                                    Vec3<uint32_t> srcSize, Vec3<uint32_t> dstSize, ze_event_handle_t hSignalEvent) override {
        appendCopyImageBlitCalledTimes++;
        appendImageRegionCopySize = copySize;
        appendImageRegionSrcOrigin = srcOffsets;
        appendImageRegionDstOrigin = dstOffsets;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyKernelWithGACalledTimes = 0;
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
    uint32_t appendMemoryCopyBlitRegionCalledTimes = 0;
    uint32_t appendMemoryCopyKernel2dCalledTimes = 0;
    uint32_t appendMemoryCopyKernel3dCalledTimes = 0;
    uint32_t appendBlitFillCalledTimes = 0;
    uint32_t appendCopyImageBlitCalledTimes = 0;
    Vec3<size_t> appendImageRegionCopySize = {0, 0, 0};
    Vec3<size_t> appendImageRegionSrcOrigin = {9, 9, 9};
    Vec3<size_t> appendImageRegionDstOrigin = {9, 9, 9};
};

using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyKernelWithGACalled, Platforms) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled, Platforms) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled, Platforms) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListAnd3DWhbufferenMemoryCopyRegionCalledThenCopyKernel3DCalled, Platforms) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel3dCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendWriteGlobalTimestampCalledThenPipeControlWithTimestampWriteEncoded, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto &commandContainer = commandList->commandContainer;

    uint64_t timestampAddress = 0x12345678555500;
    uint32_t timestampAddressLow = (uint32_t)(timestampAddress & 0xFFFFFFFF);
    uint32_t timestampAddressHigh = (uint32_t)(timestampAddress >> 32);
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

    const auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto iterator = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto cmd = genCmdCast<PIPE_CONTROL *>(*iterator);
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
    EXPECT_EQ(cmd->getAddressHigh(), timestampAddressHigh);
    EXPECT_EQ(cmd->getAddress(), timestampAddressLow);
    EXPECT_EQ(POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP, cmd->getPostSyncOperation());
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenAppendWriteGlobalTimestampReturnsSuccess, Platforms) {
    Mock<CommandQueue> cmdQueue;
    uint64_t timestampAddress = 0x12345678555500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    bool ret = commandList->initialize(device, false);
    ASSERT_TRUE(ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    EXPECT_CALL(cmdQueue, executeCommandLists).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(cmdQueue, synchronize).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    auto result = commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->cmdQImmediate = nullptr;
}

using AppendMemoryCopy = CommandListCreate;

template <GFXCORE_FAMILY gfxCoreFamily>
class MockAppendMemoryCopy : public MockCommandListHw<gfxCoreFamily> {
  public:
    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return L0::CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocation(device, buffer, bufferSize);
    }
};

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenTwoNewAllocationAreAddedToHostMapPtr, Platforms) {
    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.hostPtrMap.size(), 2u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenPipeControlWithDcFlushAdded, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);

    auto &commandContainer = cmdList.commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    PIPE_CONTROL *cmd = nullptr;
    while (itor != genCmdList.end()) {
        cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());
    }
    EXPECT_TRUE(cmd->getDcFlushEnable());
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAdded, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr);

    auto &commandContainer = cmdList.commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    PIPE_CONTROL *cmd = nullptr;
    while (itor != genCmdList.end()) {
        cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());
    }
    EXPECT_TRUE(cmd->getDcFlushEnable());
}

HWTEST2_F(CommandListCreate, givenCommandListAnd2DWhbufferenMemoryCopyRegionCalledThenCopyKernel2DCalled, Platforms) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel2dCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppendMemoryFillCalledThenAppendBlitFillCalled, Platforms) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr);
    EXPECT_GT(cmdList.appendBlitFillCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendMemoryFillCalledThenAppendBlitFillNotCalled, Platforms) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr);
    EXPECT_EQ(cmdList.appendBlitFillCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr);
    EXPECT_GT(commandList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_EQ(commandList.appendMemoryCopyBlitCalledTimes, 0u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);

    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
}

using ImageSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyFromMemoryToImageThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, &dstRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhenImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhenImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen1DImageCopyFromMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_1D;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen1DImageCopyToMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_1D;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen1DArrayImageCopyFromMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_1DARRAY;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen1DArrayImageCopyToMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_1DARRAY;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyToMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    zeDesc.depth = 9;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyFromMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    zeDesc.depth = 9;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen3DImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen3DImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyFromImageToMemoryThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), &srcRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyFromImageToImageThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    ze_image_desc_t zeDesc = {};
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
}

using BlitBlockCopyPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionWithinMaxBlitSizeThenOneBlitCommandHasBeenSpown, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint32_t offsetX = 0x10;
    uint32_t offsetY = 0x10;
    Vec3<size_t> copySize = {0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<uint32_t> srcSize = {0x1000, 0x100, 1};
    Vec3<uint32_t> dstSize = {0x100, 0x100, 1};
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionWithinMaxBlitSizeThenOffsetAndSizeAreInPixels, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint32_t offsetX = 0x10;
    uint32_t offsetY = 0x10;
    Vec3<size_t> copySize = {0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<uint32_t> srcSize = {0x1000, 0x100, 1};
    Vec3<uint32_t> dstSize = {0x100, 0x100, 1};
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize.x, srcRegion.originX, dstRegion.originY, srcSize.x, dstSize.x);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    auto cmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getDestinationX1CoordinateLeft(), offsetX / bytesPerPixel);
    EXPECT_EQ(cmd->getTransferWidth(), (offsetX + static_cast<uint32_t>(copySize.x)) / bytesPerPixel);
}
HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionGreaterThanMaxBlitSizeThenMoreThanOneBlitCommandHasBeenSpown, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint32_t offsetX = 0x1;
    uint32_t offsetY = 0x1;
    Vec3<size_t> copySize = {BlitterConstants::maxBlitWidth + 0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<uint32_t> srcSize = {2 * BlitterConstants::maxBlitWidth, 2 * BlitterConstants::maxBlitHeight, 1};
    Vec3<uint32_t> dstSize = srcSize;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForRegionSize : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListForRegionSize() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcptr,
                                           NEO::GraphicsAllocation *dstptr,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, Vec3<size_t> copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           Vec3<uint32_t> srcSize, Vec3<uint32_t> dstSize, ze_event_handle_t hSignalEvent) override {
        this->srcSize = srcSize;
        this->dstSize = dstSize;
        return ZE_RESULT_SUCCESS;
    }
    Vec3<uint32_t> srcSize = {0, 0, 0};
    Vec3<uint32_t> dstSize = {0, 0, 0};
};
HWTEST2_F(CommandListCreate, givenZeroAsPitchAndSlicePitchWhenMemoryCopyRegionCalledSizesEqualOffsetPlusCopySize, Platforms) {
    MockCommandListForRegionSize<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {0x10, 0x10, 0, 0x100, 0x100, 1};
    ze_copy_region_t srcRegion = dstRegion;
    uint32_t pitch = 0;
    uint32_t slicePitch = 0;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, pitch, slicePitch, srcPtr, &srcRegion, pitch, slicePitch, nullptr);
    EXPECT_EQ(cmdList.dstSize.x, dstRegion.width + dstRegion.originX);
    EXPECT_EQ(cmdList.dstSize.y, dstRegion.height + dstRegion.originY);
    EXPECT_EQ(cmdList.dstSize.z, dstRegion.depth + dstRegion.originZ);

    EXPECT_EQ(cmdList.srcSize.x, srcRegion.width + srcRegion.originX);
    EXPECT_EQ(cmdList.srcSize.y, srcRegion.height + srcRegion.originY);
    EXPECT_EQ(cmdList.srcSize.z, srcRegion.depth + srcRegion.originZ);
}

HWTEST2_F(CommandListCreate, givenPitchAndSlicePitchWhenMemoryCopyRegionCalledSizesAreBasedOnPitch, Platforms) {
    MockCommandListForRegionSize<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {0x10, 0x10, 0, 0x100, 0x100, 1};
    ze_copy_region_t srcRegion = dstRegion;
    uint32_t pitch = 0x1000;
    uint32_t slicePitch = 0x100000;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, pitch, slicePitch, srcPtr, &srcRegion, pitch, slicePitch, nullptr);
    EXPECT_EQ(cmdList.dstSize.x, pitch);
    EXPECT_EQ(cmdList.dstSize.y, slicePitch / pitch);

    EXPECT_EQ(cmdList.srcSize.x, pitch);
    EXPECT_EQ(cmdList.srcSize.y, slicePitch / pitch);
}
template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForMemFill : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListForMemFill() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlit(NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset, uint32_t size, ze_event_handle_t hSignalEvent) override {
        appendMemoryCopyBlitCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
};

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr);
    EXPECT_GT(commandList.appendMemoryCopyBlitCalledTimes, 1u);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);

    itor = find<MI_FLUSH_DW *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

} // namespace ult
} // namespace L0
