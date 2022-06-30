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
