/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/mipmap.h"

#include "opencl/source/mem_obj/image.h"

#include <cstdint>
#include <limits>

namespace NEO {

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

size_t getMipOffset(Image *image, const size_t *origin) {
    if (isMipMapped(image) == false) {
        return 0;
    }
    UNRECOVERABLE_IF(origin == nullptr);
    auto lod = findMipLevel(image->getImageDesc().image_type, origin);
    return image->getMipLevelsHostSize(lod);
}
} // namespace NEO
