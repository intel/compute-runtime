/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/white_box.h"

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
    using BaseClass::addFlushRequiredCommand;
    using BaseClass::appendBlitFill;
    using BaseClass::appendCopyImageBlit;
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
    using BaseClass::cmdQImmediate;
    using BaseClass::commandContainer;
    using BaseClass::commandListPerThreadScratchSize;
    using BaseClass::commandListPreemptionMode;
    using BaseClass::commandsToPatch;
    using BaseClass::containsAnyKernel;
    using BaseClass::containsCooperativeKernelsFlag;
    using BaseClass::csr;
    using BaseClass::engineGroupType;
    using BaseClass::estimateBufferSizeMultiTileBarrier;
    using BaseClass::finalStreamState;
    using BaseClass::flags;
    using BaseClass::getAlignedAllocation;
    using BaseClass::getAllocationFromHostPtrMap;
    using BaseClass::getHostPtrAlloc;
    using BaseClass::hostPtrMap;
    using BaseClass::indirectAllocationsAllowed;
    using BaseClass::initialize;
    using BaseClass::partitionCount;
    using BaseClass::patternAllocations;
    using BaseClass::requiredStreamState;
    using BaseClass::unifiedMemoryControls;
    using BaseClass::updateStreamProperties;

    WhiteBox() : ::L0::CommandListCoreFamily<gfxCoreFamily>(BaseClass::defaultNumIddsPerBlock) {}

    ze_result_t appendLaunchKernelWithParams(::L0::Kernel *kernel,
                                             const ze_group_count_t *threadGroupDimensions,
                                             ::L0::Event *event,
                                             const CmdListKernelLaunchParams &launchParams) override {

        usedKernelLaunchParams = launchParams;
        appendKernelEventValue = event;
        return BaseClass::appendLaunchKernelWithParams(kernel, threadGroupDimensions,
                                                       event, launchParams);
    }

    ze_result_t appendLaunchMultipleKernelsIndirect(uint32_t numKernels,
                                                    const ze_kernel_handle_t *phKernels,
                                                    const uint32_t *pNumLaunchArguments,
                                                    const ze_group_count_t *pLaunchArgumentsBuffer,
                                                    ze_event_handle_t hEvent,
                                                    uint32_t numWaitEvents,
                                                    ze_event_handle_t *phWaitEvents) override {
        appendEventMultipleKernelIndirectEventHandleValue = hEvent;
        return BaseClass::appendLaunchMultipleKernelsIndirect(numKernels, phKernels, pNumLaunchArguments, pLaunchArgumentsBuffer,
                                                              hEvent, numWaitEvents, phWaitEvents);
    }

    ze_result_t appendLaunchKernelIndirect(ze_kernel_handle_t hKernel,
                                           const ze_group_count_t *pDispatchArgumentsBuffer,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents) override {
        appendEventKernelIndirectEventHandleValue = hEvent;
        return BaseClass::appendLaunchKernelIndirect(hKernel, pDispatchArgumentsBuffer,
                                                     hEvent, numWaitEvents, phWaitEvents);
    }

    CmdListKernelLaunchParams usedKernelLaunchParams;
    ::L0::Event *appendKernelEventValue = nullptr;
    ze_event_handle_t appendEventMultipleKernelIndirectEventHandleValue = nullptr;
    ze_event_handle_t appendEventKernelIndirectEventHandleValue = nullptr;
};

template <GFXCORE_FAMILY gfxCoreFamily>
using CommandListCoreFamily = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;

template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>
    : public L0::CommandListCoreFamilyImmediate<gfxCoreFamily> {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using BaseClass = L0::CommandListCoreFamilyImmediate<gfxCoreFamily>;
    using BaseClass::clearCommandsToPatch;
    using BaseClass::cmdQImmediate;
    using BaseClass::commandsToPatch;
    using BaseClass::csr;
    using BaseClass::finalStreamState;
    using BaseClass::isFlushTaskSubmissionEnabled;
    using BaseClass::partitionCount;
    using BaseClass::requiredStreamState;

    WhiteBox() : BaseClass(BaseClass::defaultNumIddsPerBlock) {}
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandListImmediate : public CommandListCoreFamilyImmediate<gfxCoreFamily> {
    using CommandListCoreFamilyImmediate<gfxCoreFamily>::requiredStreamState;
    using CommandListCoreFamilyImmediate<gfxCoreFamily>::containsAnyKernel;
};

template <>
struct WhiteBox<::L0::CommandList> : public ::L0::CommandListImp {
    using BaseClass = ::L0::CommandListImp;
    using BaseClass::BaseClass;
    using BaseClass::cmdQImmediate;
    using BaseClass::commandContainer;
    using BaseClass::commandListPreemptionMode;
    using BaseClass::csr;
    using BaseClass::initialize;
    using BaseClass::nonImmediateLogicalStateHelper;
    using BaseClass::partitionCount;

    WhiteBox(Device *device);
    ~WhiteBox() override;
};

using CommandList = WhiteBox<::L0::CommandList>;

struct MockCommandList : public CommandList {
    using BaseClass = CommandList;

    MockCommandList(Device *device = nullptr);
    ~MockCommandList() override;

    ADDMETHOD_NOBASE(close, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendLaunchKernel, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_kernel_handle_t hFunction,
                      const ze_group_count_t *threadGroupDimensions,
                      ze_event_handle_t hEvent, uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents,
                      const CmdListKernelLaunchParams &launchParams));

    ADDMETHOD_NOBASE(appendLaunchCooperativeKernel, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_kernel_handle_t hKernel,
                      const ze_group_count_t *pLaunchFuncArgs,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendLaunchKernelIndirect, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_kernel_handle_t hFunction,
                      const ze_group_count_t *pDispatchArgumentsBuffer,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendLaunchMultipleKernelsIndirect, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numFunctions,
                      const ze_kernel_handle_t *phFunctions,
                      const uint32_t *pNumLaunchArguments,
                      const ze_group_count_t *pLaunchArgumentsBuffer,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendEventReset, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hEvent));

    ADDMETHOD_NOBASE(appendBarrier, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

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
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendImageCopyToMemory, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      ze_image_handle_t hSrcImage,
                      const ze_image_region_t *pSrcRegion,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendImageCopyRegion, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      ze_image_handle_t hSrcImage,
                      const ze_image_region_t *pDstRegion,
                      const ze_image_region_t *pSrcRegion,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendImageCopy, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      ze_image_handle_t hSrcImage,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

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
                      ze_event_handle_t *phWaitEvents));

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
                      ze_event_handle_t *phWaitEvents));

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
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendSignalEvent, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hEvent));

    ADDMETHOD_NOBASE(appendWaitOnEvents, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numEvents,
                      ze_event_handle_t *phEvent));

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
                      ze_event_handle_t *phWaitEvents));

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
                      uint32_t value));

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
                     (void *desc, void *ptr,
                      uint32_t data, ze_event_handle_t hSignalEvent));

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

    uint8_t *batchBuffer = nullptr;
    NEO::GraphicsAllocation *mockAllocation = nullptr;
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockAppendMemoryCopy : public CommandListCoreFamily<gfxCoreFamily> {
  public:
    using BaseClass = CommandListCoreFamily<gfxCoreFamily>;

    ADDMETHOD(appendMemoryCopyKernelWithGA, ze_result_t, false, ZE_RESULT_SUCCESS,
              (void *dstPtr, NEO::GraphicsAllocation *dstPtrAlloc,
               uint64_t dstOffset, void *srcPtr,
               NEO::GraphicsAllocation *srcPtrAlloc,
               uint64_t srcOffset, uint64_t size,
               uint64_t elementSize, Builtin builtin,
               Event *signalEvent,
               bool isStateless),
              (dstPtr, dstPtrAlloc, dstOffset, srcPtr, srcPtrAlloc, srcOffset, size, elementSize, builtin, signalEvent, isStateless));

    ADDMETHOD_NOBASE(appendMemoryCopyBlit, ze_result_t, ZE_RESULT_SUCCESS,
                     (uintptr_t dstPtr,
                      NEO::GraphicsAllocation *dstPtrAlloc,
                      uint64_t dstOffset, uintptr_t srcPtr,
                      NEO::GraphicsAllocation *srcPtrAlloc,
                      uint64_t srcOffset,
                      uint64_t size));

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize, bool allowHostCopy) override {
        return L0::CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocation(device, buffer, bufferSize, allowHostCopy);
    }

    ze_result_t appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         size_t srcOffset, Event *signalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        srcAlignedPtr = srcAlignedAllocation->alignedAllocationPtr;
        dstAlignedPtr = dstAlignedAllocation->alignedAllocationPtr;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel2d(dstAlignedAllocation, srcAlignedAllocation, builtin, dstRegion, dstPitch, dstOffset, srcRegion, srcPitch, srcOffset, signalEvent, numWaitEvents, phWaitEvents);
    }

    ze_result_t appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         uint32_t srcSlicePitch, size_t srcOffset,
                                         Event *signalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents) override {
        srcAlignedPtr = srcAlignedAllocation->alignedAllocationPtr;
        dstAlignedPtr = dstAlignedAllocation->alignedAllocationPtr;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel3d(dstAlignedAllocation, srcAlignedAllocation, builtin, dstRegion, dstPitch, dstSlicePitch, dstOffset, srcRegion, srcPitch, srcSlicePitch, srcOffset, signalEvent, numWaitEvents, phWaitEvents);
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
        srcBlitCopyRegionOffset = srcOffset;
        dstBlitCopyRegionOffset = dstOffset;
        return L0::CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyBlitRegion(srcAllocation, dstAllocation, srcOffset, dstOffset, srcRegion, dstRegion, copySize, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, srcSize, dstSize, signalEvent, numWaitEvents, phWaitEvents);
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
    MockCommandListImmediateHw() : BaseClass() {}
    using BaseClass::applyMemoryRangesBarrier;
    using BaseClass::isFlushTaskSubmissionEnabled;
    using BaseClass::isSyncModeQueue;

    ze_result_t executeCommandListImmediate(bool performMigration) override {
        ++executeCommandListImmediateCalledCount;
        return executeCommandListImmediateReturnValue;
    }

    ze_result_t executeCommandListImmediateWithFlushTask(bool performMigration) override {
        ++executeCommandListImmediateWithFlushTaskCalledCount;
        return executeCommandListImmediateWithFlushTaskReturnValue;
    }

    ze_result_t executeCommandListImmediateReturnValue = ZE_RESULT_SUCCESS;
    uint32_t executeCommandListImmediateCalledCount = 0;

    ze_result_t executeCommandListImmediateWithFlushTaskReturnValue = ZE_RESULT_SUCCESS;
    uint32_t executeCommandListImmediateWithFlushTaskCalledCount = 0;
};

struct CmdListHelper {
    NEO::GraphicsAllocation *isaAllocation = nullptr;
    NEO::ResidencyContainer residencyContainer;
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
    ze_result_t appendLaunchKernel(ze_kernel_handle_t hKernel,
                                   const ze_group_count_t *threadGroupDimensions,
                                   ze_event_handle_t hEvent,
                                   uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents,
                                   const CmdListKernelLaunchParams &launchParams) override {

        const auto kernel = Kernel::fromHandle(hKernel);
        cmdListHelper.isaAllocation = kernel->getIsaAllocation();
        cmdListHelper.residencyContainer = kernel->getResidencyContainer();
        cmdListHelper.groupSize = kernel->getGroupSize();
        cmdListHelper.threadGroupDimensions = *threadGroupDimensions;

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
