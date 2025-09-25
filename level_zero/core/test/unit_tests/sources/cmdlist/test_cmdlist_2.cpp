/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

#include "test_traits_common.h"

namespace L0 {

namespace ult {

struct CommandListAppend : Test<CommandListFixture> {
    CmdListMemoryCopyParams copyParams = {};
};

using CommandListCreateTests = Test<CommandListCreateFixture>;

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListHw : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListHw() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}
    MockCommandListHw(bool failOnFirst) : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>(), failOnFirstCopy(failOnFirst) {}

    AlignedAllocationData getAlignedAllocationData(L0::Device *device, bool sharedSystemEnabled, const void *buffer, uint64_t bufferSize, bool allowHostCopy, bool copyOffload) override {
        getAlignedAllocationCalledTimes++;
        if (buffer && returnMockAllocationStruct) {
            auto alignedPtr = reinterpret_cast<uintptr_t>(alignDown(buffer, sizeof(uint32_t)));
            auto offset = reinterpret_cast<uintptr_t>(buffer) - alignedPtr;
            return {alignedPtr, offset, &alignedAlloc, true};
        }
        if (buffer && !failAlignedAlloc) {
            return {0, 0, &alignedAlloc, true};
        }
        return {0, 0, nullptr, false};
    }

    ze_result_t appendMemoryCopyKernelWithGA(uintptr_t dstPtr,
                                             NEO::GraphicsAllocation *dstPtrAlloc,
                                             uint64_t dstOffset,
                                             uintptr_t srcPtr,
                                             NEO::GraphicsAllocation *srcPtrAlloc,
                                             uint64_t srcOffset,
                                             uint64_t size,
                                             uint64_t elementSize,
                                             Builtin builtin,
                                             L0::Event *signalEvent,
                                             bool isStateless,
                                             CmdListKernelLaunchParams &launchParams) override {
        appendMemoryCopyKernelWithGACalledTimes++;
        if (isStateless) {
            appendMemoryCopyKernelWithGAStatelessCalledTimes++;
        }
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        if (failOnFirstCopy &&
            (appendMemoryCopyKernelWithGACalledTimes == 1 || appendMemoryCopyKernelWithGAStatelessCalledTimes == 1)) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                     NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset, uintptr_t srcPtr,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset,
                                     uint64_t size, Event *signalEvent, CmdListMemoryCopyParams &memoryCopyParams) override {
        appendMemoryCopyBlitCalledTimes++;
        if (failOnFirstCopy && appendMemoryCopyBlitCalledTimes == 1) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyBlitRegion(AlignedAllocationData *srcAllocationData,
                                           AlignedAllocationData *dstAllocationData,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                           L0::Event *signalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams, bool doubleStreamCopyOffload) override {
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        appendMemoryCopyBlitRegionCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    void setAdditionalBlitProperties(NEO::BlitProperties &blitProperties, Event *signalEvent, uint64_t forceAggregatedEventIncValue, bool useAdditionalTimestamp) override {}
    ze_result_t appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         size_t srcOffset, L0::Event *signalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                         bool relaxedOrderingDispatch, const bool isStateless) override {
        appendMemoryCopyKernel2dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         uint32_t srcSlicePitch, size_t srcOffset,
                                         L0::Event *signalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch, const bool isStateless) override {
        appendMemoryCopyKernel3dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendBlitFill(void *ptr, const void *pattern,
                               size_t patternSize, size_t size,
                               L0::Event *signalEvent, uint32_t numWaitEvents,
                               ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override {
        appendBlitFillCalledTimes++;
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendCopyImageBlit(uintptr_t srcPtr, NEO::GraphicsAllocation *src,
                                    uintptr_t dstPtr, NEO::GraphicsAllocation *dst,
                                    const Vec3<size_t> &srcOffsets, const Vec3<size_t> &dstOffsets,
                                    size_t srcRowPitch, size_t srcSlicePitch,
                                    size_t dstRowPitch, size_t dstSlicePitch,
                                    size_t bytesPerPixel, const Vec3<size_t> &copySize,
                                    const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                    L0::Event *signalEvent, uint32_t numWaitEvents,
                                    ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override {
        appendCopyImageBlitCalledTimes++;
        appendCopyImageSrcRowPitch = srcRowPitch;
        appendCopyImageSrcSlicePitch = srcSlicePitch;
        appendCopyImageDstRowPitch = dstRowPitch;
        appendCopyImageDstSlicePitch = dstSlicePitch;
        appendImageRegionCopySize = copySize;
        appendImageRegionSrcOrigin = srcOffsets;
        appendImageRegionDstOrigin = dstOffsets;
        appendCopyImageSrcSize = srcSize;
        appendCopyImageDstSize = dstSize;
        appendSrcAlloc = src;
        appendDstAlloc = dst;
        appendSrcPtr = srcPtr;
        appendDstPtr = dstPtr;
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        return ZE_RESULT_SUCCESS;
    }
    uint8_t mockAlignedAllocData[2 * MemoryConstants::pageSize]{};

    size_t appendCopyImageSrcRowPitch = 0;
    size_t appendCopyImageSrcSlicePitch = 0;
    size_t appendCopyImageDstRowPitch = 0;
    size_t appendCopyImageDstSlicePitch = 0;
    Vec3<size_t> appendImageRegionCopySize = {0, 0, 0};
    Vec3<size_t> appendImageRegionSrcOrigin = {9, 9, 9};
    Vec3<size_t> appendImageRegionDstOrigin = {9, 9, 9};
    Vec3<size_t> appendCopyImageSrcSize{0, 0, 0};
    Vec3<size_t> appendCopyImageDstSize{0, 0, 0};
    uintptr_t appendSrcPtr;
    uintptr_t appendDstPtr;
    NEO::GraphicsAllocation *appendSrcAlloc;
    NEO::GraphicsAllocation *appendDstAlloc;

    void *alignedDataPtr = alignUp(mockAlignedAllocData, MemoryConstants::pageSize);

    NEO::MockGraphicsAllocation alignedAlloc{alignedDataPtr, reinterpret_cast<uint64_t>(alignedDataPtr), MemoryConstants::pageSize};

    uint32_t appendMemoryCopyKernelWithGACalledTimes = 0;
    uint32_t appendMemoryCopyKernelWithGAStatelessCalledTimes = 0;
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
    uint32_t appendMemoryCopyBlitRegionCalledTimes = 0;
    uint32_t appendMemoryCopyKernel2dCalledTimes = 0;
    uint32_t appendMemoryCopyKernel3dCalledTimes = 0;
    uint32_t appendBlitFillCalledTimes = 0;
    uint32_t appendCopyImageBlitCalledTimes = 0;
    uint32_t getAlignedAllocationCalledTimes = 0;
    bool failOnFirstCopy = false;
    bool useEvents = false;
    bool failAlignedAlloc = false;
    bool returnMockAllocationStruct = false;
};

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledWithNullDstPtrThenAppendMemoryCopyWithappendMemoryCopyReturnsError) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = nullptr;
    ze_result_t ret = cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledWithNullSrcPtrThenAppendMemoryCopyWithappendMemoryCopyReturnsError) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = nullptr;
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_result_t ret = cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledWithNullSrcPtrAndDstPtrThenAppendMemoryCopyWithappendMemoryCopyReturnsError) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = nullptr;
    void *dstPtr = nullptr;
    ze_result_t ret = cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryCopyRegionCalledWithNullSrcPtrAndDstPtrThenAppendMemoryCopyRegionReturnsError) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = nullptr;
    void *dstPtr = nullptr;
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    ze_result_t ret = cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryCopyRegionCalledWithNullSrcPtrThenAppendMemoryCopyRegionReturnsError) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = nullptr;
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    ze_result_t ret = cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryCopyRegionCalledWithNullDstPtrThenAppendMemoryCopyRegionReturnsError) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x2345);
    void *dstPtr = nullptr;
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    ze_result_t ret = cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryFillCalledWithNullDstPtrThenAppendMemoryFillReturnsError) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.failAlignedAlloc = true;
    auto result = driverHandle->importExternalPointer(dstPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_result_t ret = cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
    result = driverHandle->releaseImportedPointer(dstPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListAppend, givenCommandListWhenQueryKernelTimestampsCalledWithNullDstPtrThenAppendQueryKernelTimestampsReturnsError) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *dstPtr = nullptr;
    ze_event_handle_t eventHandle = {};
    ze_result_t ret = cmdList.appendQueryKernelTimestamps(1u, &eventHandle, dstPtr, nullptr, nullptr, 1, nullptr);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithStatelessKernelIsCalled, IsAtLeastXeHpcCore) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, cmdList.appendMemoryCopyKernelWithGACalledTimes);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithoutStatelessKernelIsCalled, IsAtMostXeHpgCore) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST_F(CommandListAppend, givenCommandListWhen4GByteMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyKernelWithGAStatelessCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x100001234);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x100000000, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST_F(CommandListAppend, givenCommandListWhenMemoryCopyRegionCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
}

HWTEST_F(CommandListCreateTests, givenCommandListWhenPageFaultCopyCalledThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    size_t size = (sizeof(uint32_t) * 4);
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);

    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 1u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, device->getNEODevice()->getCompilerProductHelper().isForceToStatelessRequired() ? 1u : 0u);
}

HWTEST_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledWithCopyEngineThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    size_t size = (sizeof(uint32_t) * 4);
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 1u);
}

HWTEST_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalledForMiddleAndRightSizesAreCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    size_t size = ((sizeof(uint32_t) * 4) + 1);
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 2u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, device->getNEODevice()->getCompilerProductHelper().isForceToStatelessRequired() ? 2u : 0u);
}

HWTEST_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledAndErrorOnMidCopyThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalledForMiddleIsCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList(true);
    size_t size = ((sizeof(uint32_t) * 4) + 1);
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 1u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, device->getNEODevice()->getCompilerProductHelper().isForceToStatelessRequired() ? 1u : 0u);
}

HWTEST_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledWithCopyEngineThenappendPageFaultCopyWithappendMemoryCopyCalledOnlyOnce) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    size_t size = ((sizeof(uint32_t) * 4) + 1);
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 1u);
}

HWTEST_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledWithCopyEngineAndErrorOnMidOperationThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalledForMiddleIsCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList(true);
    size_t size = ((sizeof(uint32_t) * 4) + 1);
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 1u);
}

HWTEST_F(CommandListAppend, givenCommandListWhen4GBytePageFaultCopyCalledThenPageFaultCopyWithappendMemoryCopyKernelWithGAStatelessCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    size_t size = 0x100000000;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 1u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 1u);
}

HWTEST_F(CommandListAppend, givenCommandListWhen4GBytePageFaultCopyCalledThenPageFaultCopyWithappendMemoryCopyKernelWithGAStatelessCalledForMiddleAndRightSizesAreCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    size_t size = 0x100000001;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 2u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 2u);
}

HWTEST_F(CommandListAppend, givenCommandListAnd3dBufferWhenMemoryCopyRegionCalledThenCopyKernel3DCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel3dCalledTimes, 0u);
}

HWTEST_F(CommandListAppend, givenCommandListAnd2dBufferWhenMemoryCopyRegionCalledThenCopyKernel2DCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel2dCalledTimes, 0u);
}

HWTEST_F(CommandListAppend, givenCopyOnlyCommandListWhenAppendMemoryFillCalledThenAppendBlitFillCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendBlitFillCalledTimes, 0u);
}

HWTEST_F(CommandListAppend, givenCommandListWhenAppendMemoryFillCalledThenAppendBlitFillNotCalled) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendBlitFillCalledTimes, 0u);
}

HWTEST_F(CommandListCreateTests, givenCommandListWhenMemoryCopyWithSignalEventsThenSemaphoreWaitAndPipeControlAreFound) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    auto &commandContainer = commandList->getCmdContainer();

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
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

    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 2, events.data(), copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 2, events.data(), copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<SEMAPHORE_WAIT *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<SEMAPHORE_WAIT *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<SEMAPHORE_WAIT *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreateTests, givenCommandListWhenMemoryCopyWithSignalEventScopeSetToDeviceThenSinglePipeControlIsAddedWithDcFlush, IsTGLLP) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    auto &commandContainer = commandList->getCmdContainer();

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, event.get(), 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto iterator = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    uint32_t postSyncFound = 0;
    ASSERT_NE(0u, iterator.size());
    for (auto it : iterator) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if ((cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) &&
            (cmd->getImmediateData() == Event::STATE_SIGNALED) &&
            (cmd->getDcFlushEnable())) {
            postSyncFound++;
        }
    }

    EXPECT_EQ(1u, postSyncFound);
}

HWTEST2_F(CommandListCreateTests, givenCommandListWhenMemoryCopyWithSignalEventScopeSetToSubDeviceThenB2BPipeControlIsAddedWithDcFlushForLastPC, IsTGLLP) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    auto &commandContainer = commandList->getCmdContainer();

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_SUBDEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, event.get(), 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto iterator = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    uint32_t postSyncFound = 0;
    ASSERT_NE(0u, iterator.size());
    for (auto it : iterator) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if ((cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) &&
            (cmd->getImmediateData() == Event::STATE_SIGNALED) &&
            (cmd->getDcFlushEnable())) {
            postSyncFound++;
        }
    }

    EXPECT_EQ(1u, postSyncFound);
}

using ImageSupport = IsGen12LP;

HWTEST2_F(CommandListAppend, givenCommandListWhenAppendImageCopyFromMemoryCalledWithNullSrcPtrThenAppendImageCopyFromMemoryReturnsError, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *srcPtr = nullptr;
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};

    ze_result_t ret = cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, &dstRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_INVALID_NULL_POINTER);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenAppendImageCopyToMemoryCalledWithNullDstPtrThenAppendImageCopyToMemoryReturnsError, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *dstPtr = nullptr;
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);
    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_result_t ret = cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), &srcRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_INVALID_NULL_POINTER);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListWhenCopyFromMemoryToImageThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, &dstRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
    EXPECT_FALSE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhenImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
    EXPECT_TRUE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenImmediateCommandListWhenImageCopyFromOrToMemoryWithExternalHostPtrThenRequireTaskCountUpdate, ImageSupport) {
    ze_command_queue_desc_t desc = {};
    ze_result_t ret = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::ult::CommandList> cmdList(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, ret)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    void *hostPtr = reinterpret_cast<void *>(0x1234);
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.width = 1;
    zeDesc.height = 1;
    zeDesc.depth = 1;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;
    cmdList->appendImageCopyFromMemory(imageHW->toHandle(), hostPtr, nullptr, nullptr, 0, nullptr, copyParams);
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList->getCsr(false));
    if (L0GfxCoreHelper::useImmediateComputeFlushTask(device->getNEODevice()->getRootDeviceEnvironment())) {
        ImmediateDispatchFlags &recordedImmediateDispatchFlags = ultCsr->recordedImmediateDispatchFlags;
        EXPECT_TRUE(recordedImmediateDispatchFlags.requireTaskCountUpdate);
    } else {
        DispatchFlags &recordedDispatchFlags = ultCsr->recordedDispatchFlags;
        EXPECT_TRUE(recordedDispatchFlags.guardCommandBufferWithPipeControl);
    }

    cmdList->appendImageCopyToMemory(hostPtr, imageHW->toHandle(), nullptr, nullptr, 0u, nullptr, copyParams);
    if (L0GfxCoreHelper::useImmediateComputeFlushTask(device->getNEODevice()->getRootDeviceEnvironment())) {
        ImmediateDispatchFlags &recordedImmediateDispatchFlags = ultCsr->recordedImmediateDispatchFlags;
        EXPECT_TRUE(recordedImmediateDispatchFlags.requireTaskCountUpdate);
    } else {
        DispatchFlags &recordedDispatchFlags = ultCsr->recordedDispatchFlags;
        EXPECT_TRUE(recordedDispatchFlags.guardCommandBufferWithPipeControl);
    }
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhenImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
    EXPECT_FALSE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen1DImageCopyFromMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_1D;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen1DImageCopyToMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_1D;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
    EXPECT_TRUE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen1DArrayImageCopyFromMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_1DARRAY;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    zeDesc.arraylevels = 7;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.arraylevels, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen1DArrayImageCopyToMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_1DARRAY;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    zeDesc.arraylevels = 7;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.arraylevels, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyToMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    zeDesc.depth = 9;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyFromMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    zeDesc.depth = 9;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DArrayImageCopyFromMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.height = 6;
    zeDesc.depth = 9;
    zeDesc.arraylevels = 7;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.arraylevels};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DArrayImageCopyToMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.height = 6;
    zeDesc.depth = 9;
    zeDesc.arraylevels = 7;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.arraylevels};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen3DImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen3DImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST_F(CommandListAppend, givenCopyCommandListWhen1DArrayImageCopyRegionThenAppendCopyImageBlitCalledWithCorrectSizes) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_1DARRAY,
                              {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                               ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                              20,
                              1,
                              1,
                              4,
                              0};
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 0, 0, 4, 1, 1};
    ze_image_region_t dstRegion = srcRegion;
    srcRegion.originX = 8;
    cmdList.appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(cmdList.appendCopyImageSrcRowPitch, imageHWSrc->getImageInfo().rowPitch);
    EXPECT_EQ(cmdList.appendCopyImageSrcSlicePitch, srcRegion.height * imageHWSrc->getImageInfo().rowPitch);
    EXPECT_EQ(cmdList.appendCopyImageDstRowPitch, imageHWDst->getImageInfo().rowPitch);
    EXPECT_EQ(cmdList.appendCopyImageDstSlicePitch, dstRegion.height * imageHWDst->getImageInfo().rowPitch);
    Vec3<size_t> expectedRegionCopySize = {srcRegion.width, srcRegion.height, srcRegion.depth};
    Vec3<size_t> expectedRegionSrcOrigin = {srcRegion.originX, srcRegion.originY, srcRegion.originZ};
    Vec3<size_t> expectedRegionDstOrigin = {dstRegion.originX, dstRegion.originY, dstRegion.originZ};
    Vec3<size_t> expectedCopyImageSrcSize = {zeDesc.width, zeDesc.arraylevels, zeDesc.depth};
    Vec3<size_t> expectedCopyImageDstSize = {zeDesc.width, zeDesc.arraylevels, zeDesc.depth};
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionSrcOrigin);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionDstOrigin);
    EXPECT_EQ(cmdList.appendCopyImageSrcSize, expectedCopyImageSrcSize);
    EXPECT_EQ(cmdList.appendCopyImageDstSize, expectedCopyImageDstSize);
}

HWTEST_F(CommandListAppend, givenCopyCommandListWhen2DArrayImageCopyRegionThenAppendCopyImageBlitCalledWithCorrectSizes) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_2DARRAY,
                              {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                               ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                              20,
                              30,
                              1,
                              2,
                              0};
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    ze_image_region_t dstRegion = srcRegion;
    srcRegion.originX = 8;
    srcRegion.originY = 8;
    cmdList.appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(cmdList.appendCopyImageSrcRowPitch, imageHWSrc->getImageInfo().rowPitch);
    EXPECT_EQ(cmdList.appendCopyImageSrcSlicePitch, srcRegion.height * imageHWSrc->getImageInfo().rowPitch);
    EXPECT_EQ(cmdList.appendCopyImageDstRowPitch, imageHWDst->getImageInfo().rowPitch);
    EXPECT_EQ(cmdList.appendCopyImageDstSlicePitch, dstRegion.height * imageHWDst->getImageInfo().rowPitch);
    Vec3<size_t> expectedRegionCopySize = {srcRegion.width, srcRegion.height, srcRegion.depth};
    Vec3<size_t> expectedRegionSrcOrigin = {srcRegion.originX, srcRegion.originY, srcRegion.originZ};
    Vec3<size_t> expectedRegionDstOrigin = {dstRegion.originX, dstRegion.originY, dstRegion.originZ};
    Vec3<size_t> expectedCopyImageSrcSize = {zeDesc.width, zeDesc.height, zeDesc.arraylevels};
    Vec3<size_t> expectedCopyImageDstSize = {zeDesc.width, zeDesc.height, zeDesc.arraylevels};
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionSrcOrigin);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionDstOrigin);
    EXPECT_EQ(cmdList.appendCopyImageSrcSize, expectedCopyImageSrcSize);
    EXPECT_EQ(cmdList.appendCopyImageDstSize, expectedCopyImageDstSize);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListWhenCopyFromImageToMemoryThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), &srcRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListWhenCopyFromImageToImageThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
    EXPECT_FALSE(cmdList.useEvents);
}

HWTEST_F(CommandListAppend, givenCopyCommandListWhenImageCopyFromToMemoryExtWithInvalidInputThenErrorReturned) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_1D,
                              {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                               ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                              4,
                              1,
                              1,
                              0,
                              0};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    ze_image_region_t imgRegion = {0, 0, 0, static_cast<uint32_t>(zeDesc.width), 1, 1};
    uint32_t rowPitch = static_cast<uint32_t>(image->getImageInfo().rowPitch);
    uint32_t slicePitch = rowPitch;
    uint32_t data[4];

    auto res = commandList->appendImageCopyFromMemoryExt(nullptr, &data[0], &imgRegion, rowPitch, slicePitch, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
    res = commandList->appendImageCopyFromMemoryExt(image->toHandle(), nullptr, &imgRegion, rowPitch, slicePitch, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_NULL_POINTER);

    res = commandList->appendImageCopyToMemoryExt(&data[0], nullptr, &imgRegion, rowPitch, slicePitch, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
    res = commandList->appendImageCopyToMemoryExt(nullptr, image->toHandle(), &imgRegion, rowPitch, slicePitch, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_NULL_POINTER);
}

HWTEST2_F(CommandListAppend, givenComputeCommandListAndEventIsUsedWhenCopyFromImageToImageThenKernelImageCopyCalled, ImageSupport) {
    Mock<::L0::KernelImp> *mockKernel = static_cast<Mock<::L0::KernelImp> *>(device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion));
    mockKernel->setArgRedescribedImageCallBase = false;

    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::compute, 0u);
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendCopyImageBlitCalledTimes, 0u);
    EXPECT_EQ(event.get(), cmdList.appendKernelEventValue);
}

struct IsAtLeastDg2AndSupportsImages {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily >= IGFX_DG2 && TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::imagesSupported;
    }
};

using DirectSubmissionCommandListTest = Test<DirectSubmissionCommandListFixture>;

HWTEST2_F(DirectSubmissionCommandListTest, givenComputeCommandListWhenCopyImageToMemoryThenTextureCacheFlushIsAddedPriorToWalker, IsAtLeastDg2AndSupportsImages) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    if (!neoDevice->getDeviceInfo().imageSupport || neoDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    Mock<::L0::KernelImp> *mockKernel = static_cast<Mock<::L0::KernelImp> *>(device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion));
    mockKernel->setArgRedescribedImageCallBase = false;

    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::compute, 0u);
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_1D,
                              {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                               ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                              4,
                              1,
                              1,
                              0,
                              0};
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {0, 0, 0, 1, 1, 1};
    uint32_t data[16];
    CmdListMemoryCopyParams copyParams = {};
    cmdList.appendImageCopyToMemory(&data[0], imageHWSrc->toHandle(), &srcRegion, nullptr, 0, nullptr, copyParams);

    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(cmdList.commandContainer.getCommandStream()->getCpuBase(), 0), cmdList.commandContainer.getCommandStream()->getUsed()));

    auto allPcs = findAll<PIPE_CONTROL *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(0u, allPcs.size());

    bool textureCacheInvFound = false;
    auto iteratorPc = genCmdList.begin();
    for (auto it : allPcs) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);

        if (cmd->getTextureCacheInvalidationEnable()) {
            textureCacheInvFound = true;
            iteratorPc = it;
            break;
        }
    }

    EXPECT_TRUE(textureCacheInvFound);

    auto itorWalker = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(iteratorPc, genCmdList.end());
    ASSERT_NE(itorWalker, genCmdList.end());
}

using BlitBlockCopyPlatforms = IsGen12LP;
HWTEST2_F(CommandListCreateTests, givenCopyCommandListWhenCopyRegionWithinMaxBlitSizeThenOneBlitCommandHasBeenSpown, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    uint32_t offsetX = 0x10;
    uint32_t offsetY = 0x10;
    Vec3<size_t> copySize = {0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<size_t> srcSize = {0x1000, 0x100, 1};
    Vec3<size_t> dstSize = {0x100, 0x100, 1};
    auto size = 0x1000;
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    AlignedAllocationData srcAllocationData = {mockAllocationSrc.gpuAddress, 0, &mockAllocationSrc, false};
    AlignedAllocationData dstAllocationData = {mockAllocationDst.gpuAddress, 0, &mockAllocationDst, false};
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopyBlitRegion(&srcAllocationData, &dstAllocationData, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr, 0, nullptr, copyParams, false);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<XY_COPY_BLT *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreateTests, givenCopyCommandListWhenCopyRegionWithinMaxBlitSizeThenDestinationCoordinatesAreCorrectlySet, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    uint32_t offsetX = 0x10;
    uint32_t offsetY = 0x10;
    Vec3<size_t> copySize = {0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<size_t> srcSize = {0x1000, 0x100, 1};
    Vec3<size_t> dstSize = {0x100, 0x100, 1};
    auto size = 0x1000;
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    AlignedAllocationData srcAllocationData = {mockAllocationSrc.gpuAddress, 0, &mockAllocationSrc, false};
    AlignedAllocationData dstAllocationData = {mockAllocationDst.gpuAddress, 0, &mockAllocationDst, false};
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopyBlitRegion(&srcAllocationData, &dstAllocationData, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr, 0, nullptr, copyParams, false);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize.x, srcRegion.originX, dstRegion.originY, srcSize.x, dstSize.x);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    auto cmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getDestinationX2CoordinateRight(), static_cast<uint32_t>(copySize.x) / bytesPerPixel);
    EXPECT_EQ(cmd->getDestinationY2CoordinateBottom(), static_cast<uint32_t>(copySize.y));
}
HWTEST2_F(CommandListCreateTests, givenCopyCommandListWhenCopyRegionGreaterThanMaxBlitSizeThenMoreThanOneBlitCommandHasBeenSpown, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    uint32_t offsetX = 0x1;
    uint32_t offsetY = 0x1;
    Vec3<size_t> copySize = {BlitterConstants::maxBlitWidth + 0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<size_t> srcSize = {2 * BlitterConstants::maxBlitWidth, 2 * BlitterConstants::maxBlitHeight, 1};
    Vec3<size_t> dstSize = srcSize;
    auto size = 0x1000;
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    AlignedAllocationData srcAllocationData = {mockAllocationSrc.gpuAddress, 0, &mockAllocationSrc, false};
    AlignedAllocationData dstAllocationData = {mockAllocationDst.gpuAddress, 0, &mockAllocationDst, false};
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopyBlitRegion(&srcAllocationData, &dstAllocationData, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr, 0, nullptr, copyParams, false);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForRegionSize : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListForRegionSize() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

    AlignedAllocationData getAlignedAllocationData(L0::Device *device, bool sharedSystemEnabled, const void *buffer, uint64_t bufferSize, bool allowHostCopy, bool copyOffload) override {
        return {0, 0, &mockAllocationPtr, true};
    }
    ze_result_t appendMemoryCopyBlitRegion(AlignedAllocationData *srcAllocationData,
                                           AlignedAllocationData *dstAllocationData,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                           L0::Event *signalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams, bool doubleStreamCopyOffload) override {
        this->srcSize = srcSize;
        this->dstSize = dstSize;
        return ZE_RESULT_SUCCESS;
    }
    Vec3<size_t> srcSize = {0, 0, 0};
    Vec3<size_t> dstSize = {0, 0, 0};
    NEO::MockGraphicsAllocation mockAllocationPtr = {0,
                                                     1u /*num gmms*/,
                                                     AllocationType::internalHostMemory,
                                                     reinterpret_cast<void *>(0x1234),
                                                     1,
                                                     0u,
                                                     MemoryPool::system4KBPages,
                                                     MemoryManager::maxOsContextCount,
                                                     0x1234};
};

HWTEST_F(CommandListCreateTests, givenZeroAsPitchAndSlicePitchWhenMemoryCopyRegionCalledThenSizesEqualOffsetPlusCopySize) {
    MockCommandListForRegionSize<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {0x10, 0x10, 0, 0x100, 0x100, 1};
    ze_copy_region_t srcRegion = dstRegion;
    uint32_t pitch = 0;
    uint32_t slicePitch = 0;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, pitch, slicePitch, srcPtr, &srcRegion, pitch, slicePitch, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.dstSize.x, dstRegion.width + dstRegion.originX);
    EXPECT_EQ(cmdList.dstSize.y, dstRegion.height + dstRegion.originY);
    EXPECT_EQ(cmdList.dstSize.z, dstRegion.depth + dstRegion.originZ);

    EXPECT_EQ(cmdList.srcSize.x, srcRegion.width + srcRegion.originX);
    EXPECT_EQ(cmdList.srcSize.y, srcRegion.height + srcRegion.originY);
    EXPECT_EQ(cmdList.srcSize.z, srcRegion.depth + srcRegion.originZ);
}

HWTEST_F(CommandListCreateTests, givenPitchAndSlicePitchWhenMemoryCopyRegionCalledThenSizesAreBasedOnPitch) {
    MockCommandListForRegionSize<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {0x10, 0x10, 0, 0x100, 0x100, 1};
    ze_copy_region_t srcRegion = dstRegion;
    uint32_t pitch = 0x1000;
    uint32_t slicePitch = 0x100000;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, pitch, slicePitch, srcPtr, &srcRegion, pitch, slicePitch, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.dstSize.x, pitch);
    EXPECT_EQ(cmdList.dstSize.y, slicePitch / pitch);

    EXPECT_EQ(cmdList.srcSize.x, pitch);
    EXPECT_EQ(cmdList.srcSize.y, slicePitch / pitch);
}

HWTEST2_F(CommandListCreateTests, givenCommandListThenSshCorrectlyReserved, IsGen12LP) {
    MockCommandListHw<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::compute, 0u);
    auto &gfxCoreHelper = commandList.device->getGfxCoreHelper();
    auto size = gfxCoreHelper.getRenderSurfaceStateSize();
    EXPECT_EQ(commandList.getReserveSshSize(), size);
}

using CommandListAppendMemoryCopyBlit = Test<CommandListFixture>;

HWTEST_F(CommandListAppendMemoryCopyBlit, whenAppendMemoryCopyBlitIsAppendedAndNoSpaceIsAvailableThenNextCommandBufferIsCreated) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    DebugManagerStateRestore restorer;
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    uint64_t size = 1024;

    ze_result_t res = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, res, false));

    auto firstBatchBufferAllocation = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    auto useSize = commandList->getCmdContainer().getCommandStream()->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    commandList->getCmdContainer().getCommandStream()->getSpace(useSize);

    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    void *srcPtr = reinterpret_cast<void *>(mockAllocationSrc.getGpuAddress());
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  1u /*num gmms*/,
                                                  AllocationType::internalHostMemory,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::system4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    void *dstPtr = reinterpret_cast<void *>(mockAllocationDst.getGpuAddress());
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryCopy(dstPtr,
                                                srcPtr,
                                                size,
                                                nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondBatchBufferAllocation = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);
}

struct CommandListAppendImage : Test<CommandListFixture> {
    CmdListMemoryCopyParams copyParams = {};
};

HWTEST2_F(CommandListAppendImage, givenCommandListWhenAppendImageCopyFromMemoryIsCalledWithNullGraphicsAllocationAndSharedSystemNotEnabledThenErrorIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = true;
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->appendImageCopyFromMemory(image->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCommandListWhenAppendImageCopyFromMemoryIsCalledWithNullGraphicsAllocationAndSharedSystemEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = true;
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyFromMemory(image->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCommandListWhenAppendImageCopyFromMemoryIsCalledWithValidGraphicsAllocationThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = false;
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyFromMemory(image->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCommandListWhenAppendImageCopyToMemoryIsCalledWithNullGraphicsAllocationAndSharedSystemNotEnabledThenErrorIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = true;
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), nullptr, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCommandListWhenAppendImageCopyToMemoryIsCalledWithNullGraphicsAllocationAndSharedSystemEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = true;
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), nullptr, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCommandListWhenAppendImageCopyToMemoryIsCalledWithValidGraphicsAllocationThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = false;
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), nullptr, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCopyCommandListWhenAppendImageCopyFromMemoryIsCalledWithSharedSystemAndEmitMemAdviseEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr));
    std::unique_ptr<L0::Image> image(imagePtr);
    ze_image_region_t dstImgRegion = {2, 1, 1, 4, 2, 2};

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyFromMemory(image->toHandle(), srcPtr, &dstImgRegion, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCopyCommandListWhenAppendImageCopyFromMemoryIsCalledWithSharedSystemEnabledAndEmitMemAdviseNotEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(-1);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr));
    std::unique_ptr<L0::Image> image(imagePtr);
    ze_image_region_t dstImgRegion = {2, 1, 1, 4, 2, 2};

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyFromMemory(image->toHandle(), srcPtr, &dstImgRegion, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenComputeCommandListWhenAppendImageCopyFromMemoryIsCalledWithSharedSystemEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyBufferToImage3dBytes);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr));
    std::unique_ptr<L0::Image> image(imagePtr);
    ze_image_region_t dstImgRegion = {2, 1, 1, 4, 2, 2};

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyFromMemory(image->toHandle(), srcPtr, &dstImgRegion, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCopyCommandListWhenAppendImageCopyToMemoryIsCalledWithSharedSystemAndEmitMemAdviseEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    auto result = L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> image(imagePtr);
    ze_image_region_t srcImgRegion = {2, 1, 1, 4, 2, 2};

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), &srcImgRegion, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenCopyCommandListWhenAppendImageCopyToMemoryIsCalledWithSharedSystemEnabledAndEmitMemAdviseNotEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;

    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(-1);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    auto result = L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> image(imagePtr);
    ze_image_region_t srcImgRegion = {2, 1, 1, 4, 2, 2};

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), &srcImgRegion, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendImage, givenComputeCommandListWhenAppendImageCopyToMemoryIsCalledWithSharedSystemEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImage3dToBufferBytes);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    auto result = L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> image(imagePtr);
    ze_image_region_t srcImgRegion = {2, 1, 1, 4, 2, 2};

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), &srcImgRegion, nullptr, 0, nullptr, copyParams));
}

struct CommandListAppendMemoryCopyRegion : Test<CommandListFixture> {
    CmdListMemoryCopyParams copyParams = {};
};

HWTEST2_F(CommandListAppendMemoryCopyRegion, givenCommandListWhenAppendMemoryCopyRegionIsCalledWithValidSrcPtrAndDstPtrButNullGraphicsAllocationAndSharedSystemNotEnabledThenErrorIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = true;
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    void *dstPtr = reinterpret_cast<void *>(0x1234);
    void *srcPtr = reinterpret_cast<void *>(0x2345);

    ze_copy_region_t dstRegion = {0, 0, 0, 4, 4, 1};
    ze_copy_region_t srcRegion = {0, 0, 0, 4, 4, 1};

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendMemoryCopyRegion, givenCommandListWhenAppendMemoryCopyRegionIsCalledWithValidSrcPtrAndDstPtrButNullGraphicsAllocationAndSharedSystemEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = true;
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    void *dstPtr = reinterpret_cast<void *>(0x1234);
    void *srcPtr = reinterpret_cast<void *>(0x2345);

    ze_copy_region_t dstRegion = {0, 0, 0, 4, 4, 1};
    ze_copy_region_t srcRegion = {0, 0, 0, 4, 4, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendMemoryCopyRegion, givenCommandListWhenAppendMemoryCopyRegionIsCalledWithValidGraphicsAllocationThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<MockCommandListHw<FamilyType::gfxCoreFamily>>();
    commandList->failAlignedAlloc = false;
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    void *dstPtr = reinterpret_cast<void *>(0x1234);
    void *srcPtr = reinterpret_cast<void *>(0x2345);

    ze_copy_region_t dstRegion = {0, 0, 0, 4, 4, 1};
    ze_copy_region_t srcRegion = {0, 0, 0, 4, 4, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendMemoryCopyRegion, givenCopyCommandListWhenAppendMemoryCopyRegionIsCalledWithSharedSystemAndEmitMemAdviseEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    void *dstPtr = reinterpret_cast<void *>(0x1234);
    void *srcPtr = reinterpret_cast<void *>(0x2345);

    ze_copy_region_t dstRegion = {0, 0, 0, 4, 4, 2};
    ze_copy_region_t srcRegion = {0, 0, 0, 4, 4, 2};

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendMemoryCopyRegion, givenCopyCommandListWhenAppendMemoryCopyRegionIsCalledWithSharedSystemEnabledAndEmitMemAdviseNotEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(-1);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::copy, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    void *dstPtr = reinterpret_cast<void *>(0x1234);
    void *srcPtr = reinterpret_cast<void *>(0x2345);

    ze_copy_region_t dstRegion = {0, 0, 0, 4, 4, 2};
    ze_copy_region_t srcRegion = {0, 0, 0, 4, 4, 2};

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams));
}

HWTEST2_F(CommandListAppendMemoryCopyRegion, givenComputeCommandListWhenAppendMemoryCopyRegionIsCalledWithSharedSystemEnabledThenSuccessIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto kernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::copyBufferRectBytes3d);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->checkPassedArgumentValues = true;
    mockBuiltinKernel->passedArgumentValues.clear();
    mockBuiltinKernel->passedArgumentValues.resize(6);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u));

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    void *dstPtr = reinterpret_cast<void *>(0x1234);
    void *srcPtr = reinterpret_cast<void *>(0x2345);

    ze_copy_region_t dstRegion = {0, 0, 0, 4, 4, 2};
    ze_copy_region_t srcRegion = {0, 0, 0, 4, 4, 2};

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams));
}

template <typename GfxFamily>
struct MockL0GfxCoreHelperSupportsCmdListHeapSharingHw : L0::L0GfxCoreHelperHw<GfxFamily> {
    bool platformSupportsCmdListHeapSharing() const override { return true; }
};

HWTEST_F(CommandListCreateTests, givenPlatformSupportsSharedHeapsWhenImmediateCmdListCreatedThenSharedHeapsFollowsTheSameSetting) {
    MockL0GfxCoreHelperSupportsCmdListHeapSharingHw<FamilyType> mockL0GfxCoreHelperSupport{};
    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelperSupport));
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);

    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandListImmediate(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue)));
    ASSERT_NE(nullptr, commandListImmediate);

    EXPECT_TRUE(commandListImmediate->immediateCmdListHeapSharing);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);
    l0GfxCoreHelperBackup.release();
}

template <typename GfxFamily>
struct MockL0GfxCoreHelperNoSupportsCmdListHeapSharingHw : L0::L0GfxCoreHelperHw<GfxFamily> {
    bool platformSupportsCmdListHeapSharing() const override { return false; }
};

HWTEST_F(CommandListCreateTests, givenPlatformNotSupportsSharedHeapsWhenImmediateCmdListCreatedThenSharedHeapsIsNotEnabled) {
    MockL0GfxCoreHelperNoSupportsCmdListHeapSharingHw<FamilyType> mockL0GfxCoreHelperNoSupport;
    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelperNoSupport));
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);

    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandListImmediate(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue)));
    ASSERT_NE(nullptr, commandListImmediate);

    EXPECT_FALSE(commandListImmediate->immediateCmdListHeapSharing);

    commandListImmediate.reset(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue)));
    ASSERT_NE(nullptr, commandListImmediate);

    EXPECT_FALSE(commandListImmediate->immediateCmdListHeapSharing);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);
    l0GfxCoreHelperBackup.release();
}

using PrimaryBatchBufferCmdListTest = Test<PrimaryBatchBufferCmdListFixture>;

HWTEST_F(PrimaryBatchBufferCmdListTest, givenForcedPrimaryBatchBufferWhenRegularAndImmediateObjectCreatedThenRegularAndImmediateSetPrimaryFlag) {
    EXPECT_TRUE(commandList->dispatchCmdListBatchBufferAsPrimary);
    EXPECT_TRUE(commandQueue->dispatchCmdListBatchBufferAsPrimary);

    EXPECT_TRUE(commandListImmediate->dispatchCmdListBatchBufferAsPrimary);
    ASSERT_NE(nullptr, commandListImmediate->cmdQImmediate);
    auto immediateCmdQueue = static_cast<L0::ult::CommandQueue *>(commandListImmediate->cmdQImmediate);
    EXPECT_TRUE(immediateCmdQueue->dispatchCmdListBatchBufferAsPrimary);
}

HWTEST_F(PrimaryBatchBufferCmdListTest, givenPrimaryBatchBufferWhenAppendingKernelAndClosingCommandListThenExpectAlignedSpaceForBatchBufferStart) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto &cmdContainer = commandList->getCmdContainer();
    auto &cmdListStream = *cmdContainer.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    size_t cmdListUsed = cmdListStream.getUsed();

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    size_t actualUse = cmdListUsed + sizeof(MI_BATCH_BUFFER_START);
    size_t expectedAlignedSize = alignUp(actualUse, NEO::CommandContainer::minCmdBufferPtrAlign);

    EXPECT_EQ(expectedAlignedSize, cmdContainer.getAlignedPrimarySize());
    if (expectedAlignedSize > actualUse) {
        size_t noopSize = expectedAlignedSize - actualUse;
        ASSERT_LE(noopSize, NEO::CommandContainer::minCmdBufferPtrAlign);

        uint8_t noopPadding[NEO::CommandContainer::minCmdBufferPtrAlign];
        memset(noopPadding, 0, noopSize);

        auto noopPtr = ptrOffset(cmdListStream.getSpace(0), sizeof(MI_BATCH_BUFFER_START));
        EXPECT_EQ(0, memcmp(noopPadding, noopPtr, noopSize));
    }

    void *expectedEndPtr = ptrOffset(cmdListStream.getCpuBase(), cmdListUsed);
    EXPECT_EQ(expectedEndPtr, cmdContainer.getEndCmdPtr());

    result = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdListUsed = cmdListStream.getUsed();

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    expectedAlignedSize = alignUp(cmdListUsed + sizeof(MI_BATCH_BUFFER_START), NEO::CommandContainer::minCmdBufferPtrAlign);
    EXPECT_EQ(expectedAlignedSize, cmdContainer.getAlignedPrimarySize());

    expectedEndPtr = ptrOffset(cmdListStream.getCpuBase(), cmdListUsed);
    EXPECT_EQ(expectedEndPtr, cmdContainer.getEndCmdPtr());
}

HWTEST_F(PrimaryBatchBufferCmdListTest, givenPrimaryBatchBufferWhenCommandListHasMultipleCommandBuffersThenBuffersAreChainedAndAligned) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto &cmdContainer = commandList->getCmdContainer();
    auto &cmdListStream = *cmdContainer.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto firstChainBufferAllocation = cmdListStream.getGraphicsAllocation();
    cmdListStream.getSpace(cmdListStream.getAvailableSpace() - sizeof(MI_BATCH_BUFFER_START));
    size_t firstCmdBufferUsed = cmdListStream.getUsed();
    auto bbStartSpace = ptrOffset(cmdListStream.getCpuBase(), firstCmdBufferUsed);

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondChainBufferAllocation = cmdListStream.getGraphicsAllocation();
    size_t secondCmdBufferUsed = cmdListStream.getUsed();

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(firstChainBufferAllocation, secondChainBufferAllocation);
    auto bbStartGpuAddress = secondChainBufferAllocation->getGpuAddress();
    auto bbStartChainToSecond = genCmdCast<MI_BATCH_BUFFER_START *>(bbStartSpace);
    ASSERT_NE(nullptr, bbStartChainToSecond);

    EXPECT_EQ(bbStartGpuAddress, bbStartChainToSecond->getBatchBufferStartAddress());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, bbStartChainToSecond->getSecondLevelBatchBuffer());

    size_t expectedAlignedUse = alignUp(firstCmdBufferUsed + sizeof(MI_BATCH_BUFFER_START), NEO::CommandContainer::minCmdBufferPtrAlign);
    EXPECT_EQ(expectedAlignedUse, cmdContainer.getAlignedPrimarySize());

    void *expectedEndPtr = ptrOffset(cmdListStream.getCpuBase(), secondCmdBufferUsed);
    EXPECT_EQ(expectedEndPtr, cmdContainer.getEndCmdPtr());
}

HWTEST_F(PrimaryBatchBufferCmdListTest, givenRegularCmdListWhenFlushingThenPassStallingCmdsInfo) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->recordFlushedBatchBuffer = true;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->close());

    auto cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr));

    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST_F(PrimaryBatchBufferCmdListTest, givenRegularCmdListWhenNoPreambleExpectedAndForceBbStartThenDispatchBbStart) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto cmdListHandle = commandList->toHandle();

    auto regularCmdBufferAllocation = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    // 1st dispatch can carry state preamble
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &queueStream = commandQueue->commandStream;

    auto offsetBefore = queueStream.getUsed();
    commandQueue->triggerBbStartJump();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto offsetAfter = queueStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(queueStream.getCpuBase(), offsetBefore),
        offsetAfter - offsetBefore));
    ASSERT_NE(0u, cmdList.size());

    auto firstCmdIt = cmdList.begin();
    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(*firstCmdIt);
    ASSERT_NE(nullptr, bbStartCmd);

    EXPECT_EQ(regularCmdBufferAllocation->getGpuAddress(), bbStartCmd->getBatchBufferStartAddress());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, bbStartCmd->getSecondLevelBatchBuffer());
}

HWTEST2_F(PrimaryBatchBufferCmdListTest, givenRelaxedOrderingAndRegularCmdListAndSubmittedToImmediateWhenFlushingThenPassStallingCmdsInfo, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    bool useImmediateFlushTask = getHelper<L0GfxCoreHelper>().platformSupportsImmediateComputeFlushTask();
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    auto immCommandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(immCommandList.get());
    whiteBoxCmdList->enableInOrderExecution();
    uint64_t *hostAddress = ptrOffset(whiteBoxCmdList->inOrderExecInfo->getBaseHostAddress(), whiteBoxCmdList->inOrderExecInfo->getAllocationOffset());
    for (uint32_t i = 0; i < whiteBoxCmdList->inOrderExecInfo->getNumHostPartitionsToWait(); i++) {
        *hostAddress = std::numeric_limits<uint64_t>::max();
        hostAddress = ptrOffset(hostAddress, device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
    }
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->close());

    auto cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCommandList->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr));

    if (useImmediateFlushTask) {
        EXPECT_FALSE(ultCsr->recordedImmediateDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_TRUE(ultCsr->recordedImmediateDispatchFlags.hasStallingCmds);
    } else {
        EXPECT_FALSE(ultCsr->recordedDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_FALSE(ultCsr->recordedDispatchFlags.hasStallingCmds);
    }
}

HWTEST_F(PrimaryBatchBufferCmdListTest, givenCmdListWhenCallingSynchronizeThenUnregisterCsrClient) {
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->close());

    auto csr = commandQueue->getCsr();

    auto numClients = csr->getNumClients();

    auto cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr));

    EXPECT_EQ(numClients + 1, csr->getNumClients());

    commandQueue->synchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(numClients, csr->getNumClients());
}

HWTEST_F(PrimaryBatchBufferCmdListTest, givenPrimaryBatchBufferWhenCopyCommandListAndQueueAreCreatedThenFirstDispatchCreatesGlobalInitPreambleAndLaterDispatchProvideCmdListBuffer) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    ze_result_t returnValue;
    uint32_t count = 0u;
    returnValue = device->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_GT(count, 0u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    returnValue = device->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    uint32_t ordinal = 0u;
    for (ordinal = 0u; ordinal < count; ordinal++) {
        if ((properties[ordinal].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
            !(properties[ordinal].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
            if (properties[ordinal].numQueues == 0) {
                continue;
            }
            break;
        }
    }

    if (ordinal == count) {
        GTEST_SKIP();
    }

    void *dstPtr = nullptr;
    void *srcPtr = nullptr;
    const size_t size = 64;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    returnValue = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 4u, &dstPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 4u, &srcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_command_queue_desc_t desc{};
    desc.ordinal = ordinal;
    desc.index = 0u;

    ze_command_queue_handle_t commandQueueHandle;
    returnValue = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto commandQueueCopy = static_cast<L0::ult::CommandQueue *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    ASSERT_NE(commandQueueCopy, nullptr);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueueCopy->getCsr());
    ultCsr->recordFlushedBatchBuffer = true;

    std::unique_ptr<L0::ult::CommandList> commandListCopy;
    commandListCopy.reset(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto &cmdContainerCopy = commandListCopy->getCmdContainer();
    auto &cmdListStream = *cmdContainerCopy.getCommandStream();
    auto firstCmdBufferAllocation = cmdContainerCopy.getCmdBufferAllocations()[0];
    CmdListMemoryCopyParams copyParams = {};
    returnValue = commandListCopy->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    size_t firstCmdBufferUsed = cmdListStream.getUsed();
    auto bbStartSpace = ptrOffset(cmdListStream.getCpuBase(), firstCmdBufferUsed);

    returnValue = commandListCopy->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(bbStartSpace, cmdContainerCopy.getEndCmdPtr());
    size_t expectedAlignedUse = alignUp(firstCmdBufferUsed + sizeof(MI_BATCH_BUFFER_START), NEO::CommandContainer::minCmdBufferPtrAlign);
    EXPECT_EQ(expectedAlignedUse, cmdContainerCopy.getAlignedPrimarySize());

    size_t blitterContextInitSize = ultCsr->getCmdsSizeForHardwareContext();

    auto cmdListHandle = commandListCopy->toHandle();
    returnValue = commandQueueCopy->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(bbStartSpace);
    ASSERT_NE(nullptr, bbStartCmd);

    auto &cmdQueueStream = commandQueueCopy->commandStream;
    if (blitterContextInitSize > 0) {
        EXPECT_EQ(cmdQueueStream.getGraphicsAllocation(), ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);
    } else {
        EXPECT_EQ(firstCmdBufferAllocation, ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);
        EXPECT_EQ(cmdQueueStream.getGpuBase(), bbStartCmd->getBatchBufferStartAddress());
    }
    size_t queueSizeUsed = cmdQueueStream.getUsed();

    returnValue = commandQueueCopy->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(bbStartSpace);
    ASSERT_NE(nullptr, bbStartCmd);

    EXPECT_EQ(cmdQueueStream.getGpuBase() + queueSizeUsed, bbStartCmd->getBatchBufferStartAddress());

    commandQueueCopy->destroy();
    commandListCopy.reset(nullptr);

    returnValue = context->freeMem(dstPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = context->freeMem(srcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

using PrimaryBatchBufferPreamblelessCmdListTest = Test<PrimaryBatchBufferPreamblelessCmdListFixture>;

HWTEST2_F(PrimaryBatchBufferPreamblelessCmdListTest,
          givenPrimaryBatchBufferWhenExecutingSingleCommandListTwiceInSingleCallAndFirstTimeNotExpectsPreambleThenProperlyDispatchPreambleForSecondInstance,
          IsAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    if (device->getProductHelper().isNewCoherencyModelSupported()) {
        GTEST_SKIP();
    }

    // command list 1 will have two kernels, transition from cached MOCS to uncached MOCS state
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    kernel->privateState.kernelRequiresUncachedMocsCount++;

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // command list 2 will have two kernels, transition from uncached MOCS to cached MOCS state
    result = commandList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    kernel->privateState.kernelRequiresUncachedMocsCount--;

    result = commandList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList2->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // first command list settles global init and leaves state as uncached MOCS
    auto commandListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;
    auto queueSizeUsed = cmdQueueStream.getUsed();

    ze_command_list_handle_t sameCommandListTwice[] = {commandList2->toHandle(), commandList2->toHandle()};
    // second command list requires uncached MOCS state, so no dynamic preamble for the fist instance, but leaves cached MOCS state
    // second instance require dynamic preamble to reload MOCS to uncached state
    result = commandQueue->executeCommandLists(2, sameCommandListTwice, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueSizeUsed),
        cmdQueueStream.getUsed() - queueSizeUsed));

    auto cmdQueueSbaDirtyCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_TRUE(cmdQueueSbaDirtyCmds.size() >= 1u);
    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*cmdQueueSbaDirtyCmds[0]);

    auto uncachedMocs = device->getMOCS(false, false) >> 1;
    EXPECT_EQ((uncachedMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(PrimaryBatchBufferPreamblelessCmdListTest,
          givenPrimaryBatchBufferWhenExecutingCommandWithoutPreambleThenUseCommandListBufferAsStartingBuffer,
          IsAtLeastXeCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->recordFlushedBatchBuffer = true;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;
    if (ultCsr->heaplessStateInitialized) {
        EXPECT_NE(cmdQueueStream.getGraphicsAllocation(), ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);
    } else {
        EXPECT_EQ(cmdQueueStream.getGraphicsAllocation(), ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);
    }

    size_t queueUsedSize = cmdQueueStream.getUsed();
    auto gpuReturnAddress = cmdQueueStream.getGpuBase() + queueUsedSize;

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdContainer = commandList->getCmdContainer();
    auto firstCmdBufferAllocation = cmdContainer.getCmdBufferAllocations()[0];
    EXPECT_EQ(firstCmdBufferAllocation, ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);

    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmdContainer.getEndCmdPtr());
    ASSERT_NE(nullptr, bbStartCmd);
    EXPECT_EQ(gpuReturnAddress, bbStartCmd->getBatchBufferStartAddress());
}

HWTEST2_F(PrimaryBatchBufferPreamblelessCmdListTest,
          givenPrimaryBatchBufferWhenExecutingMultipleCommandListsAndEachWithoutPreambleThenUseCommandListBufferAsStartingBufferAndChainAllCommandLists,
          IsAtLeastXeCore) {

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->recordFlushedBatchBuffer = true;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_list_handle_t commandLists[] = {commandList->toHandle(),
                                               commandList2->toHandle(),
                                               commandList3->toHandle()};

    result = commandQueue->executeCommandLists(1, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    if (ultCsr->heaplessStateInitialized) {
        EXPECT_NE(cmdQueueStream.getGraphicsAllocation(), ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);
    } else {
        EXPECT_EQ(cmdQueueStream.getGraphicsAllocation(), ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);
    }

    size_t queueUsedSize = cmdQueueStream.getUsed();
    auto gpuReturnAddress = cmdQueueStream.getGpuBase() + queueUsedSize;

    result = commandList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList2->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList3->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList3->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(3, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueUsedSize),
        cmdQueueStream.getUsed() - queueUsedSize));
    auto cmdQueueBbStartCmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, cmdQueueBbStartCmds.size());

    auto &cmdContainer1stCmdList = commandList->getCmdContainer();
    auto dispatchCmdBufferAllocation = cmdContainer1stCmdList.getCmdBufferAllocations()[0];
    EXPECT_EQ(dispatchCmdBufferAllocation, ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);

    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmdContainer1stCmdList.getEndCmdPtr());
    ASSERT_NE(nullptr, bbStartCmd);

    auto &cmdContainer2ndCmdList = commandList2->getCmdContainer();
    auto secondCmdBufferAllocation = cmdContainer2ndCmdList.getCmdBufferAllocations()[0];
    EXPECT_EQ(secondCmdBufferAllocation->getGpuAddress(), bbStartCmd->getBatchBufferStartAddress());

    bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmdContainer2ndCmdList.getEndCmdPtr());
    ASSERT_NE(nullptr, bbStartCmd);

    auto &cmdContainer3rdCmdList = commandList3->getCmdContainer();
    auto thirdCmdBufferAllocation = cmdContainer3rdCmdList.getCmdBufferAllocations()[0];
    EXPECT_EQ(thirdCmdBufferAllocation->getGpuAddress(), bbStartCmd->getBatchBufferStartAddress());

    bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmdContainer3rdCmdList.getEndCmdPtr());
    ASSERT_NE(nullptr, bbStartCmd);
    EXPECT_EQ(gpuReturnAddress, bbStartCmd->getBatchBufferStartAddress());
}

HWTEST2_F(PrimaryBatchBufferPreamblelessCmdListTest,
          givenPrimaryBatchBufferWhenExecutingMultipleCommandListsAndSecondWithPreambleThenUseCommandListBufferAsStartingBufferAndChainFirstListToQueuePreambleAndAfterToSecondList,
          IsAtLeastXeCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    if (device->getProductHelper().isNewCoherencyModelSupported()) {
        GTEST_SKIP();
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->recordFlushedBatchBuffer = true;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    ze_result_t result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_list_handle_t commandLists[] = {commandList->toHandle(),
                                               commandList2->toHandle(),
                                               commandList3->toHandle()};

    result = commandQueue->executeCommandLists(1, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;
    EXPECT_EQ(cmdQueueStream.getGraphicsAllocation(), ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);

    size_t queueUsedSize = cmdQueueStream.getUsed();
    auto gpuReturnAddress = cmdQueueStream.getGpuBase() + queueUsedSize;

    kernel->privateState.kernelRequiresUncachedMocsCount++;

    result = commandList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList2->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList3->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList3->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(3, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // 1st command list is preambleless
    auto &cmdContainer1stCmdList = commandList->getCmdContainer();
    auto dispatchCmdBufferAllocation = cmdContainer1stCmdList.getCmdBufferAllocations()[0];
    EXPECT_EQ(dispatchCmdBufferAllocation, ultCsr->latestFlushedBatchBuffer.commandBufferAllocation);

    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmdContainer1stCmdList.getEndCmdPtr());
    ASSERT_NE(nullptr, bbStartCmd);

    // ending BB_START of 1st command list points to dynamic preamble - dirty stateless mocs SBA command
    EXPECT_EQ(gpuReturnAddress, bbStartCmd->getBatchBufferStartAddress());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueUsedSize),
        cmdQueueStream.getUsed() - queueUsedSize));
    auto cmdQueueSbaDirtyCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_TRUE(cmdQueueSbaDirtyCmds.size() >= 1u);

    auto cmdQueueBbStartCmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, cmdQueueBbStartCmds.size());

    auto chainFromPreambleToSecondBbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(*cmdQueueBbStartCmds[0]);

    auto &cmdContainer2ndCmdList = commandList2->getCmdContainer();
    auto secondCmdBufferAllocation = cmdContainer2ndCmdList.getCmdBufferAllocations()[0];
    EXPECT_EQ(secondCmdBufferAllocation->getGpuAddress(), chainFromPreambleToSecondBbStartCmd->getBatchBufferStartAddress());

    bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmdContainer2ndCmdList.getEndCmdPtr());
    ASSERT_NE(nullptr, bbStartCmd);

    auto &cmdContainer3rdCmdList = commandList3->getCmdContainer();
    auto thirdCmdBufferAllocation = cmdContainer3rdCmdList.getCmdBufferAllocations()[0];
    EXPECT_EQ(thirdCmdBufferAllocation->getGpuAddress(), bbStartCmd->getBatchBufferStartAddress());

    bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmdContainer3rdCmdList.getEndCmdPtr());
    ASSERT_NE(nullptr, bbStartCmd);

    size_t sbaSize = sizeof(STATE_BASE_ADDRESS) + NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleBarrier();
    if (commandQueue->doubleSbaWa) {
        sbaSize += sizeof(STATE_BASE_ADDRESS);
    }

    gpuReturnAddress += sizeof(MI_BATCH_BUFFER_START) + sbaSize;
    EXPECT_EQ(gpuReturnAddress, bbStartCmd->getBatchBufferStartAddress());
}

HWTEST_F(CommandListAppend, givenCopyCommandListWhenImageCopyFromFromMemoryExtThenNotAlignedSrcPtrPassedToBltDispatch) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.returnMockAllocationStruct = true;
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_1D,
                              {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                               ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                              4,
                              1,
                              1,
                              0,
                              0};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    ze_image_region_t imgRegion = {0, 0, 0, static_cast<uint32_t>(zeDesc.width), 1, 1};
    uint32_t rowPitch = static_cast<uint32_t>(image->getImageInfo().rowPitch);
    uint32_t slicePitch = rowPitch;
    void *data;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 64u, 64u, &data);
    void *srcPtr = ptrOffset(data, 3);

    cmdList.appendImageCopyFromMemoryExt(image->toHandle(), srcPtr, &imgRegion, rowPitch, slicePitch, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(cmdList.appendSrcPtr, reinterpret_cast<uintptr_t>(srcPtr));
    EXPECT_EQ(cmdList.appendSrcAlloc, nullptr);
    context->freeMem(data);
}

HWTEST_F(CommandListAppend, givenCopyCommandListWhenImageCopyFromToMemoryExtThenNotAlignedDstPtrPassedToBltDispatch) {
    MockCommandListHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.returnMockAllocationStruct = true;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_image_desc_t zeDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_1D,
                              {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                               ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                              4,
                              1,
                              1,
                              0,
                              0};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    image->initialize(device, &zeDesc);

    ze_image_region_t imgRegion = {0, 0, 0, static_cast<uint32_t>(zeDesc.width), 1, 1};
    uint32_t rowPitch = static_cast<uint32_t>(image->getImageInfo().rowPitch);
    uint32_t slicePitch = rowPitch;
    void *data;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 64u, 64u, &data);
    void *dstPtr = ptrOffset(data, 3);

    cmdList.appendImageCopyToMemoryExt(dstPtr, image->toHandle(), &imgRegion, rowPitch, slicePitch, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(cmdList.appendDstPtr, reinterpret_cast<uintptr_t>(dstPtr));
    EXPECT_EQ(cmdList.appendDstAlloc, nullptr);
    context->freeMem(data);
}
} // namespace ult
} // namespace L0
