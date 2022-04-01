/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/logger.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/timestamp_packet.h"

#include <memory>
#include <string>

namespace NEO {

FileLogger<globalDebugFunctionalityLevel> &FileLoggerInstance() {
    static FileLogger<globalDebugFunctionalityLevel> fileLoggerInstance(std::string("igdrcl.log"), DebugManager.flags);
    return fileLoggerInstance;
}

template <DebugFunctionalityLevel DebugLevel>
FileLogger<DebugLevel>::FileLogger(std::string filename, const DebugVariables &flags) {
    logFileName = filename;
    std::remove(logFileName.c_str());

    dumpKernels = flags.DumpKernels.get();
    logApiCalls = flags.LogApiCalls.get();
    logAllocationMemoryPool = flags.LogAllocationMemoryPool.get();
    logAllocationType = flags.LogAllocationType.get();
    logAllocationStdout = flags.LogAllocationStdout.get();
}

template <DebugFunctionalityLevel DebugLevel>
FileLogger<DebugLevel>::~FileLogger() = default;

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode) {
    std::unique_lock<std::mutex> theLock(mutex);
    std::ofstream outFile(filename, mode);
    if (outFile.is_open()) {
        outFile.write(str, length);
        outFile.close();
    }
}

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::dumpKernel(const std::string &name, const std::string &src) {
    if (false == enabled()) {
        return;
    }

    if (dumpKernels) {
        DBG_LOG(LogApiCalls, "Kernel size", src.size(), src.c_str());
        writeToFile(name + ".txt", src.c_str(), src.size(), std::ios::trunc);
    }
}

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::logApiCall(const char *function, bool enter, int32_t errorCode) {
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

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::logAllocation(GraphicsAllocation const *graphicsAllocation) {
    if (logAllocationType) {
        printDebugString(true, stdout, "Created Graphics Allocation of type %s\n", getAllocationTypeString(graphicsAllocation));
    }

    if (false == enabled()) {
        return;
    }

    if (logAllocationMemoryPool || logAllocationType) {
        std::stringstream ss;
        std::thread::id thisThread = std::this_thread::get_id();

        ss << " ThreadID: " << thisThread;
        ss << " AllocationType: " << getAllocationTypeString(graphicsAllocation);
        ss << " MemoryPool: " << graphicsAllocation->getMemoryPool();
        ss << " Root device index: " << graphicsAllocation->getRootDeviceIndex();
        ss << " GPU address: 0x" << std::hex << graphicsAllocation->getGpuAddress() << " - 0x" << std::hex << graphicsAllocation->getGpuAddress() + graphicsAllocation->getUnderlyingBufferSize() - 1;

        ss << graphicsAllocation->getAllocationInfoString();
        ss << std::endl;
        auto str = ss.str();
        if (logAllocationStdout) {
            printf("%s", str.c_str());
            return;
        }

        if (logAllocationMemoryPool || logAllocationType) {
            writeToFile(logFileName, str.c_str(), str.size(), std::ios::app);
        }
    }
}

template <DebugFunctionalityLevel DebugLevel>
size_t FileLogger<DebugLevel>::getInput(const size_t *input, int32_t index) {
    if (enabled() == false)
        return 0;
    return input != nullptr ? input[index] : 0;
}

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::dumpBinaryProgram(int32_t numDevices, const size_t *lengths, const unsigned char **binaries) {
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
    case AllocationType::BUFFER:
        return "BUFFER";
    case AllocationType::BUFFER_HOST_MEMORY:
        return "BUFFER_HOST_MEMORY";
    case AllocationType::COMMAND_BUFFER:
        return "COMMAND_BUFFER";
    case AllocationType::CONSTANT_SURFACE:
        return "CONSTANT_SURFACE";
    case AllocationType::EXTERNAL_HOST_PTR:
        return "EXTERNAL_HOST_PTR";
    case AllocationType::FILL_PATTERN:
        return "FILL_PATTERN";
    case AllocationType::GLOBAL_SURFACE:
        return "GLOBAL_SURFACE";
    case AllocationType::IMAGE:
        return "IMAGE";
    case AllocationType::INDIRECT_OBJECT_HEAP:
        return "INDIRECT_OBJECT_HEAP";
    case AllocationType::INSTRUCTION_HEAP:
        return "INSTRUCTION_HEAP";
    case AllocationType::INTERNAL_HEAP:
        return "INTERNAL_HEAP";
    case AllocationType::INTERNAL_HOST_MEMORY:
        return "INTERNAL_HOST_MEMORY";
    case AllocationType::KERNEL_ISA:
        return "KERNEL_ISA";
    case AllocationType::KERNEL_ISA_INTERNAL:
        return "KERNEL_ISA_INTERNAL";
    case AllocationType::LINEAR_STREAM:
        return "LINEAR_STREAM";
    case AllocationType::MAP_ALLOCATION:
        return "MAP_ALLOCATION";
    case AllocationType::MCS:
        return "MCS";
    case AllocationType::PIPE:
        return "PIPE";
    case AllocationType::PREEMPTION:
        return "PREEMPTION";
    case AllocationType::PRINTF_SURFACE:
        return "PRINTF_SURFACE";
    case AllocationType::PRIVATE_SURFACE:
        return "PRIVATE_SURFACE";
    case AllocationType::PROFILING_TAG_BUFFER:
        return "PROFILING_TAG_BUFFER";
    case AllocationType::SCRATCH_SURFACE:
        return "SCRATCH_SURFACE";
    case AllocationType::SHARED_BUFFER:
        return "SHARED_BUFFER";
    case AllocationType::SHARED_CONTEXT_IMAGE:
        return "SHARED_CONTEXT_IMAGE";
    case AllocationType::SHARED_IMAGE:
        return "SHARED_IMAGE";
    case AllocationType::SHARED_RESOURCE_COPY:
        return "SHARED_RESOURCE_COPY";
    case AllocationType::SURFACE_STATE_HEAP:
        return "SURFACE_STATE_HEAP";
    case AllocationType::SVM_CPU:
        return "SVM_CPU";
    case AllocationType::SVM_GPU:
        return "SVM_GPU";
    case AllocationType::SVM_ZERO_COPY:
        return "SVM_ZERO_COPY";
    case AllocationType::TAG_BUFFER:
        return "TAG_BUFFER";
    case AllocationType::GLOBAL_FENCE:
        return "GLOBAL_FENCE";
    case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
        return "TIMESTAMP_PACKET_TAG_BUFFER";
    case AllocationType::UNKNOWN:
        return "UNKNOWN";
    case AllocationType::WRITE_COMBINED:
        return "WRITE_COMBINED";
    case AllocationType::DEBUG_CONTEXT_SAVE_AREA:
        return "DEBUG_CONTEXT_SAVE_AREA";
    case AllocationType::DEBUG_SBA_TRACKING_BUFFER:
        return "DEBUG_SBA_TRACKING_BUFFER";
    case AllocationType::DEBUG_MODULE_AREA:
        return "DEBUG_MODULE_AREA";
    case AllocationType::WORK_PARTITION_SURFACE:
        return "WORK_PARTITION_SURFACE";
    case AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
        return "GPU_TIMESTAMP_DEVICE_BUFFER";
    case AllocationType::RING_BUFFER:
        return "RING_BUFFER";
    case AllocationType::SEMAPHORE_BUFFER:
        return "SEMAPHORE_BUFFER";
    case AllocationType::UNIFIED_SHARED_MEMORY:
        return "UNIFIED_SHARED_MEMORY";
    case AllocationType::SW_TAG_BUFFER:
        return "SW_TAG_BUFFER";
    default:
        return "ILLEGAL_VALUE";
    }
}

template class FileLogger<DebugFunctionalityLevel::None>;
template class FileLogger<DebugFunctionalityLevel::RegKeys>;
template class FileLogger<DebugFunctionalityLevel::Full>;

} // namespace NEO
