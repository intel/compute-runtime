/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/prefetch_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"
#include "hw_cmds_xe_hpc_core_base.h"

namespace L0 {
template <>
NEO::PipeControlArgs CommandListCoreFamily<IGFX_XE_HPC_CORE>::createBarrierFlags() {
    NEO::PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
    return args;
}

template <>
ze_result_t CommandListCoreFamily<IGFX_XE_HPC_CORE>::appendMemoryPrefetch(const void *ptr, size_t size) {
    auto svmAllocMgr = device->getDriverHandle()->getSvmAllocsManager();
    auto allocData = svmAllocMgr->getSVMAlloc(ptr);

    if (!allocData) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (NEO::DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.get() > 0) {
        this->performMemoryPrefetch = true;
        auto prefetchManager = device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
        if (prefetchManager) {
            prefetchManager->insertAllocation(this->prefetchContext, *allocData);
        }
    }

    if (NEO::DebugManager.flags.AddStatePrefetchCmdToMemoryPrefetchAPI.get() != 1) {
        return ZE_RESULT_SUCCESS;
    }

    auto gpuAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    auto &hwInfo = device->getHwInfo();

    commandContainer.addToResidencyContainer(gpuAlloc);

    size_t offset = ptrDiff(ptr, gpuAlloc->getGpuAddress());

    NEO::LinearStream &cmdStream = *commandContainer.getCommandStream();

    NEO::EncodeMemoryPrefetch<GfxFamily>::programMemoryPrefetch(cmdStream, *gpuAlloc, static_cast<uint32_t>(size), offset, hwInfo);

    return ZE_RESULT_SUCCESS;
}

template <>
void CommandListCoreFamily<IGFX_XE_HPC_CORE>::applyMemoryRangesBarrier(uint32_t numRanges,
                                                                       const size_t *pRangeSizes,
                                                                       const void **pRanges) {
    NEO::PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
    NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
}

template struct CommandListCoreFamily<IGFX_XE_HPC_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE>;

} // namespace L0
