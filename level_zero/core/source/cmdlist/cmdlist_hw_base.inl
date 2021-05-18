/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/software_tags_manager.h"

#include "level_zero/core/source/kernel/kernel_imp.h"

#include "pipe_control_args.h"

#include <algorithm>

namespace L0 {
struct DeviceImp;

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandListCoreFamily<gfxCoreFamily>::getReserveSshSize() {
    auto &helper = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily);
    return helper.getRenderSurfaceStateSize();
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(ze_kernel_handle_t hKernel,
                                                                               const ze_group_count_t *pThreadGroupDimensions,
                                                                               ze_event_handle_t hEvent,
                                                                               bool isIndirect,
                                                                               bool isPredicate,
                                                                               bool isCooperative) {
    const auto kernel = Kernel::fromHandle(hKernel);
    UNRECOVERABLE_IF(kernel == nullptr);
    appendEventForProfiling(hEvent, true);
    const auto functionImmutableData = kernel->getImmutableData();
    auto perThreadScratchSize = std::max<std::uint32_t>(this->getCommandListPerThreadScratchSize(),
                                                        kernel->getImmutableData()->getDescriptor().kernelAttributes.perThreadScratchSize[0]);

    this->setCommandListPerThreadScratchSize(perThreadScratchSize);

    auto kernelPreemptionMode = obtainFunctionPreemptionMode(kernel);
    commandListPreemptionMode = std::min(commandListPreemptionMode, kernelPreemptionMode);

    kernel->patchGlobalOffset();

    if (!isIndirect) {
        kernel->setGroupCount(pThreadGroupDimensions->groupCountX,
                              pThreadGroupDimensions->groupCountY,
                              pThreadGroupDimensions->groupCountZ);
    }

    if (isIndirect && pThreadGroupDimensions) {
        prepareIndirectParams(pThreadGroupDimensions);
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

    if (kernel->usesSyncBuffer()) {
        auto retVal = (isCooperative
                           ? programSyncBuffer(*kernel, *device->getNEODevice(), pThreadGroupDimensions)
                           : ZE_RESULT_ERROR_INVALID_ARGUMENT);
        if (retVal) {
            return retVal;
        }
    }

    KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    this->containsStatelessUncachedResource |= kernelImp->getKernelRequiresUncachedMocs();
    uint32_t partitionCount = 0;

    NEO::Device *neoDevice = device->getNEODevice();

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::KernelNameTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            kernel->getKernelDescriptor().kernelMetadata.kernelName.c_str());
    }

    updateStreamProperties(*kernel, false);

    NEO::EncodeDispatchKernel<GfxFamily>::encode(commandContainer,
                                                 reinterpret_cast<const void *>(pThreadGroupDimensions),
                                                 isIndirect,
                                                 isPredicate,
                                                 kernel,
                                                 0,
                                                 false,
                                                 false,
                                                 neoDevice,
                                                 commandListPreemptionMode,
                                                 this->containsStatelessUncachedResource,
                                                 partitionCount,
                                                 internalUsage);

    if (neoDevice->getDebugger()) {
        auto *ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
        auto surfaceState = neoDevice->getDebugger()->getDebugSurfaceReservedSurfaceState(*ssh);
        auto debugSurface = device->getDebugSurface();
        auto mocs = device->getMOCS(true, false);
        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(surfaceState, debugSurface->getGpuAddress(),
                                                         debugSurface->getUnderlyingBufferSize(), mocs,
                                                         false, false, false, neoDevice->getNumAvailableDevices(),
                                                         debugSurface, neoDevice->getGmmHelper(), kernelImp->getKernelDescriptor().kernelAttributes.flags.useGlobalAtomics, 1u);
    }

    appendSignalEventPostWalker(hEvent);

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

} // namespace L0
