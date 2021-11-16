/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

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

    if (NEO::DebugManager.flags.AddStatePrefetchCmdToMemoryPrefetchAPI.get() != 1) {
        return ZE_RESULT_SUCCESS;
    }

    auto gpuAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    auto &hwInfo = device->getHwInfo();

    commandContainer.addToResidencyContainer(gpuAlloc);

    size_t offset = ptrDiff(ptr, gpuAlloc->getGpuAddress());

    NEO::LinearStream &cmdStream = *commandContainer.getCommandStream();

    size_t estimatedSizeRequired = NEO::EncodeMemoryPrefetch<GfxFamily>::getSizeForMemoryPrefetch(size);
    increaseCommandStreamSpace(estimatedSizeRequired);

    NEO::EncodeMemoryPrefetch<GfxFamily>::programMemoryPrefetch(cmdStream, *gpuAlloc, static_cast<uint32_t>(size), offset, hwInfo);

    return ZE_RESULT_SUCCESS;
}
} // namespace L0
