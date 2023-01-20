/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sharings/va/va_sharing.h"

namespace NEO {
class Context;
class Image;

struct SharedSurfaceInfo {
    uint32_t imageFourcc = 0;
    size_t imageOffset = 0;
    size_t imagePitch = 0;
    ImageInfo imgInfo = {};
    VAImageID imageId = 0;
    unsigned int sharedHandle = 0;
    cl_uint plane;

    cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    cl_channel_order channelOrder = CL_RG;
    cl_channel_type channelType = CL_UNORM_INT8;
};

class VASurface : VASharing {
  public:
    static Image *createSharedVaSurface(Context *context, VASharingFunctions *sharingFunctions,
                                        cl_mem_flags flags, cl_mem_flags_intel flagsIntel, VASurfaceID *surface, cl_uint plane,
                                        cl_int *errcodeRet);

    void synchronizeObject(UpdateData &updateData) override;

    void getMemObjectInfo(size_t &paramValueSize, void *&paramValue) override;

    static bool validate(cl_mem_flags flags, cl_uint plane);
    static const ClSurfaceFormatInfo *getExtendedSurfaceFormatInfo(uint32_t formatFourCC);
    static bool isSupportedFourCCTwoPlaneFormat(int fourcc);
    static bool isSupportedFourCCThreePlaneFormat(int fourcc);
    static bool isSupportedFourCCPackedFormat(int fourcc);
    static bool isSupportedPlanarFormat(uint32_t imageFourcc);
    static bool isSupportedPackedFormat(uint32_t imageFourcc);
    static VAStatus getSurfaceDescription(SharedSurfaceInfo &surfaceInfo, VASharingFunctions *sharingFunctions, VASurfaceID *surface);
    static void applyPlanarOptions(SharedSurfaceInfo &sharedSurfaceInfo, cl_uint plane, cl_mem_flags flags, bool supportOcl21);
    static void applyPackedOptions(SharedSurfaceInfo &sharedSurfaceInfo);
    static void applyPlaneSettings(SharedSurfaceInfo &sharedSurfaceInfo, cl_uint plane);

  protected:
    VASurface(VASharingFunctions *sharingFunctions, VAImageID imageId,
              cl_uint plane, VASurfaceID *surfaceId, bool interopUserSync)
        : VASharing(sharingFunctions, imageId), plane(plane), surfaceId(*surfaceId), interopUserSync(interopUserSync) {
        surfaceIdPtr = &this->surfaceId;
    };

    cl_uint plane;
    VASurfaceID surfaceId;
    VASurfaceID *surfaceIdPtr;
    bool interopUserSync;
};
} // namespace NEO
