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

#pragma once
#include "runtime/sharings/sharing.h"
#include "runtime/sharings/va/va_sharing_defines.h"
#include <functional>

namespace OCLRT {
class VASharingFunctions : public SharingFunctions {
  public:
    VASharingFunctions(VADisplay vaDisplay);
    ~VASharingFunctions() override;

    uint32_t getId() const override {
        return VASharingFunctions::sharingId;
    }
    static const uint32_t sharingId;

    bool isValidVaDisplay() {
        return vaDisplayIsValidPFN(vaDisplay) == 1;
    }

    VAStatus deriveImage(VASurfaceID vaSurface, VAImage *vaImage) {
        return vaDeriveImagePFN(vaDisplay, vaSurface, vaImage);
    }

    VAStatus destroyImage(VAImageID vaImageId) {
        return vaDestroyImagePFN(vaDisplay, vaImageId);
    }

    VAStatus extGetSurfaceHandle(VASurfaceID *vaSurface, unsigned int *handleId) {
        return vaExtGetSurfaceHandlePFN(vaDisplay, vaSurface, handleId);
    }

    VAStatus syncSurface(VASurfaceID vaSurface) {
        return vaSyncSurfacePFN(vaDisplay, vaSurface);
    }

    void *getLibFunc(const char *func) {
        if (vaGetLibFuncPFN) {
            return vaGetLibFuncPFN(vaDisplay, func);
        }
        return nullptr;
    }

    void initFunctions();
    static std::function<void *(const char *, int)> fdlopen;
    static std::function<void *(void *handle, const char *symbol)> fdlsym;
    static std::function<int(void *handle)> fdlclose;

    static bool isVaLibraryAvailable();

  protected:
    void *libHandle = nullptr;
    VADisplay vaDisplay = nullptr;
    VADisplayIsValidPFN vaDisplayIsValidPFN = [](VADisplay vaDisplay) { return 0; };
    VADeriveImagePFN vaDeriveImagePFN;
    VADestroyImagePFN vaDestroyImagePFN;
    VASyncSurfacePFN vaSyncSurfacePFN;
    VAExtGetSurfaceHandlePFN vaExtGetSurfaceHandlePFN;
    VAGetLibFuncPFN vaGetLibFuncPFN;
};
} // namespace OCLRT
