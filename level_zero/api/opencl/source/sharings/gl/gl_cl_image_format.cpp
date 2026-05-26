/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/gl/gl_sharing.h"
#include "level_zero/api/opencl/source/sharings/gl/gl_texture.h"

#include "GL/gl.h"
#include "config.h"

namespace NEO {
namespace LEO {

bool GlTexture::setClImageFormat(int glFormat, cl_image_format &clImgFormat) {
    auto clFormat = GlSharing::glToCLFormats.find(static_cast<GLenum>(glFormat));

    if (clFormat != GlSharing::glToCLFormats.end()) {
        clImgFormat.image_channel_data_type = clFormat->second.image_channel_data_type;
        clImgFormat.image_channel_order = clFormat->second.image_channel_order;
        return true;
    }
    return false;
}

} // namespace LEO
} // namespace NEO
