/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/logger.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/sys_calls_common.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace NEO {

std::string getFileLoggerFileName(const DebugVariables &flags) {
    const auto &forcedLogFileName = flags.OverrideIgdrclLogFileName.get();
    if (forcedLogFileName != "unk") {
        return forcedLogFileName;
    }

    return "igdrcl_" + std::to_string(SysCalls::getProcessId()) + ".log";
}

FileLogger<globalDebugFunctionalityLevel> &fileLoggerInstance() {
    static FileLogger<globalDebugFunctionalityLevel> fileLoggerInstance(getFileLoggerFileName(debugManager.flags), debugManager.flags);
    return fileLoggerInstance;
}

FileLogger<globalDebugFunctionalityLevel> &usmReusePerfLoggerInstance() {
    static FileLogger<globalDebugFunctionalityLevel> usmReusePerfLoggerInstance(std::string("usm_reuse_perf.csv"), debugManager.flags);
    return usmReusePerfLoggerInstance;
}

template <DebugFunctionalityLevel debugLevel>
FileLogger<debugLevel>::FileLogger(std::string filename, const DebugVariables &flags) {
    logFileName = std::move(filename);
    if (enabled()) {
        std::remove(logFileName.c_str());
    }

    dumpKernels = flags.DumpKernels.get();
    logApiCalls = flags.LogApiCalls.get();
    logAllocationMemoryPool = flags.LogAllocationMemoryPool.get();
    logAllocationType = flags.LogAllocationType.get();
    logAllocationStdout = flags.LogAllocationStdout.get();
    logAllocationSummaryReport = flags.LogAllocationSummaryReport.get();
}

template <DebugFunctionalityLevel debugLevel>
FileLogger<debugLevel>::~FileLogger() {
    if (logAllocationSummaryReport) {
        printAllocationSummaryReport();
    }
}

void AllocationSummaryTracker::printReport() {
    std::unordered_map<std::string, size_t> sysSummarySnapshot;
    std::unordered_map<std::string, size_t> localSummarySnapshot;
    std::unordered_map<std::string, size_t> peakSysSnapshot;
    std::unordered_map<std::string, size_t> peakLocalSnapshot;
    size_t peakSysTotal = 0;
    size_t peakLocalTotal = 0;

    {
        std::lock_guard theLock(mutex);
        sysSummarySnapshot = systemMemoryAllocationSummary;
        localSummarySnapshot = localMemoryAllocationSummary;
        peakSysSnapshot = peakSystemMemoryByType;
        peakLocalSnapshot = peakLocalMemoryByType;
        peakSysTotal = peakTotalSystemMemory;
        peakLocalTotal = peakTotalLocalMemory;
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
    std::lock_guard theLock(mutex);
    if (isLocalMemory) {
        localMemoryAllocationSummary[allocTypeName] += allocationSize;
    } else {
        systemMemoryAllocationSummary[allocTypeName] += allocationSize;
    }
}

void AllocationSummaryTracker::trackLiveAllocation(const char *allocTypeName, size_t allocationSize, bool isLocalMemory) {
    std::lock_guard theLock(mutex);
    if (isLocalMemory) {
        currentLocalMemoryByType[allocTypeName] += allocationSize;
        currentTotalLocalMemory += allocationSize;
        if (currentTotalLocalMemory > peakTotalLocalMemory) {
            peakTotalLocalMemory = currentTotalLocalMemory;
            peakLocalMemoryByType = currentLocalMemoryByType;
        }
    } else {
        currentSystemMemoryByType[allocTypeName] += allocationSize;
        currentTotalSystemMemory += allocationSize;
        if (currentTotalSystemMemory > peakTotalSystemMemory) {
            peakTotalSystemMemory = currentTotalSystemMemory;
            peakSystemMemoryByType = currentSystemMemoryByType;
        }
    }
}

void AllocationSummaryTracker::untrackLiveAllocation(const char *allocTypeName, size_t allocationSize, bool isLocalMemory) {
    std::lock_guard theLock(mutex);
    if (isLocalMemory) {
        auto it = currentLocalMemoryByType.find(allocTypeName);
        if (it != currentLocalMemoryByType.end()) {
            it->second = (it->second >= allocationSize) ? it->second - allocationSize : 0;
            if (it->second == 0) {
                currentLocalMemoryByType.erase(it);
            }
        }
        currentTotalLocalMemory = (currentTotalLocalMemory >= allocationSize) ? currentTotalLocalMemory - allocationSize : 0;
    } else {
        auto it = currentSystemMemoryByType.find(allocTypeName);
        if (it != currentSystemMemoryByType.end()) {
            it->second = (it->second >= allocationSize) ? it->second - allocationSize : 0;
            if (it->second == 0) {
                currentSystemMemoryByType.erase(it);
            }
        }
        currentTotalSystemMemory = (currentTotalSystemMemory >= allocationSize) ? currentTotalSystemMemory - allocationSize : 0;
    }
}

template <DebugFunctionalityLevel debugLevel>
void FileLogger<debugLevel>::writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode) {
    std::lock_guard theLock(mutex);
    writeDataToFile(filename.c_str(), {str, length}, (mode & std::ios::app) != 0);
}

template <DebugFunctionalityLevel debugLevel>
void FileLogger<debugLevel>::logDebugString(bool enableLog, std::string_view debugString) {
    if (enabled()) {
        if (enableLog) {
            writeToFile(logFileName, debugString.data(), debugString.size(), std::ios::app);
        }
    }
}

template <DebugFunctionalityLevel debugLevel>
void FileLogger<debugLevel>::dumpKernel(const std::string &name, const std::string &src) {
    if (false == enabled()) {
        return;
    }

    if (dumpKernels) {
        DBG_LOG(LogApiCalls, "Kernel size", src.size(), src.c_str());
        writeToFile(name + ".txt", src.c_str(), src.size(), std::ios::trunc);
    }
}

template <DebugFunctionalityLevel debugLevel>
void FileLogger<debugLevel>::logApiCall(const char *function, bool enter, int32_t errorCode) {
    if (false == enabled()) {
        return;
    }

    if (logApiCalls) {
        std::thread::id thisThread = std::this_thread::get_id();

        std::stringstream ss;
        ss << "ThreadID: " << thisThread << " ";
        if (enter) {
            ss << "Function Enter: ";
        } else {
            ss << "Function Leave (" << errorCode << "): ";
        }
        ss << function << std::endl;

        auto str = ss.str();
        writeToFile(logFileName, str.c_str(), str.size(), std::ios::app);
    }
}

template <DebugFunctionalityLevel debugLevel>
size_t FileLogger<debugLevel>::getInput(const size_t *input, int32_t index) {
    if (enabled() == false) {
        return 0;
    }
    return input != nullptr ? input[index] : 0;
}

template <DebugFunctionalityLevel debugLevel>
void FileLogger<debugLevel>::dumpBinaryProgram(int32_t numDevices, const size_t *lengths, const unsigned char **binaries) {
    if (false == enabled()) {
        return;
    }

    if (dumpKernels) {
        if (lengths != nullptr && binaries != nullptr &&
            lengths[0] != 0 && binaries[0] != nullptr) {
            writeToFile("programBinary.bin", reinterpret_cast<const char *>(binaries[0]), lengths[0], std::ios::trunc | std::ios::binary);
        }
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

template class FileLogger<DebugFunctionalityLevel::none>;
template class FileLogger<DebugFunctionalityLevel::regKeys>;
template class FileLogger<DebugFunctionalityLevel::full>;

template <>
void FileLoggerProxy<true>::logString(std::string_view data) {
    NEO::fileLoggerInstance().logDebugString(true, data);
}

} // namespace NEO
