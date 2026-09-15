/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/image/image_format_desc_helper.h"

#include "third_party/opencl_headers/CL/cl_ext.h"

namespace L0 {

cl_channel_type getClChannelDataType(const ze_image_format_t &imgDescription) {
    switch (imgDescription.layout) {
    case ZE_IMAGE_FORMAT_LAYOUT_8:
    case ZE_IMAGE_FORMAT_LAYOUT_8_8:
    case ZE_IMAGE_FORMAT_LAYOUT_8_8_8:
    case ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8:
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_UINT) {
            return CL_UNSIGNED_INT8;
        }
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_SINT) {
            return CL_SIGNED_INT8;
        }
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_UNORM) {
            return CL_UNORM_INT8;
        }
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_SNORM) {
            return CL_SNORM_INT8;
        }
        break;
    case ZE_IMAGE_FORMAT_LAYOUT_16:
    case ZE_IMAGE_FORMAT_LAYOUT_16_16:
    case ZE_IMAGE_FORMAT_LAYOUT_16_16_16:
    case ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16:
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_UINT) {
            return CL_UNSIGNED_INT16;
        }
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_SINT) {
            return CL_SIGNED_INT16;
        }
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_UNORM) {
            return CL_UNORM_INT16;
        }
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_SNORM) {
            return CL_SNORM_INT16;
        }

        return CL_HALF_FLOAT;
    case ZE_IMAGE_FORMAT_LAYOUT_32:
    case ZE_IMAGE_FORMAT_LAYOUT_32_32:
    case ZE_IMAGE_FORMAT_LAYOUT_32_32_32:
    case ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32:
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_UINT) {
            return CL_UNSIGNED_INT32;
        }
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_SINT) {
            return CL_SIGNED_INT32;
        }
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_FLOAT) {
            return CL_FLOAT;
        }
        break;
    case ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2:
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_UNORM) {
            return CL_UNORM_INT_101010_2;
        }
        break;
    case ZE_IMAGE_FORMAT_LAYOUT_5_6_5:
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_UNORM) {
            return CL_UNORM_SHORT_565;
        }
        break;
    case ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1:
        if (imgDescription.type == ZE_IMAGE_FORMAT_TYPE_UNORM) {
            return CL_UNORM_SHORT_555;
        }
        break;
    case ZE_IMAGE_FORMAT_LAYOUT_NV12:
    case ZE_IMAGE_FORMAT_LAYOUT_YUYV:
    case ZE_IMAGE_FORMAT_LAYOUT_VYUY:
    case ZE_IMAGE_FORMAT_LAYOUT_YVYU:
    case ZE_IMAGE_FORMAT_LAYOUT_UYVY:
        return CL_UNORM_INT8;
    default:
        break;
    }

    return CL_INVALID_VALUE;
}

cl_channel_order getClChannelOrder(const ze_image_format_t &imgDescription, bool srgb) {
    // Swizzles cannot express the component interleaving of YUV layouts, the layout itself is the channel order.
    switch (imgDescription.layout) {
    case ZE_IMAGE_FORMAT_LAYOUT_NV12:
        return CL_NV12_INTEL;
    case ZE_IMAGE_FORMAT_LAYOUT_YUYV:
        return CL_YUYV_INTEL;
    case ZE_IMAGE_FORMAT_LAYOUT_VYUY:
        return CL_VYUY_INTEL;
    case ZE_IMAGE_FORMAT_LAYOUT_YVYU:
        return CL_YVYU_INTEL;
    case ZE_IMAGE_FORMAT_LAYOUT_UYVY:
        return CL_UYVY_INTEL;
    default:
        break;
    }

    Swizzles imgSwizzles{imgDescription.x, imgDescription.y, imgDescription.z, imgDescription.w};

    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_1}) {
        return CL_R;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_D, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_1}) {
        return CL_DEPTH;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_R}) {
        return CL_A;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_1}) {
        return CL_RG;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_G}) {
        return CL_RA;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_1}) {
        return CL_RGB;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A}) {
        return srgb ? CL_sRGBA : CL_RGBA;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_A}) {
        return srgb ? CL_sBGRA : CL_BGRA;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B}) {
        return CL_ARGB;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R}) {
        return CL_ABGR;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_1}) {
        return CL_LUMINANCE;
    }
    if (imgSwizzles == Swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_R}) {
        return CL_INTENSITY;
    }

    return CL_INVALID_VALUE;
}

bool isTwoPlaneYuv420Format(ze_image_format_layout_t layout) {
    return layout == ZE_IMAGE_FORMAT_LAYOUT_NV12 ||
           layout == ZE_IMAGE_FORMAT_LAYOUT_P010 ||
           layout == ZE_IMAGE_FORMAT_LAYOUT_P012 ||
           layout == ZE_IMAGE_FORMAT_LAYOUT_P016;
}

} // namespace L0
