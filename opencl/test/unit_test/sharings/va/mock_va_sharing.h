/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"

#include "opencl/source/sharings/va/va_sharing.h"

#include <drm/drm_fourcc.h>
#include <va/va_drmcommon.h>

namespace NEO {

class VASharingFunctionsMock : public VASharingFunctions {
  public:
    using VASharingFunctions::mutex;
    using VASharingFunctions::supported2PlaneFormats;
    using VASharingFunctions::supported3PlaneFormats;
    using VASharingFunctions::supportedPackedFormats;

    VAImage mockVaImage = {};
    int32_t derivedImageFormatFourCC = VA_FOURCC_NV12;
    int32_t derivedImageFormatBpp = 8;
    uint16_t derivedImageHeight = 256;
    uint16_t derivedImageWidth = 256;
    VAStatus queryImageFormatsReturnStatus = VA_STATUS_SUCCESS;
    VAStatus syncSurfaceReturnStatus = VA_STATUS_SUCCESS;
    VAStatus deriveImageReturnStatus = VA_STATUS_SUCCESS;

    bool isValidDisplayCalled = false;
    bool deriveImageCalled = false;
    bool destroyImageCalled = false;
    bool syncSurfaceCalled = false;
    bool extGetSurfaceHandleCalled = false;
    bool exportSurfaceHandleCalled = false;
    VASurfaceID syncedSurfaceID = 0;
    osHandle acquiredVaHandle = 0;

    bool haveExportSurfaceHandle = false;

    uint32_t receivedSurfaceMemType = 0;
    uint32_t receivedSurfaceFlags = 0;

    VADRMPRIMESurfaceDescriptor mockVaSurfaceDesc{
        VA_FOURCC_NV12,
        256,
        256,
        1,
        {{8, 98304, I915_FORMAT_MOD_Y_TILED}, {}, {}, {}},
        2,
        {
            {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}},
            {DRM_FORMAT_GR88, 1, {}, {65536, 0, 0, 0}, {256, 0, 0, 0}},
            {0, 0, {}, {0, 0, 0, 0}, {0, 0, 0, 0}},
            {0, 0, {}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        }};

    VASharingFunctionsMock(VADisplay vaDisplay) : VASharingFunctions(vaDisplay) {}
    VASharingFunctionsMock() : VASharingFunctionsMock(nullptr){};

    VAStatus deriveImage(VASurfaceID vaSurface, VAImage *vaImage) override {
        if (deriveImageReturnStatus != VA_STATUS_SUCCESS) {
            return deriveImageReturnStatus;
        }
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

    VAStatus exportSurfaceHandle(VASurfaceID vaSurface, uint32_t memType, uint32_t flags, void *descriptor) override {
        exportSurfaceHandleCalled = true;
        receivedSurfaceMemType = memType;
        receivedSurfaceFlags = flags;
        if (haveExportSurfaceHandle) {
            if (memType != VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2) {
                return VA_STATUS_ERROR_UNSUPPORTED_MEMORY_TYPE;
            }
            *(static_cast<VADRMPRIMESurfaceDescriptor *>(descriptor)) = mockVaSurfaceDesc;
            return VA_STATUS_SUCCESS;
        }
        return VA_STATUS_ERROR_UNIMPLEMENTED;
    }

    VAStatus syncSurface(VASurfaceID vaSurface) override {
        syncSurfaceCalled = true;
        syncedSurfaceID = vaSurface;
        return syncSurfaceReturnStatus;
    }

    VAStatus queryImageFormats(VADisplay vaDisplay, VAImageFormat *formatList, int *numFormats) override {
        if (queryImageFormatsReturnStatus != VA_STATUS_SUCCESS) {
            return queryImageFormatsReturnStatus;
        }
        if (numFormats) {
            *numFormats = 6;
        }

        if (formatList) {
            formatList[0].fourcc = VA_FOURCC_NV12;
            formatList[0].bits_per_pixel = 12;
            formatList[0].byte_order = VA_LSB_FIRST;

            formatList[1].fourcc = VA_FOURCC_P010;
            formatList[1].bits_per_pixel = 10;
            formatList[1].byte_order = VA_LSB_FIRST;

            formatList[2].fourcc = VA_FOURCC_P016;
            formatList[2].bits_per_pixel = 16;
            formatList[2].byte_order = VA_LSB_FIRST;

            formatList[3].fourcc = VA_FOURCC_RGBP;
            formatList[3].bits_per_pixel = 8;
            formatList[3].byte_order = VA_LSB_FIRST;

            formatList[4].fourcc = VA_FOURCC_YUY2;
            formatList[4].bits_per_pixel = 16;
            formatList[4].byte_order = VA_LSB_FIRST;

            formatList[5].fourcc = VA_FOURCC_Y210;
            formatList[5].bits_per_pixel = 32;
            formatList[5].byte_order = VA_LSB_FIRST;
        }
        return VA_STATUS_SUCCESS;
    }

    int maxNumImageFormats(VADisplay vaDisplay) override {
        return 6;
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
