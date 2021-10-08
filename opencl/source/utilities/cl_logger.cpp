/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/utilities/cl_logger.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/event/event.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {

template <DebugFunctionalityLevel DebugLevel>
ClFileLogger<DebugLevel>::ClFileLogger(FileLogger<DebugLevel> &baseLoggerIn, const DebugVariables &flags) : baseLogger(baseLoggerIn) {
    dumpKernelArgsEnabled = flags.DumpKernelArgs.get();
}

ClFileLogger<globalDebugFunctionalityLevel> &getClFileLogger() {
    static ClFileLogger<globalDebugFunctionalityLevel> clFileLoggerInstance(FileLoggerInstance(), DebugManager.flags);
    return clFileLoggerInstance;
}

template <DebugFunctionalityLevel DebugLevel>
void ClFileLogger<DebugLevel>::dumpKernelArgs(const MultiDispatchInfo *multiDispatchInfo) {
    if (false == baseLogger.enabled()) {
        return;
    }

    if (dumpKernelArgsEnabled == false || multiDispatchInfo == nullptr) {
        return;
    }

    for (auto &dispatchInfo : *multiDispatchInfo) {
        auto kernel = dispatchInfo.getKernel();
        if (kernel == nullptr) {
            continue;
        }
        const auto &kernelDescriptor = kernel->getKernelInfo().kernelDescriptor;
        const auto &explicitArgs = kernelDescriptor.payloadMappings.explicitArgs;
        for (unsigned int i = 0; i < explicitArgs.size(); i++) {
            std::string type;
            std::string fileName;
            const char *ptr = nullptr;
            size_t size = 0;
            uint64_t flags = 0;
            std::unique_ptr<char[]> argVal = nullptr;

            const auto &arg = explicitArgs[i];
            if (arg.getTraits().getAddressQualifier() == KernelArgMetadata::AddrLocal) {
                type = "local";
            } else if (arg.is<ArgDescriptor::ArgTImage>()) {
                type = "image";
                auto clMem = reinterpret_cast<const _cl_mem *>(kernel->getKernelArg(i));
                auto memObj = castToObject<MemObj>(clMem);
                if (memObj != nullptr) {
                    ptr = static_cast<char *>(memObj->getCpuAddress());
                    size = memObj->getSize();
                    flags = memObj->getFlags();
                }
            } else if (arg.is<ArgDescriptor::ArgTSampler>()) {
                type = "sampler";
            } else if (arg.is<ArgDescriptor::ArgTPointer>()) {
                type = "buffer";
                auto clMem = reinterpret_cast<const _cl_mem *>(kernel->getKernelArg(i));
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
                for (const auto &element : arg.as<ArgDescValue>().elements) {
                    auto pSource = ptrOffset(crossThreadData, element.offset);
                    auto pDestination = ptrOffset(argVal.get(), element.sourceOffset);
                    memcpy_s(pDestination, element.size, pSource, element.size);
                    totalArgSize += element.size;
                }
                size = totalArgSize;
                ptr = argVal.get();
            }

            if (ptr && size) {
                fileName = kernelDescriptor.kernelMetadata.kernelName + "_arg_" + std::to_string(i) + "_" + type + "_size_" + std::to_string(size) + "_flags_" + std::to_string(flags) + ".bin";
                baseLogger.writeToFile(fileName, ptr, size, std::ios::trunc | std::ios::binary);
            }
        }
    }
}

template <DebugFunctionalityLevel DebugLevel>
const std::string ClFileLogger<DebugLevel>::getEvents(const uintptr_t *input, uint32_t numOfEvents) {
    if (false == baseLogger.enabled()) {
        return "";
    }

    std::stringstream os;
    for (uint32_t i = 0; i < numOfEvents; i++) {
        if (input != nullptr) {
            cl_event event = (reinterpret_cast<const cl_event *>(input))[i];
            os << "cl_event " << event << ", Event " << (Event *)event << ", ";
        }
    }
    return os.str();
}

template <DebugFunctionalityLevel DebugLevel>
const std::string ClFileLogger<DebugLevel>::getMemObjects(const uintptr_t *input, uint32_t numOfObjects) {
    if (false == baseLogger.enabled()) {
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
template class ClFileLogger<DebugFunctionalityLevel::None>;
template class ClFileLogger<DebugFunctionalityLevel::RegKeys>;
template class ClFileLogger<DebugFunctionalityLevel::Full>;
} // namespace NEO
