/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/mem_obj/image.h"
#include "runtime/sharings/va/va_sharing.h"

namespace NEO {
class Context;
class Image;

class VASurface : VASharing {
  public:
    static Image *createSharedVaSurface(Context *context, VASharingFunctions *sharingFunctions,
                                        cl_mem_flags flags, VASurfaceID *surface, cl_uint plane,
                                        cl_int *errcodeRet);

    void synchronizeObject(UpdateData &updateData) override;

    void getMemObjectInfo(size_t &paramValueSize, void *&paramValue) override;

    static bool validate(cl_mem_flags flags, cl_uint plane);
    static const SurfaceFormatInfo *getExtendedSurfaceFormatInfo(uint32_t formatFourCC);
    static bool isSupportedFourCC(int fourcc);

  protected:
    VASurface(VASharingFunctions *sharingFunctions, VAImageID imageId,
              cl_uint plane, VASurfaceID *surfaceId, bool interopUserSync)
        : VASharing(sharingFunctions, imageId), plane(plane), surfaceId(surfaceId), interopUserSync(interopUserSync){};

    cl_uint plane;
    VASurfaceID *surfaceId;
    bool interopUserSync;
};
} // namespace NEO
