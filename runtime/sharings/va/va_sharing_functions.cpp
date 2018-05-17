/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "runtime/os_interface/debug_settings_manager.h"
#include "va_sharing_functions.h"
#include <dlfcn.h>

namespace Os {
extern const char *libvaDllName;
}

namespace OCLRT {
std::function<void *(const char *, int)> VASharingFunctions::fdlopen = dlopen;
std::function<void *(void *handle, const char *symbol)> VASharingFunctions::fdlsym = dlsym;
std::function<int(void *handle)> VASharingFunctions::fdlclose = dlclose;

VASharingFunctions::VASharingFunctions(VADisplay vaDisplay) : vaDisplay(vaDisplay) {
    initFunctions();
};

VASharingFunctions::~VASharingFunctions() {
    if (libHandle != nullptr) {
        fdlclose(libHandle);
        libHandle = nullptr;
    }
}

bool VASharingFunctions::isVaLibraryAvailable() {
    auto lib = fdlopen(Os::libvaDllName, RTLD_LAZY);
    if (lib) {
        fdlclose(lib);
        return true;
    }
    return false;
}

void VASharingFunctions::initFunctions() {
    if (DebugManager.flags.EnableVaLibCalls.get()) {
        libHandle = fdlopen(Os::libvaDllName, RTLD_LAZY);
        if (libHandle) {
            vaDisplayIsValidPFN = reinterpret_cast<VADisplayIsValidPFN>(fdlsym(libHandle, "vaDisplayIsValid"));
            vaDeriveImagePFN = reinterpret_cast<VADeriveImagePFN>(fdlsym(libHandle, "vaDeriveImage"));
            vaDestroyImagePFN = reinterpret_cast<VADestroyImagePFN>(fdlsym(libHandle, "vaDestroyImage"));
            vaSyncSurfacePFN = reinterpret_cast<VASyncSurfacePFN>(fdlsym(libHandle, "vaSyncSurface"));
            vaGetLibFuncPFN = reinterpret_cast<VAGetLibFuncPFN>(fdlsym(libHandle, "vaGetLibFunc"));
            vaExtGetSurfaceHandlePFN = reinterpret_cast<VAExtGetSurfaceHandlePFN>(getLibFunc("DdiMedia_ExtGetSurfaceHandle"));
        } else {
            vaDisplayIsValidPFN = nullptr;
            vaDeriveImagePFN = nullptr;
            vaDestroyImagePFN = nullptr;
            vaSyncSurfacePFN = nullptr;
            vaGetLibFuncPFN = nullptr;
            vaExtGetSurfaceHandlePFN = nullptr;
        }
    }
}
}
