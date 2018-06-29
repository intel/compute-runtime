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

#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
#include "runtime/gmm_helper/gmm_lib.h"
#include "runtime/api/cl_types.h"

namespace OCLRT {
enum class OCLPlane;
struct HardwareInfo;
struct ImageInfo;
class GmmResourceInfo;

class Gmm {
  public:
    virtual ~Gmm() = default;
    Gmm() = delete;
    Gmm(ImageInfo &inputOutputImgInfo);
    Gmm(const void *alignedPtr, size_t alignedSize, bool uncacheable);
    Gmm(GMM_RESOURCE_INFO *inputGmm);

    void queryImageParams(ImageInfo &inputOutputImgInfo);

    uint32_t getRenderHAlignment();
    uint32_t getRenderVAlignment();

    void applyAuxFlagsForImage(ImageInfo &imgInfo);
    void applyAuxFlagsForBuffer(bool preferRenderCompression);
    bool unifiedAuxTranslationCapable() const;

    uint32_t queryQPitch(GMM_RESOURCE_TYPE resType);
    void updateImgInfo(ImageInfo &imgInfo, cl_image_desc &imgDesc, cl_uint arrayIndex);
    uint8_t resourceCopyBlt(void *sys, void *gpu, uint32_t pitch, uint32_t height, unsigned char upload, OCLPlane plane);

    GMM_RESCREATE_PARAMS resourceParams = {};
    std::unique_ptr<GmmResourceInfo> gmmResourceInfo;

    bool isRenderCompressed = false;
};
} // namespace OCLRT
