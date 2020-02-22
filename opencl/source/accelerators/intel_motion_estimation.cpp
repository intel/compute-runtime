/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/accelerators/intel_motion_estimation.h"

namespace NEO {

cl_int VmeAccelerator::validateVmeArgs(Context *context,
                                       cl_accelerator_type_intel typeId,
                                       size_t descriptorSize,
                                       const void *descriptor) {
    const cl_motion_estimation_desc_intel *descObj =
        (const cl_motion_estimation_desc_intel *)descriptor;

    DEBUG_BREAK_IF(!context);
    DEBUG_BREAK_IF(typeId != CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL);

    if ((descriptorSize != sizeof(cl_motion_estimation_desc_intel)) ||
        (descriptor == NULL)) {
        return CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL;
    }

    switch (descObj->mb_block_type) {
    case CL_ME_MB_TYPE_16x16_INTEL:
    case CL_ME_MB_TYPE_8x8_INTEL:
    case CL_ME_MB_TYPE_4x4_INTEL:
        break;
    default:
        return CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL;
    }

    switch (descObj->subpixel_mode) {
    case CL_ME_SUBPIXEL_MODE_INTEGER_INTEL:
    case CL_ME_SUBPIXEL_MODE_HPEL_INTEL:
    case CL_ME_SUBPIXEL_MODE_QPEL_INTEL:
        break;
    default:
        return CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL;
    }

    switch (descObj->sad_adjust_mode) {
    case CL_ME_SAD_ADJUST_MODE_NONE_INTEL:
    case CL_ME_SAD_ADJUST_MODE_HAAR_INTEL:
        break;
    default:
        return CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL;
    }

    switch (descObj->search_path_type) {
    case CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL:
    case CL_ME_SEARCH_PATH_RADIUS_4_4_INTEL:
    case CL_ME_SEARCH_PATH_RADIUS_16_12_INTEL:
        break;
    default:
        return CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL;
    }

    return CL_SUCCESS;
}
} // namespace NEO
