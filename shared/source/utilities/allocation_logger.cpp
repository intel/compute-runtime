/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/allocation_logger.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace NEO {

template <DebugFunctionalityLevel debugLevel>
AllocationLogger<debugLevel>::AllocationLogger(FileLogger<debugLevel> &fileLoggerIn, const DebugVariables &flags) : fileLogger(fileLoggerIn) {
    this->logAllocationMemoryPool = flags.LogAllocationMemoryPool.get();
    this->logAllocationType = flags.LogAllocationType.get();
    this->logAllocationStdout = flags.LogAllocationStdout.get();
    this->logAllocationSummaryReport = flags.LogAllocationSummaryReport.get();
}

template <DebugFunctionalityLevel debugLevel>
AllocationLogger<debugLevel>::~AllocationLogger() {
    if (this->logAllocationSummaryReport) {
        this->allocationTracker.printReport();
    }
}

template class AllocationLogger<DebugFunctionalityLevel::none>;
template class AllocationLogger<DebugFunctionalityLevel::regKeys>;
template class AllocationLogger<DebugFunctionalityLevel::full>;

AllocationLogger<globalDebugFunctionalityLevel> &allocationLoggerInstance() {
    static AllocationLogger<globalDebugFunctionalityLevel> allocationLoggerInstance(fileLoggerInstance(), debugManager.flags);
    return allocationLoggerInstance;
}

void AllocationSummaryTracker::printReport() {
    std::unordered_map<std::string, size_t> sysSummarySnapshot;
    std::unordered_map<std::string, size_t> localSummarySnapshot;
    std::unordered_map<std::string, size_t> peakSysSnapshot;
    std::unordered_map<std::string, size_t> peakLocalSnapshot;
    size_t peakSysTotal = 0;
    size_t peakLocalTotal = 0;

    {
        std::lock_guard theLock(this->mutex);
        sysSummarySnapshot = this->systemMemoryAllocationSummary;
        localSummarySnapshot = this->localMemoryAllocationSummary;
        peakSysSnapshot = this->peakSystemMemoryByType;
        peakLocalSnapshot = this->peakLocalMemoryByType;
        peakSysTotal = this->peakTotalSystemMemory;
        peakLocalTotal = this->peakTotalLocalMemory;
    }

    size_t totalSystemMemoryAllocated = 0;
    for (auto &[allocationTypeName, totalBytes] : sysSummarySnapshot) {
        totalSystemMemoryAllocated += totalBytes;
    }
    size_t totalLocalMemoryAllocated = 0;
    for (auto &[allocationTypeName, totalBytes] : localSummarySnapshot) {
        totalLocalMemoryAllocated += totalBytes;
    }

    if (totalSystemMemoryAllocated == 0 && totalLocalMemoryAllocated == 0) {
        return;
    }

    PRINT_STRING(true, stdout, "=== Allocation Summary Report ===\n");

    auto printMemorySection = [](const char *sectionTitle, size_t totalBytesInSection, std::unordered_map<std::string, size_t> &allocationTypeMap) {
        PRINT_STRING(true, stdout, "%s (total: %zu bytes):\n", sectionTitle, totalBytesInSection);
        std::vector<std::pair<std::string, size_t>> entriesSortedBySize(allocationTypeMap.begin(), allocationTypeMap.end());
        std::sort(entriesSortedBySize.begin(), entriesSortedBySize.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
        for (auto &[allocationTypeName, totalBytes] : entriesSortedBySize) {
            double percentageOfTotal = 100.0 * static_cast<double>(totalBytes) / static_cast<double>(totalBytesInSection);
            PRINT_STRING(true, stdout, "  %-40s %12zu bytes  (%6.2f%%)\n", allocationTypeName.c_str(), totalBytes, percentageOfTotal);
        }
    };

    if (totalSystemMemoryAllocated > 0) {
        printMemorySection("System Memory Allocations", totalSystemMemoryAllocated, sysSummarySnapshot);
    }
    if (totalLocalMemoryAllocated > 0) {
        printMemorySection("Local Memory Allocations", totalLocalMemoryAllocated, localSummarySnapshot);
    }

    if (peakSysTotal > 0 || peakLocalTotal > 0) {
        PRINT_STRING(true, stdout, "=== Peak Live Memory ===\n");
        if (peakSysTotal > 0) {
            printMemorySection("Peak System Memory", peakSysTotal, peakSysSnapshot);
        }
        if (peakLocalTotal > 0) {
            printMemorySection("Peak Local Memory", peakLocalTotal, peakLocalSnapshot);
        }
    }

    PRINT_STRING(true, stdout, "=================================\n");
}

void AllocationSummaryTracker::trackAllocationForSummary(const char *allocTypeName, size_t allocationSize, bool isLocalMemory) {
    std::lock_guard theLock(this->mutex);
    if (isLocalMemory) {
        this->localMemoryAllocationSummary[allocTypeName] += allocationSize;
    } else {
        this->systemMemoryAllocationSummary[allocTypeName] += allocationSize;
    }
}

void AllocationSummaryTracker::trackLiveAllocation(const char *allocTypeName, size_t allocationSize, bool isLocalMemory) {
    std::lock_guard theLock(this->mutex);
    if (isLocalMemory) {
        this->currentLocalMemoryByType[allocTypeName] += allocationSize;
        this->currentTotalLocalMemory += allocationSize;
        if (this->currentTotalLocalMemory > this->peakTotalLocalMemory) {
            this->peakTotalLocalMemory = this->currentTotalLocalMemory;
            this->peakLocalMemoryByType = this->currentLocalMemoryByType;
        }
    } else {
        this->currentSystemMemoryByType[allocTypeName] += allocationSize;
        this->currentTotalSystemMemory += allocationSize;
        if (this->currentTotalSystemMemory > this->peakTotalSystemMemory) {
            this->peakTotalSystemMemory = this->currentTotalSystemMemory;
            this->peakSystemMemoryByType = this->currentSystemMemoryByType;
        }
    }
}

void AllocationSummaryTracker::untrackLiveAllocation(const char *allocTypeName, size_t allocationSize, bool isLocalMemory) {
    std::lock_guard theLock(this->mutex);
    if (isLocalMemory) {
        auto it = this->currentLocalMemoryByType.find(allocTypeName);
        if (it != this->currentLocalMemoryByType.end()) {
            it->second = (it->second >= allocationSize) ? it->second - allocationSize : 0;
            if (it->second == 0) {
                this->currentLocalMemoryByType.erase(it);
            }
        }
        this->currentTotalLocalMemory = (this->currentTotalLocalMemory >= allocationSize) ? this->currentTotalLocalMemory - allocationSize : 0;
    } else {
        auto it = this->currentSystemMemoryByType.find(allocTypeName);
        if (it != this->currentSystemMemoryByType.end()) {
            it->second = (it->second >= allocationSize) ? it->second - allocationSize : 0;
            if (it->second == 0) {
                this->currentSystemMemoryByType.erase(it);
            }
        }
        this->currentTotalSystemMemory = (this->currentTotalSystemMemory >= allocationSize) ? this->currentTotalSystemMemory - allocationSize : 0;
    }
}

const char *getAllocationTypeString(GraphicsAllocation const *graphicsAllocation) {
    auto type = graphicsAllocation->getAllocationType();

    switch (type) {
    case AllocationType::buffer:
        return "BUFFER";
    case AllocationType::bufferHostMemory:
        return "BUFFER_HOST_MEMORY";
    case AllocationType::commandBuffer:
        return "COMMAND_BUFFER";
    case AllocationType::constantSurface:
        return "CONSTANT_SURFACE";
    case AllocationType::externalHostPtr:
        return "EXTERNAL_HOST_PTR";
    case AllocationType::fillPattern:
        return "FILL_PATTERN";
    case AllocationType::globalSurface:
        return "GLOBAL_SURFACE";
    case AllocationType::image:
        return "IMAGE";
    case AllocationType::indirectObjectHeap:
        return "INDIRECT_OBJECT_HEAP";
    case AllocationType::instructionHeap:
        return "INSTRUCTION_HEAP";
    case AllocationType::internalHeap:
        return "INTERNAL_HEAP";
    case AllocationType::internalHostMemory:
        return "INTERNAL_HOST_MEMORY";
    case AllocationType::kernelArgsBuffer:
        return "KERNEL_ARGS_BUFFER";
    case AllocationType::kernelIsa:
        return "KERNEL_ISA";
    case AllocationType::kernelIsaInternal:
        return "KERNEL_ISA_INTERNAL";
    case AllocationType::linearStream:
        return "LINEAR_STREAM";
    case AllocationType::mapAllocation:
        return "MAP_ALLOCATION";
    case AllocationType::mcs:
        return "MCS";
    case AllocationType::preemption:
        return "PREEMPTION";
    case AllocationType::printfSurface:
        return "PRINTF_SURFACE";
    case AllocationType::privateSurface:
        return "PRIVATE_SURFACE";
    case AllocationType::profilingTagBuffer:
        return "PROFILING_TAG_BUFFER";
    case AllocationType::scratchSurface:
        return "SCRATCH_SURFACE";
    case AllocationType::sharedBuffer:
        return "SHARED_BUFFER";
    case AllocationType::sharedImage:
        return "SHARED_IMAGE";
    case AllocationType::sharedResourceCopy:
        return "SHARED_RESOURCE_COPY";
    case AllocationType::surfaceStateHeap:
        return "SURFACE_STATE_HEAP";
    case AllocationType::svmCpu:
        return "SVM_CPU";
    case AllocationType::svmGpu:
        return "SVM_GPU";
    case AllocationType::svmZeroCopy:
        return "SVM_ZERO_COPY";
    case AllocationType::syncBuffer:
        return "SYNC_BUFFER";
    case AllocationType::tagBuffer:
        return "TAG_BUFFER";
    case AllocationType::globalFence:
        return "GLOBAL_FENCE";
    case AllocationType::timestampPacketTagBuffer:
        return "TIMESTAMP_PACKET_TAG_BUFFER";
    case AllocationType::unknown:
        return "UNKNOWN";
    case AllocationType::writeCombined:
        return "WRITE_COMBINED";
    case AllocationType::debugContextSaveArea:
        return "DEBUG_CONTEXT_SAVE_AREA";
    case AllocationType::debugSbaTrackingBuffer:
        return "DEBUG_SBA_TRACKING_BUFFER";
    case AllocationType::debugModuleArea:
        return "DEBUG_MODULE_AREA";
    case AllocationType::workPartitionSurface:
        return "WORK_PARTITION_SURFACE";
    case AllocationType::gpuTimestampDeviceBuffer:
        return "GPU_TIMESTAMP_DEVICE_BUFFER";
    case AllocationType::ringBuffer:
        return "RING_BUFFER";
    case AllocationType::semaphoreBuffer:
        return "SEMAPHORE_BUFFER";
    case AllocationType::unifiedSharedMemory:
        return "UNIFIED_SHARED_MEMORY";
    case AllocationType::swTagBuffer:
        return "SW_TAG_BUFFER";
    case AllocationType::deferredTasksList:
        return "DEFERRED_TASKS_LIST";
    case AllocationType::assertBuffer:
        return "ASSERT_BUFFER";
    case AllocationType::syncDispatchToken:
        return "SYNC_DISPATCH_TOKEN";
    case AllocationType::hostFunction:
        return "HOST_FUNCTION";
    default:
        return "ILLEGAL_VALUE";
    }
}

const char *getMemoryPoolString(GraphicsAllocation const *graphicsAllocation) {
    auto pool = graphicsAllocation->getMemoryPool();

    switch (pool) {
    case MemoryPool::memoryNull:
        return "MemoryNull";
    case MemoryPool::system4KBPages:
        return "System4KBPages";
    case MemoryPool::system64KBPages:
        return "System64KBPages";
    case MemoryPool::system4KBPagesWith32BitGpuAddressing:
        return "System4KBPagesWith32BitGpuAddressing";
    case MemoryPool::system64KBPagesWith32BitGpuAddressing:
        return "System64KBPagesWith32BitGpuAddressing";
    case MemoryPool::systemCpuInaccessible:
        return "SystemCpuInaccessible";
    case MemoryPool::localMemory:
        return "LocalMemory";
    }

    UNRECOVERABLE_IF(true);
    return "ILLEGAL_VALUE";
}

} // namespace NEO
