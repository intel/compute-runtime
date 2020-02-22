/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <cstdint>

namespace NEO {

class MemObj;
class Image;

uint32_t getMipLevelOriginIdx(cl_mem_object_type imageType);

uint32_t findMipLevel(cl_mem_object_type imageType, const size_t *origin);

inline bool isMipMapped(const cl_image_desc &imgDesc) {
    return (imgDesc.num_mip_levels > 1);
}

bool isMipMapped(const MemObj *memObj);

uint32_t getMipOffset(Image *image, const size_t *origin);

} // namespace NEO
