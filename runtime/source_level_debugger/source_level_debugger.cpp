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
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "runtime/os_interface/os_interface.h"

namespace OCLRT {
const char *SourceLevelDebugger::notifyNewDeviceSymbol = "notifyNewDevice";
const char *SourceLevelDebugger::notifySourceCodeSymbol = "notifySourceCode";
const char *SourceLevelDebugger::getDebuggerOptionSymbol = "getDebuggerOption";
const char *SourceLevelDebugger::notifyKernelDebugDataSymbol = "notifyKernelDebugData";
const char *SourceLevelDebugger::initSymbol = "init";

class SourceLevelDebugger::SourceLevelDebuggerInterface {
  public:
    SourceLevelDebuggerInterface() = default;
    ~SourceLevelDebuggerInterface() = default;

    typedef int (*pfNotifyNewDevice)(GfxDbgNewDeviceData *data);
    typedef int (*pfNotifySourceCode)(GfxDbgSourceCode *data);
    typedef int (*pfGetDebuggerOption)(GfxDbgOption *);
    typedef int (*pfNotifyKernelDebugData)(GfxDbgKernelDebugData *data);
    typedef int (*pfInit)(GfxDbgTargetCaps *data);

    pfNotifyNewDevice fNotifyNewDevice = nullptr;
    pfNotifySourceCode fNotifySourceCode = nullptr;
    pfGetDebuggerOption fGetDebuggerOption = nullptr;
    pfNotifyKernelDebugData fNotifyKernelDebugData = nullptr;
    pfInit fInit = nullptr;
};

SourceLevelDebugger::SourceLevelDebugger() {
    debuggerLibrary.reset(SourceLevelDebugger::loadDebugger());

    if (debuggerLibrary.get() == nullptr) {
        return;
    }
    interface = new SourceLevelDebuggerInterface;
    getFunctions();
    isActive = true;
}

SourceLevelDebugger::~SourceLevelDebugger() {
    if (interface) {
        delete interface;
    }
}

bool SourceLevelDebugger::isDebuggerActive() {
    return isActive;
}

void SourceLevelDebugger::getFunctions() {
    UNRECOVERABLE_IF(debuggerLibrary.get() == nullptr);

    interface->fNotifyNewDevice = reinterpret_cast<SourceLevelDebuggerInterface::pfNotifyNewDevice>(debuggerLibrary->getProcAddress(notifyNewDeviceSymbol));
    interface->fNotifySourceCode = reinterpret_cast<SourceLevelDebuggerInterface::pfNotifySourceCode>(debuggerLibrary->getProcAddress(notifySourceCodeSymbol));
    interface->fGetDebuggerOption = reinterpret_cast<SourceLevelDebuggerInterface::pfGetDebuggerOption>(debuggerLibrary->getProcAddress(getDebuggerOptionSymbol));
    interface->fNotifyKernelDebugData = reinterpret_cast<SourceLevelDebuggerInterface::pfNotifyKernelDebugData>(debuggerLibrary->getProcAddress(notifyKernelDebugDataSymbol));
    interface->fInit = reinterpret_cast<SourceLevelDebuggerInterface::pfInit>(debuggerLibrary->getProcAddress(initSymbol));
}

void SourceLevelDebugger::notifyNewDevice(uint32_t deviceHandle) const {
    if (isActive) {
        GfxDbgNewDeviceData newDevice;
        newDevice.version = IGFXDBG_CURRENT_VERSION;
        newDevice.dh = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(deviceHandle));
        newDevice.udh = GfxDeviceHandle(0);
        interface->fNotifyNewDevice(&newDevice);
    }
}
void SourceLevelDebugger::notifySourceCode(uint32_t deviceHandle, const char *source, size_t sourceSize) const {
    if (isActive) {
        GfxDbgSourceCode sourceCode;
        char fileName[FILENAME_MAX] = "";

        sourceCode.version = IGFXDBG_CURRENT_VERSION;
        sourceCode.hDevice = reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(deviceHandle));
        sourceCode.sourceCode = source;
        sourceCode.sourceCodeSize = static_cast<unsigned int>(sourceSize);
        sourceCode.sourceName = &fileName[0];
        sourceCode.sourceNameMaxLen = sizeof(fileName);

        interface->fNotifySourceCode(&sourceCode);
    }
}

bool SourceLevelDebugger::isOptimizationDisabled() const {
    if (isActive) {
        char value;
        GfxDbgOption option;
        option.version = IGFXDBG_CURRENT_VERSION;
        option.optionName = DBG_OPTION_IS_OPTIMIZATION_DISABLED;
        option.valueLen = sizeof(value);
        option.value = &value;

        int result = interface->fGetDebuggerOption(&option);
        if (result == 1) {
            if (option.value[0] == '1') {
                return true;
            }
        }
    }
    return false;
}

void SourceLevelDebugger::notifyKernelDebugData() const {
    GfxDbgKernelDebugData kernelDebugData;
    interface->fNotifyKernelDebugData(&kernelDebugData);
}
} // namespace OCLRT
