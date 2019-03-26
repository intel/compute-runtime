/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/sharings/va/va_sharing.h"

namespace NEO {

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
} // namespace NEO
