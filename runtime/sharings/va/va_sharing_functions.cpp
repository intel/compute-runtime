/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "va_sharing_functions.h"

#include "core/debug_settings/debug_settings_manager.h"
#include "runtime/sharings/va/va_surface.h"

#include <dlfcn.h>

namespace Os {
extern const char *libvaDllName;
}

namespace NEO {
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
            vaQueryImageFormatsPFN = reinterpret_cast<VAQueryImageFormatsPFN>(fdlsym(libHandle, "vaQueryImageFormats"));
            vaMaxNumImageFormatsPFN = reinterpret_cast<VAMaxNumImageFormatsPFN>(fdlsym(libHandle, "vaMaxNumImageFormats"));

        } else {
            vaDisplayIsValidPFN = nullptr;
            vaDeriveImagePFN = nullptr;
            vaDestroyImagePFN = nullptr;
            vaSyncSurfacePFN = nullptr;
            vaGetLibFuncPFN = nullptr;
            vaExtGetSurfaceHandlePFN = nullptr;
            vaQueryImageFormatsPFN = nullptr;
            vaMaxNumImageFormatsPFN = nullptr;
        }
    }
}

void VASharingFunctions::querySupportedVaImageFormats(VADisplay vaDisplay) {
    UNRECOVERABLE_IF(supportedFormats.size() != 0);
    int maxFormats = this->maxNumImageFormats(vaDisplay);
    if (maxFormats > 0) {
        std::unique_ptr<VAImageFormat[]> allVaFormats(new VAImageFormat[maxFormats]);
        this->queryImageFormats(vaDisplay, allVaFormats.get(), &maxFormats);

        for (int i = 0; i < maxFormats; i++) {
            if (VASurface::isSupportedFourCC(allVaFormats[i].fourcc)) {
                supportedFormats.emplace_back(allVaFormats[i]);
            }
        }
    }
}

cl_int VASharingFunctions::getSupportedFormats(cl_mem_flags flags,
                                               cl_mem_object_type imageType,
                                               cl_uint numEntries,
                                               VAImageFormat *formats,
                                               cl_uint *numImageFormats) {
    if (flags != CL_MEM_READ_ONLY && flags != CL_MEM_WRITE_ONLY && flags != CL_MEM_READ_WRITE) {
        return CL_INVALID_VALUE;
    }

    if (imageType != CL_MEM_OBJECT_IMAGE2D) {
        return CL_INVALID_VALUE;
    }

    if (numImageFormats != nullptr) {
        *numImageFormats = static_cast<cl_uint>(supportedFormats.size());
    }

    if (formats != nullptr && supportedFormats.size() > 0) {
        uint32_t elementsToCopy = std::min(numEntries, static_cast<uint32_t>(supportedFormats.size()));
        memcpy_s(formats, elementsToCopy * sizeof(VAImageFormat), &supportedFormats[0], elementsToCopy * sizeof(VAImageFormat));
    }

    return CL_SUCCESS;
}
} // namespace NEO
