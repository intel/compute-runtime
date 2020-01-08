/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gmm_helper/gmm_lib.h"
#include "core/helpers/surface_format_info.h"
#include "core/utilities/arrayref.h"

#include "CL/cl.h"

namespace NEO {

struct ClSurfaceFormatInfo {
    cl_image_format OCLImageFormat;
    SurfaceFormatInfo surfaceFormat;
};

class SurfaceFormats {
  private:
    static const ClSurfaceFormatInfo readOnlySurfaceFormats[];
    static const ClSurfaceFormatInfo writeOnlySurfaceFormats[];
    static const ClSurfaceFormatInfo readWriteSurfaceFormats[];
    static const ClSurfaceFormatInfo readOnlyDepthSurfaceFormats[];
    static const ClSurfaceFormatInfo readWriteDepthSurfaceFormats[];

    static const ClSurfaceFormatInfo packedYuvSurfaceFormats[];
    static const ClSurfaceFormatInfo planarYuvSurfaceFormats[];

  public:
    static ArrayRef<const ClSurfaceFormatInfo> readOnly() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> writeOnly() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readWrite() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> packedYuv() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> planarYuv() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readOnlyDepth() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readWriteDepth() noexcept;

    static ArrayRef<const ClSurfaceFormatInfo> surfaceFormats(cl_mem_flags flags) noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> surfaceFormats(cl_mem_flags flags, const cl_image_format *imageFormat) noexcept;
};

} // namespace NEO
