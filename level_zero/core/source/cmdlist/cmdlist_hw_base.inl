/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/software_tags_manager.h"

#include "level_zero/core/source/kernel/kernel_imp.h"

#include <algorithm>

namespace L0 {
struct DeviceImp;

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandListCoreFamily<gfxCoreFamily>::getReserveSshSize() {
    auto &helper = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily);
    return helper.getRenderSurfaceStateSize();
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(Kernel *kernel,
                                                                               const ze_group_count_t *threadGroupDimensions,
                                                                               Event *event,
                                                                               const CmdListKernelLaunchParams &launchParams) {
    UNRECOVERABLE_IF(kernel == nullptr);
    const auto &kernelDescriptor = kernel->getKernelDescriptor();
    appendEventForProfiling(event, true, false);
    const auto functionImmutableData = kernel->getImmutableData();
    auto perThreadScratchSize = std::max<std::uint32_t>(this->getCommandListPerThreadScratchSize(),
                                                        kernel->getImmutableData()->getDescriptor().kernelAttributes.perThreadScratchSize[0]);
    this->setCommandListPerThreadScratchSize(perThreadScratchSize);

    auto slmEnable = (kernel->getImmutableData()->getDescriptor().kernelAttributes.slmInlineSize > 0);
    this->setCommandListSLMEnable(slmEnable);

    auto kernelPreemptionMode = obtainFunctionPreemptionMode(kernel);
    commandListPreemptionMode = std::min(commandListPreemptionMode, kernelPreemptionMode);

    kernel->patchGlobalOffset();

    if (kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize != 0U &&
        nullptr == kernel->getPrivateMemoryGraphicsAllocation()) {
        auto privateMemoryGraphicsAllocation = kernel->allocatePrivateMemoryGraphicsAllocation();
        kernel->patchCrossthreadDataWithPrivateAllocation(privateMemoryGraphicsAllocation);
        this->commandContainer.addToResidencyContainer(privateMemoryGraphicsAllocation);
        this->ownedPrivateAllocations.push_back(privateMemoryGraphicsAllocation);
    }

    if (!launchParams.isIndirect) {
        kernel->setGroupCount(threadGroupDimensions->groupCountX,
                              threadGroupDimensions->groupCountY,
                              threadGroupDimensions->groupCountZ);
    }

    if (launchParams.isIndirect && threadGroupDimensions) {
        prepareIndirectParams(threadGroupDimensions);
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

    bool isMixingRegularAndCooperativeKernelsAllowed = NEO::DebugManager.flags.AllowMixingRegularAndCooperativeKernels.get();
    if ((!containsAnyKernel) || isMixingRegularAndCooperativeKernelsAllowed) {
        containsCooperativeKernelsFlag = (containsCooperativeKernelsFlag || launchParams.isCooperative);
    } else if (containsCooperativeKernelsFlag != launchParams.isCooperative) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (kernel->usesSyncBuffer()) {
        auto retVal = (launchParams.isCooperative
                           ? programSyncBuffer(*kernel, *device->getNEODevice(), threadGroupDimensions)
                           : ZE_RESULT_ERROR_INVALID_ARGUMENT);
        if (retVal) {
            return retVal;
        }
    }

    KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    this->containsStatelessUncachedResource |= kernelImp->getKernelRequiresUncachedMocs();
    this->requiresQueueUncachedMocs |= kernelImp->getKernelRequiresQueueUncachedMocs();

    NEO::Device *neoDevice = device->getNEODevice();

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::KernelNameTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            kernel->getKernelDescriptor().kernelMetadata.kernelName.c_str(), 0u);
    }

    updateStreamProperties(*kernel, false, launchParams.isCooperative);
    NEO::EncodeDispatchKernelArgs dispatchKernelArgs{
        0,                                                     // eventAddress
        neoDevice,                                             // device
        kernel,                                                // dispatchInterface
        reinterpret_cast<const void *>(threadGroupDimensions), // threadGroupDimensions
        commandListPreemptionMode,                             // preemptionMode
        0,                                                     // partitionCount
        launchParams.isIndirect,                               // isIndirect
        launchParams.isPredicate,                              // isPredicate
        false,                                                 // isTimestampEvent
        this->containsStatelessUncachedResource,               // requiresUncachedMocs
        false,                                                 // useGlobalAtomics
        internalUsage,                                         // isInternal
        launchParams.isCooperative,                            // isCooperative
        false,                                                 // isHostScopeSignalEvent
        false,                                                 // isKernelUsingSystemAllocation
        cmdListType == CommandListType::TYPE_IMMEDIATE         // isKernelDispatchedFromImmediateCmdList
    };

    NEO::EncodeDispatchKernel<GfxFamily>::encode(commandContainer, dispatchKernelArgs, getLogicalStateHelper());
    this->containsStatelessUncachedResource = dispatchKernelArgs.requiresUncachedMocs;

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
        args.areMultipleSubDevicesInContext = false;
        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
        *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateSpace) = surfaceState;
    }

    appendSignalEventPostWalker(event, false);

    commandContainer.addToResidencyContainer(functionImmutableData->getIsaGraphicsAllocation());
    auto &residencyContainer = kernel->getResidencyContainer();
    for (auto resource : residencyContainer) {
        commandContainer.addToResidencyContainer(resource);
    }

    if (functionImmutableData->getDescriptor().kernelAttributes.flags.usesPrintf) {
        storePrintfFunction(kernel);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionPrologue(uint32_t partitionDataSize) {}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionEpilogue() {}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendComputeBarrierCommand() {
    NEO::PipeControlArgs args = createBarrierFlags();
    NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline NEO::PipeControlArgs CommandListCoreFamily<gfxCoreFamily>::createBarrierFlags() {
    NEO::PipeControlArgs args;
    return args;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandListCoreFamily<gfxCoreFamily>::appendMultiTileBarrier(NEO::Device &neoDevice) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandListCoreFamily<gfxCoreFamily>::estimateBufferSizeMultiTileBarrier(const NEO::HardwareInfo &hwInfo) {
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelSplit(Kernel *kernel,
                                                                          const ze_group_count_t *threadGroupDimensions,
                                                                          Event *event,
                                                                          const CmdListKernelLaunchParams &launchParams) {
    return appendLaunchKernelWithParams(kernel, threadGroupDimensions, nullptr, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendEventForProfilingAllWalkers(Event *event, bool beforeWalker) {
    if (beforeWalker) {
        appendEventForProfiling(event, true, false);
    } else {
        appendSignalEventPostWalker(event, false);
    }
}

} // namespace L0
