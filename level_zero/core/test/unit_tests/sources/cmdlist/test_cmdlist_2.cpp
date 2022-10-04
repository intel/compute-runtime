/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {

extern L0HwHelper *l0HwHelperFactory[IGFX_MAX_CORE];

namespace ult {

using CommandListCreate = Test<DeviceFixture>;
using CommandListAppend = Test<CommandListFixture>;

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListHw : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListHw() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}
    MockCommandListHw(bool failOnFirst) : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>(), failOnFirstCopy(failOnFirst) {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize, bool allowHostCopy) override {
        getAlignedAllocationCalledTimes++;
        if (buffer) {
            return {0, 0, &alignedAlloc, true};
        }
        return {0, 0, nullptr, false};
    }

    ze_result_t appendMemoryCopyKernelWithGA(void *dstPtr,
                                             NEO::GraphicsAllocation *dstPtrAlloc,
                                             uint64_t dstOffset,
                                             void *srcPtr,
                                             NEO::GraphicsAllocation *srcPtrAlloc,
                                             uint64_t srcOffset,
                                             uint64_t size,
                                             uint64_t elementSize,
                                             Builtin builtin,
                                             Event *signalEvent,
                                             bool isStateless) override {
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
                                     uint64_t size) override {
        appendMemoryCopyBlitCalledTimes++;
        if (failOnFirstCopy && appendMemoryCopyBlitCalledTimes == 1) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcAllocation,
                                           NEO::GraphicsAllocation *dstAllocation,
                                           size_t srcOffset,
                                           size_t dstOffset,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                           Event *signalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        appendMemoryCopyBlitRegionCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         size_t srcOffset, Event *signalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        appendMemoryCopyKernel2dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         uint32_t srcSlicePitch, size_t srcOffset,
                                         Event *signalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents) override {
        appendMemoryCopyKernel3dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendBlitFill(void *ptr, const void *pattern,
                               size_t patternSize, size_t size,
                               Event *signalEvent, uint32_t numWaitEvents,
                               ze_event_handle_t *phWaitEvents) override {
        appendBlitFillCalledTimes++;
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendCopyImageBlit(NEO::GraphicsAllocation *src,
                                    NEO::GraphicsAllocation *dst,
                                    const Vec3<size_t> &srcOffsets, const Vec3<size_t> &dstOffsets,
                                    size_t srcRowPitch, size_t srcSlicePitch,
                                    size_t dstRowPitch, size_t dstSlicePitch,
                                    size_t bytesPerPixel, const Vec3<size_t> &copySize,
                                    const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                    Event *signalEvent) override {
        appendCopyImageBlitCalledTimes++;
        appendImageRegionCopySize = copySize;
        appendImageRegionSrcOrigin = srcOffsets;
        appendImageRegionDstOrigin = dstOffsets;
        if (signalEvent) {
            useEvents = true;
        } else {
            useEvents = false;
        }
        return ZE_RESULT_SUCCESS;
    }
    uint8_t mockAlignedAllocData[2 * MemoryConstants::pageSize]{};

    Vec3<size_t> appendImageRegionCopySize = {0, 0, 0};
    Vec3<size_t> appendImageRegionSrcOrigin = {9, 9, 9};
    Vec3<size_t> appendImageRegionDstOrigin = {9, 9, 9};

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
};

HWTEST2_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledWithNullDstPtrThenAppendMemoryCopyWithappendMemoryCopyReturnsError, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = nullptr;
    ze_result_t ret = cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledWithNullSrcPtrThenAppendMemoryCopyWithappendMemoryCopyReturnsError, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = nullptr;
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_result_t ret = cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledWithNullSrcPtrAndDstPtrThenAppendMemoryCopyWithappendMemoryCopyReturnsError, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = nullptr;
    void *dstPtr = nullptr;
    ze_result_t ret = cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.getAlignedAllocationCalledTimes, 0u);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyKernelWithGACalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhen4GByteMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyKernelWithGAStatelessCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x100001234);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x100000000, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenMemoryCopyRegionCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenPageFaultCopyCalledThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    size_t size = (sizeof(uint32_t) * 4);
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 1u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledWithCopyEngineThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    size_t size = (sizeof(uint32_t) * 4);
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 1u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalledForMiddleAndRightSizesAreCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    size_t size = ((sizeof(uint32_t) * 4) + 1);
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 2u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledAndErrorOnMidCopyThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalledForMiddleIsCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList(true);
    size_t size = ((sizeof(uint32_t) * 4) + 1);
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 1u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledWithCopyEngineThenappendPageFaultCopyWithappendMemoryCopyCalledOnlyOnce, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    size_t size = ((sizeof(uint32_t) * 4) + 1);
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 1u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenPageFaultCopyCalledWithCopyEngineAndErrorOnMidOperationThenappendPageFaultCopyWithappendMemoryCopyKernelWithGACalledForMiddleIsCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList(true);
    size_t size = ((sizeof(uint32_t) * 4) + 1);
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 1u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhen4GBytePageFaultCopyCalledThenPageFaultCopyWithappendMemoryCopyKernelWithGAStatelessCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    size_t size = 0x100000000;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 1u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 1u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhen4GBytePageFaultCopyCalledThenPageFaultCopyWithappendMemoryCopyKernelWithGAStatelessCalledForMiddleAndRightSizesAreCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    size_t size = 0x100000001;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    cmdList.appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 2u);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGAStatelessCalledTimes, 2u);
}

HWTEST2_F(CommandListAppend, givenCommandListAnd3DWhbufferenMemoryCopyRegionCalledThenCopyKernel3DCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel3dCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListAnd2DWhbufferenMemoryCopyRegionCalledThenCopyKernel2DCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel2dCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenImmediateCommandListWithFlushTaskEnabledWhenAppendingMemoryCopyRegionThenSuccessIsReturned, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
    cmdList.initialize(device, NEO::EngineGroupType::Compute, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    auto result = cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppend, givenCopyOnlyCommandListWhenAppendMemoryFillCalledThenAppendBlitFillCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendBlitFillCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCommandListWhenAppendMemoryFillCalledThenAppendBlitFillNotCalled, IsAtLeastSkl) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendBlitFillCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyWithSignalEventsThenSemaphoreWaitAndPipeControlAreFound, IsAtLeastSkl) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 2u, events.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 2u, events.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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

using platformSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyWithSignalEventScopeSetToDeviceThenSinglePipeControlIsAddedWithDcFlush, platformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, event.get(), 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyWithSignalEventScopeSetToSubDeviceThenB2BPipeControlIsAddedWithDcFlushForLastPC, platformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_SUBDEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x1001, event.get(), 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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

using ImageSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(CommandListAppend, givenCopyCommandListWhenCopyFromMemoryToImageThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, &dstRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
    EXPECT_FALSE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhenImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, event->toHandle(), 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
    EXPECT_TRUE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhenImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
    EXPECT_FALSE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen1DImageCopyFromMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen1DImageCopyToMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_1D;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, event->toHandle(), 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
    EXPECT_TRUE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen1DArrayImageCopyFromMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen1DArrayImageCopyToMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyToMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyFromMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen3DImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListAndNullDestinationRegionWhen3DImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
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

HWTEST2_F(CommandListAppend, givenCopyCommandListWhenCopyFromImageToMemoryThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), &srcRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListAppend, givenCopyCommandListWhenCopyFromImageToImageThenBlitImageCopyCalled, ImageSupport) {
    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
    EXPECT_FALSE(cmdList.useEvents);
}

HWTEST2_F(CommandListAppend, givenComputeCommandListAndEventIsUsedWhenCopyFromImageToImageThenKernelImageCopyCalled, ImageSupport) {
    Mock<::L0::Kernel> *mockKernel = static_cast<Mock<::L0::Kernel> *>(device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImageRegion));
    mockKernel->setArgRedescribedImageCallBase = false;

    MockCommandListHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Compute, 0u);
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, event->toHandle(), 0, nullptr);
    EXPECT_EQ(cmdList.appendCopyImageBlitCalledTimes, 0u);
    EXPECT_EQ(event.get(), cmdList.appendKernelEventValue);
}

using BlitBlockCopyPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionWithinMaxBlitSizeThenOneBlitCommandHasBeenSpown, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);

    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, 0, 0, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr, 0, nullptr);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<XY_COPY_BLT *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionWithinMaxBlitSizeThenDestinationCoordinatesAreCorrectlySet, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, 0, 0, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr, 0, nullptr);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize.x, srcRegion.originX, dstRegion.originY, srcSize.x, dstSize.x);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    auto cmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getDestinationX2CoordinateRight(), static_cast<uint32_t>(copySize.x) / bytesPerPixel);
    EXPECT_EQ(cmd->getDestinationY2CoordinateBottom(), static_cast<uint32_t>(copySize.y));
}
HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionGreaterThanMaxBlitSizeThenMoreThanOneBlitCommandHasBeenSpown, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, 0, 0, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr, 0, nullptr);
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

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize, bool allowHostCopy) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcAllocation,
                                           NEO::GraphicsAllocation *dstAllocation,
                                           size_t srcOffset,
                                           size_t dstOffset,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                           Event *signalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        this->srcSize = srcSize;
        this->dstSize = dstSize;
        return ZE_RESULT_SUCCESS;
    }
    Vec3<size_t> srcSize = {0, 0, 0};
    Vec3<size_t> dstSize = {0, 0, 0};
};
HWTEST2_F(CommandListCreate, givenZeroAsPitchAndSlicePitchWhenMemoryCopyRegionCalledThenSizesEqualOffsetPlusCopySize, IsAtLeastSkl) {
    MockCommandListForRegionSize<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {0x10, 0x10, 0, 0x100, 0x100, 1};
    ze_copy_region_t srcRegion = dstRegion;
    uint32_t pitch = 0;
    uint32_t slicePitch = 0;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, pitch, slicePitch, srcPtr, &srcRegion, pitch, slicePitch, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.dstSize.x, dstRegion.width + dstRegion.originX);
    EXPECT_EQ(cmdList.dstSize.y, dstRegion.height + dstRegion.originY);
    EXPECT_EQ(cmdList.dstSize.z, dstRegion.depth + dstRegion.originZ);

    EXPECT_EQ(cmdList.srcSize.x, srcRegion.width + srcRegion.originX);
    EXPECT_EQ(cmdList.srcSize.y, srcRegion.height + srcRegion.originY);
    EXPECT_EQ(cmdList.srcSize.z, srcRegion.depth + srcRegion.originZ);
}

HWTEST2_F(CommandListCreate, givenPitchAndSlicePitchWhenMemoryCopyRegionCalledThenSizesAreBasedOnPitch, IsAtLeastSkl) {
    MockCommandListForRegionSize<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::Copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {0x10, 0x10, 0, 0x100, 0x100, 1};
    ze_copy_region_t srcRegion = dstRegion;
    uint32_t pitch = 0x1000;
    uint32_t slicePitch = 0x100000;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, pitch, slicePitch, srcPtr, &srcRegion, pitch, slicePitch, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.dstSize.x, pitch);
    EXPECT_EQ(cmdList.dstSize.y, slicePitch / pitch);

    EXPECT_EQ(cmdList.srcSize.x, pitch);
    EXPECT_EQ(cmdList.srcSize.y, slicePitch / pitch);
}

using SupportedPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_DG1>;
HWTEST2_F(CommandListCreate, givenCommandListThenSshCorrectlyReserved, SupportedPlatforms) {
    MockCommandListHw<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::Compute, 0u);
    auto &helper = NEO::HwHelper::get(commandList.device->getHwInfo().platform.eRenderCoreFamily);
    auto size = helper.getRenderSurfaceStateSize();
    EXPECT_EQ(commandList.getReserveSshSize(), size);
}

using CommandListAppendMemoryCopyBlit = Test<CommandListFixture>;

HWTEST2_F(CommandListAppendMemoryCopyBlit, whenAppendMemoryCopyBlitIsAppendedAndNoSpaceIsAvailableThenNextCommandBufferIsCreated, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    uint64_t size = 1024;

    ze_result_t res = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, res));

    auto firstBatchBufferAllocation = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    auto useSize = commandList->commandContainer.getCommandStream()->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    commandList->commandContainer.getCommandStream()->getSpace(useSize);

    auto ptr = reinterpret_cast<void *>(0x1234);
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    NEO::MockGraphicsAllocation mockAllocationSrc(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    void *srcPtr = reinterpret_cast<void *>(mockAllocationSrc.getGpuAddress());
    NEO::MockGraphicsAllocation mockAllocationDst(0,
                                                  AllocationType::INTERNAL_HOST_MEMORY,
                                                  ptr,
                                                  size,
                                                  0u,
                                                  MemoryPool::System4KBPages,
                                                  MemoryManager::maxOsContextCount,
                                                  canonizedGpuAddress);
    void *dstPtr = reinterpret_cast<void *>(mockAllocationDst.getGpuAddress());

    auto result = commandList->appendMemoryCopy(dstPtr,
                                                srcPtr,
                                                size,
                                                nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondBatchBufferAllocation = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);
}

template <typename GfxFamily>
struct MockL0HwHelperSupportsCmdListHeapSharingHw : L0::L0HwHelperHw<GfxFamily> {
    bool platformSupportsCmdListHeapSharing(const HardwareInfo &hwInfo) const override { return true; }
};

HWTEST2_F(CommandListCreate, givenPlatformSupportsSharedHeapsWhenImmediateCmdListCreatedWithFlushTaskSetThenSharedHeapsFollowsTheSameSetting, IsAtLeastSkl) {
    MockL0HwHelperSupportsCmdListHeapSharingHw<FamilyType> mockL0HwHelperSupport;
    VariableBackup<L0HwHelper *> l0HwHelperFactoryBackup{&L0::l0HwHelperFactory[static_cast<size_t>(device->getHwInfo().platform.eRenderCoreFamily)]};
    l0HwHelperFactoryBackup = &mockL0HwHelperSupport;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandListImmediate(whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue)));
    ASSERT_NE(nullptr, commandListImmediate);

    EXPECT_TRUE(commandListImmediate->isFlushTaskSubmissionEnabled);
    EXPECT_TRUE(commandListImmediate->immediateCmdListHeapSharing);

    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    commandListImmediate.reset(whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue)));
    ASSERT_NE(nullptr, commandListImmediate);

    EXPECT_FALSE(commandListImmediate->isFlushTaskSubmissionEnabled);
    EXPECT_FALSE(commandListImmediate->immediateCmdListHeapSharing);
}

template <typename GfxFamily>
struct MockL0HwHelperNoSupportsCmdListHeapSharingHw : L0::L0HwHelperHw<GfxFamily> {
    bool platformSupportsCmdListHeapSharing(const HardwareInfo &hwInfo) const override { return false; }
};

HWTEST2_F(CommandListCreate, givenPlatformNotSupportsSharedHeapsWhenImmediateCmdListCreatedWithFlushTaskSetThenSharedHeapsIsNotEnabled, IsAtLeastSkl) {
    MockL0HwHelperNoSupportsCmdListHeapSharingHw<FamilyType> mockL0HwHelperNoSupport;
    VariableBackup<L0HwHelper *> l0HwHelperFactoryBackup{&L0::l0HwHelperFactory[static_cast<size_t>(device->getHwInfo().platform.eRenderCoreFamily)]};
    l0HwHelperFactoryBackup = &mockL0HwHelperNoSupport;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandListImmediate(whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue)));
    ASSERT_NE(nullptr, commandListImmediate);

    EXPECT_TRUE(commandListImmediate->isFlushTaskSubmissionEnabled);
    EXPECT_FALSE(commandListImmediate->immediateCmdListHeapSharing);

    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(0);

    commandListImmediate.reset(whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue)));
    ASSERT_NE(nullptr, commandListImmediate);

    EXPECT_FALSE(commandListImmediate->isFlushTaskSubmissionEnabled);
    EXPECT_FALSE(commandListImmediate->immediateCmdListHeapSharing);
}

} // namespace ult
} // namespace L0
