/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/task_count_helper.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

#include <atomic>
#include <functional>
#include <mutex>

namespace NEO {
struct SvmAllocationData;
struct CompletionStamp;
class LinearStream;
} // namespace NEO

namespace L0 {
using CsrMutex = std::recursive_mutex;
using MutexLock = std::unique_lock<CsrMutex>;

struct EventPool;
struct Event;
inline constexpr size_t commonImmediateCommandSize = 4 * MemoryConstants::kiloByte;

struct CpuMemCopyInfo {
    void *const dstPtr;
    void *const srcPtr;
    const size_t size;
    NEO::SvmAllocationData *dstAllocData{nullptr};
    NEO::SvmAllocationData *srcAllocData{nullptr};
    bool dstIsImportedHostPtr = false;
    bool srcIsImportedHostPtr = false;

    CpuMemCopyInfo(void *dstPtr, void *srcPtr, size_t size) : dstPtr(dstPtr), srcPtr(srcPtr), size(size) {}
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamilyImmediate : public CommandListCoreFamily<gfxCoreFamily> {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using BaseClass = CommandListCoreFamily<gfxCoreFamily>;
    using BaseClass::assignInOrderExecInfoToEvent;
    using BaseClass::BaseClass;
    using BaseClass::copyThroughLockedPtrEnabled;
    using BaseClass::getCopyOffloadModeForOperation;
    using BaseClass::getCsr;
    using BaseClass::isCopyOffloadEnabled;
    using BaseClass::isCopyOnly;
    using BaseClass::isDualStreamCopyOffloadOperation;
    using BaseClass::isInOrderExecutionEnabled;
    using BaseClass::isSkippingInOrderBarrierAllowed;
    using BaseClass::isTbxMode;
    using BaseClass::patternAllocations;

    using ComputeFlushMethodType = NEO::CompletionStamp (CommandListCoreFamilyImmediate<gfxCoreFamily>::*)(NEO::LinearStream &, size_t, bool, bool, NEO::AppendOperations, bool);

    CommandListCoreFamilyImmediate(uint32_t numIddsPerBlock);

    ze_result_t appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                   const ze_group_count_t &threadGroupDimensions,
                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents,
                                   CmdListKernelLaunchParams &launchParams) override;

    ze_result_t appendLaunchKernelIndirect(ze_kernel_handle_t kernelHandle,
                                           const ze_group_count_t &pDispatchArgumentsBuffer,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override;

    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent,
                              uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override;

    ze_result_t appendMemoryCopy(void *dstptr,
                                 const void *srcptr,
                                 size_t size,
                                 ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

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
                                       ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                 size_t patternSize, size_t size,
                                 ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendSignalEvent(ze_event_handle_t hEvent, bool relaxedOrderingDispatch) override;

    ze_result_t appendEventReset(ze_event_handle_t hEvent) override;

    ze_result_t appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation,
                                    NEO::GraphicsAllocation *srcAllocation,
                                    size_t size, bool flushHost) override;

    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                                   bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) override;

    ze_result_t appendWriteGlobalTimestamp(uint64_t *dstptr, ze_event_handle_t hSignalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendMemoryCopyFromContext(void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
                                            size_t size, ze_event_handle_t hSignalEvent,
                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override;

    ze_result_t appendImageCopyFromMemory(ze_image_handle_t hDstImage,
                                          const void *srcPtr,
                                          const ze_image_region_t *pDstRegion,
                                          ze_event_handle_t hEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendImageCopyToMemory(void *dstPtr,
                                        ze_image_handle_t hSrcImage,
                                        const ze_image_region_t *pSrcRegion,
                                        ze_event_handle_t hEvent,
                                        uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendImageCopyFromMemoryExt(ze_image_handle_t hDstImage,
                                             const void *srcPtr,
                                             const ze_image_region_t *pDstRegion,
                                             uint32_t srcRowPitch,
                                             uint32_t srcSlicePitch,
                                             ze_event_handle_t hEvent,
                                             uint32_t numWaitEvents,
                                             ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendImageCopyToMemoryExt(void *dstPtr,
                                           ze_image_handle_t hSrcImage,
                                           const ze_image_region_t *pSrcRegion,
                                           uint32_t destRowPitch,
                                           uint32_t destSlicePitch,
                                           ze_event_handle_t hEvent,
                                           uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendImageCopy(ze_image_handle_t dst, ze_image_handle_t src,
                                ze_event_handle_t hEvent,
                                uint32_t numWaitEvents,
                                ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendImageCopyRegion(ze_image_handle_t hDstImage,
                                      ze_image_handle_t hSrcImage,
                                      const ze_image_region_t *pDstRegion,
                                      const ze_image_region_t *pSrcRegion,
                                      ze_event_handle_t hEvent,
                                      uint32_t numWaitEvents,
                                      ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendMemoryRangesBarrier(uint32_t numRanges,
                                          const size_t *pRangeSizes,
                                          const void **pRanges,
                                          ze_event_handle_t hSignalEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendWaitOnMemory(void *desc, void *ptr, uint64_t data, ze_event_handle_t signalEventHandle, bool useQwordData) override;

    ze_result_t appendWriteToMemory(void *desc, void *ptr,
                                    uint64_t data) override;

    ze_result_t appendWaitExternalSemaphores(uint32_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                             const ze_external_semaphore_wait_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                             uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendSignalExternalSemaphores(size_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                               const ze_external_semaphore_signal_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t hostSynchronize(uint64_t timeout) override;

    ze_result_t close() override {
        return ZE_RESULT_SUCCESS;
    }

    MOCKABLE_VIRTUAL ze_result_t executeCommandListImmediateWithFlushTask(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation,
                                                                          bool copyOffloadSubmission, bool requireTaskCountUpdate,
                                                                          MutexLock *outerLock,
                                                                          std::unique_lock<std::mutex> *outerLockForIndirect);
    ze_result_t executeCommandListImmediateWithFlushTaskImpl(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation,
                                                             bool requireTaskCountUpdate,
                                                             CommandQueue *cmdQ,
                                                             MutexLock *outerLock,
                                                             std::unique_lock<std::mutex> *outerLockForIndirect);
    ze_result_t appendCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                                   ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    NEO::CompletionStamp flushRegularTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation, bool requireTaskCountUpdate);
    NEO::CompletionStamp flushImmediateRegularTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation, bool requireTaskCountUpdate);
    NEO::CompletionStamp flushImmediateRegularTaskStateless(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation, bool requireTaskCountUpdate);
    NEO::CompletionStamp flushBcsTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool requireTaskCountUpdate, NEO::AppendOperations appendOperation, NEO::CommandStreamReceiver *csr);

    template <bool streamStatesSupported>
    void handleHeapsAndResidencyForImmediateRegularTask(void *&sshCpuBaseAddress);
    void handleDebugSurfaceStateUpdate(NEO::IndirectHeap *ssh);

    void checkAvailableSpace(uint32_t numEvents, bool hasRelaxedOrderingDependencies, size_t commandSize, bool requestCommandBufferInLocalMem);
    void updateDispatchFlagsWithRequiredStreamState(NEO::DispatchFlags &dispatchFlags);

    MOCKABLE_VIRTUAL ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies,
                                                NEO::AppendOperations appendOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent, bool requireTaskCountUpdate,
                                                MutexLock *outerLock,
                                                std::unique_lock<std::mutex> *outerLockForIndirect);

    bool preferCopyThroughLockedPtr(CpuMemCopyInfo &cpuMemCopyInfo, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
    bool isSuitableUSMHostAlloc(NEO::SvmAllocationData *alloc);
    bool isSuitableUSMDeviceAlloc(NEO::SvmAllocationData *alloc);
    bool isSuitableUSMSharedAlloc(NEO::SvmAllocationData *alloc);
    ze_result_t performCpuMemcpy(const CpuMemCopyInfo &cpuMemCopyInfo, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
    void *obtainLockedPtrFromDevice(NEO::SvmAllocationData *alloc, void *ptr, bool &lockingFailed);
    TransferType getTransferType(const CpuMemCopyInfo &cpuMemCopyInfo);
    size_t getTransferThreshold(TransferType transferType);
    bool isBarrierRequired();
    bool isRelaxedOrderingDispatchAllowed(uint32_t numWaitEvents, bool copyOffload) override;
    bool skipInOrderNonWalkerSignalingAllowed(ze_event_handle_t signalEvent) const override;

  protected:
    using BaseClass::inOrderExecInfo;

    void printKernelsPrintfOutput(bool hangDetected);
    ze_result_t flushInOrderCounterSignal(bool waitOnInOrderCounterRequired) override;
    MOCKABLE_VIRTUAL ze_result_t synchronizeInOrderExecution(uint64_t timeout, bool copyOffloadSync) const;
    ze_result_t hostSynchronize(uint64_t timeout, bool handlePostWaitOperations);
    bool hasStallingCmdsForRelaxedOrdering(uint32_t numWaitEvents, bool relaxedOrderingDispatch) const;
    void setupFlushMethod(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) override;
    void allocateOrReuseKernelPrivateMemoryIfNeeded(Kernel *kernel, uint32_t sizePerHwThread) override;
    void handleInOrderNonWalkerSignaling(Event *event, bool &hasStallingCmds, bool &relaxedOrderingDispatch, ze_result_t &result);
    CommandQueue *getCmdQImmediate(CopyOffloadMode copyOffloadMode) const;
    NEO::LinearStream *getOptionalEpilogueCmdStream(NEO::LinearStream *taskCmdStream, NEO::AppendOperations appendOperation);

    MOCKABLE_VIRTUAL void checkAssert();
    ComputeFlushMethodType computeFlushMethod = nullptr;
    uint64_t relaxedOrderingCounter = 0;
    std::atomic<bool> dependenciesPresent{false};
    bool latestFlushIsHostVisible = false;
    bool latestFlushIsDualCopyOffload = false;
    bool keepRelaxedOrderingEnabled = false;
};

template <PRODUCT_FAMILY gfxProductFamily>
struct CommandListImmediateProductFamily;
} // namespace L0
