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
#include "runtime/helpers/aligned_memory.h"
#include "runtime/sharings/va/va_sharing.h"

namespace OCLRT {

extern int vaDisplayIsValidCalled;
extern int vaDeriveImageCalled;
extern int vaDestroyImageCalled;
extern int vaSyncSurfaceCalled;
extern int vaGetLibFuncCalled;
extern int vaExtGetSurfaceHandleCalled;
extern osHandle acquiredVaHandle;
extern VAImage mockVaImage;
extern uint16_t vaSharingFunctionsMockWidth, vaSharingFunctionsMockHeight;

class VASharingFunctionsMock : public VASharingFunctions {

    static int mockVaDisplayIsValid(VADisplay vaDisplay) {
        vaDisplayIsValidCalled++;
        return 1; // success
    };

    static VAStatus mockVaDeriveImage(VADisplay vaDisplay, VASurfaceID vaSurface, VAImage *vaImage) {
        uint32_t pitch;
        vaDeriveImageCalled++;
        vaImage->height = vaSharingFunctionsMockHeight;
        vaImage->width = vaSharingFunctionsMockWidth;
        pitch = alignUp(vaSharingFunctionsMockWidth, 128);
        vaImage->offsets[1] = alignUp(vaImage->height, 32) * pitch;
        vaImage->offsets[2] = vaImage->offsets[1] + 1;
        vaImage->pitches[0] = pitch;
        vaImage->pitches[1] = pitch;
        vaImage->pitches[2] = pitch;
        mockVaImage.width = vaImage->width;
        mockVaImage.height = vaImage->height;
        return (VAStatus)0; // success
    };

    static VAStatus mockVaDestroyImage(VADisplay vaDisplay, VAImageID vaImageId) {
        vaDestroyImageCalled++;
        return (VAStatus)0; // success
    };

    static VAStatus mockVaSyncSurface(VADisplay vaDisplay, VASurfaceID vaSurface) {
        vaSyncSurfaceCalled++;
        return (VAStatus)0; // success
    };

    static void *mockVaGetLibFunc(VADisplay vaDisplay, const char *func) {
        vaGetLibFuncCalled++;
        return nullptr;
    };

    static VAStatus mockExtGetSurfaceHandle(VADisplay vaDisplay, VASurfaceID *vaSurface, unsigned int *handleId) {
        vaExtGetSurfaceHandleCalled++;
        *handleId = acquiredVaHandle;
        return (VAStatus)0; // success
    };

    void initMembers();

  public:
    VASharingFunctionsMock(VADisplay vaDisplay) : VASharingFunctions(vaDisplay) {
        initMembers();
    }
    VASharingFunctionsMock() : VASharingFunctionsMock(nullptr){};
};

class MockVaSharing {
  public:
    void updateAcquiredHandle() {
        acquiredVaHandle = sharingHandle;
    }
    void updateAcquiredHandle(unsigned int handle) {
        sharingHandle = handle;
        acquiredVaHandle = sharingHandle;
    }

    VASharingFunctionsMock m_sharingFunctions;
    osHandle sharingHandle = 0;
};
} // namespace OCLRT
