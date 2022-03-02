/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"

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
    using MI_BATCH_BUFFER_END = GfxFamily::MI_BATCH_BUFFER_END;
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    if (!allocData) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto allowPrefetchingKmdMigratedSharedAllocation = false;
    if (NEO::DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.get() != -1) {
        allowPrefetchingKmdMigratedSharedAllocation = !!NEO::DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.get();
    }

    if (allowPrefetchingKmdMigratedSharedAllocation) {
        auto memoryManager = device->getDriverHandle()->getMemoryManager();
        if (memoryManager->isKmdMigrationAvailable(device->getRootDeviceIndex()) &&
            (allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY)) {
            auto alloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
            memoryManager->setMemPrefetch(alloc, device->getRootDeviceIndex());
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
    NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), args);
}

template struct CommandListCoreFamily<IGFX_XE_HPC_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE>;

} // namespace L0
