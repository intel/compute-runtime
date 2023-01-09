/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace NEO {
struct SvmAllocationData;
struct CompletionStamp;
class LinearStream;
} // namespace NEO

namespace L0 {

struct EventPool;
struct Event;
inline constexpr size_t maxImmediateCommandSize = 4 * MemoryConstants::kiloByte;

struct CpuMemCopyInfo {
    void *const dstPtr;
    const void *const srcPtr;
    const size_t size;
    NEO::SvmAllocationData *dstAllocData{nullptr};
    NEO::SvmAllocationData *srcAllocData{nullptr};

    CpuMemCopyInfo(void *dstPtr, const void *srcPtr, size_t size) : dstPtr(dstPtr), srcPtr(srcPtr), size(size) {}
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamilyImmediate : public CommandListCoreFamily<gfxCoreFamily> {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using BaseClass = CommandListCoreFamily<gfxCoreFamily>;
    using BaseClass::BaseClass;
    using BaseClass::executeCommandListImmediate;
    using BaseClass::isCopyOnly;

    ze_result_t appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                   const ze_group_count_t *threadGroupDimensions,
                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents,
                                   const CmdListKernelLaunchParams &launchParams) override;

    ze_result_t appendLaunchKernelIndirect(ze_kernel_handle_t kernelHandle,
                                           const ze_group_count_t *pDispatchArgumentsBuffer,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent,
                              uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendMemoryCopy(void *dstptr,
                                 const void *srcptr,
                                 size_t size,
                                 ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendMemoryCopyRegion(void *dstPtr,
                                       const ze_copy_region_t *dstRegion,
                                       uint32_t dstPitch,
                                       uint32_t dstSlicePitch,
                                       const void *srcPtr,
                                       const ze_copy_region_t *srcRegion,
                                       uint32_t srcPitch,
                                       uint32_t srcSlicePitch,
                                       ze_event_handle_t hSignalEvent,
                                       uint32_t numWaitEvents,
                                       ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                 size_t patternSize, size_t size,
                                 ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendSignalEvent(ze_event_handle_t hEvent) override;

    ze_result_t appendEventReset(ze_event_handle_t hEvent) override;

    ze_result_t appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation,
                                    NEO::GraphicsAllocation *srcAllocation,
                                    size_t size, bool flushHost) override;

    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, bool relaxedOrderingAllowed) override;

    ze_result_t appendWriteGlobalTimestamp(uint64_t *dstptr, ze_event_handle_t hSignalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendMemoryCopyFromContext(void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
                                            size_t size, ze_event_handle_t hSignalEvent,
                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendImageCopyFromMemory(ze_image_handle_t hDstImage,
                                          const void *srcPtr,
                                          const ze_image_region_t *pDstRegion,
                                          ze_event_handle_t hEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendImageCopyToMemory(void *dstPtr,
                                        ze_image_handle_t hSrcImage,
                                        const ze_image_region_t *pSrcRegion,
                                        ze_event_handle_t hEvent,
                                        uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendImageCopy(
        ze_image_handle_t dst, ze_image_handle_t src,
        ze_event_handle_t hEvent,
        uint32_t numWaitEvents,
        ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendImageCopyRegion(ze_image_handle_t hDstImage,
                                      ze_image_handle_t hSrcImage,
                                      const ze_image_region_t *pDstRegion,
                                      const ze_image_region_t *pSrcRegion,
                                      ze_event_handle_t hEvent,
                                      uint32_t numWaitEvents,
                                      ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendMemoryRangesBarrier(uint32_t numRanges,
                                          const size_t *pRangeSizes,
                                          const void **pRanges,
                                          ze_event_handle_t hSignalEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendLaunchCooperativeKernel(ze_kernel_handle_t kernelHandle,
                                              const ze_group_count_t *launchKernelArgs,
                                              ze_event_handle_t hSignalEvent,
                                              uint32_t numWaitEvents,
                                              ze_event_handle_t *waitEventHandles) override;

    MOCKABLE_VIRTUAL ze_result_t executeCommandListImmediateWithFlushTask(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies);
    ze_result_t executeCommandListImmediateWithFlushTaskImpl(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, CommandQueue *cmdQ);

    NEO::CompletionStamp flushRegularTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies);
    NEO::CompletionStamp flushBcsTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::CommandStreamReceiver *csr);

    void checkAvailableSpace();
    void updateDispatchFlagsWithRequiredStreamState(NEO::DispatchFlags &dispatchFlags);

    ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, ze_event_handle_t hSignalEvent);

    void createLogicalStateHelper() override {}
    NEO::LogicalStateHelper *getLogicalStateHelper() const override;

    bool preferCopyThroughLockedPtr(CpuMemCopyInfo &cpuMemCopyInfo);
    bool isSuitableUSMHostAlloc(NEO::SvmAllocationData *alloc);
    bool isSuitableUSMDeviceAlloc(NEO::SvmAllocationData *alloc);
    bool isSuitableUSMSharedAlloc(NEO::SvmAllocationData *alloc);
    ze_result_t performCpuMemcpy(const CpuMemCopyInfo &cpuMemCopyInfo, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
    void *obtainLockedPtrFromDevice(NEO::SvmAllocationData *alloc, void *ptr);
    bool waitForEventsFromHost();
    void checkWaitEventsState(uint32_t numWaitEvents, ze_event_handle_t *waitEventList);
    TransferType getTransferType(NEO::SvmAllocationData *dstAlloc, NEO::SvmAllocationData *srcAlloc);

  protected:
    void printKernelsPrintfOutput(bool hangDetected);
    std::atomic<bool> dependenciesPresent{false};
};

template <PRODUCT_FAMILY gfxProductFamily>
struct CommandListImmediateProductFamily;
} // namespace L0
