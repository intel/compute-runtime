/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/sharings/sharing.h"
#include "opencl/source/sharings/va/va_sharing_defines.h"

#include <functional>
#include <mutex>
#include <vector>

namespace NEO {

class VASharingFunctions : public SharingFunctions {
  public:
    VASharingFunctions(VADisplay vaDisplay);
    ~VASharingFunctions() override;

    uint32_t getId() const override {
        return VASharingFunctions::sharingId;
    }
    static const uint32_t sharingId;

    MOCKABLE_VIRTUAL bool isValidVaDisplay() {
        return vaDisplayIsValidPFN(vaDisplay) == 1;
    }

    MOCKABLE_VIRTUAL VAStatus deriveImage(VASurfaceID vaSurface, VAImage *vaImage) {
        return vaDeriveImagePFN(vaDisplay, vaSurface, vaImage);
    }

    MOCKABLE_VIRTUAL VAStatus destroyImage(VAImageID vaImageId) {
        return vaDestroyImagePFN(vaDisplay, vaImageId);
    }

    MOCKABLE_VIRTUAL VAStatus extGetSurfaceHandle(VASurfaceID *vaSurface, unsigned int *handleId) {
        return vaExtGetSurfaceHandlePFN(vaDisplay, vaSurface, handleId);
    }

    MOCKABLE_VIRTUAL VAStatus exportSurfaceHandle(VASurfaceID vaSurface, uint32_t memType, uint32_t flags, void *descriptor) {
        return vaExportSurfaceHandlePFN(vaDisplay, vaSurface, memType, flags, descriptor);
    }

    MOCKABLE_VIRTUAL VAStatus syncSurface(VASurfaceID vaSurface) {
        return vaSyncSurfacePFN(vaDisplay, vaSurface);
    }

    MOCKABLE_VIRTUAL VAStatus queryImageFormats(VADisplay vaDisplay, VAImageFormat *formatList, int *numFormats) {
        return vaQueryImageFormatsPFN(vaDisplay, formatList, numFormats);
    }

    MOCKABLE_VIRTUAL int maxNumImageFormats(VADisplay vaDisplay) {
        if (vaMaxNumImageFormatsPFN) {
            return vaMaxNumImageFormatsPFN(vaDisplay);
        }
        return 0;
    }

    void *getLibFunc(const char *func) {
        if (vaGetLibFuncPFN) {
            return vaGetLibFuncPFN(vaDisplay, func);
        }
        return nullptr;
    }

    void initFunctions();
    void querySupportedVaImageFormats(VADisplay vaDisplay);

    cl_int getSupportedFormats(cl_mem_flags flags,
                               cl_mem_object_type imageType,
                               cl_uint plane,
                               cl_uint numEntries,
                               VAImageFormat *formats,
                               cl_uint *numImageFormats);

    static std::function<void *(const char *, int)> fdlopen;
    static std::function<void *(void *handle, const char *symbol)> fdlsym;
    static std::function<int(void *handle)> fdlclose;

    static bool isVaLibraryAvailable();

    std::mutex mutex;

  protected:
    void *libHandle = nullptr;
    VADisplay vaDisplay = nullptr;
    VADisplayIsValidPFN vaDisplayIsValidPFN = [](VADisplay vaDisplay) { return 0; };
    VADeriveImagePFN vaDeriveImagePFN;
    VADestroyImagePFN vaDestroyImagePFN;
    VASyncSurfacePFN vaSyncSurfacePFN;
    VAExtGetSurfaceHandlePFN vaExtGetSurfaceHandlePFN;
    VAExportSurfaceHandlePFN vaExportSurfaceHandlePFN;
    VAGetLibFuncPFN vaGetLibFuncPFN;
    VAQueryImageFormatsPFN vaQueryImageFormatsPFN;
    VAMaxNumImageFormatsPFN vaMaxNumImageFormatsPFN;

    std::vector<VAImageFormat> supported2PlaneFormats;
    std::vector<VAImageFormat> supported3PlaneFormats;
};
} // namespace NEO
