/*
 * Copyright (c) 2018, Intel Corporation
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

#include "igfx_debug_interchange_types.h"

#include "runtime/helpers/debug_helpers.h"
#include "runtime/program/kernel_info.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "runtime/os_interface/os_interface.h"

namespace OCLRT {
const char *SourceLevelDebugger::notifyNewDeviceSymbol = "notifyNewDevice";
const char *SourceLevelDebugger::notifySourceCodeSymbol = "notifySourceCode";
const char *SourceLevelDebugger::getDebuggerOptionSymbol = "getDebuggerOption";
const char *SourceLevelDebugger::notifyKernelDebugDataSymbol = "notifyKernelDebugData";
const char *SourceLevelDebugger::initSymbol = "init";
const char *SourceLevelDebugger::isDebuggerActiveSymbol = "isDebuggerActive";
const char *SourceLevelDebugger::notifyDeviceDestructionSymbol = "notifyDeviceDestruction";

class SourceLevelDebugger::SourceLevelDebuggerInterface {
  public:
    SourceLevelDebuggerInterface() = default;
    ~SourceLevelDebuggerInterface() = default;

    typedef int (*NotifyNewDeviceFunction)(GfxDbgNewDeviceData *data);
    typedef int (*NotifySourceCodeFunction)(GfxDbgSourceCode *data);
    typedef int (*GetDebuggerOptionFunction)(GfxDbgOption *);
    typedef int (*NotifyKernelDebugDataFunction)(GfxDbgKernelDebugData *data);
    typedef int (*InitFunction)(GfxDbgTargetCaps *data);
    typedef int (*IsDebuggerActiveFunction)(void);
    typedef int (*NotifyDeviceDestructionFunction)(GfxDbgDeviceDestructionData *data);

    NotifyNewDeviceFunction notifyNewDeviceFunc = nullptr;
    NotifySourceCodeFunction notifySourceCodeFunc = nullptr;
    GetDebuggerOptionFunction getDebuggerOptionFunc = nullptr;
    NotifyKernelDebugDataFunction notifyKernelDebugDataFunc = nullptr;
    InitFunction initFunc = nullptr;
    IsDebuggerActiveFunction isDebuggerActive = nullptr;
    NotifyDeviceDestructionFunction notifyDeviceDestructionFunc = nullptr;
};

SourceLevelDebugger *SourceLevelDebugger::create() {
    auto library = SourceLevelDebugger::loadDebugger();
    if (library) {
        auto isActiveFunc = reinterpret_cast<SourceLevelDebuggerInterface::IsDebuggerActiveFunction>(library->getProcAddress(isDebuggerActiveSymbol));
        int result = isActiveFunc();
        if (result == 1) {
            // pass library ownership to Source Level Debugger
            return new SourceLevelDebugger(library);
        }
        delete library;
    }
    return nullptr;
}
SourceLevelDebugger::SourceLevelDebugger(OsLibrary *library) {
    debuggerLibrary.reset(library);

    if (debuggerLibrary.get() == nullptr) {
        return;
    }
    sourceLevelDebuggerInterface = new SourceLevelDebuggerInterface;
    getFunctions();

    if (sourceLevelDebuggerInterface->isDebuggerActive == nullptr) {
        return;
    }

    int result = sourceLevelDebuggerInterface->isDebuggerActive();
    if (result == 1) {
        UNRECOVERABLE_IF(sourceLevelDebuggerInterface->getDebuggerOptionFunc == nullptr);
        UNRECOVERABLE_IF(sourceLevelDebuggerInterface->initFunc == nullptr);
        UNRECOVERABLE_IF(sourceLevelDebuggerInterface->notifyKernelDebugDataFunc == nullptr);
        UNRECOVERABLE_IF(sourceLevelDebuggerInterface->notifyNewDeviceFunc == nullptr);
        UNRECOVERABLE_IF(sourceLevelDebuggerInterface->notifySourceCodeFunc == nullptr);
        UNRECOVERABLE_IF(sourceLevelDebuggerInterface->notifyDeviceDestructionFunc == nullptr);
        isActive = true;
    }
}

SourceLevelDebugger::~SourceLevelDebugger() {
    if (sourceLevelDebuggerInterface) {
        delete sourceLevelDebuggerInterface;
    }
}

bool SourceLevelDebugger::isDebuggerActive() {
    return isActive;
}

void SourceLevelDebugger::getFunctions() {
    UNRECOVERABLE_IF(debuggerLibrary.get() == nullptr);

    sourceLevelDebuggerInterface->notifyNewDeviceFunc = reinterpret_cast<SourceLevelDebuggerInterface::NotifyNewDeviceFunction>(debuggerLibrary->getProcAddress(notifyNewDeviceSymbol));
    sourceLevelDebuggerInterface->notifySourceCodeFunc = reinterpret_cast<SourceLevelDebuggerInterface::NotifySourceCodeFunction>(debuggerLibrary->getProcAddress(notifySourceCodeSymbol));
    sourceLevelDebuggerInterface->getDebuggerOptionFunc = reinterpret_cast<SourceLevelDebuggerInterface::GetDebuggerOptionFunction>(debuggerLibrary->getProcAddress(getDebuggerOptionSymbol));
    sourceLevelDebuggerInterface->notifyKernelDebugDataFunc = reinterpret_cast<SourceLevelDebuggerInterface::NotifyKernelDebugDataFunction>(debuggerLibrary->getProcAddress(notifyKernelDebugDataSymbol));
    sourceLevelDebuggerInterface->initFunc = reinterpret_cast<SourceLevelDebuggerInterface::InitFunction>(debuggerLibrary->getProcAddress(initSymbol));
    sourceLevelDebuggerInterface->isDebuggerActive = reinterpret_cast<SourceLevelDebuggerInterface::IsDebuggerActiveFunction>(debuggerLibrary->getProcAddress(isDebuggerActiveSymbol));
    sourceLevelDebuggerInterface->notifyDeviceDestructionFunc = reinterpret_cast<SourceLevelDebuggerInterface::NotifyDeviceDestructionFunction>(debuggerLibrary->getProcAddress(notifyDeviceDestructionSymbol));
}

bool SourceLevelDebugger::notifyNewDevice(uint32_t deviceHandle) {
    if (isActive) {
        GfxDbgNewDeviceData newDevice;
        newDevice.version = IGFXDBG_CURRENT_VERSION;
        newDevice.dh = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(deviceHandle));
        newDevice.udh = GfxDeviceHandle(0);
        int result = sourceLevelDebuggerInterface->notifyNewDeviceFunc(&newDevice);
        DEBUG_BREAK_IF(static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS);
        static_cast<void>(result);
        this->deviceHandle = deviceHandle;
    }
    return false;
}

bool SourceLevelDebugger::notifyDeviceDestruction() {
    if (isActive) {
        GfxDbgDeviceDestructionData deviceDestruction;
        deviceDestruction.version = IGFXDBG_CURRENT_VERSION;
        deviceDestruction.dh = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(this->deviceHandle));
        int result = sourceLevelDebuggerInterface->notifyDeviceDestructionFunc(&deviceDestruction);
        DEBUG_BREAK_IF(static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS);
        static_cast<void>(result);
        this->deviceHandle = 0;
        return true;
    }
    return false;
}

bool SourceLevelDebugger::notifySourceCode(const char *source, size_t sourceSize, std::string &file) const {
    if (isActive) {
        GfxDbgSourceCode sourceCode;
        char fileName[FILENAME_MAX] = "";

        sourceCode.version = IGFXDBG_CURRENT_VERSION;
        sourceCode.hDevice = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(this->deviceHandle));
        sourceCode.sourceCode = source;
        sourceCode.sourceCodeSize = static_cast<unsigned int>(sourceSize);
        sourceCode.sourceName = &fileName[0];
        sourceCode.sourceNameMaxLen = sizeof(fileName);

        int result = sourceLevelDebuggerInterface->notifySourceCodeFunc(&sourceCode);
        DEBUG_BREAK_IF(static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS);
        static_cast<void>(result);
        file = fileName;
    }
    return false;
}

bool SourceLevelDebugger::isOptimizationDisabled() const {
    if (isActive) {
        const size_t optionValueSize = 4;
        char value[optionValueSize] = {0};
        GfxDbgOption option;
        option.version = IGFXDBG_CURRENT_VERSION;
        option.optionName = DBG_OPTION_IS_OPTIMIZATION_DISABLED;
        option.valueLen = sizeof(value);
        option.value = &value[0];

        int result = sourceLevelDebuggerInterface->getDebuggerOptionFunc(&option);
        if (result == 1) {
            if (option.value[0] == '1') {
                return true;
            }
        }
    }
    return false;
}

bool SourceLevelDebugger::notifyKernelDebugData(const KernelInfo *kernelInfo) const {
    if (isActive) {
        GfxDbgKernelDebugData kernelDebugData;
        kernelDebugData.hDevice = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(this->deviceHandle));
        kernelDebugData.version = IGFXDBG_CURRENT_VERSION;
        kernelDebugData.hProgram = reinterpret_cast<GenRtProgramHandle>(0);

        kernelDebugData.kernelName = kernelInfo->name.c_str();
        kernelDebugData.kernelBinBuffer = const_cast<void *>(kernelInfo->heapInfo.pKernelHeap);
        kernelDebugData.KernelBinSize = kernelInfo->heapInfo.pKernelHeader->KernelHeapSize;

        kernelDebugData.dbgVisaBuffer = kernelInfo->debugData.vIsa;
        kernelDebugData.dbgVisaSize = kernelInfo->debugData.vIsaSize;
        kernelDebugData.dbgGenIsaBuffer = kernelInfo->debugData.genIsa;
        kernelDebugData.dbgGenIsaSize = kernelInfo->debugData.genIsaSize;

        int result = sourceLevelDebuggerInterface->notifyKernelDebugDataFunc(&kernelDebugData);
        DEBUG_BREAK_IF(static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS);
        static_cast<void>(result);
    }
    return false;
}

bool SourceLevelDebugger::initialize(bool useLocalMemory) {
    if (isActive) {
        GfxDbgTargetCaps caps = {IGFXDBG_CURRENT_VERSION, useLocalMemory};
        int result = sourceLevelDebuggerInterface->initFunc(&caps);
        if (static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS) {
            isActive = false;
        }
    }
    return false;
}
} // namespace OCLRT
