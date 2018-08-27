/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
