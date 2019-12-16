/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/sharings/unified/unified_sharing.h"

namespace NEO {
class Image;
class Context;

class UnifiedImage : public UnifiedSharing {
    using UnifiedSharing::UnifiedSharing;

  public:
    static Image *createSharedUnifiedImage(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription description,
                                           const cl_image_format *imageFormat, const cl_image_desc *imageDesc, cl_int *errcodeRet);

  protected:
    static GraphicsAllocation *createGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description);
};
} // namespace NEO
