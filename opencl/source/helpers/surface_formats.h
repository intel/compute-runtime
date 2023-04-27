/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/utilities/arrayref.h"

#include "CL/cl.h"

namespace NEO {

struct ClSurfaceFormatInfo {
    cl_image_format oclImageFormat;
    SurfaceFormatInfo surfaceFormat;
};

class SurfaceFormats {
  private:
    static const ClSurfaceFormatInfo readOnlySurfaceFormats12[];
    static const ClSurfaceFormatInfo readOnlySurfaceFormats20[];
    static const ClSurfaceFormatInfo writeOnlySurfaceFormats[];
    static const ClSurfaceFormatInfo readWriteSurfaceFormats[];
    static const ClSurfaceFormatInfo readOnlyDepthSurfaceFormats[];
    static const ClSurfaceFormatInfo readWriteDepthSurfaceFormats[];

    static const ClSurfaceFormatInfo packedYuvSurfaceFormats[];
    static const ClSurfaceFormatInfo planarYuvSurfaceFormats[];
    static const ClSurfaceFormatInfo packedSurfaceFormats[];

  public:
    static ArrayRef<const ClSurfaceFormatInfo> readOnly12() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readOnly20() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> writeOnly() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readWrite() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> packedYuv() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> planarYuv() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> packed() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readOnlyDepth() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readWriteDepth() noexcept;

    static ArrayRef<const ClSurfaceFormatInfo> surfaceFormats(cl_mem_flags flags, bool supportsOcl20Features) noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> surfaceFormats(cl_mem_flags flags, const cl_image_format *imageFormat, bool supportsOcl20Features) noexcept;
};

} // namespace NEO
