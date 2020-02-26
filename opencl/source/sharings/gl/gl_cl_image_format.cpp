/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/sharings/gl/gl_sharing.h"
#include "opencl/source/sharings/gl/gl_texture.h"

#include "GL/gl.h"
#include "config.h"

namespace NEO {
bool GlTexture::setClImageFormat(int glFormat, cl_image_format &clImgFormat) {
    auto clFormat = GlSharing::gLToCLFormats.find(static_cast<GLenum>(glFormat));

    if (clFormat != GlSharing::gLToCLFormats.end()) {
        clImgFormat.image_channel_data_type = clFormat->second.image_channel_data_type;
        clImgFormat.image_channel_order = clFormat->second.image_channel_order;
        return true;
    }
    return false;
}
} // namespace NEO
