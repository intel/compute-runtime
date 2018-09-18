/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/surface_formats.h"

#include "GL/gl.h"
#include "GL/glext.h"

using namespace OCLRT;
namespace OCLRT {
GMM_CUBE_FACE_ENUM GmmHelper::getCubeFaceIndex(uint32_t target) {
    switch (target) {
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        return __GMM_CUBE_FACE_NEG_X;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        return __GMM_CUBE_FACE_POS_X;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        return __GMM_CUBE_FACE_NEG_Y;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        return __GMM_CUBE_FACE_POS_Y;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return __GMM_CUBE_FACE_NEG_Z;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        return __GMM_CUBE_FACE_POS_Z;
    }
    return __GMM_NO_CUBE_MAP;
}
} // namespace OCLRT

void Gmm::applyAuxFlagsForImage(ImageInfo &imgInfo) {}
void Gmm::applyAuxFlagsForBuffer(bool preferRenderCompression) {}

void Gmm::applyMemoryFlags(bool systemMemoryPool) { this->useSystemMemoryPool = systemMemoryPool; }
