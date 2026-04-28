/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/allocation_type.h"

namespace NEO {

/**
 * @brief Classifies allocation types as Application vs Internal resources
 */
class AppResourceClassification {
  public:
    static bool isApplicationResource(AllocationType type) {
        switch (type) {
        // Application allocations
        case AllocationType::buffer:
        case AllocationType::bufferHostMemory:
        case AllocationType::image:
        case AllocationType::sharedBuffer:
        case AllocationType::sharedImage:
        case AllocationType::sharedResourceCopy:
        case AllocationType::unifiedSharedMemory:
        case AllocationType::svmCpu:
        case AllocationType::svmGpu:
        case AllocationType::svmZeroCopy:
        case AllocationType::mapAllocation:
        case AllocationType::externalHostPtr:
        case AllocationType::writeCombined:
        case AllocationType::fillPattern:
        case AllocationType::globalSurface:
        case AllocationType::constantSurface:
        case AllocationType::kernelIsa:
        case AllocationType::printfSurface:
        case AllocationType::assertBuffer:
        case AllocationType::privateSurface:
            return true;

        // Driver internal allocations
        case AllocationType::unknown:
        case AllocationType::commandBuffer:
        case AllocationType::indirectObjectHeap:
        case AllocationType::instructionHeap:
        case AllocationType::internalHeap:
        case AllocationType::internalHostMemory:
        case AllocationType::kernelArgsBuffer:
        case AllocationType::kernelIsaInternal:
        case AllocationType::linearStream:
        case AllocationType::mcs:
        case AllocationType::preemption:
        case AllocationType::profilingTagBuffer:
        case AllocationType::scratchSurface:
        case AllocationType::surfaceStateHeap:
        case AllocationType::syncBuffer:
        case AllocationType::tagBuffer:
        case AllocationType::globalFence:
        case AllocationType::timestampPacketTagBuffer:
        case AllocationType::ringBuffer:
        case AllocationType::semaphoreBuffer:
        case AllocationType::debugContextSaveArea:
        case AllocationType::debugSbaTrackingBuffer:
        case AllocationType::debugModuleArea:
        case AllocationType::workPartitionSurface:
        case AllocationType::gpuTimestampDeviceBuffer:
        case AllocationType::swTagBuffer:
        case AllocationType::deferredTasksList:
        case AllocationType::syncDispatchToken:
        case AllocationType::hostFunction:
        case AllocationType::count:
            return false;
        }
        return false;
    }
};

} // namespace NEO
