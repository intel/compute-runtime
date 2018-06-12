/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/accelerators/intel_motion_estimation.h"

namespace OCLRT {

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
} // namespace OCLRT
