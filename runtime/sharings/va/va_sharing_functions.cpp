/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "va_sharing_functions.h"

#include "runtime/os_interface/debug_settings_manager.h"

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
} // namespace OCLRT
