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
        return interceptor->sourceCodeRetVal;
    }
    return IgfxdbgRetVal::IGFXDBG_SUCCESS;
}

int DebuggerLibrary::getDebuggerOption(GfxDbgOption *option) {
    if (interceptor) {
        interceptor->optionArgIn = *option;
        interceptor->optionCalled = true;

        if (interceptor->optionArgOut && option->valueLen > 0) {
            memcpy_s(option->value, option->valueLen, interceptor->optionArgOut->value, interceptor->optionArgOut->valueLen);
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
