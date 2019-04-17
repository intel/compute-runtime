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

class VASharingFunctionsMock : public VASharingFunctions {
  public:
    VAImage mockVaImage = {};
    int32_t derivedImageFormatFourCC = VA_FOURCC_NV12;
    int32_t derivedImageFormatBpp = 8;
    uint16_t derivedImageHeight = 256;
    uint16_t derivedImageWidth = 256;

    bool isValidDisplayCalled = false;
    bool deriveImageCalled = false;
    bool destroyImageCalled = false;
    bool syncSurfaceCalled = false;
    bool extGetSurfaceHandleCalled = false;

    osHandle acquiredVaHandle = 0;

    VASharingFunctionsMock(VADisplay vaDisplay) : VASharingFunctions(vaDisplay) {}
    VASharingFunctionsMock() : VASharingFunctionsMock(nullptr){};

    VAStatus deriveImage(VASurfaceID vaSurface, VAImage *vaImage) override {
        deriveImageCalled = true;
        uint32_t pitch;
        vaImage->height = derivedImageHeight;
        vaImage->width = derivedImageWidth;
        pitch = alignUp(derivedImageWidth, 128);
        vaImage->offsets[1] = alignUp(vaImage->height, 32) * pitch;
        vaImage->offsets[2] = vaImage->offsets[1] + 1;
        vaImage->pitches[0] = pitch;
        vaImage->pitches[1] = pitch;
        vaImage->pitches[2] = pitch;
        vaImage->format.fourcc = derivedImageFormatFourCC;
        vaImage->format.bits_per_pixel = derivedImageFormatBpp;
        mockVaImage.width = vaImage->width;
        mockVaImage.height = vaImage->height;
        return VA_STATUS_SUCCESS;
    }

    bool isValidVaDisplay() override {
        isValidDisplayCalled = true;
        return 1;
    }

    VAStatus destroyImage(VAImageID vaImageId) override {
        destroyImageCalled = true;
        return VA_STATUS_SUCCESS;
    }

    VAStatus extGetSurfaceHandle(VASurfaceID *vaSurface, unsigned int *handleId) override {
        extGetSurfaceHandleCalled = true;
        *handleId = acquiredVaHandle;
        return VA_STATUS_SUCCESS;
    }

    VAStatus syncSurface(VASurfaceID vaSurface) override {
        syncSurfaceCalled = true;
        return VA_STATUS_SUCCESS;
    }
};

class MockVaSharing {
  public:
    void updateAcquiredHandle() {
        sharingFunctions.acquiredVaHandle = sharingHandle;
    }
    void updateAcquiredHandle(unsigned int handle) {
        sharingHandle = handle;
        sharingFunctions.acquiredVaHandle = sharingHandle;
    }

    VASharingFunctionsMock sharingFunctions;
    osHandle sharingHandle = 0;
};
} // namespace NEO
