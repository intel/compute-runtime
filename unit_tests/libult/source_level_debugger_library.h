/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/os_library.h"

#include "igfx_debug_interchange_types.h"

#include <string>

#define IGFXDBG_CURRENT_VERSION 4

struct DebuggerLibraryInterceptor {
    GfxDbgNewDeviceData newDeviceArgIn;
    GfxDbgSourceCode sourceCodeArgIn;
    GfxDbgOption optionArgIn;
    GfxDbgKernelDebugData kernelDebugDataArgIn;
    GfxDbgTargetCaps targetCapsArgIn;
    GfxDbgDeviceDestructionData deviceDestructionArgIn;

    GfxDbgNewDeviceData *newDeviceArgOut = nullptr;
    GfxDbgSourceCode *sourceCodeArgOut = nullptr;
    GfxDbgOption *optionArgOut = nullptr;
    GfxDbgKernelDebugData *kernelDebugDataArgOut = nullptr;
    GfxDbgTargetCaps *targetCapsArgOut = nullptr;
    GfxDbgDeviceDestructionData *deviceDestructionArgOut = nullptr;

    bool newDeviceCalled = false;
    bool sourceCodeCalled = false;
    bool optionCalled = false;
    bool kernelDebugDataCalled = false;
    bool initCalled = false;
    bool deviceDestructionCalled = false;

    int newDeviceRetVal = 0;
    int sourceCodeRetVal = 0;
    int optionRetVal = 0;
    int kernelDebugDataRetVal = 0;
    int initRetVal = 0;
    int deviceDestructionRetVal = 0;
};

class DebuggerLibrary : public NEO::OsLibrary {
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
    static int notifyDeviceDestruction(GfxDbgDeviceDestructionData *);

    static bool isLibraryAvailable;
    static bool debuggerActive;
};
