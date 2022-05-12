/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "va_sharing_functions.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/sharings/va/va_surface.h"

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
    bool enableVaLibCalls = true;
    if (DebugManager.flags.EnableVaLibCalls.get() != -1) {
        enableVaLibCalls = !!DebugManager.flags.EnableVaLibCalls.get();
    }

    if (enableVaLibCalls) {
        libHandle = fdlopen(Os::libvaDllName, RTLD_LAZY);
        if (libHandle) {
            vaDisplayIsValidPFN = reinterpret_cast<VADisplayIsValidPFN>(fdlsym(libHandle, "vaDisplayIsValid"));
            vaDeriveImagePFN = reinterpret_cast<VADeriveImagePFN>(fdlsym(libHandle, "vaDeriveImage"));
            vaDestroyImagePFN = reinterpret_cast<VADestroyImagePFN>(fdlsym(libHandle, "vaDestroyImage"));
            vaSyncSurfacePFN = reinterpret_cast<VASyncSurfacePFN>(fdlsym(libHandle, "vaSyncSurface"));
            vaGetLibFuncPFN = reinterpret_cast<VAGetLibFuncPFN>(fdlsym(libHandle, "vaGetLibFunc"));
            vaExtGetSurfaceHandlePFN = reinterpret_cast<VAExtGetSurfaceHandlePFN>(getLibFunc("DdiMedia_ExtGetSurfaceHandle"));
            vaExportSurfaceHandlePFN = reinterpret_cast<VAExportSurfaceHandlePFN>(fdlsym(libHandle, "vaExportSurfaceHandle"));
            vaQueryImageFormatsPFN = reinterpret_cast<VAQueryImageFormatsPFN>(fdlsym(libHandle, "vaQueryImageFormats"));
            vaMaxNumImageFormatsPFN = reinterpret_cast<VAMaxNumImageFormatsPFN>(fdlsym(libHandle, "vaMaxNumImageFormats"));

        } else {
            vaDisplayIsValidPFN = nullptr;
            vaDeriveImagePFN = nullptr;
            vaDestroyImagePFN = nullptr;
            vaSyncSurfacePFN = nullptr;
            vaGetLibFuncPFN = nullptr;
            vaExtGetSurfaceHandlePFN = nullptr;
            vaExportSurfaceHandlePFN = nullptr;
            vaQueryImageFormatsPFN = nullptr;
            vaMaxNumImageFormatsPFN = nullptr;
        }
    }
}

void VASharingFunctions::querySupportedVaImageFormats(VADisplay vaDisplay) {
    int maxFormats = this->maxNumImageFormats(vaDisplay);
    if (maxFormats > 0) {
        std::unique_ptr<VAImageFormat[]> allVaFormats(new VAImageFormat[maxFormats]);
        auto result = this->queryImageFormats(vaDisplay, allVaFormats.get(), &maxFormats);
        if (result == VA_STATUS_SUCCESS) {
            for (int i = 0; i < maxFormats; i++) {
                if (VASurface::isSupportedFourCCTwoPlaneFormat(allVaFormats[i].fourcc)) {
                    supported2PlaneFormats.emplace_back(allVaFormats[i]);
                } else if (VASurface::isSupportedFourCCThreePlaneFormat(allVaFormats[i].fourcc)) {
                    supported3PlaneFormats.emplace_back(allVaFormats[i]);
                }
            }
        }
    }
}

cl_int VASharingFunctions::getSupportedFormats(cl_mem_flags flags,
                                               cl_mem_object_type imageType,
                                               cl_uint plane,
                                               cl_uint numEntries,
                                               VAImageFormat *formats,
                                               cl_uint *numImageFormats) {
    if (flags != CL_MEM_READ_ONLY && flags != CL_MEM_WRITE_ONLY && flags != CL_MEM_READ_WRITE &&
        flags != CL_MEM_KERNEL_READ_AND_WRITE) {
        return CL_INVALID_VALUE;
    }

    if (imageType != CL_MEM_OBJECT_IMAGE2D) {
        return CL_SUCCESS;
    }

    if (numImageFormats != nullptr) {
        if (plane == 2) {
            *numImageFormats = static_cast<cl_uint>(supported3PlaneFormats.size());
        } else if (plane < 2) {
            *numImageFormats = static_cast<cl_uint>(supported2PlaneFormats.size() + supported3PlaneFormats.size());
        }
    }

    if (plane == 2) {
        if (formats != nullptr && supported3PlaneFormats.size() > 0) {
            uint32_t elementsToCopy = std::min(numEntries, static_cast<uint32_t>(supported3PlaneFormats.size()));
            memcpy_s(formats, elementsToCopy * sizeof(VAImageFormat), &supported3PlaneFormats[0], elementsToCopy * sizeof(VAImageFormat));
        }
    } else if (plane < 2) {
        if (formats != nullptr && (supported2PlaneFormats.size() > 0 || supported3PlaneFormats.size() > 0)) {
            uint32_t elementsToCopy = std::min(numEntries, static_cast<uint32_t>(supported2PlaneFormats.size() + supported3PlaneFormats.size()));
            std::vector<VAImageFormat> tmpFormats;
            tmpFormats.insert(tmpFormats.end(), supported2PlaneFormats.begin(), supported2PlaneFormats.end());
            tmpFormats.insert(tmpFormats.end(), supported3PlaneFormats.begin(), supported3PlaneFormats.end());
            memcpy_s(formats, elementsToCopy * sizeof(VAImageFormat), &tmpFormats[0], elementsToCopy * sizeof(VAImageFormat));
        }
    }

    return CL_SUCCESS;
}
} // namespace NEO
