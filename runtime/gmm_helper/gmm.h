/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/gmm_helper/gmm_lib.h"
#include <cstdint>
#include <cstdlib>
#include <memory>

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
    Gmm(const void *alignedPtr, size_t alignedSize, bool uncacheable, bool preferRenderCompressed, bool systemMemoryPool, uint32_t devicesBitfield);
    Gmm(GMM_RESOURCE_INFO *inputGmm);

    void queryImageParams(ImageInfo &inputOutputImgInfo);

    uint32_t getRenderHAlignment();
    uint32_t getRenderVAlignment();

    void applyAuxFlagsForImage(ImageInfo &imgInfo);
    void applyAuxFlagsForBuffer(bool preferRenderCompression);
    void applyMemoryFlags(bool systemMemoryPool);

    bool unifiedAuxTranslationCapable() const;
    bool hasMultisampleControlSurface() const;

    uint32_t queryQPitch(GMM_RESOURCE_TYPE resType);
    void updateImgInfo(ImageInfo &imgInfo, cl_image_desc &imgDesc, cl_uint arrayIndex);
    uint8_t resourceCopyBlt(void *sys, void *gpu, uint32_t pitch, uint32_t height, unsigned char upload, OCLPlane plane);

    GMM_RESCREATE_PARAMS resourceParams = {};
    std::unique_ptr<GmmResourceInfo> gmmResourceInfo;

    bool isRenderCompressed = false;
    bool useSystemMemoryPool = true;
};
} // namespace OCLRT
