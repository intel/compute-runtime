/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/logger.h"

#include "core/debug_settings/debug_settings_manager.h"
#include "runtime/event/event.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/mem_obj.h"

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
    dumpKernelArgsEnabled = flags.DumpKernelArgs.get();
    logApiCalls = flags.LogApiCalls.get();
    logAllocationMemoryPool = flags.LogAllocationMemoryPool.get();
}

template <DebugFunctionalityLevel DebugLevel>
FileLogger<DebugLevel>::~FileLogger() = default;

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode) {
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
        std::unique_lock<std::mutex> theLock(mtx);
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
    if (false == enabled()) {
        return;
    }

    if (logAllocationMemoryPool) {
        std::thread::id thisThread = std::this_thread::get_id();

        std::stringstream ss;
        ss << " ThreadID: " << thisThread;
        ss << " AllocationType: " << getAllocationTypeString(graphicsAllocation);
        ss << " MemoryPool: " << graphicsAllocation->getMemoryPool();
        ss << graphicsAllocation->getAllocationInfoString();
        ss << std::endl;

        auto str = ss.str();

        std::unique_lock<std::mutex> theLock(mtx);
        writeToFile(logFileName, str.c_str(), str.size(), std::ios::app);
    }
}

template <DebugFunctionalityLevel DebugLevel>
size_t FileLogger<DebugLevel>::getInput(const size_t *input, int32_t index) {
    if (enabled() == false)
        return 0;
    return input != nullptr ? input[index] : 0;
}

template <DebugFunctionalityLevel DebugLevel>
const std::string FileLogger<DebugLevel>::getEvents(const uintptr_t *input, uint32_t numOfEvents) {
    if (false == enabled()) {
        return "";
    }

    std::stringstream os;
    for (uint32_t i = 0; i < numOfEvents; i++) {
        if (input != nullptr) {
            cl_event event = ((cl_event *)input)[i];
            os << "cl_event " << event << ", Event " << (Event *)event << ", ";
        }
    }
    return os.str();
}

template <DebugFunctionalityLevel DebugLevel>
const std::string FileLogger<DebugLevel>::getMemObjects(const uintptr_t *input, uint32_t numOfObjects) {
    if (false == enabled()) {
        return "";
    }

    std::stringstream os;
    for (uint32_t i = 0; i < numOfObjects; i++) {
        if (input != nullptr) {
            cl_mem mem = const_cast<cl_mem>(reinterpret_cast<const cl_mem *>(input)[i]);
            os << "cl_mem " << mem << ", MemObj " << static_cast<MemObj *>(mem) << ", ";
        }
    }
    return os.str();
}

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::dumpBinaryProgram(int32_t numDevices, const size_t *lengths, const unsigned char **binaries) {
    if (false == enabled()) {
        return;
    }

    if (dumpKernels) {
        if (lengths != nullptr && binaries != nullptr &&
            lengths[0] != 0 && binaries[0] != nullptr) {
            std::unique_lock<std::mutex> theLock(mtx);
            writeToFile("programBinary.bin", reinterpret_cast<const char *>(binaries[0]), lengths[0], std::ios::trunc | std::ios::binary);
        }
    }
}

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::dumpKernelArgs(const Kernel *kernel) {
    if (false == enabled()) {
        return;
    }
    if (dumpKernelArgsEnabled && kernel != nullptr) {
        std::unique_lock<std::mutex> theLock(mtx);
        std::ofstream outFile;

        for (unsigned int i = 0; i < kernel->getKernelInfo().kernelArgInfo.size(); i++) {
            std::string type;
            std::string fileName;
            const char *ptr = nullptr;
            size_t size = 0;
            uint64_t flags = 0;
            std::unique_ptr<char[]> argVal = nullptr;

            auto &argInfo = kernel->getKernelInfo().kernelArgInfo[i];

            if (argInfo.addressQualifier == CL_KERNEL_ARG_ADDRESS_LOCAL) {
                type = "local";
            } else if (argInfo.typeStr.find("image") != std::string::npos) {
                type = "image";
                auto clMem = (const cl_mem)kernel->getKernelArg(i);
                auto memObj = castToObject<MemObj>(clMem);
                if (memObj != nullptr) {
                    ptr = static_cast<char *>(memObj->getCpuAddress());
                    size = memObj->getSize();
                    flags = memObj->getMemoryPropertiesFlags();
                }
            } else if (argInfo.typeStr.find("sampler") != std::string::npos) {
                type = "sampler";
            } else if (argInfo.typeStr.find("*") != std::string::npos) {
                type = "buffer";
                auto clMem = (const cl_mem)kernel->getKernelArg(i);
                auto memObj = castToObject<MemObj>(clMem);
                if (memObj != nullptr) {
                    ptr = static_cast<char *>(memObj->getCpuAddress());
                    size = memObj->getSize();
                    flags = memObj->getMemoryPropertiesFlags();
                }
            } else {
                type = "immediate";
                auto crossThreadData = kernel->getCrossThreadData();
                auto crossThreadDataSize = kernel->getCrossThreadDataSize();
                argVal = std::unique_ptr<char[]>(new char[crossThreadDataSize]);

                size_t totalArgSize = 0;
                for (const auto &kernelArgPatchInfo : argInfo.kernelArgPatchInfoVector) {
                    auto pSource = ptrOffset(crossThreadData, kernelArgPatchInfo.crossthreadOffset);
                    auto pDestination = ptrOffset(argVal.get(), kernelArgPatchInfo.sourceOffset);
                    memcpy_s(pDestination, kernelArgPatchInfo.size, pSource, kernelArgPatchInfo.size);
                    totalArgSize += kernelArgPatchInfo.size;
                }
                size = totalArgSize;
                ptr = argVal.get();
            }

            if (ptr && size) {
                fileName = kernel->getKernelInfo().name + "_arg_" + std::to_string(i) + "_" + type + "_size_" + std::to_string(size) + "_flags_" + std::to_string(flags) + ".bin";
                writeToFile(fileName, ptr, size, std::ios::trunc | std::ios::binary);
            }
        }
    }
}

template <DebugFunctionalityLevel DebugLevel>
void FileLogger<DebugLevel>::dumpKernelArgs(const MultiDispatchInfo *multiDispatchInfo) {
    if (enabled() == false) {
        return;
    }

    if (dumpKernelArgsEnabled == false || multiDispatchInfo == nullptr) {
        return;
    }

    for (auto &dispatchInfo : *multiDispatchInfo) {
        dumpKernelArgs(dispatchInfo.getKernel());
    }
}

template <DebugFunctionalityLevel DebugLevel>
const char *FileLogger<DebugLevel>::getAllocationTypeString(GraphicsAllocation const *graphicsAllocation) {
    if (false == enabled()) {
        return nullptr;
    }

    auto type = graphicsAllocation->getAllocationType();

    switch (type) {
    case GraphicsAllocation::AllocationType::BUFFER:
        return "BUFFER";
    case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
        return "BUFFER_COMPRESSED";
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
        return "BUFFER_HOST_MEMORY";
    case GraphicsAllocation::AllocationType::COMMAND_BUFFER:
        return "COMMAND_BUFFER";
    case GraphicsAllocation::AllocationType::CONSTANT_SURFACE:
        return "CONSTANT_SURFACE";
    case GraphicsAllocation::AllocationType::DEVICE_QUEUE_BUFFER:
        return "DEVICE_QUEUE_BUFFER";
    case GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR:
        return "EXTERNAL_HOST_PTR";
    case GraphicsAllocation::AllocationType::FILL_PATTERN:
        return "FILL_PATTERN";
    case GraphicsAllocation::AllocationType::GLOBAL_SURFACE:
        return "GLOBAL_SURFACE";
    case GraphicsAllocation::AllocationType::IMAGE:
        return "IMAGE";
    case GraphicsAllocation::AllocationType::INDIRECT_OBJECT_HEAP:
        return "INDIRECT_OBJECT_HEAP";
    case GraphicsAllocation::AllocationType::INSTRUCTION_HEAP:
        return "INSTRUCTION_HEAP";
    case GraphicsAllocation::AllocationType::INTERNAL_HEAP:
        return "INTERNAL_HEAP";
    case GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY:
        return "INTERNAL_HOST_MEMORY";
    case GraphicsAllocation::AllocationType::KERNEL_ISA:
        return "KERNEL_ISA";
    case GraphicsAllocation::AllocationType::LINEAR_STREAM:
        return "LINEAR_STREAM";
    case GraphicsAllocation::AllocationType::MAP_ALLOCATION:
        return "MAP_ALLOCATION";
    case GraphicsAllocation::AllocationType::MCS:
        return "MCS";
    case GraphicsAllocation::AllocationType::PIPE:
        return "PIPE";
    case GraphicsAllocation::AllocationType::PREEMPTION:
        return "PREEMPTION";
    case GraphicsAllocation::AllocationType::PRINTF_SURFACE:
        return "PRINTF_SURFACE";
    case GraphicsAllocation::AllocationType::PRIVATE_SURFACE:
        return "PRIVATE_SURFACE";
    case GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER:
        return "PROFILING_TAG_BUFFER";
    case GraphicsAllocation::AllocationType::SCRATCH_SURFACE:
        return "SCRATCH_SURFACE";
    case GraphicsAllocation::AllocationType::SHARED_BUFFER:
        return "SHARED_BUFFER";
    case GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE:
        return "SHARED_CONTEXT_IMAGE";
    case GraphicsAllocation::AllocationType::SHARED_IMAGE:
        return "SHARED_IMAGE";
    case GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY:
        return "SHARED_RESOURCE_COPY";
    case GraphicsAllocation::AllocationType::SURFACE_STATE_HEAP:
        return "SURFACE_STATE_HEAP";
    case GraphicsAllocation::AllocationType::SVM_CPU:
        return "SVM_CPU";
    case GraphicsAllocation::AllocationType::SVM_GPU:
        return "SVM_GPU";
    case GraphicsAllocation::AllocationType::SVM_ZERO_COPY:
        return "SVM_ZERO_COPY";
    case GraphicsAllocation::AllocationType::TAG_BUFFER:
        return "TAG_BUFFER";
    case GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
        return "TIMESTAMP_PACKET_TAG_BUFFER";
    case GraphicsAllocation::AllocationType::UNKNOWN:
        return "UNKNOWN";
    case GraphicsAllocation::AllocationType::WRITE_COMBINED:
        return "WRITE_COMBINED";
    default:
        return "ILLEGAL_VALUE";
    }
}

template class FileLogger<DebugFunctionalityLevel::None>;
template class FileLogger<DebugFunctionalityLevel::RegKeys>;
template class FileLogger<DebugFunctionalityLevel::Full>;

} // namespace NEO
