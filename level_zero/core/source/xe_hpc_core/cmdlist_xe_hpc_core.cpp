/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_dg2_and_pvc.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe_hpc_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"

namespace L0 {

template <>
ze_result_t CommandListCoreFamily<IGFX_XE_HPC_CORE>::appendMemoryPrefetch(const void *ptr, size_t size) {
    auto svmAllocMgr = device->getDriverHandle()->getSvmAllocsManager();
    auto allocData = svmAllocMgr->getSVMAlloc(ptr);

    if (!allocData) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (NEO::debugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.get() == true) {
        this->performMemoryPrefetch = true;
        auto prefetchManager = device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
        if (prefetchManager) {
            prefetchManager->insertAllocation(this->prefetchContext, ptr, *allocData);
        }
    }

    if (NEO::debugManager.flags.AddStatePrefetchCmdToMemoryPrefetchAPI.get() != 1) {
        return ZE_RESULT_SUCCESS;
    }

    auto gpuAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    commandContainer.addToResidencyContainer(gpuAlloc);

    size_t offset = ptrDiff(ptr, gpuAlloc->getGpuAddress());

    NEO::LinearStream &cmdStream = *commandContainer.getCommandStream();

    NEO::EncodeMemoryPrefetch<GfxFamily>::programMemoryPrefetch(cmdStream, *gpuAlloc, static_cast<uint32_t>(size), offset, device->getNEODevice()->getRootDeviceEnvironment());

    return ZE_RESULT_SUCCESS;
}

template struct CommandListCoreFamily<IGFX_XE_HPC_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE>;

} // namespace L0
