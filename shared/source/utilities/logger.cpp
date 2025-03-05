/*
 * Copyright (C) 2019-2025 Intel Corporation
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
#include "shared/source/utilities/io_functions.h"

#include <fstream>
#include <memory>
#include <string>

namespace NEO {

FileLogger<globalDebugFunctionalityLevel> &fileLoggerInstance() {
    static FileLogger<globalDebugFunctionalityLevel> fileLoggerInstance(std::string("igdrcl.log"), debugManager.flags);
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
}

template <DebugFunctionalityLevel debugLevel>
FileLogger<debugLevel>::~FileLogger() = default;

template <DebugFunctionalityLevel debugLevel>
void FileLogger<debugLevel>::writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode) {
    std::lock_guard theLock(mutex);
    std::ofstream outFile(filename, mode);
    if (outFile.is_open()) {
        outFile.write(str, length);
        outFile.close();
    }
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
        if (enter)
            ss << "Function Enter: ";
        else
            ss << "Function Leave (" << errorCode << "): ";
        ss << function << std::endl;

        auto str = ss.str();
        writeToFile(logFileName, str.c_str(), str.size(), std::ios::app);
    }
}

template <DebugFunctionalityLevel debugLevel>
size_t FileLogger<debugLevel>::getInput(const size_t *input, int32_t index) {
    if (enabled() == false)
        return 0;
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
    case AllocationType::pipe:
        return "PIPE";
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

} // namespace NEO
