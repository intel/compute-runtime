/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include <unordered_map>

namespace NEO {
class GraphicsAllocation;
}

namespace L0 {
struct Device;

namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>
    : public ::L0::CommandListCoreFamily<gfxCoreFamily> {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using BaseClass = ::L0::CommandListCoreFamily<gfxCoreFamily>;
    using BaseClass::addCmdForPatching;
    using BaseClass::addFlushRequiredCommand;
    using BaseClass::addPatchScratchAddressInImplicitArgs;
    using BaseClass::allocateOrReuseKernelPrivateMemoryIfNeeded;
    using BaseClass::allowCbWaitEventsNoopDispatch;
    using BaseClass::appendBlitFill;
    using BaseClass::appendCopyImageBlit;
    using BaseClass::appendDispatchOffsetRegister;
    using BaseClass::appendEventForProfiling;
    using BaseClass::appendEventForProfilingCopyCommand;
    using BaseClass::appendLaunchKernelWithParams;
    using BaseClass::appendMemoryCopyBlit;
    using BaseClass::appendMemoryCopyBlitRegion;
    using BaseClass::appendMultiTileBarrier;
    using BaseClass::appendSignalEventPostWalker;
    using BaseClass::appendWriteKernelTimestamp;
    using BaseClass::applyMemoryRangesBarrier;
    using BaseClass::clearCommandsToPatch;
    using BaseClass::cmdListHeapAddressModel;
    using BaseClass::cmdListType;
    using BaseClass::cmdQImmediate;
    using BaseClass::cmdQImmediateCopyOffload;
    using BaseClass::commandContainer;
    using BaseClass::commandListPerThreadScratchSize;
    using BaseClass::commandListPreemptionMode;
    using BaseClass::commandsToPatch;
    using BaseClass::compactL3FlushEvent;
    using BaseClass::compactL3FlushEventPacket;
    using BaseClass::containsAnyKernel;
    using BaseClass::containsCooperativeKernelsFlag;
    using BaseClass::copyOperationFenceSupported;
    using BaseClass::copyOperationOffloadEnabled;
    using BaseClass::currentBindingTablePoolBaseAddress;
    using BaseClass::currentDynamicStateBaseAddress;
    using BaseClass::currentIndirectObjectBaseAddress;
    using BaseClass::currentSurfaceStateBaseAddress;
    using BaseClass::dcFlushSupport;
    using BaseClass::device;
    using BaseClass::disablePatching;
    using BaseClass::dispatchCmdListBatchBufferAsPrimary;
    using BaseClass::doubleSbaWa;
    using BaseClass::enablePatching;
    using BaseClass::engineGroupType;
    using BaseClass::estimateBufferSizeMultiTileBarrier;
    using BaseClass::eventSignalPipeControl;
    using BaseClass::finalStreamState;
    using BaseClass::flags;
    using BaseClass::frontEndStateTracking;
    using BaseClass::getAlignedAllocationData;
    using BaseClass::getAllocationFromHostPtrMap;
    using BaseClass::getDcFlushRequired;
    using BaseClass::getHostPtrAlloc;
    using BaseClass::getInOrderIncrementValue;
    using BaseClass::heaplessModeEnabled;
    using BaseClass::heaplessStateInitEnabled;
    using BaseClass::hostPtrMap;
    using BaseClass::immediateCmdListHeapSharing;
    using BaseClass::indirectAllocationsAllowed;
    using BaseClass::initialize;
    using BaseClass::inOrderAtomicSignalingEnabled;
    using BaseClass::inOrderExecInfo;
    using BaseClass::inOrderPatchCmds;
    using BaseClass::interruptEvents;
    using BaseClass::isFlushTaskSubmissionEnabled;
    using BaseClass::isInOrderNonWalkerSignalingRequired;
    using BaseClass::isQwordInOrderCounter;
    using BaseClass::isRelaxedOrderingDispatchAllowed;
    using BaseClass::isSyncModeQueue;
    using BaseClass::isTbxMode;
    using BaseClass::isTimestampEventForMultiTile;
    using BaseClass::latestOperationRequiredNonWalkerInOrderCmdsChaining;
    using BaseClass::obtainKernelPreemptionMode;
    using BaseClass::partitionCount;
    using BaseClass::patternAllocations;
    using BaseClass::pipeControlMultiKernelEventSync;
    using BaseClass::pipelineSelectStateTracking;
    using BaseClass::requiredStreamState;
    using BaseClass::requiresDcFlushForDcMitigation;
    using BaseClass::requiresQueueUncachedMocs;
    using BaseClass::scratchAddressPatchingEnabled;
    using BaseClass::setupTimestampEventForMultiTile;
    using BaseClass::signalAllEventPackets;
    using BaseClass::stateBaseAddressTracking;
    using BaseClass::stateComputeModeTracking;
    using BaseClass::syncDispatchQueueId;
    using BaseClass::synchronizedDispatchMode;
    using BaseClass::unifiedMemoryControls;
    using BaseClass::updateInOrderExecInfo;
    using BaseClass::updateStreamProperties;
    using BaseClass::updateStreamPropertiesForFlushTaskDispatchFlags;
    using BaseClass::updateStreamPropertiesForRegularCommandLists;

    WhiteBox() : ::L0::CommandListCoreFamily<gfxCoreFamily>(BaseClass::defaultNumIddsPerBlock) {}

    ze_result_t appendLaunchKernelWithParams(::L0::Kernel *kernel,
                                             const ze_group_count_t &threadGroupDimensions,
                                             ::L0::Event *event,
                                             CmdListKernelLaunchParams &launchParams) override {

        kernelUsed = kernel;
        usedKernelLaunchParams = launchParams;
        if (launchParams.isKernelSplitOperation && (launchParams.numKernelsExecutedInSplitLaunch == 0)) {
            firstKernelInSplitOperation = kernel;
        }
        appendKernelEventValue = event;
        return BaseClass::appendLaunchKernelWithParams(kernel, threadGroupDimensions,
                                                       event, launchParams);
    }

    ze_result_t appendLaunchMultipleKernelsIndirect(uint32_t numKernels,
                                                    const ze_kernel_handle_t *kernelHandles,
                                                    const uint32_t *pNumLaunchArguments,
                                                    const ze_group_count_t *pLaunchArgumentsBuffer,
                                                    ze_event_handle_t hEvent,
                                                    uint32_t numWaitEvents,
                                                    ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override {
        appendEventMultipleKernelIndirectEventHandleValue = hEvent;
        return BaseClass::appendLaunchMultipleKernelsIndirect(numKernels, kernelHandles, pNumLaunchArguments, pLaunchArgumentsBuffer,
                                                              hEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);
    }

    ze_result_t appendLaunchKernelIndirect(ze_kernel_handle_t kernelHandle,
                                           const ze_group_count_t &pDispatchArgumentsBuffer,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override {
        appendEventKernelIndirectEventHandleValue = hEvent;
        return BaseClass::appendLaunchKernelIndirect(kernelHandle, pDispatchArgumentsBuffer,
                                                     hEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);
    }

    size_t getOwnedPrivateAllocationsSize() {
        return this->ownedPrivateAllocations.size();
    }

    CmdListKernelLaunchParams usedKernelLaunchParams;
    ::L0::Event *appendKernelEventValue = nullptr;
    ::L0::Kernel *firstKernelInSplitOperation = nullptr;
    ze_event_handle_t appendEventMultipleKernelIndirectEventHandleValue = nullptr;
    ze_event_handle_t appendEventKernelIndirectEventHandleValue = nullptr;
    Kernel *kernelUsed;
};

template <GFXCORE_FAMILY gfxCoreFamily>
using CommandListCoreFamily = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;

template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>
    : public L0::CommandListCoreFamilyImmediate<gfxCoreFamily> {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using BaseClass = L0::CommandListCoreFamilyImmediate<gfxCoreFamily>;
    using BaseClass::addCmdForPatching;
    using BaseClass::allowCbWaitEventsNoopDispatch;
    using BaseClass::appendBlitFill;
    using BaseClass::appendLaunchKernelWithParams;
    using BaseClass::appendMemoryCopyBlitRegion;
    using BaseClass::clearCommandsToPatch;
    using BaseClass::cmdListHeapAddressModel;
    using BaseClass::cmdListType;
    using BaseClass::cmdQImmediate;
    using BaseClass::cmdQImmediateCopyOffload;
    using BaseClass::commandContainer;
    using BaseClass::commandsToPatch;
    using BaseClass::compactL3FlushEvent;
    using BaseClass::compactL3FlushEventPacket;
    using BaseClass::copyOperationFenceSupported;
    using BaseClass::copyOperationOffloadEnabled;
    using BaseClass::dcFlushSupport;
    using BaseClass::device;
    using BaseClass::disablePatching;
    using BaseClass::dispatchEventRemainingPacketsPostSyncOperation;
    using BaseClass::doubleSbaWa;
    using BaseClass::enablePatching;
    using BaseClass::engineGroupType;
    using BaseClass::eventSignalPipeControl;
    using BaseClass::finalStreamState;
    using BaseClass::frontEndStateTracking;
    using BaseClass::getCmdQImmediate;
    using BaseClass::getDcFlushRequired;
    using BaseClass::getHostPtrAlloc;
    using BaseClass::getInOrderIncrementValue;
    using BaseClass::hostSynchronize;
    using BaseClass::immediateCmdListHeapSharing;
    using BaseClass::inOrderAtomicSignalingEnabled;
    using BaseClass::inOrderExecInfo;
    using BaseClass::inOrderPatchCmds;
    using BaseClass::interruptEvents;
    using BaseClass::isBcsSplitNeeded;
    using BaseClass::isFlushTaskSubmissionEnabled;
    using BaseClass::isInOrderNonWalkerSignalingRequired;
    using BaseClass::isQwordInOrderCounter;
    using BaseClass::isSyncModeQueue;
    using BaseClass::isTbxMode;
    using BaseClass::latestFlushIsHostVisible;
    using BaseClass::latestOperationRequiredNonWalkerInOrderCmdsChaining;
    using BaseClass::partitionCount;
    using BaseClass::pipeControlMultiKernelEventSync;
    using BaseClass::pipelineSelectStateTracking;
    using BaseClass::programRegionGroupBarrier;
    using BaseClass::relaxedOrderingCounter;
    using BaseClass::requiredStreamState;
    using BaseClass::requiresQueueUncachedMocs;
    using BaseClass::signalAllEventPackets;
    using BaseClass::stateBaseAddressTracking;
    using BaseClass::stateComputeModeTracking;
    using BaseClass::syncDispatchQueueId;
    using BaseClass::synchronizedDispatchMode;
    using BaseClass::synchronizeInOrderExecution;
    using BaseClass::updateInOrderExecInfo;

    WhiteBox() : BaseClass(BaseClass::defaultNumIddsPerBlock) {}

    ADDMETHOD_CONST(synchronizeInOrderExecution, ze_result_t, true, ZE_RESULT_SUCCESS,
                    (uint64_t timeout, bool copyOffloadSync), (timeout, copyOffloadSync));
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandListImmediate : public CommandListCoreFamilyImmediate<gfxCoreFamily> {
    using BaseClass = CommandListCoreFamilyImmediate<gfxCoreFamily>;
    using BaseClass::checkAssert;
    using BaseClass::cmdListCurrentStartOffset;
    using BaseClass::cmdListHeapAddressModel;
    using BaseClass::cmdQImmediate;
    using BaseClass::commandContainer;
    using BaseClass::compactL3FlushEventPacket;
    using BaseClass::containsAnyKernel;

    using BaseClass::device;
    using BaseClass::dynamicHeapRequired;
    using BaseClass::finalStreamState;
    using BaseClass::immediateCmdListHeapSharing;
    using BaseClass::indirectAllocationsAllowed;
    using BaseClass::isFlushTaskSubmissionEnabled;
    using BaseClass::isSyncModeQueue;
    using BaseClass::isTbxMode;
    using BaseClass::partitionCount;
    using BaseClass::pipeControlMultiKernelEventSync;
    using BaseClass::requiredStreamState;
    using CommandList::kernelWithAssertAppended;
};

template <>
struct WhiteBox<::L0::CommandListImp> : public ::L0::CommandListImp {
    using BaseClass = ::L0::CommandListImp;
    using BaseClass::BaseClass;
    using BaseClass::cmdListHeapAddressModel;
    using BaseClass::cmdListType;
    using BaseClass::cmdQImmediate;
    using BaseClass::cmdQImmediateCopyOffload;
    using BaseClass::commandContainer;
    using BaseClass::commandListPreemptionMode;
    using BaseClass::commandsToPatch;
    using BaseClass::copyOperationOffloadEnabled;
    using BaseClass::copyThroughLockedPtrEnabled;
    using BaseClass::currentBindingTablePoolBaseAddress;
    using BaseClass::currentDynamicStateBaseAddress;
    using BaseClass::currentIndirectObjectBaseAddress;
    using BaseClass::currentSurfaceStateBaseAddress;
    using BaseClass::device;
    using BaseClass::dispatchCmdListBatchBufferAsPrimary;
    using BaseClass::doubleSbaWa;
    using BaseClass::finalStreamState;
    using BaseClass::frontEndStateTracking;
    using BaseClass::getDcFlushRequired;
    using BaseClass::heaplessModeEnabled;
    using BaseClass::heaplessStateInitEnabled;
    using BaseClass::immediateCmdListHeapSharing;
    using BaseClass::initialize;
    using BaseClass::inOrderExecInfo;
    using BaseClass::interruptEvents;
    using BaseClass::isFlushTaskSubmissionEnabled;
    using BaseClass::isSyncModeQueue;
    using BaseClass::isTbxMode;
    using BaseClass::minimalSizeForBcsSplit;
    using BaseClass::partitionCount;
    using BaseClass::pipelineSelectStateTracking;
    using BaseClass::requiredStreamState;
    using BaseClass::requiresQueueUncachedMocs;
    using BaseClass::scratchAddressPatchingEnabled;
    using BaseClass::signalAllEventPackets;
    using BaseClass::stateBaseAddressTracking;
    using BaseClass::stateComputeModeTracking;
    using BaseClass::statelessBuiltinsEnabled;
    using CommandList::flags;
    using CommandList::internalUsage;
    using CommandList::kernelWithAssertAppended;

    WhiteBox();
    ~WhiteBox() override;

    static WhiteBox<L0::CommandListImp> *whiteboxCast(L0::CommandList *cmdlist) {
        return static_cast<WhiteBox<L0::CommandListImp> *>(static_cast<L0::CommandListImp *>(cmdlist));
    }
};

using CommandList = WhiteBox<::L0::CommandListImp>;

struct MockCommandList : public CommandList {
    using BaseClass = CommandList;

    MockCommandList(Device *device = nullptr);
    ~MockCommandList() override;

    ADDMETHOD_NOBASE(close, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE_VOIDRETURN(patchInOrderCmds, (void));

    ADDMETHOD_NOBASE(appendLaunchKernel, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_kernel_handle_t kernelHandle,
                      const ze_group_count_t &threadGroupDimensions,
                      ze_event_handle_t hEvent, uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents,
                      CmdListKernelLaunchParams &launchParams, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendLaunchKernelIndirect, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_kernel_handle_t kernelHandle,
                      const ze_group_count_t &DispatchArgumentsBuffer,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendLaunchMultipleKernelsIndirect, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numKernels,
                      const ze_kernel_handle_t *kernelHandles,
                      const uint32_t *pNumLaunchArguments,
                      const ze_group_count_t *pLaunchArgumentsBuffer,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendSoftwareTag, ze_result_t, ZE_RESULT_SUCCESS,
                     (const char *data));

    ADDMETHOD_NOBASE(appendEventReset, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hEvent));

    ADDMETHOD_NOBASE(appendBarrier, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendMemoryRangesBarrier, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numRanges,
                      const size_t *pRangeSizes,
                      const void **pRanges,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendImageCopyFromMemory, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      const void *srcptr,
                      const ze_image_region_t *pDstRegion,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendImageCopyToMemory, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      ze_image_handle_t hSrcImage,
                      const ze_image_region_t *pSrcRegion,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendImageCopyFromMemoryExt, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      const void *srcptr,
                      const ze_image_region_t *pDstRegion,
                      uint32_t srcRowPitch, uint32_t srcSlicePitch,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendImageCopyToMemoryExt, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      ze_image_handle_t hSrcImage,
                      const ze_image_region_t *pSrcRegion,
                      uint32_t destRowPitch, uint32_t destSlicePitch,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendImageCopyRegion, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      ze_image_handle_t hSrcImage,
                      const ze_image_region_t *pDstRegion,
                      const ze_image_region_t *pSrcRegion,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendImageCopy, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      ze_image_handle_t hSrcImage,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendMemAdvise, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_device_handle_t hDevice,
                      const void *ptr,
                      size_t size,
                      ze_memory_advice_t advice));

    ADDMETHOD_NOBASE(appendMemoryCopy, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      const void *srcptr,
                      size_t size,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams));

    ADDMETHOD_NOBASE(appendPageFaultCopy, ze_result_t, ZE_RESULT_SUCCESS,
                     (NEO::GraphicsAllocation * dstptr,
                      NEO::GraphicsAllocation *srcptr,
                      size_t size,
                      bool flushHost));

    ADDMETHOD_NOBASE(appendMemoryCopyRegion, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      const ze_copy_region_t *dstRegion,
                      uint32_t dstPitch,
                      uint32_t dstSlicePitch,
                      const void *srcptr,
                      const ze_copy_region_t *srcRegion,
                      uint32_t srcPitch,
                      uint32_t srcSlicePitch,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams));

    ADDMETHOD_NOBASE(appendMemoryPrefetch, ze_result_t, ZE_RESULT_SUCCESS,
                     (const void *ptr,
                      size_t count));

    ADDMETHOD_NOBASE(appendMemoryFill, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *ptr,
                      const void *pattern,
                      size_t pattern_size,
                      size_t size,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendSignalEvent, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hEvent, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(appendWaitOnEvents, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numEvents,
                      ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                      bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation));

    ADDMETHOD_NOBASE(appendWriteGlobalTimestamp, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint64_t * dstptr,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendQueryKernelTimestamps, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numEvents,
                      ze_event_handle_t *phEvents,
                      void *dstptr,
                      const size_t *pOffsets,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents))

    ADDMETHOD_NOBASE(appendMemoryCopyFromContext, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      ze_context_handle_t hContextSrc,
                      const void *srcptr,
                      size_t size,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch));

    ADDMETHOD_NOBASE(reserveSpace, ze_result_t, ZE_RESULT_SUCCESS,
                     (size_t size,
                      void **ptr));

    ADDMETHOD_NOBASE(reset, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendMetricMemoryBarrier, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendMetricStreamerMarker, ze_result_t, ZE_RESULT_SUCCESS,
                     (zet_metric_streamer_handle_t hMetricStreamer,
                      uint32_t value));

    ADDMETHOD_NOBASE(appendMetricQueryBegin, ze_result_t, ZE_RESULT_SUCCESS,
                     (zet_metric_query_handle_t hMetricQuery));

    ADDMETHOD_NOBASE(appendMetricQueryEnd, ze_result_t, ZE_RESULT_SUCCESS,
                     (zet_metric_query_handle_t hMetricQuery,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendMILoadRegImm, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t reg,
                      uint32_t value,
                      bool isBcs));

    ADDMETHOD_NOBASE(appendMILoadRegReg, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t reg1,
                      uint32_t reg2));

    ADDMETHOD_NOBASE(appendMILoadRegMem, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t reg1,
                      uint64_t address));

    ADDMETHOD_NOBASE(appendMIStoreRegMem, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t reg1,
                      uint64_t address));

    ADDMETHOD_NOBASE(appendMIMath, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *aluArray,
                      size_t aluCount));

    ADDMETHOD_NOBASE(appendMIBBStart, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint64_t address,
                      size_t predication,
                      bool secondLevel));

    ADDMETHOD_NOBASE(appendMIBBEnd, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendMINoop, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendPipeControl, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstPtr,
                      uint64_t value));
    ADDMETHOD_NOBASE(appendWaitOnMemory, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *desc, void *ptr, uint64_t data, ze_event_handle_t signalEventHandle, bool useQwordData));

    ADDMETHOD_NOBASE(appendWriteToMemory, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *desc, void *ptr,
                      uint64_t data));

    ADDMETHOD_NOBASE(executeCommandListImmediate, ze_result_t, ZE_RESULT_SUCCESS,
                     (bool perforMigration));

    ADDMETHOD_NOBASE(initialize, ze_result_t, ZE_RESULT_SUCCESS,
                     (L0::Device * device,
                      NEO::EngineGroupType engineGroupType,
                      ze_command_list_flags_t flags));

    ADDMETHOD_NOBASE_VOIDRETURN(appendMultiPartitionPrologue, (uint32_t partitionDataSize));
    ADDMETHOD_NOBASE_VOIDRETURN(appendMultiPartitionEpilogue, (void));
    ADDMETHOD_NOBASE(hostSynchronize, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint64_t timeout));
    ADDMETHOD_NOBASE(appendCommandLists, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                      ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents));

    uint8_t *batchBuffer = nullptr;
    NEO::GraphicsAllocation *mockAllocation = nullptr;
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListCoreFamily : public CommandListCoreFamily<gfxCoreFamily> {
  public:
    using BaseClass = CommandListCoreFamily<gfxCoreFamily>;
    using BaseClass::allocateOrReuseKernelPrivateMemoryIfNeeded;
    using BaseClass::commandContainer;
    using BaseClass::dcFlushSupport;
    using BaseClass::device;
    using BaseClass::dummyBlitWa;
    using BaseClass::enableInOrderExecution;
    using BaseClass::encodeMiFlush;
    using BaseClass::ownedPrivateAllocations;
    using BaseClass::taskCountUpdateFenceRequired;

    ADDMETHOD(appendMemoryCopyKernelWithGA, ze_result_t, false, ZE_RESULT_SUCCESS,
              (void *dstPtr, NEO::GraphicsAllocation *dstPtrAlloc,
               uint64_t dstOffset, void *srcPtr,
               NEO::GraphicsAllocation *srcPtrAlloc,
               uint64_t srcOffset, uint64_t size,
               uint64_t elementSize, Builtin builtin,
               L0::Event *signalEvent,
               bool isStateless,
               CmdListKernelLaunchParams &launchParams),
              (dstPtr, dstPtrAlloc, dstOffset, srcPtr, srcPtrAlloc, srcOffset, size, elementSize, builtin, signalEvent, isStateless, launchParams));

    ADDMETHOD_NOBASE(appendMemoryCopyBlit, ze_result_t, ZE_RESULT_SUCCESS,
                     (uintptr_t dstPtr,
                      NEO::GraphicsAllocation *dstPtrAlloc,
                      uint64_t dstOffset, uintptr_t srcPtr,
                      NEO::GraphicsAllocation *srcPtrAlloc,
                      uint64_t srcOffset,
                      uint64_t size));

    ADDMETHOD_VOIDRETURN(allocateOrReuseKernelPrivateMemory,
                         false,
                         (L0::Kernel * kernel,
                          uint32_t sizePerHwThread,
                          NEO::PrivateAllocsToReuseContainer &privateAllocsToReuse),
                         (kernel, sizePerHwThread, privateAllocsToReuse));

    ADDMETHOD_VOIDRETURN(allocateOrReuseKernelPrivateMemoryIfNeeded,
                         false,
                         (L0::Kernel * kernel,
                          uint32_t sizePerHwThread),
                         (kernel, sizePerHwThread));

    AlignedAllocationData getAlignedAllocationData(L0::Device *device, const void *buffer, uint64_t bufferSize, bool allowHostCopy, bool copyOffload) override {
        return L0::CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocationData(device, buffer, bufferSize, allowHostCopy, copyOffload);
    }

    ze_result_t appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         size_t srcOffset, L0::Event *signalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override {
        srcAlignedPtr = srcAlignedAllocation->alignedAllocationPtr;
        dstAlignedPtr = dstAlignedAllocation->alignedAllocationPtr;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel2d(dstAlignedAllocation, srcAlignedAllocation, builtin, dstRegion, dstPitch, dstOffset, srcRegion, srcPitch, srcOffset, signalEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);
    }

    ze_result_t appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         uint32_t srcSlicePitch, size_t srcOffset,
                                         L0::Event *signalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override {
        srcAlignedPtr = srcAlignedAllocation->alignedAllocationPtr;
        dstAlignedPtr = dstAlignedAllocation->alignedAllocationPtr;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel3d(dstAlignedAllocation, srcAlignedAllocation, builtin, dstRegion, dstPitch, dstSlicePitch, dstOffset, srcRegion, srcPitch, srcSlicePitch, srcOffset, signalEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);
    }

    ze_result_t appendMemoryCopyBlitRegion(AlignedAllocationData *srcAllocationData,
                                           AlignedAllocationData *dstAllocationData,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                           L0::Event *signalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override {
        srcBlitCopyRegionOffset = srcAllocationData->offset;
        dstBlitCopyRegionOffset = dstAllocationData->offset;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyBlitRegion(srcAllocationData, dstAllocationData, srcRegion, dstRegion, copySize, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, srcSize, dstSize, signalEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);
    }
    uintptr_t srcAlignedPtr;
    uintptr_t dstAlignedPtr;
    size_t srcBlitCopyRegionOffset = 0;
    size_t dstBlitCopyRegionOffset = 0;
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListImmediateHw : public WhiteBox<::L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
  public:
    using BaseClass = WhiteBox<::L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>;
    MockCommandListImmediateHw() : BaseClass() {
        this->cmdListType = CommandList::CommandListType::typeImmediate;
    }
    using BaseClass::appendSignalEventPostWalker;
    using BaseClass::applyMemoryRangesBarrier;
    using BaseClass::cmdListType;
    using BaseClass::cmdQImmediate;
    using BaseClass::copyThroughLockedPtrEnabled;
    using BaseClass::dcFlushSupport;
    using BaseClass::dependenciesPresent;
    using BaseClass::dummyBlitWa;
    using BaseClass::isFlushTaskSubmissionEnabled;
    using BaseClass::isSyncModeQueue;
    using BaseClass::isTbxMode;
    using BaseClass::setupFillKernelArguments;

    ze_result_t executeCommandListImmediate(bool performMigration) override {
        ++executeCommandListImmediateCalledCount;
        if (callBaseExecute) {
            return BaseClass::executeCommandListImmediate(performMigration);
        }
        return executeCommandListImmediateReturnValue;
    }

    ze_result_t executeCommandListImmediateWithFlushTask(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool kernelOperation, bool copyOffloadSubmission, bool requireTaskCountUpdate) override {
        ++executeCommandListImmediateWithFlushTaskCalledCount;
        if (callBaseExecute) {
            return BaseClass::executeCommandListImmediateWithFlushTask(performMigration, hasStallingCmds, hasRelaxedOrderingDependencies, kernelOperation, copyOffloadSubmission, requireTaskCountUpdate);
        }
        return executeCommandListImmediateWithFlushTaskReturnValue;
    }

    void checkAssert() override {
        checkAssertCalled++;
    }

    ADDMETHOD_VOIDRETURN(allocateOrReuseKernelPrivateMemory,
                         false,
                         (L0::Kernel * kernel,
                          uint32_t sizePerHwThread,
                          NEO::PrivateAllocsToReuseContainer &privateAllocsToReuse),
                         (kernel, sizePerHwThread, privateAllocsToReuse));

    ADDMETHOD_VOIDRETURN(allocateOrReuseKernelPrivateMemoryIfNeeded,
                         false,
                         (L0::Kernel * kernel,
                          uint32_t sizePerHwThread),
                         (kernel, sizePerHwThread));

    uint32_t checkAssertCalled = 0;
    bool callBaseExecute = false;

    ze_result_t executeCommandListImmediateReturnValue = ZE_RESULT_SUCCESS;
    uint32_t executeCommandListImmediateCalledCount = 0;

    ze_result_t executeCommandListImmediateWithFlushTaskReturnValue = ZE_RESULT_SUCCESS;
    uint32_t executeCommandListImmediateWithFlushTaskCalledCount = 0;
};

struct CmdListHelper {
    NEO::GraphicsAllocation *isaAllocation = nullptr;
    NEO::ResidencyContainer argumentsResidencyContainer;
    ze_group_count_t threadGroupDimensions;
    const uint32_t *groupSize = nullptr;
    uint32_t useOnlyGlobalTimestamp = std::numeric_limits<uint32_t>::max();
    bool isBuiltin = false;
    bool isDstInSystem = false;
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForAppendLaunchKernel : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {

  public:
    CmdListHelper cmdListHelper;
    ze_result_t appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                   const ze_group_count_t &threadGroupDimensions,
                                   ze_event_handle_t hEvent,
                                   uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents,
                                   CmdListKernelLaunchParams &launchParams, bool relaxedOrderingDispatch) override {

        const auto kernel = Kernel::fromHandle(kernelHandle);
        cmdListHelper.isaAllocation = kernel->getIsaAllocation();
        cmdListHelper.argumentsResidencyContainer = kernel->getArgumentsResidencyContainer();
        cmdListHelper.groupSize = kernel->getGroupSize();
        cmdListHelper.threadGroupDimensions = threadGroupDimensions;

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
        cmdListHelper.isBuiltin = launchParams.isBuiltInKernel;
        cmdListHelper.isDstInSystem = launchParams.isDestinationAllocationInSystemMemory;

        return ZE_RESULT_SUCCESS;
    }
};

} // namespace ult
} // namespace L0
