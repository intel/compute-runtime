/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/utilities/arrayref.h"

#include "CL/cl.h"
#include "CL/cl_gl.h"

namespace NEO {
namespace LEO {

struct ClSurfaceFormatInfo {
    cl_image_format oclImageFormat;
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
    static const ClSurfaceFormatInfo packedSurfaceFormats[];

  public:
    static ArrayRef<const ClSurfaceFormatInfo> readOnly() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> writeOnly() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readWrite() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> packedYuv() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> planarYuv() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> packed() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readOnlyDepth() noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> readWriteDepth() noexcept;

    static ArrayRef<const ClSurfaceFormatInfo> surfaceFormats(cl_mem_flags flags) noexcept;
    static ArrayRef<const ClSurfaceFormatInfo> surfaceFormats(cl_mem_flags flags, const cl_image_format *imageFormat) noexcept;

    static bool isImage2d(cl_mem_object_type imageType) {
        return imageType == CL_MEM_OBJECT_IMAGE2D;
    }

    static bool isImage2dOr2dArray(cl_mem_object_type imageType) {
        return imageType == CL_MEM_OBJECT_IMAGE2D || imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY;
    }

    static bool isImage3d(cl_mem_object_type imageType) {
        return imageType == CL_MEM_OBJECT_IMAGE3D;
    }

    static bool isImageArray(cl_mem_object_type imageType) {
        return (imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY || imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY);
    }

    static bool isDepthFormat(const cl_image_format &imageFormat) {
        return imageFormat.image_channel_order == CL_DEPTH || imageFormat.image_channel_order == CL_DEPTH_STENCIL;
    }

    static size_t getImageHeight(const cl_image_desc &imageDesc) {
        switch (imageDesc.image_type) {
        case CL_MEM_OBJECT_IMAGE3D:
        case CL_MEM_OBJECT_IMAGE2D:
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
            return imageDesc.image_height;
        default:
            return 1u;
        }
    }

    static size_t getImageDepth(const cl_image_desc &imageDesc) {
        switch (imageDesc.image_type) {
        case CL_MEM_OBJECT_IMAGE3D:
            return imageDesc.image_depth;
        default:
            return 1u;
        }
    }

    static size_t getImageArraySize(const cl_image_desc &imageDesc) {
        switch (imageDesc.image_type) {
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
            return imageDesc.image_array_size;
        default:
            return 1u;
        }
    }
};

} // namespace LEO
} // namespace NEO
