/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "debug_settings_manager.h"

#include "core/helpers/ptr_math.h"
#include "runtime/event/event.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/os_interface/definitions/translate_debug_settings.h"
#include "runtime/utilities/debug_settings_reader_creator.h"

#include "CL/cl.h"

#include <cstdio>
#include <sstream>

namespace std {
static std::string to_string(const std::string &arg) {
    return arg;
}
} // namespace std

namespace NEO {

DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager;

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::DebugSettingsManager() {

    logFileName = "igdrcl.log";
    if (registryReadAvailable()) {
        readerImpl = SettingsReaderCreator::create();
        injectSettingsFromReader();
        dumpFlags();
    }
    translateDebugSettings(flags);
    std::remove(logFileName.c_str());
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode) {
    std::ofstream outFile(filename, mode);
    if (outFile.is_open()) {
        outFile.write(str, length);
        outFile.close();
    }
}

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::~DebugSettingsManager() = default;

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::getHardwareInfoOverride(std::string &hwInfoConfig) {
    std::string str = flags.HardwareInfoOverride.get();
    if (str[0] == '\"') {
        str.pop_back();
        hwInfoConfig = str.substr(1, std::string::npos);
    } else {
        hwInfoConfig = str;
    }
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::dumpKernel(const std::string &name, const std::string &src) {
    if (false == debugKernelDumpingAvailable()) {
        return;
    }

    if (flags.DumpKernels.get()) {
        DBG_LOG(LogApiCalls, "Kernel size", src.size(), src.c_str());
        writeToFile(name + ".txt", src.c_str(), src.size(), std::ios::trunc);
    }
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::logApiCall(const char *function, bool enter, int32_t errorCode) {
    if (false == debugLoggingAvailable()) {
        return;
    }

    if (flags.LogApiCalls.get()) {
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
void DebugSettingsManager<DebugLevel>::logAllocation(GraphicsAllocation const *graphicsAllocation) {
    if (false == debugLoggingAvailable()) {
        return;
    }

    if (flags.LogAllocationMemoryPool.get()) {
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
size_t DebugSettingsManager<DebugLevel>::getInput(const size_t *input, int32_t index) {
    if (debugLoggingAvailable() == false)
        return 0;
    return input != nullptr ? input[index] : 0;
}

template <DebugFunctionalityLevel DebugLevel>
const std::string DebugSettingsManager<DebugLevel>::getEvents(const uintptr_t *input, uint32_t numOfEvents) {
    if (false == debugLoggingAvailable()) {
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
const std::string DebugSettingsManager<DebugLevel>::getMemObjects(const uintptr_t *input, uint32_t numOfObjects) {
    if (false == debugLoggingAvailable()) {
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
template <typename DataType>
void DebugSettingsManager<DebugLevel>::dumpNonDefaultFlag(const char *variableName, const DataType &variableValue, const DataType &defaultValue) {
    if (variableValue != defaultValue) {
        const auto variableStringValue = std::to_string(variableValue);
        printDebugString(true, stdout, "Non-default value of debug variable: %s = %s\n", variableName, variableStringValue.c_str());
    }
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::dumpFlags() const {
    if (flags.PrintDebugSettings.get() == false) {
        return;
    }

    std::ofstream settingsDumpFile{settingsDumpFileName, std::ios::out};
    DEBUG_BREAK_IF(!settingsDumpFile.good());

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)   \
    settingsDumpFile << #variableName << " = " << flags.variableName.get() << '\n'; \
    dumpNonDefaultFlag(#variableName, flags.variableName.get(), defaultValue);
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::dumpBinaryProgram(int32_t numDevices, const size_t *lengths, const unsigned char **binaries) {
    if (false == debugKernelDumpingAvailable()) {
        return;
    }

    if (flags.DumpKernels.get()) {
        if (lengths != nullptr && binaries != nullptr &&
            lengths[0] != 0 && binaries[0] != nullptr) {
            std::unique_lock<std::mutex> theLock(mtx);
            writeToFile("programBinary.bin", reinterpret_cast<const char *>(binaries[0]), lengths[0], std::ios::trunc | std::ios::binary);
        }
    }
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::dumpKernelArgs(const Kernel *kernel) {
    if (false == kernelArgDumpingAvailable()) {
        return;
    }
    if (flags.DumpKernelArgs.get() && kernel != nullptr) {
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
                    flags = memObj->getFlags();
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
                    flags = memObj->getFlags();
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
void DebugSettingsManager<DebugLevel>::dumpKernelArgs(const MultiDispatchInfo *multiDispatchInfo) {
    if (kernelArgDumpingAvailable() == false) {
        return;
    }

    if (flags.DumpKernelArgs.get() == false || multiDispatchInfo == nullptr) {
        return;
    }

    for (auto &dispatchInfo : *multiDispatchInfo) {
        dumpKernelArgs(dispatchInfo.getKernel());
    }
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::injectSettingsFromReader() {
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)            \
    {                                                                                        \
        dataType tempData = readerImpl->getSetting(#variableName, flags.variableName.get()); \
        flags.variableName.set(tempData);                                                    \
    }
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}

template <DebugFunctionalityLevel DebugLevel>
const char *DebugSettingsManager<DebugLevel>::getAllocationTypeString(GraphicsAllocation const *graphicsAllocation) {
    if (false == debugLoggingAvailable()) {
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

template class DebugSettingsManager<DebugFunctionalityLevel::None>;
template class DebugSettingsManager<DebugFunctionalityLevel::Full>;
template class DebugSettingsManager<DebugFunctionalityLevel::RegKeys>;
}; // namespace NEO
