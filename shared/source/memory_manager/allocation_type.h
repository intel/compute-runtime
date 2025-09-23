/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <limits>

namespace NEO {
enum class AllocationType {
    unknown = 0,
    buffer,
    bufferHostMemory,
    commandBuffer,
    constantSurface,
    externalHostPtr,
    fillPattern,
    globalSurface,
    image,
    indirectObjectHeap,
    instructionHeap,
    internalHeap,
    internalHostMemory,
    kernelArgsBuffer,
    kernelIsa,
    kernelIsaInternal,
    linearStream,
    mapAllocation,
    mcs,
    preemption,
    printfSurface,
    privateSurface,
    profilingTagBuffer,
    scratchSurface,
    sharedBuffer,
    sharedImage,
    sharedResourceCopy,
    surfaceStateHeap,
    svmCpu,
    svmGpu,
    svmZeroCopy,
    syncBuffer,
    tagBuffer,
    globalFence,
    timestampPacketTagBuffer,
    writeCombined,
    ringBuffer,
    semaphoreBuffer,
    debugContextSaveArea,
    debugSbaTrackingBuffer,
    debugModuleArea,
    unifiedSharedMemory,
    workPartitionSurface,
    gpuTimestampDeviceBuffer,
    swTagBuffer,
    deferredTasksList,
    assertBuffer,
    syncDispatchToken,
    hostFunction,
    count
};

enum class GfxMemoryAllocationMethod : uint32_t {
    useUmdSystemPtr,
    allocateByKmd,
    notDefined = std::numeric_limits<uint32_t>::max()
};
} // namespace NEO
