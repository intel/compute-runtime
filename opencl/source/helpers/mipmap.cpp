/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/mipmap.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "opencl/source/mem_obj/image.h"

#include <algorithm>
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

uint32_t getMipOffset(Image *image, const size_t *origin) {
    if (isMipMapped(image) == false) {
        return 0;
    }
    UNRECOVERABLE_IF(origin == nullptr);
    auto bytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    size_t offset{};
    auto imageType = image->getImageDesc().image_type;
    auto lod = findMipLevel(imageType, origin);
    auto baseWidth = image->getImageDesc().image_width;
    auto baseHeight = image->getImageDesc().image_height;
    if (lod) {
        size_t mipHeight = baseHeight;
        size_t mipWidth = baseWidth;
        bool translate = false;
        if (lod >= 2) {
            translate = true;
            mipWidth += std::max<size_t>(baseWidth >> 2, 1);
        }
        for (size_t currentLod = 3; currentLod <= lod; currentLod++) {
            mipHeight += std::max<size_t>(baseHeight >> currentLod, 1);
            mipWidth += std::max<size_t>(baseWidth >> currentLod, 1);
        }
        if (imageType == CL_MEM_OBJECT_IMAGE1D) {
            offset = mipWidth;
        } else {
            offset = baseWidth * mipHeight;
            if (translate) {
                offset += std::max<size_t>(baseWidth >> 1, 1);
            }
        }
    }
    return static_cast<uint32_t>(bytesPerPixel * offset);
}
} // namespace NEO
