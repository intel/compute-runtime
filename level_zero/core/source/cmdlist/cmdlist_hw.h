/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cmdlist/cmdlist_imp.h"

#include "igfxfmid.h"

namespace NEO {
enum class ImageType;
}

namespace L0 {
struct AlignedAllocationData {
    uintptr_t alignedAllocationPtr = 0u;
    size_t offset = 0u;
    NEO::GraphicsAllocation *alloc = nullptr;
    bool needsFlush = false;
};

struct EventPool;
struct Event;

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamily : CommandListImp {
    using BaseClass = CommandListImp;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    using CommandListImp::CommandListImp;
    ze_result_t initialize(Device *device, NEO::EngineGroupType engineGroupType) override;
    virtual void programL3(bool isSLMused);

    ze_result_t close() override;
    ze_result_t appendEventReset(ze_event_handle_t hEvent) override;
    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendMemoryRangesBarrier(uint32_t numRanges,
                                          const size_t *pRangeSizes,
                                          const void **pRanges,
                                          ze_event_handle_t hSignalEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendImageCopyFromMemory(ze_image_handle_t hDstImage, const void *srcptr,
                                          const ze_image_region_t *pDstRegion,
                                          ze_event_handle_t hEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendImageCopyToMemory(void *dstptr, ze_image_handle_t hSrcImage,
                                        const ze_image_region_t *pSrcRegion, ze_event_handle_t hEvent,
                                        uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendImageCopyRegion(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                                      const ze_image_region_t *pDstRegion, const ze_image_region_t *pSrcRegion,
                                      ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                      ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendImageCopy(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                                ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendLaunchKernel(ze_kernel_handle_t hKernel,
                                   const ze_group_count_t *pThreadGroupDimensions,
                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendLaunchCooperativeKernel(ze_kernel_handle_t hKernel,
                                              const ze_group_count_t *pLaunchFuncArgs,
                                              ze_event_handle_t hSignalEvent,
                                              uint32_t numWaitEvents,
                                              ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendLaunchKernelIndirect(ze_kernel_handle_t hKernel,
                                           const ze_group_count_t *pDispatchArgumentsBuffer,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendLaunchMultipleKernelsIndirect(uint32_t numKernels,
                                                    const ze_kernel_handle_t *phKernels,
                                                    const uint32_t *pNumLaunchArguments,
                                                    const ze_group_count_t *pLaunchArgumentsBuffer,
                                                    ze_event_handle_t hEvent,
                                                    uint32_t numWaitEvents,
                                                    ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendMemAdvise(ze_device_handle_t hDevice,
                                const void *ptr, size_t size,
                                ze_memory_advice_t advice) override;
    ze_result_t appendMemoryCopy(void *dstptr, const void *srcptr, size_t size,
                                 ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendPageFaultCopy(NEO::GraphicsAllocation *dstptr,
                                    NEO::GraphicsAllocation *srcptr,
                                    size_t size,
                                    bool flushHost) override;
    ze_result_t appendMemoryCopyRegion(void *dstPtr,
                                       const ze_copy_region_t *dstRegion,
                                       uint32_t dstPitch,
                                       uint32_t dstSlicePitch,
                                       const void *srcPtr,
                                       const ze_copy_region_t *srcRegion,
                                       uint32_t srcPitch,
                                       uint32_t srcSlicePitch,
                                       ze_event_handle_t hSignalEvent) override;
    ze_result_t appendMemoryPrefetch(const void *ptr, size_t count) override;
    ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                 size_t patternSize, size_t size,
                                 ze_event_handle_t hEvent) override;

    ze_result_t appendMILoadRegImm(uint32_t reg, uint32_t value) override;
    ze_result_t appendMILoadRegReg(uint32_t reg1, uint32_t reg2) override;
    ze_result_t appendMILoadRegMem(uint32_t reg1, uint64_t address) override;
    ze_result_t appendMIStoreRegMem(uint32_t reg1, uint64_t address) override;
    ze_result_t appendMIMath(void *aluArray, size_t aluCount) override;
    ze_result_t appendMIBBStart(uint64_t address, size_t predication, bool secondLevel) override;
    ze_result_t appendMIBBEnd() override;
    ze_result_t appendMINoop() override;
    ze_result_t appendPipeControl(void *dstPtr, uint64_t value) override;

    ze_result_t appendQueryKernelTimestamps(uint32_t numEvents, ze_event_handle_t *phEvents, void *dstptr,
                                            const size_t *pOffsets, ze_event_handle_t hSignalEvent,
                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendSignalEvent(ze_event_handle_t hEvent) override;
    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent) override;
    ze_result_t appendWriteGlobalTimestamp(uint64_t *dstptr, ze_event_handle_t hSignalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendMemoryCopyFromContext(void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
                                            size_t size, ze_event_handle_t hSignalEvent,
                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t reserveSpace(size_t size, void **ptr) override;
    ze_result_t reset() override;
    ze_result_t executeCommandListImmediate(bool performMigration) override;

  protected:
    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyKernelWithGA(void *dstPtr, NEO::GraphicsAllocation *dstPtrAlloc,
                                                              uint64_t dstOffset, void *srcPtr,
                                                              NEO::GraphicsAllocation *srcPtrAlloc,
                                                              uint64_t srcOffset, uint32_t size,
                                                              uint32_t elementSize, Builtin builtin);

    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                                      NEO::GraphicsAllocation *dstPtrAlloc,
                                                      uint64_t dstOffset, uintptr_t srcPtr,
                                                      NEO::GraphicsAllocation *srcPtrAlloc,
                                                      uint64_t srcOffset,
                                                      uint32_t size,
                                                      ze_event_handle_t hSignalEvent);

    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcAlloc,
                                                            NEO::GraphicsAllocation *dstAlloc,
                                                            ze_copy_region_t srcRegion,
                                                            ze_copy_region_t dstRegion, Vec3<size_t> copySize,
                                                            size_t srcRowPitch, size_t srcSlicePitch,
                                                            size_t dstRowPitch, size_t dstSlicePitch,
                                                            Vec3<uint32_t> srcSize, Vec3<uint32_t> dstSize, ze_event_handle_t hSignalEvent);

    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyKernel2d(NEO::GraphicsAllocation *dstAlloc, NEO::GraphicsAllocation *srcAlloc,
                                                          Builtin builtin, const ze_copy_region_t *dstRegion,
                                                          uint32_t dstPitch, size_t dstOffset,
                                                          const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                                          size_t srcOffset, ze_event_handle_t hSignalEvent,
                                                          uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);

    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyKernel3d(NEO::GraphicsAllocation *dstAlloc, NEO::GraphicsAllocation *srcAlloc,
                                                          Builtin builtin, const ze_copy_region_t *dstRegion,
                                                          uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                                          const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                                          uint32_t srcSlicePitch, size_t srcOffset,
                                                          ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                                          ze_event_handle_t *phWaitEvents);

    MOCKABLE_VIRTUAL ze_result_t appendBlitFill(void *ptr, const void *pattern,
                                                size_t patternSize, size_t size,
                                                ze_event_handle_t hEvent);

    MOCKABLE_VIRTUAL ze_result_t appendCopyImageBlit(NEO::GraphicsAllocation *src,
                                                     NEO::GraphicsAllocation *dst,
                                                     Vec3<size_t> srcOffsets, Vec3<size_t> dstOffsets,
                                                     size_t srcRowPitch, size_t srcSlicePitch,
                                                     size_t dstRowPitch, size_t dstSlicePitch,
                                                     size_t bytesPerPixel, Vec3<size_t> copySize,
                                                     Vec3<uint32_t> srcSize, Vec3<uint32_t> dstSize, ze_event_handle_t hSignalEvent);

    ze_result_t appendLaunchKernelWithParams(ze_kernel_handle_t hKernel,
                                             const ze_group_count_t *pThreadGroupDimensions,
                                             ze_event_handle_t hEvent,
                                             bool isIndirect,
                                             bool isPredicate);

    ze_result_t prepareIndirectParams(const ze_group_count_t *pThreadGroupDimensions);

    void applyMemoryRangesBarrier(uint32_t numRanges, const size_t *pRangeSizes,
                                  const void **pRanges);

    ze_result_t setGlobalWorkSizeIndirect(NEO::CrossThreadDataOffset offsets[3], void *crossThreadAddress, uint32_t lws[3]);
    void appendEventForProfiling(ze_event_handle_t hEvent, bool beforeWalker);
    void appendEventForProfilingCopyCommand(ze_event_handle_t hEvent, bool beforeWalker);
    void appendSignalEventPostWalker(ze_event_handle_t hEvent);
    bool useMemCopyToBlitFill(size_t patternSize);
    void programStateBaseAddress(NEO::CommandContainer &container, bool genericMediaStateClearRequired);

    uint64_t getInputBufferSize(NEO::ImageType imageType, uint64_t bytesPerPixel, const ze_image_region_t *region);
    virtual AlignedAllocationData getAlignedAllocation(Device *device, const void *buffer, uint64_t bufferSize);
    ze_result_t addEventsToCmdList(ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
};

template <PRODUCT_FAMILY gfxProductFamily>
struct CommandListProductFamily;

} // namespace L0
