/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/source/xe_hp_core/hw_info.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"

#include "igfxfmid.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct EncodeStateBaseAddress;

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandListCoreFamily<gfxCoreFamily>::getReserveSshSize() {
    return 4 * MemoryConstants::pageSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendBarrier(ze_event_handle_t hSignalEvent,
                                                                uint32_t numWaitEvents,
                                                                ze_event_handle_t *phWaitEvents) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);
    if (ret) {
        return ret;
    }
    appendEventForProfiling(hSignalEvent, true);

    if (!hSignalEvent) {
        if (isCopyOnly()) {
            NEO::MiFlushArgs args;
            NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(), 0, 0, args);
        } else {
            NEO::PipeControlArgs args;
            args.hdcPipelineFlush = true;
            NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), args);
        }
    } else {
        appendSignalEventPostWalker(hSignalEvent);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::applyMemoryRangesBarrier(uint32_t numRanges,
                                                                    const size_t *pRangeSizes,
                                                                    const void **pRanges) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    NEO::LinearStream *commandStream = commandContainer.getCommandStream();
    NEO::SVMAllocsManager *svmAllocsManager =
        device->getDriverHandle()->getSvmAllocsManager();

    StackVec<NEO::L3Range, NEO::maxFlushSubrangeCount> subranges;
    uint64_t postSyncAddressToFlush = 0;
    for (uint32_t i = 0; i < numRanges; i++) {
        const uint64_t pRange = reinterpret_cast<const uint64_t>(pRanges[i]);
        size_t pRangeSize = pRangeSizes[i];
        uint64_t pFlushRange;
        size_t pFlushRangeSize;
        NEO::SvmAllocationData *allocData =
            svmAllocsManager->getSVMAllocs()->get(pRanges[i]);

        if (allocData == nullptr || pRangeSize > allocData->size) {
            continue;
        }

        pFlushRange = pRange;

        if (NEO::L3Range::meetsMinimumAlignment(pRange) == false) {
            pFlushRange = alignDown(pRange, MemoryConstants::pageSize);
        }
        pRangeSize = (pRange + pRangeSize) - pFlushRange;
        pFlushRangeSize = pRangeSize;
        if (NEO::L3Range::meetsMinimumAlignment(pRangeSize) == false) {
            pFlushRangeSize = alignUp(pRangeSize, MemoryConstants::pageSize);
        }
        coverRangeExact(pFlushRange,
                        pFlushRangeSize,
                        subranges,
                        GfxFamily::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);
    }
    for (size_t subrangeNumber = 0; subrangeNumber < subranges.size(); subrangeNumber += NEO::maxFlushSubrangeCount) {
        size_t rangeCount = subranges.size() <= subrangeNumber + NEO::maxFlushSubrangeCount ? subranges.size() - subrangeNumber : NEO::maxFlushSubrangeCount;
        NEO::Range<NEO::L3Range> range = CreateRange(subranges.begin() + subrangeNumber, rangeCount);

        NEO::flushGpuCache<GfxFamily>(commandStream, range, postSyncAddressToFlush, device->getHwInfo());
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void programEventL3Flush(ze_event_handle_t hEvent,
                         Device *device,
                         uint32_t partitionCount,
                         NEO::CommandContainer &commandContainer) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using POST_SYNC_OPERATION = typename GfxFamily::PIPE_CONTROL::POST_SYNC_OPERATION;
    auto event = Event::fromHandle(hEvent);

    auto eventPartitionOffset = (partitionCount > 1) ? (partitionCount * event->getSinglePacketSize())
                                                     : event->getSinglePacketSize();
    uint64_t eventAddress = event->getPacketAddress(device) + eventPartitionOffset;
    if (event->isEventTimestampFlagSet()) {
        eventAddress += event->getContextEndOffset();
    }

    if (partitionCount > 1) {
        event->setPacketsInUse(event->getPacketsInUse() + partitionCount);
    } else {
        event->setPacketsInUse(event->getPacketsInUse() + 1);
    }

    NEO::PipeControlArgs args;
    args.dcFlushEnable = true;
    if (partitionCount > 1) {
        args.workloadPartitionOffset = true;
        NEO::EncodeSetMMIO<GfxFamily>::encodeIMM(*commandContainer.getCommandStream(),
                                                 NEO::PartitionRegisters<GfxFamily>::addressOffsetCCSOffset,
                                                 static_cast<uint32_t>(event->getSinglePacketSize()),
                                                 true);
    }
    NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
        *commandContainer.getCommandStream(), POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
        eventAddress, Event::STATE_SIGNALED,
        commandContainer.getDevice()->getHardwareInfo(),
        args);

    if (partitionCount > 1) {
        NEO::EncodeSetMMIO<GfxFamily>::encodeIMM(*commandContainer.getCommandStream(),
                                                 NEO::PartitionRegisters<GfxFamily>::addressOffsetCCSOffset,
                                                 CommonConstants::partitionAddressOffset,
                                                 true);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(ze_kernel_handle_t hKernel,
                                                                               const ze_group_count_t *pThreadGroupDimensions,
                                                                               ze_event_handle_t hEvent,
                                                                               bool isIndirect,
                                                                               bool isPredicate,
                                                                               bool isCooperative) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using COMPUTE_WALKER = typename GfxFamily::COMPUTE_WALKER;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    if (NEO::DebugManager.flags.ForcePipeControlPriorToWalker.get()) {
        increaseCommandStreamSpace(NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl());

        NEO::PipeControlArgs args = {};
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), args);
    }
    NEO::Device *neoDevice = device->getNEODevice();

    const auto kernel = Kernel::fromHandle(hKernel);
    UNRECOVERABLE_IF(kernel == nullptr);
    const auto functionImmutableData = kernel->getImmutableData();
    auto &kernelDescriptor = kernel->getKernelDescriptor();
    commandListPerThreadScratchSize = std::max<uint32_t>(commandListPerThreadScratchSize, kernelDescriptor.kernelAttributes.perThreadScratchSize[0]);

    auto functionPreemptionMode = obtainFunctionPreemptionMode(kernel);
    commandListPreemptionMode = std::min(commandListPreemptionMode, functionPreemptionMode);

    kernel->patchGlobalOffset();
    if (isIndirect && pThreadGroupDimensions) {
        prepareIndirectParams(pThreadGroupDimensions);
    }
    if (!isIndirect) {
        kernel->setGroupCount(pThreadGroupDimensions->groupCountX,
                              pThreadGroupDimensions->groupCountY,
                              pThreadGroupDimensions->groupCountZ);
    }
    NEO::GraphicsAllocation *eventAlloc = nullptr;
    uint64_t eventAddress = 0;
    bool isTimestampEvent = false;
    bool L3FlushEnable = false;
    if (hEvent) {
        auto event = Event::fromHandle(hEvent);
        eventAlloc = &event->getAllocation(this->device);
        commandContainer.addToResidencyContainer(eventAlloc);
        if (NEO::MemorySynchronizationCommands<GfxFamily>::isDcFlushAllowed()) {
            L3FlushEnable = (!event->signalScope) ? false : true;
        }
        isTimestampEvent = event->isEventTimestampFlagSet();
        eventAddress = event->getPacketAddress(this->device);
    }

    if (kernel->hasIndirectAllocationsAllowed()) {
        UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();

        if (unifiedMemoryControls.indirectDeviceAllocationsAllowed) {
            this->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
        }
        if (unifiedMemoryControls.indirectHostAllocationsAllowed) {
            this->unifiedMemoryControls.indirectHostAllocationsAllowed = true;
        }
        if (unifiedMemoryControls.indirectSharedAllocationsAllowed) {
            this->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
        }

        this->indirectAllocationsAllowed = true;
    }

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::KernelNameTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            kernelDescriptor.kernelMetadata.kernelName.c_str(), 0u);
    }

    bool isMixingRegularAndCooperativeKernelsAllowed = NEO::DebugManager.flags.AllowMixingRegularAndCooperativeKernels.get();
    if ((!containsAnyKernel) || isMixingRegularAndCooperativeKernelsAllowed) {
        containsCooperativeKernelsFlag = (containsCooperativeKernelsFlag || isCooperative);
    } else if (containsCooperativeKernelsFlag != isCooperative) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (kernel->usesSyncBuffer()) {
        auto retVal = (isCooperative
                           ? programSyncBuffer(*kernel, *neoDevice, pThreadGroupDimensions)
                           : ZE_RESULT_ERROR_INVALID_ARGUMENT);
        if (retVal) {
            return retVal;
        }
    }

    auto isMultiOsContextCapable = NEO::ImplicitScalingHelper::isImplicitScalingEnabled(device->getNEODevice()->getDeviceBitfield(),
                                                                                        !isCooperative);
    updateStreamProperties(*kernel, isMultiOsContextCapable, isCooperative);

    KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    this->containsStatelessUncachedResource |= kernelImp->getKernelRequiresUncachedMocs();
    this->threadArbitrationPolicy = kernelImp->getSchedulingHintExp();

    uint32_t partitionCount = 0;
    NEO::EncodeDispatchKernel<GfxFamily>::encode(commandContainer,
                                                 reinterpret_cast<const void *>(pThreadGroupDimensions),
                                                 isIndirect,
                                                 isPredicate,
                                                 kernel,
                                                 eventAddress,
                                                 isTimestampEvent,
                                                 L3FlushEnable,
                                                 neoDevice,
                                                 commandListPreemptionMode,
                                                 this->containsStatelessUncachedResource,
                                                 kernelDescriptor.kernelAttributes.flags.useGlobalAtomics,
                                                 partitionCount,
                                                 internalUsage,
                                                 isCooperative);
    this->partitionCount = std::max(partitionCount, this->partitionCount);
    if (hEvent) {
        auto event = Event::fromHandle(hEvent);
        if (partitionCount > 1) {
            event->setPacketsInUse(partitionCount);
        }
        if (L3FlushEnable) {
            programEventL3Flush<gfxCoreFamily>(hEvent, this->device, partitionCount, commandContainer);
        }
    }

    if (neoDevice->getDebugger()) {
        auto *ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
        auto surfaceStateSpace = neoDevice->getDebugger()->getDebugSurfaceReservedSurfaceState(*ssh);
        auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;

        NEO::EncodeSurfaceStateArgs args;
        args.outMemory = &surfaceState;
        args.graphicsAddress = device->getDebugSurface()->getGpuAddress();
        args.size = device->getDebugSurface()->getUnderlyingBufferSize();
        args.mocs = device->getMOCS(false, false);
        args.numAvailableDevices = neoDevice->getNumGenericSubDevices();
        args.allocation = device->getDebugSurface();
        args.gmmHelper = neoDevice->getGmmHelper();
        args.useGlobalAtomics = kernelImp->getKernelDescriptor().kernelAttributes.flags.useGlobalAtomics;
        args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;

        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
        *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateSpace) = surfaceState;
    }
    // Attach Function residency to our CommandList residency
    {
        commandContainer.addToResidencyContainer(functionImmutableData->getIsaGraphicsAllocation());
        auto &residencyContainer = kernel->getResidencyContainer();
        for (auto resource : residencyContainer) {
            commandContainer.addToResidencyContainer(resource);
        }
    }

    // Store PrintfBuffer from a kernel
    {
        if (kernelDescriptor.kernelAttributes.flags.usesPrintf) {
            storePrintfFunction(kernel);
        }
    }

    if (kernelImp->usesRayTracing()) {
        NEO::GraphicsAllocation *memoryBackedBuffer = device->getNEODevice()->getRTMemoryBackedBuffer();
        if (memoryBackedBuffer == nullptr) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        } else {
            NEO::LinearStream *linearStream = commandContainer.getCommandStream();
            NEO::EncodeEnableRayTracing<GfxFamily>::programEnableRayTracing(*linearStream, *memoryBackedBuffer);
        }
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionPrologue(uint32_t partitionDataSize) {

    const uint64_t workPartitionAllocationGpuVa = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress();
    size_t estimatedSizeRequired = sizeof(typename GfxFamily::MI_LOAD_REGISTER_MEM) + sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
    increaseCommandStreamSpace(estimatedSizeRequired);

    NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer,
                                             NEO::PartitionRegisters<GfxFamily>::wparidCCSOffset,
                                             workPartitionAllocationGpuVa);
    NEO::EncodeSetMMIO<GfxFamily>::encodeIMM(commandContainer,
                                             NEO::PartitionRegisters<GfxFamily>::addressOffsetCCSOffset,
                                             partitionDataSize,
                                             true);
}

} // namespace L0
