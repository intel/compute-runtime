/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/mipmap.h"

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/mem_obj/image.h"

#include <cstdint>
#include <limits>

namespace OCLRT {

uint32_t getMipLevelOriginIdx(cl_mem_object_type imageType) {
    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE1D:
        return 1;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
    case CL_MEM_OBJECT_IMAGE2D:
        return 2;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
    case CL_MEM_OBJECT_IMAGE3D:
        return 3;
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        return 0;
    default:
        DEBUG_BREAK_IF(true);
        return std::numeric_limits<uint32_t>::max();
    }
}

uint32_t findMipLevel(cl_mem_object_type imageType, const size_t *origin) {
    size_t mipLevel = 0;
    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
    case CL_MEM_OBJECT_IMAGE3D:
        mipLevel = origin[getMipLevelOriginIdx(imageType)];
        break;
    default:
        mipLevel = 0;
        break;
    }

    return static_cast<uint32_t>(mipLevel);
}

bool isMipMapped(const MemObj *memObj) {
    auto image = castToObject<Image>(memObj);
    if (image == nullptr) {
        return false;
    }
    return isMipMapped(image->getImageDesc());
}

uint32_t getMipOffset(Image *image, const size_t *origin) {
    if (isMipMapped(image) == false) {
        return 0;
    }
    UNRECOVERABLE_IF(origin == nullptr);

    auto imageType = image->getImageDesc().image_type;
    GMM_REQ_OFFSET_INFO GMMReqInfo = {};
    GMMReqInfo.ReqLock = 1;
    GMMReqInfo.MipLevel = findMipLevel(imageType, origin);
    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        GMMReqInfo.ArrayIndex = static_cast<uint32_t>(origin[1]);
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        GMMReqInfo.ArrayIndex = static_cast<uint32_t>(origin[2]);
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        GMMReqInfo.Slice = static_cast<uint32_t>(origin[2]);
        break;
    default:
        break;
    }

    auto graphicsAlloc = image->getGraphicsAllocation();
    UNRECOVERABLE_IF(graphicsAlloc == nullptr);
    UNRECOVERABLE_IF(graphicsAlloc->gmm == nullptr);
    auto gmmResourceInfo = graphicsAlloc->gmm->gmmResourceInfo.get();
    UNRECOVERABLE_IF(gmmResourceInfo == nullptr);

    gmmResourceInfo->getOffset(GMMReqInfo);

    return GMMReqInfo.Lock.Offset;
}
} // namespace OCLRT
