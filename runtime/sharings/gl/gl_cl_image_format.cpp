/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_gl_private_intel.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/sharings/gl/gl_texture.h"

#include "GL/gl.h"
#include "config.h"

namespace NEO {
bool GlTexture::setClImageFormat(int glFormat, cl_image_format &clImgFormat) {
    switch (glFormat) {
    case GL_RGBA8:
        clImgFormat.image_channel_data_type = CL_UNORM_INT8;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA8I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT8;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA16:
        clImgFormat.image_channel_data_type = CL_UNORM_INT16;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA16I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT16;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA32I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT32;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA8UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT8;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA16UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT16;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA32UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT32;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA16F:
        clImgFormat.image_channel_data_type = CL_HALF_FLOAT;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA32F:
        clImgFormat.image_channel_data_type = CL_FLOAT;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA:
        clImgFormat.image_channel_data_type = CL_UNORM_INT8;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA8_SNORM:
        clImgFormat.image_channel_data_type = CL_SNORM_INT8;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_RGBA16_SNORM:
        clImgFormat.image_channel_data_type = CL_SNORM_INT16;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    case GL_BGRA:
        clImgFormat.image_channel_data_type = CL_UNORM_INT8;
        clImgFormat.image_channel_order = CL_BGRA;
        break;
    case GL_R8:
        clImgFormat.image_channel_data_type = CL_UNORM_INT8;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R8_SNORM:
        clImgFormat.image_channel_data_type = CL_SNORM_INT8;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R16:
        clImgFormat.image_channel_data_type = CL_UNORM_INT16;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R16_SNORM:
        clImgFormat.image_channel_data_type = CL_SNORM_INT16;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R16F:
        clImgFormat.image_channel_data_type = CL_HALF_FLOAT;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R32F:
        clImgFormat.image_channel_data_type = CL_FLOAT;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R8I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT8;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R16I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT16;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R32I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT32;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R8UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT8;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R16UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT16;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_R32UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT32;
        clImgFormat.image_channel_order = CL_R;
        break;
    case GL_DEPTH_COMPONENT32F:
        clImgFormat.image_channel_data_type = CL_FLOAT;
        clImgFormat.image_channel_order = CL_DEPTH;
        break;
    case GL_DEPTH_COMPONENT16:
        clImgFormat.image_channel_data_type = CL_UNORM_INT16;
        clImgFormat.image_channel_order = CL_DEPTH;
        break;
    case GL_DEPTH24_STENCIL8:
        clImgFormat.image_channel_data_type = CL_UNORM_INT24;
        clImgFormat.image_channel_order = CL_DEPTH_STENCIL;
        break;
    case GL_DEPTH32F_STENCIL8:
        clImgFormat.image_channel_data_type = CL_FLOAT;
        clImgFormat.image_channel_order = CL_DEPTH_STENCIL;
        break;
    case GL_SRGB8_ALPHA8:
        clImgFormat.image_channel_data_type = CL_UNORM_INT8;
        clImgFormat.image_channel_order = CL_sRGBA;
        break;
    case GL_RG8:
        clImgFormat.image_channel_data_type = CL_UNORM_INT8;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG8_SNORM:
        clImgFormat.image_channel_data_type = CL_SNORM_INT8;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG16:
        clImgFormat.image_channel_data_type = CL_UNORM_INT16;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG16_SNORM:
        clImgFormat.image_channel_data_type = CL_SNORM_INT16;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG16F:
        clImgFormat.image_channel_data_type = CL_HALF_FLOAT;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG32F:
        clImgFormat.image_channel_data_type = CL_FLOAT;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG8I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT8;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG16I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT16;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG32I:
        clImgFormat.image_channel_data_type = CL_SIGNED_INT32;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG8UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT8;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG16UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT16;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RG32UI:
        clImgFormat.image_channel_data_type = CL_UNSIGNED_INT32;
        clImgFormat.image_channel_order = CL_RG;
        break;
    case GL_RGB10:
        clImgFormat.image_channel_data_type = CL_UNORM_INT16;
        clImgFormat.image_channel_order = CL_RGBA;
        break;
    default:
        clImgFormat.image_channel_data_type = 0;
        clImgFormat.image_channel_order = 0;
        return false;
    }
    return true;
}
} // namespace NEO
