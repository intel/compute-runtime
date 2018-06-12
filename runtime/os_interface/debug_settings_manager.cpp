/*
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "debug_settings_manager.h"
#include "runtime/event/event.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/string.h"
#include "runtime/utilities/debug_settings_reader.h"

#include "CL/cl.h"

#include <cstdio>
#include <sstream>

namespace OCLRT {

DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager;

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::DebugSettingsManager() {

    logFileName = "igdrcl.log";
    if (registryReadAvailable()) {
        readerImpl = SettingsReader::create();

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)            \
    {                                                                                        \
        dataType tempData = readerImpl->getSetting(#variableName, flags.variableName.get()); \
        flags.variableName.set(tempData);                                                    \
    }
#include "DebugVariables.inl"
    }

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
DebugSettingsManager<DebugLevel>::~DebugSettingsManager() {
    if (readerImpl) {
        delete readerImpl;
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
            } else if (argInfo.typeStr.find("*") != std::string::npos) {
                type = "buffer";
                auto clMem = (const cl_mem)kernel->getKernelArg(i);
                auto memObj = castToObject<MemObj>(clMem);
                if (memObj != nullptr) {
                    ptr = static_cast<char *>(memObj->getCpuAddress());
                    size = memObj->getSize();
                    flags = memObj->getFlags();
                }
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

    if ((flags.DumpKernelArgs.get() == false) || (multiDispatchInfo == nullptr)) {
        return;
    }

    for (auto &dispatchInfo : *multiDispatchInfo) {
        dumpKernelArgs(dispatchInfo.getKernel());
    }
}

template <DebugFunctionalityLevel DebugLevel>
bool DebugSettingsManager<DebugLevel>::readSetting(const char *settingName, bool defaultValue) {
    if (readerImpl) {
        readerImpl->getSetting(settingName, defaultValue);
    }
    return defaultValue;
}

template class DebugSettingsManager<DebugFunctionalityLevel::None>;
template class DebugSettingsManager<DebugFunctionalityLevel::Full>;
template class DebugSettingsManager<DebugFunctionalityLevel::RegKeys>;
}; // namespace OCLRT
