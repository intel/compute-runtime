/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "source_level_debugger_library.h"

#include "runtime/helpers/string.h"

using namespace OCLRT;

bool DebuggerLibrary::debuggerActive = false;
bool DebuggerLibrary::isLibraryAvailable = false;
DebuggerLibraryInterceptor *DebuggerLibrary::interceptor = nullptr;

void *DebuggerLibrary::getProcAddress(const std::string &procName) {
    if (procName == "notifyNewDevice") {
        return reinterpret_cast<void *>(notifyNewDevice);
    } else if (procName == "notifySourceCode") {
        return reinterpret_cast<void *>(notifySourceCode);
    } else if (procName == "getDebuggerOption") {
        return reinterpret_cast<void *>(getDebuggerOption);
    } else if (procName == "notifyKernelDebugData") {
        return reinterpret_cast<void *>(notifyKernelDebugData);
    } else if (procName == "init") {
        return reinterpret_cast<void *>(init);
    } else if (procName == "isDebuggerActive") {
        return reinterpret_cast<void *>(isDebuggerActive);
    } else if (procName == "notifyDeviceDestruction") {
        return reinterpret_cast<void *>(notifyDeviceDestruction);
    }
    return nullptr;
}

OsLibrary *DebuggerLibrary::load(const std::string &name) {
    if (isLibraryAvailable) {
        return new DebuggerLibrary();
    }
    return nullptr;
}

int DebuggerLibrary::notifyNewDevice(GfxDbgNewDeviceData *newDevice) {
    if (interceptor) {
        interceptor->newDeviceArgIn = *newDevice;
        interceptor->newDeviceCalled = true;
        return interceptor->newDeviceRetVal;
    }
    return IgfxdbgRetVal::IGFXDBG_SUCCESS;
}

int DebuggerLibrary::notifySourceCode(GfxDbgSourceCode *sourceCode) {
    if (interceptor) {
        interceptor->sourceCodeArgIn = *sourceCode;
        interceptor->sourceCodeCalled = true;
        if (interceptor->sourceCodeArgOut && sourceCode->sourceNameMaxLen > 0) {
            memcpy_s(sourceCode->sourceName, sourceCode->sourceNameMaxLen, interceptor->sourceCodeArgOut->sourceName, interceptor->sourceCodeArgOut->sourceNameMaxLen);
        }
        return interceptor->sourceCodeRetVal;
    }
    return IgfxdbgRetVal::IGFXDBG_SUCCESS;
}

int DebuggerLibrary::getDebuggerOption(GfxDbgOption *option) {
    if (interceptor) {
        interceptor->optionArgIn = *option;
        interceptor->optionCalled = true;

        if (interceptor->optionArgOut && option->valueLen >= interceptor->optionArgOut->valueLen) {
            memcpy_s(option->value, option->valueLen, interceptor->optionArgOut->value, interceptor->optionArgOut->valueLen);
        } else {
            memset(option->value, 0, option->valueLen);
        }
        return interceptor->optionRetVal;
    }
    return IgfxdbgRetVal::IGFXDBG_SUCCESS;
}

int DebuggerLibrary::notifyKernelDebugData(GfxDbgKernelDebugData *kernelDebugData) {
    if (interceptor) {
        interceptor->kernelDebugDataArgIn = *kernelDebugData;
        interceptor->kernelDebugDataCalled = true;
        return interceptor->kernelDebugDataRetVal;
    }
    return IgfxdbgRetVal::IGFXDBG_SUCCESS;
}

int DebuggerLibrary::init(GfxDbgTargetCaps *targetCaps) {
    if (interceptor) {
        interceptor->targetCapsArgIn = *targetCaps;
        interceptor->initCalled = true;
        return interceptor->initRetVal;
    }
    return IgfxdbgRetVal::IGFXDBG_SUCCESS;
}

int DebuggerLibrary::isDebuggerActive(void) {
    return debuggerActive ? 1 : 0;
}

int DebuggerLibrary::notifyDeviceDestruction(GfxDbgDeviceDestructionData *deviceDestruction) {
    if (interceptor) {
        interceptor->deviceDestructionArgIn = *deviceDestruction;
        interceptor->deviceDestructionCalled = true;
        return interceptor->deviceDestructionRetVal;
    }
    return IgfxdbgRetVal::IGFXDBG_SUCCESS;
}