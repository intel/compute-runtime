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

#include "runtime/os_interface/os_library.h"

#include <string>

struct DebuggerLibraryInterceptor {
    GfxDbgNewDeviceData newDeviceArgIn;
    GfxDbgSourceCode sourceCodeArgIn;
    GfxDbgOption optionArgIn;
    GfxDbgKernelDebugData kernelDebugDataArgIn;
    GfxDbgTargetCaps targetCapsArgIn;

    GfxDbgNewDeviceData *newDeviceArgOut = nullptr;
    GfxDbgSourceCode *sourceCodeArgOut = nullptr;
    GfxDbgOption *optionArgOut = nullptr;
    GfxDbgKernelDebugData *kernelDebugDataArgOut = nullptr;
    GfxDbgTargetCaps *targetCapsArgOut = nullptr;

    bool newDeviceCalled = false;
    bool sourceCodeCalled = false;
    bool optionCalled = false;
    bool kernelDebugDataCalled = false;
    bool initCalled = false;

    int newDeviceRetVal = 0;
    int sourceCodeRetVal = 0;
    int optionRetVal = 0;
    int kernelDebugDataRetVal = 0;
    int initRetVal = 0;
};

class DebuggerLibrary : public OCLRT::OsLibrary {
  public:
    DebuggerLibrary() = default;
    void *getProcAddress(const std::string &procName) override;

    static OsLibrary *load(const std::string &name);

    bool isLoaded() override {
        return true;
    }

    static void setDebuggerActive(bool active) {
        debuggerActive = active;
    }

    static bool getDebuggerActive() {
        return debuggerActive;
    }

    static void setLibraryAvailable(bool available) {
        isLibraryAvailable = available;
    }

    static bool getLibraryAvailable() {
        return isLibraryAvailable;
    }

    static void injectDebuggerLibraryInterceptor(DebuggerLibraryInterceptor *interceptorArg) {
        interceptor = interceptorArg;
    }

    static void clearDebuggerLibraryInterceptor() {
        interceptor = nullptr;
    }

    static DebuggerLibraryInterceptor *interceptor;

  protected:
    static int notifyNewDevice(GfxDbgNewDeviceData *);
    static int notifySourceCode(GfxDbgSourceCode *);
    static int getDebuggerOption(GfxDbgOption *);
    static int notifyKernelDebugData(GfxDbgKernelDebugData *);
    static int init(GfxDbgTargetCaps *);
    static int isDebuggerActive(void);

    static bool isLibraryAvailable;
    static bool debuggerActive;
};
