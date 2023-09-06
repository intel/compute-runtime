/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/unified/unified_sharing.h"

#include "CL/cl.h"

namespace NEO {
class Image;
class Context;

class UnifiedImage : public UnifiedSharing {
    using UnifiedSharing::UnifiedSharing;

  public:
    static Image *createSharedUnifiedImage(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription description,
                                           const cl_image_format *imageFormat, const cl_image_desc *imageDesc, cl_int *errcodeRet);

    static void *swapGmm(GraphicsAllocation *graphicsAllocation, Context *context, ImageInfo *imgInfo);
};
} // namespace NEO
