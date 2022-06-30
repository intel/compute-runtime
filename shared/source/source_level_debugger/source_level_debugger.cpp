/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/os_interface.h"

#include "igfx_debug_interchange_types.h"
#include "lib_names.h"

#include <fstream>

#define IGFXDBG_CURRENT_VERSION 4

namespace NEO {

static_assert(std::is_pod<GfxDbgKernelDebugData>::value);

const char *SourceLevelDebugger::notifyNewDeviceSymbol = "notifyNewDevice";
const char *SourceLevelDebugger::notifySourceCodeSymbol = "notifySourceCode";
const char *SourceLevelDebugger::getDebuggerOptionSymbol = "getDebuggerOption";
const char *SourceLevelDebugger::notifyKernelDebugDataSymbol = "notifyKernelDebugData";
const char *SourceLevelDebugger::initSymbol = "init";
const char *SourceLevelDebugger::isDebuggerActiveSymbol = "isDebuggerActive";
const char *SourceLevelDebugger::notifyDeviceDestructionSymbol = "notifyDeviceDestruction";
const char *SourceLevelDebugger::dllName = SLD_LIBRARY_NAME;

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
    if (DebugManager.flags.EnableMockSourceLevelDebugger.get()) {
        auto sld = new SourceLevelDebugger(nullptr);
        sld->isActive = true;
        return sld;
    }
    return nullptr;
}
SourceLevelDebugger::SourceLevelDebugger(OsLibrary *library) {
    debuggerLibrary.reset(library);
    isLegacyMode = true;

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
        GfxDbgNewDeviceData newDevice = {};
        newDevice.version = IGFXDBG_CURRENT_VERSION;
        newDevice.dh = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(deviceHandle));
        newDevice.udh = GfxDeviceHandle(0);
        if (sourceLevelDebuggerInterface) {
            int result = sourceLevelDebuggerInterface->notifyNewDeviceFunc(&newDevice);
            DEBUG_BREAK_IF(static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS);
            static_cast<void>(result);
        }
        this->deviceHandle = deviceHandle;
    }
    return false;
}

bool SourceLevelDebugger::notifyDeviceDestruction() {
    if (isActive) {
        GfxDbgDeviceDestructionData deviceDestruction = {};
        deviceDestruction.version = IGFXDBG_CURRENT_VERSION;
        deviceDestruction.dh = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(this->deviceHandle));
        if (sourceLevelDebuggerInterface) {
            int result = sourceLevelDebuggerInterface->notifyDeviceDestructionFunc(&deviceDestruction);
            DEBUG_BREAK_IF(static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS);
            static_cast<void>(result);
        }
        this->deviceHandle = 0;
        return true;
    }
    return false;
}

bool SourceLevelDebugger::notifySourceCode(const char *source, size_t sourceSize, std::string &file) const {
    if (isActive) {
        GfxDbgSourceCode sourceCode = {};
        char fileName[FILENAME_MAX] = "";

        sourceCode.version = IGFXDBG_CURRENT_VERSION;
        sourceCode.hDevice = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(this->deviceHandle));
        sourceCode.sourceCode = source;
        sourceCode.sourceCodeSize = static_cast<unsigned int>(sourceSize);
        sourceCode.sourceName = &fileName[0];
        sourceCode.sourceNameMaxLen = sizeof(fileName);

        if (sourceLevelDebuggerInterface) {
            int result = sourceLevelDebuggerInterface->notifySourceCodeFunc(&sourceCode);
            DEBUG_BREAK_IF(static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS);
            static_cast<void>(result);
        }
        file = fileName;
    }
    return false;
}

bool SourceLevelDebugger::isOptimizationDisabled() const {
    if (isActive) {
        const size_t optionValueSize = 4;
        char value[optionValueSize] = {0};
        GfxDbgOption option = {};
        option.version = IGFXDBG_CURRENT_VERSION;
        option.optionName = DBG_OPTION_IS_OPTIMIZATION_DISABLED;
        option.valueLen = sizeof(value);
        option.value = &value[0];

        if (sourceLevelDebuggerInterface) {
            int result = sourceLevelDebuggerInterface->getDebuggerOptionFunc(&option);
            if (result == 1) {
                if (option.value[0] == '1') {
                    return true;
                }
            }
        } else {
            return DebugManager.flags.EnableMockSourceLevelDebugger.get() == 1 ? true : false;
        }
    }
    return false;
}

bool SourceLevelDebugger::notifyKernelDebugData(const DebugData *debugData, const std::string &name, const void *isa, size_t isaSize) const {
    if (isActive) {
        GfxDbgKernelDebugData kernelDebugData = {};
        kernelDebugData.hDevice = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(this->deviceHandle));
        kernelDebugData.version = IGFXDBG_CURRENT_VERSION;
        kernelDebugData.hProgram = reinterpret_cast<GenRtProgramHandle>(0);

        kernelDebugData.kernelName = name.c_str();
        kernelDebugData.kernelBinBuffer = const_cast<void *>(isa);
        kernelDebugData.KernelBinSize = static_cast<uint32_t>(isaSize);

        if (debugData) {
            kernelDebugData.dbgVisaBuffer = debugData->vIsa;
            kernelDebugData.dbgVisaSize = debugData->vIsaSize;
            kernelDebugData.dbgGenIsaBuffer = debugData->genIsa;
            kernelDebugData.dbgGenIsaSize = debugData->genIsaSize;

            if (NEO::DebugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::DUMP_ELF) {
                std::ofstream elfFile;
                std::string fileName = name + ".elf";

                int suffix = 0;
                while (fileExists(fileName)) {
                    fileName = name + "_" + std::to_string(suffix) + ".elf";
                    suffix++;
                }
                writeDataToFile(fileName.c_str(), kernelDebugData.dbgVisaBuffer, kernelDebugData.dbgVisaSize);
            }
        }

        if (sourceLevelDebuggerInterface) {
            int result = sourceLevelDebuggerInterface->notifyKernelDebugDataFunc(&kernelDebugData);
            DEBUG_BREAK_IF(static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS);
            static_cast<void>(result);
        }
    }
    return false;
}

bool SourceLevelDebugger::initialize(bool useLocalMemory) {
    if (isActive) {
        GfxDbgTargetCaps caps = {IGFXDBG_CURRENT_VERSION, useLocalMemory};
        if (sourceLevelDebuggerInterface) {
            int result = sourceLevelDebuggerInterface->initFunc(&caps);
            if (static_cast<IgfxdbgRetVal>(result) != IgfxdbgRetVal::IGFXDBG_SUCCESS) {
                isActive = false;
            }
        }
    }
    return false;
}
} // namespace NEO
